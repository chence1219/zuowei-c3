#include "wifi_board.h"

#include "display.h"
#include "application.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"
#include "system_info.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_network.h>
#include <esp_log.h>

#include <font_awesome.h>
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>
#include "afsk_demod.h"
#include "widgets/image/lv_image.h"
#ifdef CONFIG_CONNECTION_TYPE_NERTC
    #include "nertc_protocol.h"
#endif
#ifdef CONFIG_USE_BLUFI_NET_CONFIGURING
#include "esp_mac.h"
#include "doit_blufi.h"
#include "doit_blufi_storage.h"
#endif

static const char *TAG = "WifiBoard";

WifiBoard::WifiBoard() {
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        settings.SetInt("force_ap", 0);
    }
}

std::string WifiBoard::GetBoardType() {
    return "wifi";
}

void WifiBoard::EnterWifiConfigMode() {
#if CONFIG_CONNECTION_TYPE_NERTC 
    if (NeRtcProtocol::MountFileSystem()) {
        auto* config_json = NeRtcProtocol::ReadConfigJson();
        if(config_json) {
            cJSON* blufi_wifi = cJSON_GetObjectItem(config_json, "blufi_wifi");
            if (blufi_wifi && cJSON_IsBool(blufi_wifi) && blufi_wifi->valueint) {
                StartBlufiMode(true);
            }
        }
    }
#endif


#ifdef CONFIG_USE_BLUFI_NET_CONFIGURING
  auto &application = Application::GetInstance();
  application.SetDeviceState(kDeviceStateWifiConfiguring);
  auto &wifi_ap = WifiConfigurationAp::GetInstance();
  wifi_ap.SetLanguage(Lang::CODE);
  wifi_ap.SetSsidPrefix(CONFIG_WIFI_CONFIG_MODE_SSID_PREFIX);
  wifi_ap.Start();

  // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
  std::string hint = "请用小程序配网\n或连接以下热点进行配网:";
  hint += "\n";
  hint += wifi_ap.GetSsid();
  application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "gear",
                    Lang::Sounds::OGG_WIFICONFIG);

#ifdef CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME
  if (CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME != -1) {
    Application::GetInstance().GetAudioService().WaitForPlayCompletion(
        CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME);
    Application::GetInstance().DeInitAudioService();
  }
#endif

    // xTaskCreate(
    //     [](void *arg) {
    //     //   while (true) {
    //     //     if (Display::LockLvgl(200)) {
    //     //       if (strcmp(page_get_current_name(), "ai_chat") == 0) {
    //     //         Display::UnlockLvgl();
    //     //         break;
    //     //       }
    //     //       Display::UnlockLvgl();
    //     //     }
    //     //     vTaskDelay(pdMS_TO_TICKS(100));
    //     //   }
    //       vTaskDelay(pdMS_TO_TICKS(500));

    //     //   if (Display::LockLvgl(20000)) {
    //     //     page_start("blufi_configuration");
    //     //     Display::UnlockLvgl();
    //     //   }
    //       if (Display::LockLvgl(20000)) {
    //         blufi_configuration_set_text_fmt("请进入小程序或连接热点\n%s进行配网!", WifiConfigurationAp::GetInstance().GetSsid().c_str());
    //         ESP_LOGW(TAG, "ssid:%s", WifiConfigurationAp::GetInstance().GetSsid().c_str());
    //         Display::UnlockLvgl();
    //       }
    //       vTaskDelete(NULL);
    //     },
    //     "wifi_config_page_open", 1024*2, NULL, 1, NULL);

    doit_blufi_init();

    Ota ota;
    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // 初始重试延迟为10秒
    ESP_LOGI(TAG, "Waiting for WiFi network connection...");
    while (true) {
      esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to get STA netif");
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info");
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      if (ip_info.ip.addr == 0 || (ip_info.ip.addr & 0xFFFF0000) == 0xA9FE0000) {
        ESP_LOGI(TAG, "Waiting for valid IP address...");
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      esp_err_t err = ota.Activate();
      if (!ota.CheckVersion()) {
        retry_count++;
        if (retry_count >= MAX_RETRY) {
          ESP_LOGE(TAG, "Too many retries, exit version check");
          ResetWifiConfiguration();
          return;
        }

        ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)",
                 retry_delay, retry_count, MAX_RETRY);
        err = ota.Activate();
        for (int i = 0; i < retry_delay; i++) {
          vTaskDelay(pdMS_TO_TICKS(1000));
        }
        retry_delay *= 2; // 每次重试后延迟时间翻倍
        continue;
      }

      auto &code = ota.GetActivationCode();
      ESP_LOGI(TAG, "Activation code: %s", code.c_str());
      if (!code.empty()) {
        err = doit_blufi_send_code((uint8_t *)code.c_str());
        // This will block the loop until the activation is done or timeout
        for (int i = 0; i < 10; ++i) {
          if (err == ESP_OK || application.GetDeviceState() == kDeviceStateIdle) {
            break;
          }
          err = doit_blufi_send_code((uint8_t *)code.c_str());
          ESP_LOGI(TAG, "Waiting for send code... %d/%d", i + 1, 10);
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      } else {
        uint8_t data[6] = {0};
        memset(data, 0, sizeof(data));
        err = doit_blufi_send_code(data);
      }
      
      auto ssid_length = blufi_storage_read_wifi_ssid_length();
      char* ssid = (char*)malloc(ssid_length + 1);
      blufi_storage_read_wifi_ssid(ssid);
      std::string ssid_string(ssid, ssid_length);

      auto password_length = blufi_storage_read_wifi_password_length();
      char* password = (char*)malloc(password_length + 1);
      blufi_storage_read_wifi_password(password);
      std::string password_string(password, password_length);
      
      SsidManager::GetInstance().AddSsid(ssid_string, password_string);

      vTaskDelay(pdMS_TO_TICKS(200));
      esp_restart();
    }

#else
    auto& application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);

    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);
    wifi_ap.SetSsidPrefix(CONFIG_WIFI_CONFIG_MODE_SSID_PREFIX);
    wifi_ap.Start();

    // 等待 1.5 秒显示开发板信息
    vTaskDelay(pdMS_TO_TICKS(1500));

#ifdef CONFIG_WIFI_CONFIG_MODE_FORMAT_DISPLAY
    // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
    hint += "\n";
    hint += wifi_ap.GetSsid();
    hint += "\n";
    hint += Lang::Strings::ACCESS_VIA_BROWSER + 1;
    hint += "\n";
    hint += wifi_ap.GetWebServerUrl();
    hint += "\n\n";
#else
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
    hint += wifi_ap.GetSsid();
    hint += Lang::Strings::ACCESS_VIA_BROWSER;
    hint += wifi_ap.GetWebServerUrl();
    hint += "\n\n";
#endif

    // 播报配置 WiFi 的提示
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "gear", Lang::Sounds::OGG_WIFICONFIG);

#ifdef CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME
    if(CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME != -1){
        vTaskDelay(pdMS_TO_TICKS(CONFIG_WIFI_CONFIG_MODE_AUTO_DELAY_RELEASE_DECODER_TIME));
        application.DeInitAudioService();
    }
#endif

    #if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
    auto display = Board::GetInstance().GetDisplay();
    auto codec = Board::GetInstance().GetAudioCodec();
    int channel = 1;
    if (codec) {
        channel = codec->input_channels();
    }
    ESP_LOGI(TAG, "Start receiving WiFi credentials from audio, input channels: %d", channel);
    audio_wifi_config::ReceiveWifiCredentialsFromAudio(&application, &wifi_ap, display, channel);
    #endif
    
    // Wait forever until reset after configuration
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif
}

void WifiBoard::StartNetwork() {
    // User can press BOOT button while starting to enter WiFi configuration mode
    if (wifi_config_mode_) {
        EnterWifiConfigMode();
        return;
    }

    // If no WiFi SSID is configured, enter WiFi configuration mode
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (ssid_list.empty()) {
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }

    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.OnScanBegin([this]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
    });
    wifi_station.OnConnect([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000);
    });
    wifi_station.OnConnected([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000);
    });
    wifi_station.Start();

#ifdef CONFIG_WIFI_WAIT_FOR_CONNECTED_TIME
    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    if(CONFIG_WIFI_WAIT_FOR_CONNECTED_TIME == -1){
        while (true) {
            if (wifi_station.WaitForConnected(60 * 1000)) {
                break;
            }
        }
    }else if (!wifi_station.WaitForConnected(CONFIG_WIFI_WAIT_FOR_CONNECTED_TIME * 1000)) {
#else
    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    if (!wifi_station.WaitForConnected(60 * 1000)) {
#endif
        wifi_station.Stop();
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
}

NetworkInterface* WifiBoard::GetNetwork() {
    static EspNetwork network;
    return &network;
}

const char* WifiBoard::GetNetworkStateIcon() {
    if (wifi_config_mode_) {
        return FONT_AWESOME_WIFI;
    }
    auto& wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected()) {
        return FONT_AWESOME_WIFI_SLASH;
    }
    int8_t rssi = wifi_station.GetRssi();
    if (rssi >= -60) {
        return FONT_AWESOME_WIFI;
    } else if (rssi >= -70) {
        return FONT_AWESOME_WIFI_FAIR;
    } else {
        return FONT_AWESOME_WIFI_WEAK;
    }
}

std::string WifiBoard::GetBoardJson() {
    // Set the board type for OTA
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_json = R"({)";
    board_json += R"("type":")" + std::string(BOARD_TYPE) + R"(",)";
    board_json += R"("name":")" + std::string(BOARD_NAME) + R"(",)";
    if (!wifi_config_mode_) {
        board_json += R"("ssid":")" + wifi_station.GetSsid() + R"(",)";
        board_json += R"("rssi":)" + std::to_string(wifi_station.GetRssi()) + R"(,)";
        board_json += R"("channel":)" + std::to_string(wifi_station.GetChannel()) + R"(,)";
        board_json += R"("ip":")" + wifi_station.GetIpAddress() + R"(",)";
    }
    board_json += R"("mac":")" + SystemInfo::GetMacAddress() + R"(")";
    board_json += R"(})";
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled) {
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);
}

void WifiBoard::ResetWifiConfiguration() {
    // Set a flag and reboot the device to enter the network configuration mode
    {
        Settings settings("wifi", true);
        settings.SetInt("force_ap", 1);
    }
    GetDisplay()->ShowNotification(Lang::Strings::ENTERING_WIFI_CONFIG_MODE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Reboot the device
    esp_restart();
}

std::string WifiBoard::GetDeviceStatusJson() {
    /*
     * 返回设备状态JSON
     * 
     * 返回的JSON结构如下：
     * {
     *     "audio_speaker": {
     *         "volume": 70
     *     },
     *     "screen": {
     *         "brightness": 100,
     *         "theme": "light"
     *     },
     *     "battery": {
     *         "level": 50,
     *         "charging": true
     *     },
     *     "network": {
     *         "type": "wifi",
     *         "ssid": "Xiaozhi",
     *         "rssi": -60
     *     },
     *     "chip": {
     *         "temperature": 25
     *     }
     * }
     */
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    // Audio speaker
    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // Screen brightness
    auto backlight = board.GetBacklight();
    auto screen = cJSON_CreateObject();
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    auto display = board.GetDisplay();
    if (display && display->height() > 64) { // For LCD display only
        auto theme = display->GetTheme();
        if (theme != nullptr) {
            cJSON_AddStringToObject(screen, "theme", theme->name().c_str());
        }
    }
    cJSON_AddItemToObject(root, "screen", screen);

    // Battery
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        cJSON* battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // Network
    auto network = cJSON_CreateObject();
    auto& wifi_station = WifiStation::GetInstance();
    cJSON_AddStringToObject(network, "type", "wifi");
    cJSON_AddStringToObject(network, "ssid", wifi_station.GetSsid().c_str());
    int rssi = wifi_station.GetRssi();
    if (rssi >= -60) {
        cJSON_AddStringToObject(network, "signal", "strong");
    } else if (rssi >= -70) {
        cJSON_AddStringToObject(network, "signal", "medium");
    } else {
        cJSON_AddStringToObject(network, "signal", "weak");
    }
    cJSON_AddItemToObject(root, "network", network);

    // Chip
    float esp32temp = 0.0f;
    if (board.GetTemperature(esp32temp)) {
        auto chip = cJSON_CreateObject();
        cJSON_AddNumberToObject(chip, "temperature", esp32temp);
        cJSON_AddItemToObject(root, "chip", chip);
    }

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}
