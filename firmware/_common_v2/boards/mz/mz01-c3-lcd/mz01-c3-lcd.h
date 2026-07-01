
#include "wifi_board.h"
#include "audio_codecs/vb6824_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>

#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>

#include "settings.h"
#include "assets/lang_config.h"

#include "power_save_timer.h"

#include "esp_timer.h"

#include "../firmware/_common/display/zw_lcd_display.h"
#include "../firmware/_common/iot/zw_screen.h"
#ifdef CONFIG_IOT_ALARM  
#include "../firmware/_common/iot/alarm.h"
#endif
#include "../firmware/_common/iot/low_power.h"
#include "../firmware/_common/utils/power_manager.h"

#define TAG "Mz01C3Lcd"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class Mz01C3Lcd : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    VbAduioCodec audio_codec_;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
#ifdef CONFIG_IOT_ALARM  
    iot::Alarm *alarm_ = nullptr;
#endif
    iot::ZwScreen *screen_ = nullptr;
    iot::LowPower *low_power_ = nullptr;
    esp_timer_handle_t lcd_bl_timer_ = nullptr;
    esp_timer_handle_t boot_sound_timer_ = nullptr;

#if defined(VOLUME_UP_BUTTON_GPIO) && defined(VOLUME_DOWN_BUTTON_GPIO) && defined(CONFIG_USE_DEVICE_AEC)
    int volume_up_press = 0;
    int volume_down_press = 0;
    int volume_up_down_press = 0;

    void ToggleRealtimeChatMode(){
        auto &app = Application::GetInstance();
        app.SetRealtimeChatEnable(!app.GetRealtimeChatEnable());
        Settings settings("realtime", true);
        settings.SetInt("enable", app.GetRealtimeChatEnable());
        ESP_LOGI(TAG,"set realtime enable: %d", app.GetRealtimeChatEnable());
        app.Schedule([](){
            auto &app = Application::GetInstance();
            app.Close();
            app.Schedule([](){
                auto &app = Application::GetInstance();
                if(app.GetDeviceState() == kDeviceStateIdle){
                    auto display = Board::GetInstance().GetDisplay();
                    if(app.GetRealtimeChatEnable()){
                        display->ShowNotification(Lang::Strings::REALTIME_ABORT, 1000);
                    }else{
                        display->ShowNotification(Lang::Strings::WAKEUP_WORD_ABORT, 1000);
                    }
                }else{
                    app.Schedule([]() {
                        auto &app = Application::GetInstance();
                        auto display = Board::GetInstance().GetDisplay();
                        if(app.GetRealtimeChatEnable()){
                            display->ShowNotification(Lang::Strings::REALTIME_ABORT, 1000);
                        }else{
                            display->ShowNotification(Lang::Strings::WAKEUP_WORD_ABORT, 1000);
                        }
                    });
                }
            });
        });
    }
#endif

#if defined(BATTERY_CHANG_GPIO) || defined(BATTERY_ADC_GPIO)
    void InitializePowerManager() {
#if defined(BATTERY_CHANG_GPIO) && defined(BATTERY_ADC_GPIO)
        power_manager_ = new PowerManager(BATTERY_CHANG_GPIO, BATTERY_ADC_GPIO, BATTERY_VOLTAGE_MAX, BATTERY_VOLTAGE_MIN);
#elif defined(BATTERY_CHANG_GPIO)
        power_manager_ = new PowerManager(BATTERY_CHANG_GPIO, GPIO_NUM_NC, 0, 0);
#elif defined(BATTERY_ADC_GPIO)
        power_manager_ = new PowerManager(GPIO_NUM_NC, BATTERY_ADC_GPIO, BATTERY_VOLTAGE_MAX, BATTERY_VOLTAGE_MIN);
#endif
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }
#endif

#ifdef POWER_KRRP_GPIO
    void InitializePowerSaveTimer() {

        gpio_set_direction(POWER_KRRP_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode(POWER_KRRP_GPIO, GPIO_PULLDOWN_ONLY);
        gpio_set_level(POWER_KRRP_GPIO, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            if(low_power_ == NULL || low_power_->IsAutoPoweroff()){
                ESP_LOGW(TAG, "Enabling sleep mode");
                display_->SetChatMessage("system", "");
                display_->SetEmotion("sleepy");
                GetBacklight()->SetBrightness(10);
            }
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGW(TAG, "Enabling sleep mode");
            gpio_set_level(POWER_KRRP_GPIO, 1);
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGW(TAG, "Shutting down");
            if(low_power_ == NULL || low_power_->IsAutoPoweroff()){
                gpio_set_level(POWER_KRRP_GPIO, 0);
            }
        });
        power_save_timer_->SetEnabled(true);
    }
#endif

    void InitializeButtons() {
#ifdef BOOT_BUTTON_GPIO
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (audio_codec_.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
#ifdef CONFIG_IOT_ALARM  
            if(alarm_ && alarm_->IsAlarm()){
                alarm_->StopAlarm();
            }
#endif
            power_save_timer_->WakeUp();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeaDone([this](uint16_t count) {
            if(count == 1){
                return;
            }
            
            if (audio_codec_.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }

#if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT == 1
            if(count >= 7){
                int ret = audio_codec_.OtaStart(0); 
                if(ret == VbAduioCodec::OTA_ERR_NOT_SUPPORT){
                    ESP_LOGW(TAG, "Please enable VB6824_OTA_SUPPORT");
                }
            }else 
#endif

#ifdef CONFIG_USE_CUSTOM_OTA
            if(count >= 5){
                auto &app = Application::GetInstance();
                app.StartCheckNewVersionForCustom();
            }else 
#endif
            if(count >= 3){
                ResetWifiConfiguration();
            }
        });
#ifdef POWER_KRRP_GPIO
        boot_button_.OnLongPress([this]() {
           gpio_set_level(POWER_KRRP_GPIO, 0);
        });
#endif
#endif

#if defined(VOLUME_UP_BUTTON_GPIO) && defined(VOLUME_DOWN_BUTTON_GPIO) && defined(CONFIG_USE_DEVICE_AEC)
    volume_up_button_.OnPressDown([this]() {
            volume_up_press = 1;
            if(volume_up_press == 1 && volume_down_press == 1){
                volume_up_down_press = 1;
            }
        });
        volume_up_button_.OnPressUp([this]() {
            volume_up_press = 2; 
        });

        volume_down_button_.OnPressDown([this]() {
            volume_down_press = 1;
            if(volume_up_press == 1 && volume_down_press == 1){
                volume_up_down_press = 1;
            }
        });
        volume_down_button_.OnPressUp([this]() {
            volume_down_press = 2; 
        });
#endif

#ifdef VOLUME_UP_BUTTON_GPIO
        volume_up_button_.OnClick([this]() {
#if defined(VOLUME_UP_BUTTON_GPIO) && defined(VOLUME_DOWN_BUTTON_GPIO) && defined(CONFIG_USE_DEVICE_AEC)
            if(volume_up_down_press){
                if(volume_up_press == 2 && volume_down_press == 2){
                    ToggleRealtimeChatMode();
                }
                volume_up_press = 0;
                if(volume_up_press == 0 && volume_down_press == 0){
                    volume_up_down_press = 0;
                }
                return;
            }
#endif
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });
#endif
#ifdef VOLUME_DOWN_BUTTON_GPIO
        volume_down_button_.OnClick([this]() {
#if defined(VOLUME_UP_BUTTON_GPIO) && defined(VOLUME_DOWN_BUTTON_GPIO) && defined(CONFIG_USE_DEVICE_AEC)
            if(volume_up_down_press){
                if(volume_up_press == 2 && volume_down_press == 2){
                    ToggleRealtimeChatMode();
                }
                volume_down_press = 0;
                if(volume_up_press == 0 && volume_down_press == 0){
                    volume_up_down_press = 0;
                }
                return;
            }
#endif
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
#endif
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
#ifdef CONFIG_USE_CUSTOM_OTA
        thing_manager.AddThing(iot::CreateThing("CustomOTA"));
#endif
#ifdef CONFIG_IOT_ZW_SCREEN
        screen_ = new iot::ZwScreen();
        thing_manager.AddThing(screen_);
#else
        thing_manager.AddThing(iot::CreateThing("Screen"));
#endif
#ifdef CONFIG_IOT_ALARM        
        alarm_ = new iot::Alarm();
        thing_manager.AddThing(alarm_);
#endif
#ifdef CONFIG_IOT_LOWPOWER
        low_power_ = new iot::LowPower();
        thing_manager.AddThing(low_power_);
#endif
    }

#ifdef DISPLAY_WIDTH
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        // buscfg.flags = SPICOMMON_BUSFLAG_MASTER;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {};

        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_lcd_panel_reset(panel);

        AngleMap angle_map_ = {(DISPLAY_ANGLE)%360, (DISPLAY_ANGLE+90)%360, (DISPLAY_ANGLE+180)%360, (DISPLAY_ANGLE+270)%360};

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new ZwLcdDisplay(panel_io, panel, 
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
                                        .emoji_font = font_emoji_64_init(),
                                    },
                                    angle_map_);
    }
#endif

public:
    Mz01C3Lcd() : 
#ifdef BOOT_BUTTON_GPIO
            boot_button_(BOOT_BUTTON_GPIO, false, 2000), 
#endif 
#ifdef VOLUME_UP_BUTTON_GPIO
            volume_up_button_(VOLUME_UP_BUTTON_GPIO, false, 0, 50),
#endif
#ifdef VOLUME_DOWN_BUTTON_GPIO
            volume_down_button_(VOLUME_DOWN_BUTTON_GPIO, false, 0, 50),
#endif
            audio_codec_(CODEC_TX_GPIO, CODEC_RX_GPIO){          
        
#if defined(VOLUME_UP_BUTTON_GPIO) && defined(VOLUME_DOWN_BUTTON_GPIO) && defined(CONFIG_USE_DEVICE_AEC)
        Settings settings("realtime", false);
        bool realtime_chat_enabled = settings.GetInt("enable", true);
        auto &app = Application::GetInstance();
        app.SetRealtimeChatEnable(realtime_chat_enabled);
#endif

#if defined(BATTERY_CHANG_GPIO) || defined(BATTERY_ADC_GPIO)       
        InitializePowerManager();
#endif

#ifdef POWER_KRRP_GPIO
        InitializePowerSaveTimer();
#endif

        InitializeButtons();

#ifdef DISPLAY_WIDTH
        InitializeSpi();
        InitializeLcdDisplay();
        // GetBacklight()->RestoreBrightness();
#endif
        InitializeIot();

        audio_codec_.OnWakeUp([this](const std::string& command) {
            if (command == std::string(vb6824_get_wakeup_word())){
                if(Application::GetInstance().GetDeviceState() != kDeviceStateListening){
                    Application::GetInstance().WakeWordInvoke("你好");
                }
            }else if (command == "开始配网"){
                ResetWifiConfiguration();
            }
        });

#ifdef DISPLAY_BACKLIGHT_PIN
        if(lcd_bl_timer_ == nullptr){
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto this_ = (Mz01C3Lcd*)arg;
                    this_->GetBacklight()->RestoreBrightness();
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "lcd_bl",
                .skip_unhandled_events = true,
            };
            esp_timer_create(&timer_args, &lcd_bl_timer_);
        }
        if(lcd_bl_timer_){
            esp_timer_start_once(lcd_bl_timer_, 230*1000);
        }
#endif

#ifdef CONFIG_POWER_ON_SOUNDS
        if(boot_sound_timer_ == nullptr){
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto this_ = (Mz01C3Lcd*)arg;
                    esp_reset_reason_t reason = esp_reset_reason();
                    if(reason == ESP_RST_POWERON || reason == ESP_RST_DEEPSLEEP){
                        Application::GetInstance().PlaySound(Lang::Sounds::P3_POWER_ON);
                    }
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "boot_sound_timer",
                .skip_unhandled_events = true,
            };
            esp_timer_create(&timer_args, &boot_sound_timer_);
        }
        if(boot_sound_timer_){
            esp_timer_start_once(boot_sound_timer_, 150*1000);
        }
#endif

#ifdef CONFIG_NET_STA_SOUNDS
        esp_event_handler_register(WIFI_EVENT, 
                                    ESP_EVENT_ANY_ID,
                                    [](void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
                                        static bool conn_status = false;
                                        if(event_id == WIFI_EVENT_STA_CONNECTED){
                                            if(conn_status == false){
                                                conn_status = true;
                                                Application::GetInstance().PlaySound(Lang::Sounds::P3_NET_CONNECT);
                                            }
                                        }else if(event_id == WIFI_EVENT_STA_DISCONNECTED){
                                            if(conn_status == true){
                                                conn_status = false;
                                                Application::GetInstance().PlaySound(Lang::Sounds::P3_NET_DISCONNECT);
                                            }
                                        }
                                    },
                                    NULL);
#endif
    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec_;
    }

#ifdef DISPLAY_WIDTH
    virtual Display* GetDisplay() override {
        return display_;
    }
#endif

#ifdef DISPLAY_BACKLIGHT_PIN
    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
#endif

#ifdef BATTERY_ADC_GPIO
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }
#endif

#ifdef POWER_KRRP_GPIO
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
#endif

#ifdef BUILTIN_LED_GPIO
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }
#endif
};