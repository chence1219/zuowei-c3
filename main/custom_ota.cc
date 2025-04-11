#include "custom_ota.h"
#include "system_info.h"
#include "board.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>

#define TAG "CustomOta"


CustomOta::CustomOta() {

}

CustomOta::~CustomOta() {

}

bool CustomOta::CheckVersion() {
    current_version_ = esp_app_get_description()->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    if (check_version_url_.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    auto http = Board::GetInstance().CreateHttp();
    for (const auto& header : headers_) {
        http->SetHeader(header.first, header.second);
    }

    http->SetHeader("Content-Type", "application/json");
    std::string method = post_data_.length() > 0 ? "POST" : "GET";
    if (!http->Open(method, check_version_url_, post_data_)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return false;
    }

    auto response = http->GetBody();
    http->Close();
    delete http;

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware == NULL) {
        ESP_LOGE(TAG, "Failed to get firmware object");
        cJSON_Delete(root);
        return false;
    }
    cJSON *version = cJSON_GetObjectItem(firmware, "version");
    if (version == NULL) {
        ESP_LOGE(TAG, "Failed to get version object");
        cJSON_Delete(root);
        return false;
    }
    cJSON *url = cJSON_GetObjectItem(firmware, "url");
    if (url == NULL) {
        ESP_LOGE(TAG, "Failed to get url object");
        cJSON_Delete(root);
        return false;
    }
    cJSON *forced = cJSON_GetObjectItem(firmware, "forced");
    if (forced && forced->valueint == 1) {
        forced_ = true;
    }

    firmware_version_ = version->valuestring;
    firmware_url_ = url->valuestring;
    cJSON_Delete(root);

    // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
    has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
    if (has_new_version_) {
        ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
        ESP_LOGI(TAG, "New firmware: %s", firmware_url_.c_str());
    } else {
        ESP_LOGI(TAG, "Current is the latest version");
    }
    return true;
}
