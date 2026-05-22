#include "doit_blufi.h"
#include "doit_blufi_storage.h"
#include "host/ble_att.h"
#include "wifi_configuration_ap.h"
#include "ssid_manager.h"
#include "esp_log.h"
#include <vector>
#include <string>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#define TAG "DOIT_BLUFI"
extern "C" {
    #include "esp_coexist.h"
    }
using namespace std;
extern esp_err_t esp_coex_preference_set(esp_coex_prefer_t prefer);
/* 所有静态变量放到匿名命名空间，避免全局污染 */
namespace
{
    wifi_config_t sta_config{};
    wifi_config_t ap_config{};
    bool ble_connected = false;
    bool blufi_is_init = false;

    /* --------------- C 回调转 C++ --------------- */
    static void blufi_event_cb(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
    {
        static bool ble = false;
        switch (event)
        {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(TAG, "BLUFI init finish\n");
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(TAG, "BLUFI deinit finish\n");
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "BLUFI ble connect\n");
            ble = true;
            if (blufi_is_init)
            {
                esp_blufi_adv_stop();
            }
            break;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(TAG, "BLUFI ble disconnect\n");
            ble = false;
            if (blufi_is_init)
            {
                esp_blufi_adv_start();
            }
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            // ESP_LOGI(TAG, "BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
            // esp_wifi_set_mode(param->wifi_mode.op_mode);
            // 不接受手机下发的 opmode，强制维持 AP+STA，确保网页/HTTP 还能访问
            ESP_LOGI(TAG, "Ignore BLUFI opmode %d, keep WIFI_MODE_APSTA", param->wifi_mode.op_mode);
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            break;
            break;
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        {
            // 连接前
            // esp_wifi_set_ps(WIFI_PS_NONE);                  // 先关省电，别在鉴权时打盹
            // esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
            // esp_coex_preference_set(ESP_COEX_PREFER_WIFI);  // 共存：优先 Wi-Fi
            // ESP_LOGI(TAG, "BLUFI request wifi connect to AP\n");
            // int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            // int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
            // ESP_LOGI(TAG, "free sram: %u minimal sram: %u", free_sram, min_free_sram);
            // doit_blufi_stop_adv();
            // esp_wifi_scan_stop();
            // sta_config.sta.listen_interval = 1;                             // 连接后也尽量不漏广播
            // sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
            esp_wifi_set_config(WIFI_IF_STA, &sta_config);

            esp_wifi_connect();

           
            
            // esp_wifi_disconnect();
            // wifi_config_t sta_config2;
            // esp_wifi_get_config(WIFI_IF_STA, &sta_config2);
            // ESP_LOGI(TAG, "BLUFI request wifi connect to AP %s\n", sta_config2.sta.ssid);
            // ESP_LOGI(TAG, "BLUFI request wifi connect to AP %s\n", sta_config2.sta.password);
            // ESP_LOGI(TAG, "BLUFI request wifi connect to mode %d\n", (int)sta_config2.sta.threshold.authmode);
            // esp_err_t ret=esp_wifi_connect();
            // ESP_LOGI(TAG, "BLUFI request wifi connect to AP ret %x\n", (int)ret);
            /* 连接 WiFi */
            // ESP_LOGI(TAG, "连接wifi测试.... %s\n", sta_config.sta.ssid);
            // if (WifiConfigurationAp::GetInstance().ConnectToWifi(
            //         std::string((char *)sta_config.sta.ssid),
            //         std::string((char *)sta_config.sta.password)))
            // {
            //     ESP_LOGI(TAG, "连接wifi成功.... %s\n", sta_config.sta.ssid);
            //     blufi_storage_write_has_config(true);
            //     // esp_restart();
            // }
            // else
            // {
            //     esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            //     esp_wifi_disconnect(); // ← 可选，清理一下连接状态
            // }
            break;
        }
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(TAG, "BLUFI request wifi disconnect from AP\n");
            esp_wifi_disconnect();
            break;
        case ESP_BLUFI_EVENT_REPORT_ERROR:
            ESP_LOGE(TAG, "BLUFI report error, error code %d\n", param->report_error.state);
            esp_blufi_send_error_info(param->report_error.state);
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST:
        {
            ESP_LOGI(TAG, "Recv GET WIFI LIST \n");
            auto list = WifiConfigurationAp::GetInstance().GetScanResults();
            if (list.empty())
            {
                esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
                break;
            }
            std::vector<esp_blufi_ap_record_t> blufi_list;
            blufi_list.reserve(list.size());
            for (const auto &rec : list)
            {
                if (blufi_list.size() * sizeof(esp_blufi_ap_record_t) >
                     517 - 40) {
                    // 防止比mtu长，分配失败
                    break;
                }

                esp_blufi_ap_record_t item{};
                item.rssi = rec.rssi;
                memcpy(item.ssid, rec.ssid, sizeof(item.ssid));
                blufi_list.emplace_back(item);
                ESP_LOGI(TAG, "AP: %s, RSSI: %d", item.ssid, item.rssi);
                
                // int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                // int min_free_sram =
                //     heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
                // ESP_LOGI(TAG, "free sram: %u minimal sram: %u", free_sram,
                //          min_free_sram);
            }
            if (ble) {
                ESP_LOGI(TAG, "send wifi list, size:%d", blufi_list.size());
                esp_err_t err = esp_blufi_send_wifi_list(blufi_list.size(),
                                                        blufi_list.data());
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "send wifi list err");
                }
            } else {
                ESP_LOGE(TAG, "send wifi list failed, ble=false");
            }
            break;
        }
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            if (param->sta_ssid.ssid_len >= sizeof(sta_config.sta.ssid) / sizeof(sta_config.sta.ssid[0]))
            {
                esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
                ESP_LOGI(TAG, "Invalid STA SSID\n");
                break;
            }
            strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            esp_wifi_set_config(WIFI_IF_STA, &sta_config);
            ESP_LOGI(TAG, "Recv STA SSID %s\n", sta_config.sta.ssid);
            blufi_storage_write_wifi_ssid((char *)sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            if (param->sta_passwd.passwd_len >= sizeof(sta_config.sta.password) / sizeof(sta_config.sta.password[0]))
            {
                esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
                ESP_LOGI(TAG, "Invalid STA PASSWORD\n");
                break;
            }
            strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
            sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            // sta_config.sta.threshold.authmode = EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD;
            esp_wifi_set_config(WIFI_IF_STA, &sta_config);
            ESP_LOGI(TAG, "Recv STA PASSWORD %s\n", sta_config.sta.password);
            blufi_storage_write_wifi_password((char *)sta_config.sta.password);
            break;
        case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
            ESP_LOGI(TAG, "Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
            ESP_LOG_BUFFER_HEX("Custom Data", param->custom_data.data, param->custom_data.data_len);

            // 解析AT+OTA命令
            if (param->custom_data.data_len > 7)
            { // 至少需要"AT+OTA="的长度
                char *data_str = (char *)malloc(param->custom_data.data_len + 1);
                if (data_str)
                {
                    memcpy(data_str, param->custom_data.data, param->custom_data.data_len);
                    data_str[param->custom_data.data_len] = '\0'; // 确保字符串以null结尾

                    // 检查是否是AT+OTA命令
                    if (strncmp(data_str, "AT+OTA=", 7) == 0)
                    {
                        char *url = data_str + 7; // 跳过"AT+OTA="部分
                        ESP_LOGI(TAG, "解析到OTA URL: %s", url);
                        blufi_storage_write_ota_url(url);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "接收到的自定义数据: %s", data_str);
                    }

                    free(data_str);
                }
                else
                {
                    ESP_LOGE(TAG, "内存分配失败");
                }
            }
            break;
        case ESP_BLUFI_EVENT_RECV_USERNAME:
            /* Not handle currently */
            break;
        case ESP_BLUFI_EVENT_RECV_CA_CERT:
            /* Not handle currently */
            break;
        case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
            /* Not handle currently */
            break;
        case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
            /* Not handle currently */
            break;
        case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
            /* Not handle currently */
            break;
            ;
        case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
            /* Not handle currently */
            break;
        default:
            break;
        }
    }
} // namespace

/* --------------- 对外 C 接口 --------------- */
extern "C" void doit_blufi_init(void)
{
    blufi_is_init = true;
    static esp_blufi_callbacks_t cbs = {.event_cb = blufi_event_cb};
    ESP_ERROR_CHECK(esp_blufi_controller_init());
    ESP_ERROR_CHECK(esp_blufi_host_and_cb_init(&cbs));
}
extern "C" void doit_blufi_deinit(void)
{
    if (!blufi_is_init) {
        return;
    }

    blufi_is_init = false;
    esp_blufi_host_deinit();
    esp_blufi_controller_deinit();
}
extern "C" esp_err_t doit_blufi_send_code(uint8_t *code){
    if(doit_blufi_is_init()) {
        ESP_LOGI(TAG, "doit_blufi_send_code: %02x %02x %02x %02x %02x %02x", code[0], code[1], code[2], code[3], code[4], code[5]);
        return esp_blufi_send_custom_data((uint8_t *)code, 6);
    }
    else{
        ESP_LOGW(TAG,"blufi not init");
        return ESP_FAIL;
    }
    
}
extern "C" void doit_blufi_start_adv(void) { esp_blufi_adv_start(); }
extern "C" void doit_blufi_stop_adv(void) { esp_blufi_adv_stop(); }
extern "C" bool doit_blufi_is_init(void) { return blufi_is_init; }
