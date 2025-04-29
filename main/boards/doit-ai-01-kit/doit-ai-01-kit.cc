
#include "wifi_board.h"
#include "audio_codecs/vb6824_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>

#if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT == 1
#include <random>
#include "vb6824.h"
#include "wifi_station.h"
#endif

#define TAG "CustomBoard"

class CustomBoard : public WifiBoard {
private:
    Button boot_button_;
    VbAduioCodec audio_codec;
#if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT == 1
    std::string code;
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (jl_ws_is_start() == 1) {
                app.ShowOtaInfo(code);
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if(count >= 3 && jl_ws_is_start() == 0){
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnLongPress([this]() {
            if (esp_timer_get_time() > 20 * 1000 * 1000) {
                ESP_LOGI(TAG, "Long press, do not enter OTA mode %ld", (uint32_t)esp_timer_get_time());
                return;
            }
            
            if (jl_ws_is_start() == 1) {
                auto &app = Application::GetInstance();
                app.ShowOtaInfo(code);
                return;
            }
            EnterVBOta();
        });
    }
#else
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if(count >= 3){
                ResetWifiConfiguration();
            }
        });
    }

#endif
    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

#if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT == 1
    std::string generateRandomFourDigitString() {
        // 设置随机种子（仅需设置一次，可以放在更高层）
        static bool seed_initialized = false;
        if (!seed_initialized) {
            srand(time(NULL)); // 使用当前时间初始化种子
            seed_initialized = true;
        }
    
        // 生成四位数字
        char result[5]; // 创建一个 5 字节的字符数组（包括字符串的结束符 '\0'）
        for (int i = 0; i < 4; i++) {
            result[i] = '0' + (rand() % 10); // 生成随机数字并转换为字符
        }
        result[4] = '\0'; // 手动添加字符串结束符
        return std::string(result); // 返回生成的随机字符串
    }


    void EnterVBOta(uint8_t mode = 0) {

        auto& app = Application::GetInstance();
        auto& wifi_station = WifiStation::GetInstance();
        if(!wifi_station.IsConnected() || app.GetDeviceState() == kDeviceStateActivating) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            app.Schedule([this]() {
                this->EnterVBOta(1);
            });
            return;
        }
        if (!wifi_station.IsConnected() && mode == 0) {
            return;
        }
        if (jl_ws_is_start() == 1)
        {
            app.ShowOtaInfo(code);
            return;
        }
        
        ESP_LOGI(TAG, "升级模式");
        audio_codec.SetOutputVolume(100);
        if (mode == 0){
            code = generateRandomFourDigitString();
        }else{
            code = "0000";
        }
        app.ShowOtaInfo(code);
        jl_ws_start((char*)code.c_str());
    }
#endif

public:
    CustomBoard() : boot_button_(BOOT_BUTTON_GPIO,false,3000), audio_codec(CODEC_TX_GPIO, CODEC_RX_GPIO){          
        InitializeButtons();
        InitializeIot();
        audio_codec.OnWakeUp([this](const std::string& command) {
            if (command == std::string(vb6824_get_wakeup_word())){
                if(Application::GetInstance().GetDeviceState() != kDeviceStateListening){
                    Application::GetInstance().WakeWordInvoke("你好小智");
                }
            }else if (command == "开始配网"){
                ResetWifiConfiguration();
            }
        });
#if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT == 1
        audio_codec.OnEvent([this](vb6824_evt_t event_id, uint32_t data) {
            ESP_LOGW(TAG, "event_id: %d %ld", event_id, data);
            if (event_id == VB6824_EVT_OTA_ENTER) {
                if (data == 0 && esp_timer_get_time() > 20 * 1000 * 1000)
                {
                    return; 
                }
                
                EnterVBOta(data);
            }else if (event_id == VB6824_EVT_OTA_START) {
                ESP_LOGI(TAG, "OTA START");
                auto& app = Application::GetInstance();
                app.ReleaseDecoder();
            }
        });
#endif

    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec;
    }
};

DECLARE_BOARD(CustomBoard);
