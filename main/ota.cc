#include "ota.h"
#include "system_info.h"
#include "settings.h"
#include "assets/lang_config.h"
#include "application.h"
#include <at_modem.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <esp_heap_caps.h>
#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <mutex>

#define TAG "Ota"

namespace {

#if CONFIG_CONNECTION_TYPE_NERTC

#define NERTC_HTTP_FALLBACK_FORCE_TEST_FLOW 0

struct OtaHttpCandidate {
    std::string base_url;
    std::string host_header;
    const char* name = "";
};

struct OtaHttpResult {
    esp_err_t err = ESP_FAIL;
    int status_code = -1;
    std::string body;
    std::string final_url;
    std::string candidate_name;
    int candidate_index = -1;
};

struct OtaEndpointSet {
    const char* primary_url;
    const char* backup_url1;
    const char* backup_host1;
    const char* backup_url2;
    const char* backup_host2;
};

constexpr int kOtaRequestTimeoutMs = 15000;
constexpr char kOtaProdPrimaryUrl[] = "https://nrtc.netease.im/v1/ota";
constexpr char kOtaProdLogicalHost[] = "nrtc.netease.im";
constexpr char kOtaProdBackupUrl1[] = "https://47.83.229.207/v1/ota";
constexpr char kOtaProdBackupUrl2[] = "https://1.95.20.137/v1/ota";
constexpr char kOtaTestPrimaryUrl[] = "http://webtest.netease.im/v1/ota";
constexpr char kOtaTestLogicalHost[] = "webtest.netease.im";
constexpr char kOtaTestBackupUrl1[] = "http://1.95.20.157/v1/ota";
constexpr char kOtaTestBackupUrl2[] = "http://1.95.20.158/v1/ota";

constexpr OtaEndpointSet kOtaTestEndpoints = {
    kOtaTestPrimaryUrl,
    kOtaTestBackupUrl1,
    kOtaTestLogicalHost,
    kOtaTestBackupUrl2,
    kOtaTestLogicalHost,
};

void AppendOtaCandidate(std::vector<OtaHttpCandidate>& candidates,
                        const std::string& base_url,
                        const std::string& host_header,
                        const char* name) {
    if (base_url.empty()) {
        return;
    }

    auto duplicate = std::find_if(candidates.begin(), candidates.end(),
                                  [&](const OtaHttpCandidate& candidate) {
                                      return candidate.base_url == base_url &&
                                             candidate.host_header == host_header;
                                  });
    if (duplicate != candidates.end()) {
        return;
    }

    candidates.push_back({base_url, host_header, name});
}

std::string GetConfiguredOtaUrl() {
    std::string url = CONFIG_OTA_URL;
    return url.empty() ? std::string(kOtaProdPrimaryUrl) : url;
}

void AppendOtaProdCandidates(std::vector<OtaHttpCandidate>& candidates) {
    std::string primary_url = GetConfiguredOtaUrl();
    AppendOtaCandidate(candidates, primary_url, "", "primary");
    AppendOtaCandidate(candidates, kOtaProdBackupUrl1, kOtaProdLogicalHost, "backup1");
    AppendOtaCandidate(candidates, kOtaProdBackupUrl2, kOtaProdLogicalHost, "backup2");
}

#if CONFIG_CONNECTION_TYPE_NERTC
std::vector<OtaHttpCandidate> GetOtaBaseUrlCandidates() {
    std::vector<OtaHttpCandidate> candidates;
    AppendOtaProdCandidates(candidates);
    return candidates;
}
#else
std::vector<OtaHttpCandidate> GetOtaBaseUrlCandidates() {
    std::vector<OtaHttpCandidate> candidates;
    AppendOtaProdCandidates(candidates);
    return candidates;
}
#endif

std::string BuildOtaRequestUrl(const std::string& base_url, const char* suffix) {
    std::string url = base_url;
    if (url.empty()) {
        return url;
    }

    if (suffix && suffix[0] != '\0') {
        auto query_pos = url.find('?');
        std::string path = query_pos == std::string::npos ? url : url.substr(0, query_pos);
        std::string query = query_pos == std::string::npos ? "" : url.substr(query_pos);
        if (!path.empty() && path.back() != '/') {
            path += '/';
        }
        path += (suffix[0] == '/') ? (suffix + 1) : suffix;
        url = path + query;
    }

    auto& application = Application::GetInstance();
    if (!application.GetAppkey().empty()) {
        url += (url.find('?') == std::string::npos ? "?appkey=" : "&appkey=") + application.GetAppkey();
    }
    return url;
}

std::string ExtractHostFromUrl(const std::string& url) {
    auto scheme_pos = url.find("://");
    if (scheme_pos == std::string::npos) {
        return "";
    }

    auto host_begin = scheme_pos + 3;
    auto host_end = url.find_first_of("/?", host_begin);
    std::string host_port = host_end == std::string::npos ? url.substr(host_begin)
                                                          : url.substr(host_begin, host_end - host_begin);
    auto colon_pos = host_port.find(':');
    return colon_pos == std::string::npos ? host_port : host_port.substr(0, colon_pos);
}

bool IsIpv4Address(const std::string& host) {
    sockaddr_in addr = {};
    return !host.empty() && inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1;
}

void AppendUniqueResolvedIp(std::vector<std::string>& resolved_ips, const std::string& ip) {
    if (ip.empty() ||
        std::find(resolved_ips.begin(), resolved_ips.end(), ip) != resolved_ips.end()) {
        return;
    }
    resolved_ips.push_back(ip);
}

enum class Ml307DnsQueryStatus {
    kUnsupported,
    kSendFailed,
    kTimeout,
    kNoIpv4Address,
    kResolved,
};

struct Ml307DnsQueryResult {
    Ml307DnsQueryStatus status = Ml307DnsQueryStatus::kUnsupported;
    std::vector<std::string> resolved_ips;
};

Ml307DnsQueryResult ResolveDomainViaMl307At(const std::string& host) {
    constexpr EventBits_t kDnsLookupDoneBit = BIT0;
    constexpr int kDnsCommandTimeoutMs = 3000;
    constexpr int kDnsWaitTimeoutMs = 2000;

    auto* network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        return {};
    }

    auto* modem = static_cast<AtModem*>(network);
    auto at_uart = modem->GetAtUart();
    if (at_uart == nullptr) {
        return {};
    }

    Ml307DnsQueryResult result;
    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == nullptr) {
        return {};
    }

    std::mutex result_mutex;
    auto callback_it = at_uart->RegisterUrcCallback(
        [&](const std::string& command, const std::vector<AtArgumentValue>& arguments) {
            if (command != "MDNSGIP" || arguments.empty()) {
                return;
            }

            size_t ip_begin = 0;
            bool host_matched = false;
            for (size_t index = 0; index < arguments.size(); ++index) {
                if (arguments[index].type != AtArgumentValue::Type::String) {
                    continue;
                }
                if (arguments[index].string_value == host) {
                    host_matched = true;
                    ip_begin = index + 1;
                    break;
                }
            }

            if (!host_matched &&
                arguments[0].type == AtArgumentValue::Type::String &&
                !arguments[0].string_value.empty() &&
                arguments[0].string_value != host) {
                return;
            }

            std::lock_guard<std::mutex> lock(result_mutex);
            result.resolved_ips.clear();
            for (size_t index = ip_begin; index < arguments.size(); ++index) {
                if (arguments[index].type != AtArgumentValue::Type::String) {
                    continue;
                }
                if (!IsIpv4Address(arguments[index].string_value)) {
                    continue;
                }
                AppendUniqueResolvedIp(result.resolved_ips, arguments[index].string_value);
            }
            result.status = result.resolved_ips.empty() ? Ml307DnsQueryStatus::kNoIpv4Address
                                                        : Ml307DnsQueryStatus::kResolved;
            xEventGroupSetBits(event_group, kDnsLookupDoneBit);
        });

    const std::string command = "AT+MDNSGIP=\"" + host + "\"";
    if (!at_uart->SendCommand(command, kDnsCommandTimeoutMs)) {
        at_uart->UnregisterUrcCallback(callback_it);
        vEventGroupDelete(event_group);
        result.status = Ml307DnsQueryStatus::kSendFailed;
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(event_group,
                                           kDnsLookupDoneBit,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(kDnsWaitTimeoutMs));
    at_uart->UnregisterUrcCallback(callback_it);
    vEventGroupDelete(event_group);

    if ((bits & kDnsLookupDoneBit) == 0) {
        result.status = Ml307DnsQueryStatus::kTimeout;
    }
    return result;
}

#if NERTC_HTTP_FALLBACK_FORCE_TEST_FLOW
bool ShouldForceFallbackAfterDomainProbe(const OtaHttpCandidate& candidate,
                                         const std::string& final_url) {
    if (!candidate.host_header.empty()) {
        return false;
    }

    auto host = ExtractHostFromUrl(final_url);
    return !host.empty() && !IsIpv4Address(host);
}
#endif

void LogResolvedIpForDomainRequest(const char* request_name,
                                   const char* candidate_name,
                                   const std::string& final_url) {
    if (request_name == nullptr || std::strcmp(request_name, "ota_check_version") != 0) {
        return;
    }

    auto host = ExtractHostFromUrl(final_url);
    if (host.empty() || IsIpv4Address(host)) {
        return;
    }

    auto& board = Board::GetInstance();
    if (board.GetBoardType() == "ml307") {
        auto result = ResolveDomainViaMl307At(host);
        switch (result.status) {
            case Ml307DnsQueryStatus::kResolved: {
                std::ostringstream oss;
                for (size_t i = 0; i < result.resolved_ips.size(); ++i) {
                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << result.resolved_ips[i];
                }
                ESP_LOGI(TAG,
                         "[%s] %s pre-request domain dns resolved, host=%s, ips=%s",
                         candidate_name ? candidate_name : "",
                         request_name,
                         host.c_str(),
                         oss.str().c_str());
                return;
            }
            case Ml307DnsQueryStatus::kSendFailed:
                ESP_LOGW(TAG,
                         "[%s] %s pre-request domain dns lookup failed, host=%s, rc=-1",
                         candidate_name ? candidate_name : "",
                         request_name,
                         host.c_str());
                return;
            case Ml307DnsQueryStatus::kTimeout:
                ESP_LOGW(TAG,
                         "[%s] %s pre-request domain dns lookup timeout, host=%s",
                         candidate_name ? candidate_name : "",
                         request_name,
                         host.c_str());
                return;
            case Ml307DnsQueryStatus::kNoIpv4Address:
                ESP_LOGW(TAG,
                         "[%s] %s pre-request domain dns lookup got no ipv4 address, host=%s",
                         candidate_name ? candidate_name : "",
                         request_name,
                         host.c_str());
                return;
            case Ml307DnsQueryStatus::kUnsupported:
                return;
        }
    }

    auto* network = board.GetNetwork();
    if (network == nullptr) {
        return;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* address_list = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &address_list);
    if (rc != 0 || address_list == nullptr) {
        ESP_LOGW(TAG,
                 "[%s] %s pre-request domain dns lookup failed, host=%s, rc=%d",
                 candidate_name ? candidate_name : "",
                 request_name,
                 host.c_str(),
                 rc);
        return;
    }

    std::vector<std::string> resolved_ips;
    for (addrinfo* cursor = address_list; cursor != nullptr; cursor = cursor->ai_next) {
        if (cursor->ai_family != AF_INET || cursor->ai_addr == nullptr) {
            continue;
        }

        char ip[INET_ADDRSTRLEN] = {};
        auto* ipv4_addr = reinterpret_cast<sockaddr_in*>(cursor->ai_addr);
        if (inet_ntop(AF_INET, &ipv4_addr->sin_addr, ip, sizeof(ip)) == nullptr) {
            continue;
        }

        if (std::find(resolved_ips.begin(), resolved_ips.end(), ip) == resolved_ips.end()) {
            resolved_ips.emplace_back(ip);
        }
    }
    freeaddrinfo(address_list);

    if (resolved_ips.empty()) {
        ESP_LOGW(TAG,
                 "[%s] %s pre-request domain dns lookup got no ipv4 address, host=%s",
                 candidate_name ? candidate_name : "",
                 request_name,
                 host.c_str());
        return;
    }

    std::ostringstream oss;
    for (size_t i = 0; i < resolved_ips.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << resolved_ips[i];
    }

    ESP_LOGI(TAG,
             "[%s] %s pre-request domain dns resolved, host=%s, ips=%s",
             candidate_name ? candidate_name : "",
             request_name,
             host.c_str(),
             oss.str().c_str());
}

bool LooksLikeHtmlResponse(const std::string& body) {
    auto first = std::find_if_not(body.begin(), body.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (first == body.end()) {
        return false;
    }

    if (*first == '<') {
        return true;
    }

    std::string lower_body = body;
    std::transform(lower_body.begin(), lower_body.end(), lower_body.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower_body.find("<html") != std::string::npos ||
           lower_body.find("<!doctype html") != std::string::npos;
}

bool HasStructuredServiceErrorCode(const std::string& body) {
    if (body.empty() || LooksLikeHtmlResponse(body)) {
        return false;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return false;
    }

    cJSON* code = cJSON_GetObjectItem(root, "code");
    bool has_code = cJSON_IsObject(root) && cJSON_IsNumber(code);
    cJSON_Delete(root);
    return has_code;
}

bool IsValidJsonResponse(const std::string& body) {
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return false;
    }
    cJSON_Delete(root);
    return true;
}

bool ShouldFallbackToNextCandidate(const OtaHttpResult& result) {
    if (result.status_code <= 0) {
        return true;
    }

    if (result.status_code >= 300 && result.status_code < 400) {
        return true;
    }

    if (result.status_code >= 400 && result.status_code < 500) {
        return !HasStructuredServiceErrorCode(result.body);
    }

    if (result.status_code >= 500) {
        return true;
    }

    if (result.status_code != 200) {
        return false;
    }

    if (result.body.empty()) {
        return true;
    }

    if (LooksLikeHtmlResponse(result.body)) {
        return true;
    }

    return !IsValidJsonResponse(result.body);
}

OtaHttpResult SendOtaRequest(
    const char* request_name,
    const std::vector<OtaHttpCandidate>& candidates,
    const char* method,
    const std::string& content,
    const char* suffix,
    std::function<std::unique_ptr<Http>()> setup_http) {
    for (size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        OtaHttpResult result;
        result.candidate_index = static_cast<int>(index);
        result.candidate_name = candidate.name ? candidate.name : "";
        result.final_url = BuildOtaRequestUrl(candidate.base_url, suffix);
        if (result.final_url.size() < 10) {
            result.err = ESP_ERR_INVALID_ARG;
            return result;
        }

        auto http = setup_http();
        if (!http) {
            result.err = ESP_FAIL;
            return result;
        }

        http->SetTimeout(kOtaRequestTimeoutMs);
        if (!candidate.host_header.empty()) {
            http->SetHeader("Host", candidate.host_header);
        }
        http->SetContent(std::string(content));
        LogResolvedIpForDomainRequest(request_name, result.candidate_name.c_str(), result.final_url);
        const char* host_header = candidate.host_header.empty() ? "(url host)" : candidate.host_header.c_str();
        ESP_LOGI(TAG, "[%s][%d/%d] %s url: %s, host_header: %s",
                 result.candidate_name.c_str(),
                 static_cast<int>(index + 1),
                 static_cast<int>(candidates.size()),
                 request_name,
                 result.final_url.c_str(),
                 host_header);
        if (!http->Open(method, result.final_url)) {
            result.err = ESP_FAIL;
            ESP_LOGE(TAG,
                     "[%s][%d/%d] Failed to open HTTP connection",
                     result.candidate_name.c_str(),
                     static_cast<int>(index + 1),
                     static_cast<int>(candidates.size()));
        } else {
            result.status_code = http->GetStatusCode();
            if (result.status_code > 0) {
                result.body = http->ReadAll();
            }
            http->Close();
            result.err = result.status_code == 200 ? ESP_OK : static_cast<esp_err_t>(result.status_code);
        }

#if NERTC_HTTP_FALLBACK_FORCE_TEST_FLOW
        bool force_fallback_after_domain_probe =
            ShouldForceFallbackAfterDomainProbe(candidate, result.final_url);
        if (force_fallback_after_domain_probe) {
            if (!result.body.empty()) {
                ESP_LOGI(TAG,
                         "[%s][%d/%d] %s domain probe response: %s",
                         result.candidate_name.c_str(),
                         static_cast<int>(index + 1),
                         static_cast<int>(candidates.size()),
                         request_name,
                         result.body.c_str());
            }
            ESP_LOGW(TAG,
                     "[%s][%d/%d] %s domain probe finished, forcing fallback for test, err=0x%x status=%d body_len=%d",
                     result.candidate_name.c_str(),
                     static_cast<int>(index + 1),
                     static_cast<int>(candidates.size()),
                     request_name,
                     result.err,
                     result.status_code,
                     static_cast<int>(result.body.size()));
        }
#else
        bool force_fallback_after_domain_probe = false;
#endif

        bool should_fallback =
            force_fallback_after_domain_probe || ShouldFallbackToNextCandidate(result);
        if (!should_fallback || index + 1 >= candidates.size()) {
            return result;
        }

        ESP_LOGW(TAG,
                 "[%s][%d/%d] fallback to next OTA candidate, status=%d",
                 result.candidate_name.c_str(),
                 static_cast<int>(index + 1),
                 static_cast<int>(candidates.size()),
                 result.status_code);
    }

    return OtaHttpResult{};
}

#endif

}  // namespace


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
#if CONFIG_CONNECTION_TYPE_NERTC
    auto candidates = GetOtaBaseUrlCandidates();
    if (candidates.empty()) {
        return "";
    }
    return BuildOtaRequestUrl(candidates.front().base_url, nullptr);
#else
    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (url.empty()) {
        url = CONFIG_OTA_URL;
    }
    return url;
#endif
}

std::unique_ptr<Http> Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);
    auto user_agent = SystemInfo::GetUserAgent();
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
        ESP_LOGI(TAG, "Setup HTTP, User-Agent: %s, Serial-Number: %s", user_agent.c_str(), serial_number_.c_str());
    }
    http->SetHeader("User-Agent", user_agent);
    http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");

    return http;
}

/* 
 * Specification: https://ccnphfhqs21z.feishu.cn/wiki/FjW6wZmisimNBBkov6OcmfvknVd
 */
esp_err_t Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    // Check if there is a new firmware version available
    current_version_ = app_desc->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    std::string url = GetCheckVersionUrl();
    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return ESP_ERR_INVALID_ARG;
    }

    std::string data = board.GetSystemInfoJson();
    ESP_LOGI(TAG, "ota url: %s deviceId:%s\n", url.c_str(), board.GetBoardName().c_str());
    std::string method = data.length() > 0 ? "POST" : "GET";
#if CONFIG_CONNECTION_TYPE_NERTC
    auto candidates = GetOtaBaseUrlCandidates();
    auto result = SendOtaRequest("ota_check_version", candidates, method.c_str(), data, nullptr, [this]() {
        return SetupHttp();
    });

    auto status_code = result.status_code;
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        return result.err;
    }

    data = std::move(result.body);
    ESP_LOGI(TAG, "ota data: %s\n", data.c_str());
#else
    auto http = SetupHttp();
    http->SetContent(std::move(data));

    if (!http->Open(method, url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return ESP_FAIL;
    }

    auto status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        http->Close();
        return static_cast<esp_err_t>(status_code);
    }

    data = http->ReadAll();
    ESP_LOGI(TAG, "ota data: %s\n", data.c_str());
    http->Close();
#endif

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_ERR_INVALID_RESPONSE;
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    has_device_sdk_config_ = false;
    device_sdk_config_.clear();
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
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
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
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
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
#if CONFIG_CONNECTION_TYPE_NERTC
    firmware_md5_.clear();
#endif
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
#if CONFIG_CONNECTION_TYPE_NERTC
        cJSON *md5 = cJSON_GetObjectItem(firmware, "md5");
        if (cJSON_IsString(md5)) {
            firmware_md5_ = md5->valuestring;
        }
#endif

        if (cJSON_IsString(version) && cJSON_IsString(url)) {
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

    cJSON *agent = cJSON_GetObjectItem(root, "agent");
    if (cJSON_IsObject(agent)) {
        cJSON *pipeline = cJSON_GetObjectItem(agent, "pipeline");
        if (cJSON_IsObject(pipeline)) {
            cJSON *interrupt_mode = cJSON_GetObjectItem(pipeline, "interrupt_mode");
            if (cJSON_IsNumber(interrupt_mode)) {
                agent_interrupt_mode_ = interrupt_mode->valueint;
            }
        }
        cJSON* netease_cloud_music = cJSON_GetObjectItem(agent, "netease_cloud_music");
        if (cJSON_IsObject(netease_cloud_music)){
            cJSON *support_music = cJSON_GetObjectItem(netease_cloud_music, "support_music");
            cJSON *support_play_in_4g = cJSON_GetObjectItem(netease_cloud_music, "support_play_in_4g");
            if (cJSON_IsBool(support_music)) {
                support_air_music_player = support_music->valueint;
            }
            if (cJSON_IsBool(support_play_in_4g)){
                support_air_music_in_4G = support_play_in_4g->valueint;
            }
        }
        cJSON *device_sdk_config = cJSON_GetObjectItem(agent, "device_sdk_config");
        has_device_sdk_config_ = cJSON_IsObject(device_sdk_config);
        if (has_device_sdk_config_) {
            char* device_sdk_config_str = cJSON_PrintUnformatted(device_sdk_config);
            if (device_sdk_config_str != nullptr) {
                device_sdk_config_ = device_sdk_config_str;
                cJSON_free(device_sdk_config_str);
            } else {
                has_device_sdk_config_ = false;
                device_sdk_config_.clear();
            }
        } else {
            device_sdk_config_.clear();
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
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
#if CONFIG_CONNECTION_TYPE_NERTC
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *first_candidate = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *p = first_candidate;
    while (p != nullptr) {
        if (strcmp(p->label, "blufi") == 0) {
            ESP_LOGW(TAG, "Skipping blufi partition for main firmware OTA");
            p = esp_ota_get_next_update_partition(p);
            if (p == first_candidate) {
                break;
            }
            continue;
        }
        if (running != nullptr && p->address == running->address) {
            ESP_LOGW(TAG, "Skipping running partition %s for OTA", p->label);
            p = esp_ota_get_next_update_partition(p);
            if (p == first_candidate) {
                break;
            }
            continue;
        }
        *out = p;
        return true;
    }
    return false;
#else
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
#endif
}

bool Ota::Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    esp_ota_handle_t update_handle = 0;
    // auto update_partition = esp_ota_get_next_update_partition(NULL);
    // if (update_partition == NULL) {
    //     ESP_LOGE(TAG, "Failed to get update partition");
    //     return false;
    // }

    const esp_partition_t *update_partition;
    if (!GetNextSafePartition(&update_partition)) {
        ESP_LOGE(TAG, "No available OTA slot (ota_0/ota_1)");
        return false;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);
    bool image_header_checked = false;
    std::string image_header;

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to get firmware, status code: %d", http->GetStatusCode());
        return false;
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }

    constexpr size_t PAGE_SIZE = 4096;
    char* buffer = (char*)heap_caps_malloc(PAGE_SIZE, MALLOC_CAP_INTERNAL);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return false;
    }

    size_t buffer_offset = 0;  // Current data size in buffer
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    while (true) {
        int ret = http->Read(buffer + buffer_offset, PAGE_SIZE - buffer_offset);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            heap_caps_free(buffer);
            return false;
        }

        // Calculate speed and progress every second
        recent_read += ret;
        total_read += ret;
        buffer_offset += ret;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %uB/s", progress, total_read, content_length, recent_read);
            if (callback) {
                callback(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }

        if (!image_header_checked) {
            image_header.append(buffer, buffer_offset);
            if (image_header.size() >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, image_header.data() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));

                auto current_version = esp_app_get_description()->version;
                ESP_LOGI(TAG, "Current version: %s, New version: %s", current_version, new_app_info.version);

                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle)) {
                    esp_ota_abort(update_handle);
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    heap_caps_free(buffer);
                    return false;
                }

                image_header_checked = true;
                std::string().swap(image_header);
            }
        }

        // Write to flash when buffer is full (4KB) or it's the last chunk
        bool is_last_chunk = (ret == 0);
        if (buffer_offset == PAGE_SIZE || (is_last_chunk && buffer_offset > 0)) {
            auto err = esp_ota_write(update_handle, buffer, buffer_offset);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                heap_caps_free(buffer);
                return false;
            }

            buffer_offset = 0;
        }

        if (is_last_chunk) {
            break;
        }
    }
    http->Close();
    heap_caps_free(buffer);

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful");
    return true;
}

bool Ota::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    return Upgrade(firmware_url_, callback);
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

    std::string data = GetActivationPayload();
#if CONFIG_CONNECTION_TYPE_NERTC
    auto candidates = GetOtaBaseUrlCandidates();
    auto result = SendOtaRequest("ota_activate", candidates, "POST", data, "activate", [this]() {
        return SetupHttp();
    });
    auto status_code = result.status_code;
    if (status_code == 202) {
        return ESP_ERR_TIMEOUT;
    }
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, result.body.c_str());
        return ESP_FAIL;
    }
#else
    std::string url = GetCheckVersionUrl();
    if (url.back() != '/') {
        url += "/activate";
    } else {
        url += "activate";
    }

    auto http = SetupHttp();
    http->SetContent(std::move(data));

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return ESP_FAIL;
    }

    auto status_code = http->GetStatusCode();
    if (status_code == 202) {
        http->Close();
        return ESP_ERR_TIMEOUT;
    }
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, http->ReadAll().c_str());
        http->Close();
        return ESP_FAIL;
    }
    http->Close();
#endif

    ESP_LOGI(TAG, "Activation successful");
    return ESP_OK;
}
