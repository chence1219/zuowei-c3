#include "ota.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <esp_partition.h>
#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif
#include "esp_timer.h"

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>

#include "mbedtls/md5.h"

#define TAG "Ota"


Ota::Ota() {
#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // Read Serial Number from efuse user_data
    uint8_t serial_number[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
        } else {
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
        }
    }
#endif
}

Ota::~Ota() {
}

std::string Ota::GetCheckVersionUrl() {
    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (url.empty()) {
        url = ""; //todo CONFIG_OTA_URL
    }
    return url;
}

void Ota::SetFirmwareUrl(const std::string& url) {
    ESP_LOGI(TAG, "Set firmware URL to %s", url.c_str());
    firmware_url_ = url;
}

std::unique_ptr<BlufiHttp::HttpClient> Ota::SetupHttp() {
    auto http = std::make_unique<BlufiHttp::HttpClient>();
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
    }
    http->SetHeader("Content-Type", "application/json");

    return http;
}

/* 
 * Specification: https://ccnphfhqs21z.feishu.cn/wiki/FjW6wZmisimNBBkov6OcmfvknVd
 */
bool Ota::CheckVersion() {
    std::string url;
    {
        Settings settings("board", false);
        // Check if there is a new firmware version available
        current_version_ = settings.GetString("version");
        url = settings.GetString("ota_url");
    }
    ESP_LOGI(TAG, "Current version: %s url: %s", current_version_.c_str(), url.c_str());

    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    auto http = std::unique_ptr<BlufiHttp::HttpClient>(SetupHttp());

    // std::string data = board.GetJson(); //todo 
    
    std::string data = "{";
    data += "\"application\":{";
    data += "\"version\":\"" + current_version_ + "\",";
    data += "\"board_name\":\"" + std::string("xiaopai") + "\"";
    data += "}";
    data += "}";

    std::string method = "POST";

    if (!http->Open(method, url, data)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }

    auto status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        return false;
    }

    data.resize(http->GetBodyLength() + 1);
    http->Read(data.data(), data.size());
    http->Close();
    printf("ota xiaopai = %s\n", data.c_str());
    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (cJSON_IsString(message)) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (cJSON_IsString(code)) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
        if (cJSON_IsString(challenge)) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        cJSON* timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (cJSON_IsNumber(timeout_ms)) {
            activation_timeout_ms_ = timeout_ms->valueint;
        }
    }

    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }

    has_websocket_config_ = false;
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(websocket)) {
        Settings settings("websocket", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (cJSON_IsString(item)) {
                settings.SetString(item->string, item->valuestring);
            } else if (cJSON_IsNumber(item)) {
                settings.SetInt(item->string, item->valueint);
            }
        }
        has_websocket_config_ = true;
    } else {
        ESP_LOGI(TAG, "No websocket section found!");
    }

    has_server_time_ = false;
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (cJSON_IsObject(server_time)) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        
        if (cJSON_IsNumber(timestamp)) {
            // 设置系统时间
            struct timeval tv;
            double ts = timestamp->valuedouble;
            
            // 如果有时区偏移，计算本地时间
            if (cJSON_IsNumber(timezone_offset)) {
                ts += (timezone_offset->valueint * 60 * 1000); // 转换分钟为毫秒
            }
            
            tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    } else {
        ESP_LOGW(TAG, "No server_time section found!");
    }

    has_new_version_ = false;
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (cJSON_IsString(version)) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(url)) {
            firmware_url_ = url->valuestring;
        }

        if (!firmware_url_.empty() && cJSON_IsString(version) && cJSON_IsString(url)) {
            // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            // If the force flag is set to 1, the given version is forced to be installed
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    cJSON_Delete(root);
    return true;
}

void Ota::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

bool GetNextSafePartition(const esp_partition_t **out)
{
    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL); // 第一候选
    if (p == nullptr) return false;

    /* 正好是我们想避开的 ota_2 */
    if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_2) {
        ESP_LOGW(TAG, "ota_2(blufi) skipped, try next...");
        p = esp_ota_get_next_update_partition(p);   // 再轮一次
        if (p == nullptr) return false;
    }
    *out = p;
    return true;
}

static void compute_md5(const uint8_t *buf, size_t len, uint8_t out[16])
{
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, buf, len);
    mbedtls_md5_finish(&ctx, out);
    mbedtls_md5_free(&ctx);
}

bool Ota::Upgrade(const std::string& md5, const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());

    /* 1. 取目标分区（OTA_0）*/
    const esp_partition_t *part = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!part) {
        ESP_LOGE(TAG, "No OTA_0 partition");
        return false;
    }

    /* 2. 打开 HTTP */
    auto http = std::unique_ptr<BlufiHttp::HttpClient>(SetupHttp());
    if (!http->Open("GET", firmware_url) || http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "HTTP fail, code=%d", http->GetStatusCode());
        return false;
    }
    size_t bin_len = http->GetBodyLength();
    if (bin_len == 0) {
        ESP_LOGE(TAG, "Content-Length=0");
        return false;
    }

    /* 3. 一次性申请 PSRAM 缓存 */
    uint8_t* bin_buf = (uint8_t*)heap_caps_malloc(bin_len + 100, MALLOC_CAP_SPIRAM);
    if (!bin_buf) {
        ESP_LOGE(TAG, "No enough PSRAM for %u bytes", bin_len);
        return false;
    }

    /* 4. 完整下载 */
    size_t total = 0;
    int ret = 0;
    auto last_time = esp_timer_get_time();
    auto start_time = last_time;
    int last_bytes = 0;
    int low_speed_count = 0;

    int buffer_size = 1024 * 4;
    uint8_t* buffer = new uint8_t[buffer_size];

    while (total < bin_len) {
        // 尝试从 HTTP 流中读取数据
        // ret = http->Read((char*)bin_buf + total, bin_len - total);
        ret = http->Read((char*)buffer, buffer_size);

        auto now = esp_timer_get_time();
        if (now - last_time > 1000000) {
            auto interval = (now - last_time) / 1000000;
            last_time = now;
            if (interval <= 0) interval = 1;
            int speed = (total - last_bytes) / interval;
            last_bytes = total;
            // 调用回调函数，第二个参数传递本次实际读取的字节数
            if (upgrade_callback_) upgrade_callback_("下载中:", total * 100 / bin_len, speed);

            if (speed < 100000 && (total * 100 / bin_len) < 60) { // 小于 100k,进度小于 60
                low_speed_count++;
                ESP_LOGW(TAG, "Low speed detected: %d bytes/s, count=%d", speed, low_speed_count);
                if (low_speed_count >= 7) {
                    ESP_LOGE(TAG, "Download speed too low, aborting");
                    http->Close();
                    free(bin_buf);
                    delete[] buffer;
                    return false;
                }
            } else {
                low_speed_count = 0;
            } 
        }

        if (ret > 0) {
            // 成功读取到数据
            memcpy(bin_buf + total, buffer, ret);
            total += ret;
        } else if (ret < 0) {
            // 发生错误
            if (ret == -ESP_ERR_HTTP_EAGAIN) {
                // 这是“请重试”错误，不是致命的。
                // 打印一个调试信息，然后继续循环等待数据。
                ESP_LOGW(TAG, "ESP_ERR_HTTP_EAGAIN, no data available right now, retrying...");
                // 可以加一个短暂的延时，避免CPU在无数据时空转
                vTaskDelay(pdMS_TO_TICKS(100)); 
                continue; // 继续下一次循环
            } else {
                // 这是一个真正的、无法恢复的错误
                ESP_LOGE(TAG, "HTTP read error: %d (%s)", ret, esp_err_to_name(ret));
                // 发生致命错误，必须终止
                http->Close();
                free(bin_buf);
                return false;
            }
        } else { // ret == 0
            // 连接被对方正常关闭。如果此时数据还没下完，说明有问题。
            ESP_LOGW(TAG, "Connection closed by peer, but download may be incomplete.");
            break; // 退出循环，让后续的长度检查来判断是否成功
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    auto end_time = esp_timer_get_time();
    ESP_LOGE(TAG, " Total download time: %.2f seconds\n", (end_time - start_time) / 1000000.0);

    delete[] buffer;
    buffer = nullptr;

    http->Close();
    if (total != bin_len) {
        ESP_LOGE(TAG, "Download incomplete %u/%u ret:%d", total, bin_len, ret);
        free(bin_buf);
        return false;
    }

    /* 5. 简单版本校验（可选）*/
    esp_app_desc_t *new_desc = (esp_app_desc_t *)(bin_buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    ESP_LOGI(TAG, "New ver: %s", new_desc->version);
    if (memcmp(new_desc->version, esp_app_get_description()->version, sizeof(new_desc->version)) == 0) {
        ESP_LOGW(TAG, "Same version, skip");
        free(bin_buf);
        return false;
    }
    
    // 计算 md5
    uint8_t md5_sum[16];
    compute_md5(bin_buf, bin_len, md5_sum);

    /* 如果你想打印成 32 位 HEX 字符串 */
    char md5_str[33];
    for (int i = 0; i < 16; ++i) {
        sprintf(&md5_str[i * 2], "%02x", md5_sum[i]);
    }
    md5_str[32] = '\0';

    ESP_LOGI(TAG, "Firmware MD5: %s md5_ota:%s", md5_str, md5.c_str());
    if (std::string(md5_str) != md5) {
        ESP_LOGE(TAG, "MD5 mismatch, abort");
        free(bin_buf);
        return false;
    }

    installing_.store(true);
    if (upgrade_callback_) upgrade_callback_("准备安装:", 0, 0);

    /* 6. 一次性烧写到 OTA_0 */
    esp_ota_handle_t h = 0;
    if (esp_ota_begin(part, bin_len, &h) != ESP_OK) {
        ESP_LOGE(TAG, "ota_begin fail");
        free(bin_buf);
        return false;
    }
    int write_size = 4096 * 4;
    for (size_t i = 0; i < bin_len; i += write_size) {
        size_t chunk_size = (i + write_size > bin_len) ? (bin_len - i) : write_size;
        if (esp_ota_write(h, bin_buf + i, chunk_size) != ESP_OK) {
            ESP_LOGE(TAG, "ota_write fail");
            esp_ota_abort(h);
            free(bin_buf);
            return false;
        }
        // 调用回调函数，第三个参数传递本次实际写入的字节数
        if (upgrade_callback_) upgrade_callback_("安装中:", (i + chunk_size) * 100 / bin_len, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    if (esp_ota_end(h) != ESP_OK) {
        ESP_LOGE(TAG, "ota_end fail");
        free(bin_buf);
        return false;
    }

    installing_.store(false);

    free(bin_buf);
    ESP_LOGI(TAG, "Download & burn OK, restart to take effect");
    return true;        // 由调用方决定何时 esp_restart()
}

bool Ota::StartUpgrade(const std::string& md5, std::function<void(const std::string&, int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;

    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "free sram: %u minimal sram: %u", free_sram, min_free_sram);
    int free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int min_free_psram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "free psram: %u minimal psram: %u", free_psram, min_free_psram);

    return Upgrade(md5, firmware_url_);
}

std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;
        } else if (newer[i] < current[i]) {
            return false;
        }
    }
    
    return newer.size() > current.size();
}

std::string Ota::GetActivationPayload() {
    if (!has_serial_number_) {
        return "{}";
    }

    std::string hmac_hex;
#ifdef SOC_HMAC_SUPPORTED
    uint8_t hmac_result[32]; // SHA-256 输出为32字节
    
    // 使用Key0计算HMAC
    esp_err_t ret = esp_hmac_calculate(HMAC_KEY0, (uint8_t*)activation_challenge_.data(), activation_challenge_.size(), hmac_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HMAC calculation failed: %s", esp_err_to_name(ret));
        return "{}";
    }

    for (size_t i = 0; i < sizeof(hmac_result); i++) {
        char buffer[3];
        sprintf(buffer, "%02x", hmac_result[i]);
        hmac_hex += buffer;
    }
#endif

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
    cJSON_AddStringToObject(payload, "serial_number", serial_number_.c_str());
    cJSON_AddStringToObject(payload, "challenge", activation_challenge_.c_str());
    cJSON_AddStringToObject(payload, "hmac", hmac_hex.c_str());
    auto json_str = cJSON_PrintUnformatted(payload);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Activation payload: %s", json.c_str());
    return json;
}

esp_err_t Ota::Activate() {
    if (!has_activation_challenge_) {
        ESP_LOGW(TAG, "No activation challenge found");
        return ESP_FAIL;
    }

    std::string url = GetCheckVersionUrl();
    if (url.back() != '/') {
        url += "/activate";
    } else {
        url += "activate";
    }

    auto http = std::unique_ptr<BlufiHttp::HttpClient>(SetupHttp());

    std::string data = GetActivationPayload();
    if (!http->Open("POST", url, std::move(data))) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return ESP_FAIL;
    }
    
    auto status_code = http->GetStatusCode();
    if (status_code == 202) {
        return ESP_ERR_TIMEOUT;
    }
    data.resize(http->GetBodyLength() + 1);
    http->Read(data.data(), data.size());
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, data.c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Activation successful");
    return ESP_OK;
}
