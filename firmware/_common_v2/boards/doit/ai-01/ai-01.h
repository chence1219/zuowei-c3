
#include "wifi_board.h"
#include "audio/codecs/vb6824_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
// #include "iot/thing_manager.h"

#include <esp_log.h>

#define TAG "AI01"

class AI01 : public WifiBoard {
private:
    Button boot_button_;
    VbAduioCodec audio_codec_;

    virtual void InitializeButtons() {
        boot_button_.OnClick([this]() {
#ifdef CONFIG_VB6824_OTA_SUPPORT
            if (audio_codec_.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
#endif
            auto &app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if(count >= 3){
#ifdef CONFIG_VB6824_OTA_SUPPORT
                if (audio_codec_.InOtaMode(1) == true) {
                    ESP_LOGI(TAG, "OTA mode, do not enter chat");
                    return;
                }
#endif
                ResetWifiConfiguration();
            }
        });
#ifdef CONFIG_VB6824_OTA_SUPPORT
        boot_button_.OnLongPress([this]() {
            if (esp_timer_get_time() > 20 * 1000 * 1000) {
                ESP_LOGI(TAG, "Long press, do not enter OTA mode %ld", (uint32_t)esp_timer_get_time());
                return;
            }
            int ret = audio_codec_.OtaStart(0); 
            if(ret == VbAduioCodec::OTA_ERR_NOT_SUPPORT){
                ESP_LOGW(TAG, "Please enable VB6824_OTA_SUPPORT");
            }
        });
#endif
    }

    // // 物联网初始化，添加对 AI 可见设备
    // virtual void InitializeIot() {
    //     // auto& thing_manager = iot::ThingManager::GetInstance();
    //     // thing_manager.AddThing(iot::CreateThing("Speaker"));
    // }


public:
    AI01() : 
            boot_button_(BOOT_BUTTON_GPIO, false, 3000), 
            audio_codec_(CODEC_TX_GPIO, CODEC_RX_GPIO) {

        InitializeButtons();

        // InitializeIot();


        audio_codec_.OnWakeUp([this](const std::string& command) {
            if (command == std::string(vb6824_get_wakeup_word())){
                if(Application::GetInstance().GetDeviceState() != kDeviceStateListening){
                    Application::GetInstance().WakeWordInvoke("你好");
                }
            }else if (command == "开始配网"){
                ResetWifiConfiguration();
            }
        });
    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec_;
    }
};
