#pragma once

#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <ssid_manager.h>
#include <wifi_configuration_ap.h>
#include <wifi_station.h>

#include "application.h"
#include "assets/lang_config.h"
#include "audio/codecs/vb_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "earth_light.h"
#include "esp_lcd_panel_st7789_fix.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "led/single_led.h"
#include "mcp_aec_controller.hpp"
#include "../../../_common/mcp/mcp_alarm.hpp"
#include "mcp_ota_checker.hpp"
#include "settings.h"
#include "soc/gpio_num.h"
#include "vb_adapter.h"
#include "vb_music_controller.hpp"
#include "wifi_board.h"
#include <ssid_manager.h>
#include <wifi_configuration_ap.h>
#include <wifi_station.h>

#include "xj_lcd_display.hpp"

// #ifdef CONFIG_USE_BLUFI_NET_CONFIGURING
// #include "doit_blufi.h"
// #include "doit_blufi_storage.h"
// #include "esp_mac.h"
// #endif

#define TAG "Mz01C3Lcd"

class Mz01C3Lcd : public WifiBoard {
public:
  bool should_resume_bl_play_ = false;
  bool should_resume_bl_play_from_alarm_ = false;

 private:
  Button *boot_button_ = nullptr;
  vbAudioCodec *audio_codec_ = nullptr;
  esp_timer_handle_t boot_sound_timer_ = nullptr;
  esp_timer_handle_t toast_timer_ = nullptr;
  Led *led_ = nullptr;
  Display *display_ = nullptr;
  Backlight *backlight_ = nullptr;

  Settings *settings_7014_ = nullptr;
  std::string music_name = "";
  std::string music_lyric = "";
  uint32_t play_time = 0;
  uint32_t play_duration = 0;
  lv_obj_t *toast_cont = NULL;
  lv_obj_t *toast_label = NULL;
  bool is_alarm_ringing_ = false;
  

#if defined(CONFIG_USE_DEVICE_AEC)
  void ToggleRealtimeChatMode();
#endif

  void ForceIdle();
  void InitializeButtons();
  void InitializeSpi();
  void InitializeLcdDisplay();
  void DisableDisplayHardware();

public:
  Mz01C3Lcd();

  void ToastOpen(uint32_t timeout_ms, const char *text);
  void ToastOpen(uint32_t timeout_ms, const char *text, lv_color_t text_color);
  void ToastForceClose();

  virtual AudioCodec *GetAudioCodec() override;
  virtual Display *GetDisplay() override;
  virtual Backlight *GetBacklight() override;
  virtual Led *GetLed() override;
  virtual void OnStateChanged() override;

  //   virtual void EnterWifiConfigMode() override {
  // #ifdef CONFIG_USE_BLUFI_NET_CONFIGURING
  //     auto &application = Application::GetInstance();
  //     application.SetDeviceState(kDeviceStateWifiConfiguring);
  //     auto &wifi_ap = WifiConfigurationAp::GetInstance();
  //     wifi_ap.SetLanguage(Lang::CODE);
  //     wifi_ap.SetSsidPrefix(CONFIG_WIFI_CONFIG_MODE_SSID_PREFIX);
  //     wifi_ap.Start();

  //     // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
  //     std::string hint = "请用小程序配网\n或连接以下热点进行配网:";
  //     hint += "\n";
  //     hint += wifi_ap.GetSsid();
  //     application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(),
  //     "gear",
  //                       Lang::Sounds::OGG_WIFICONFIG);

  // #ifdef CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME
  //     if (CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME != -1) {
  //       Application::GetInstance().GetAudioService().WaitForPlayCompletion(
  //           CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME);
  //       Application::GetInstance().DeInitAudioService();
  //     }
  // #endif

  //     // xTaskCreate(
  //     //     [](void *arg) {
  //     //       //   while (true) {
  //     //       //     if (Display::LockLvgl(200)) {
  //     //       //       if (strcmp(page_get_current_name(), "ai_chat") == 0)
  //     {
  //     //       //         Display::UnlockLvgl();
  //     //       //         break;
  //     //       //       }
  //     //       //       Display::UnlockLvgl();
  //     //       //     }
  //     //       //     vTaskDelay(pdMS_TO_TICKS(100));
  //     //       //   }
  //     //       vTaskDelay(pdMS_TO_TICKS(500));

  //     //       //   if (Display::LockLvgl(20000)) {
  //     //       //     page_start("blufi_configuration");
  //     //       //     Display::UnlockLvgl();
  //     //       //   }
  //     //       // if (Display::LockLvgl(20000)) {
  //     //       //   blufi_configuration_set_text_fmt(
  //     //       //       "请进入小程序或连接热点\n%s进行配网!",
  //     //       // WifiConfigurationAp::GetInstance().GetSsid().c_str());
  //     //       //   ESP_LOGW(TAG, "ssid:%s",
  //     //       // WifiConfigurationAp::GetInstance().GetSsid().c_str());
  //     //       //   Display::UnlockLvgl();
  //     //       }
  //     //       vTaskDelete(NULL);
  //     //     },
  //     //     "wifi_config_page_open", 1024 * 2, NULL, 1, NULL);

  //     doit_blufi_init();

  //     Ota ota;
  //     const int MAX_RETRY = 10;
  //     int retry_count = 0;
  //     int retry_delay = 10; // 初始重试延迟为10秒
  //     while (true) {
  //       wifi_ap_record_t ap_info;
  //       esp_err_t result = esp_wifi_sta_get_ap_info(&ap_info);
  //       bool is_connected = (result == ESP_OK);
  //       // 等待直到已连接wifi
  //       if (is_connected == false) {
  //         vTaskDelay(pdMS_TO_TICKS(1000));
  //         continue;
  //       }

  //       if (!ota.CheckVersion()) {
  //         retry_count++;
  //         if (retry_count >= MAX_RETRY) {
  //           ESP_LOGE(TAG, "Too many retries, exit version check");
  //           ResetWifiConfiguration();
  //           return;
  //         }

  //         ESP_LOGW(TAG, "Check new version failed, retry in %d seconds
  //         (%d/%d)",
  //                  retry_delay, retry_count, MAX_RETRY);
  //         for (int i = 0; i < retry_delay; i++) {
  //           vTaskDelay(pdMS_TO_TICKS(1000));
  //         }
  //         retry_delay *= 2; // 每次重试后延迟时间翻倍
  //         continue;
  //       }

  //       auto &code = ota.GetActivationCode();
  //       ESP_LOGI(TAG, "Activation code: %s", code.c_str());
  //       if (!code.empty()) {
  //         doit_blufi_send_code((uint8_t *)code.c_str());
  //         // This will block the loop until the activation is done or timeout
  //         for (int i = 0; i < 10; ++i) {
  //           ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
  //           esp_err_t err = ota.Activate();
  //           if (err == ESP_OK) {
  //             // xEventGroupSetBits(event_group_,
  //             // MAIN_EVENT_CHECK_NEW_VERSION_DONE);
  //             break;
  //           } else if (err == ESP_ERR_TIMEOUT) {
  //             vTaskDelay(pdMS_TO_TICKS(3000));
  //           } else {
  //             vTaskDelay(pdMS_TO_TICKS(10000));
  //           }
  //           if (application.GetDeviceState() == kDeviceStateIdle) {
  //             break;
  //           }
  //         }
  //       } else {
  //         uint8_t data[6] = {0};
  //         memset(data, 0, sizeof(data));
  //         doit_blufi_send_code(data);
  //       }

  //       auto ssid_length = blufi_storage_read_wifi_ssid_length();
  //       char *ssid = (char *)malloc(ssid_length + 1);
  //       blufi_storage_read_wifi_ssid(ssid);
  //       std::string ssid_string(ssid, ssid_length);

  //       auto password_length = blufi_storage_read_wifi_password_length();
  //       char *password = (char *)malloc(password_length + 1);
  //       blufi_storage_read_wifi_password(password);
  //       std::string password_string(password, password_length);

  //       SsidManager::GetInstance().AddSsid(ssid_string, password_string);

  //       vTaskDelay(pdMS_TO_TICKS(200));
  //       esp_restart();
  //     }

  // #else
  //     auto &application = Application::GetInstance();
  //     application.SetDeviceState(kDeviceStateWifiConfiguring);

  //     auto &wifi_ap = WifiConfigurationAp::GetInstance();
  //     wifi_ap.SetLanguage(Lang::CODE);
  //     wifi_ap.SetSsidPrefix(CONFIG_WIFI_CONFIG_MODE_SSID_PREFIX);
  //     wifi_ap.Start();

  //     // 等待 1.5 秒显示开发板信息
  //     vTaskDelay(pdMS_TO_TICKS(1500));

  // #ifdef CONFIG_WIFI_CONFIG_MODE_FORMAT_DISPLAY
  //     // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
  //     std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
  //     hint += "\n";
  //     hint += wifi_ap.GetSsid();
  //     hint += "\n";
  //     hint += Lang::Strings::ACCESS_VIA_BROWSER + 1;
  //     hint += "\n";
  //     hint += wifi_ap.GetWebServerUrl();
  //     hint += "\n\n";
  // #else
  //     std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
  //     hint += wifi_ap.GetSsid();
  //     hint += Lang::Strings::ACCESS_VIA_BROWSER;
  //     hint += wifi_ap.GetWebServerUrl();
  //     hint += "\n\n";
  // #endif

  //     // 播报配置 WiFi 的提示
  //     application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(),
  //     "gear",
  //                       Lang::Sounds::OGG_WIFICONFIG);

  // #ifdef CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME
  //     if (CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME != -1) {
  //       vTaskDelay(pdMS_TO_TICKS(
  //           CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME));
  //       application.DeInitAudioService();
  //     }
  // #endif

  // #if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
  //     auto display = Board::GetInstance().GetDisplay();
  //     auto codec = Board::GetInstance().GetAudioCodec();
  //     int channel = 1;
  //     if (codec) {
  //       channel = codec->input_channels();
  //     }
  //     ESP_LOGI(TAG,
  //              "Start receiving WiFi credentials from audio, input channels:
  //              %d", channel);
  //     audio_wifi_config::ReceiveWifiCredentialsFromAudio(&application,
  //     &wifi_ap,
  //                                                        display, channel);
  // #endif

  //     // Wait forever until reset after configuration
  //     while (true) {
  //       vTaskDelay(pdMS_TO_TICKS(10000));
  //     }
  // #endif
  //   }
};
