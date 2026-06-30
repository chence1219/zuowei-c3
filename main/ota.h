#ifndef _OTA_H
#define _OTA_H

#include <functional>
#include <string>

#include <esp_err.h>
#include "board.h"

class Ota {
public:
    Ota();
    ~Ota();

    esp_err_t CheckVersion();
    esp_err_t Activate();
    bool HasActivationChallenge() { return has_activation_challenge_; }
    bool HasNewVersion() { return has_new_version_; }
    bool HasMqttConfig() { return has_mqtt_config_; }
    bool HasWebsocketConfig() { return has_websocket_config_; }
    bool HasActivationCode() { return has_activation_code_; }
    bool HasServerTime() { return has_server_time_; }
    bool HasDeviceSdkConfig() const { return has_device_sdk_config_; }
    bool StartUpgrade(std::function<void(int progress, size_t speed)> callback);
    static bool Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback);
    void MarkCurrentVersionValid();

    const std::string& GetFirmwareVersion() const { return firmware_version_; }
    const std::string& GetCurrentVersion() const { return current_version_; }
    const std::string& GetFirmwareUrl() const { return firmware_url_; }
#if CONFIG_CONNECTION_TYPE_NERTC
    const std::string& GetFirmwareMd5() const { return firmware_md5_; }
#endif
    const std::string& GetActivationMessage() const { return activation_message_; }
    const std::string& GetActivationCode() const { return activation_code_; }
    const std::string& GetDeviceSdkConfig() const { return device_sdk_config_; }
    std::string GetCheckVersionUrl();
    int GetOtaAgentInterruptMode() const { return agent_interrupt_mode_; }
    bool GetSupportAirMusicPlayer() const { return support_air_music_player; }
    bool GetSupportAirMusicIn4G() const { return support_air_music_in_4G; }

protected:
    std::string activation_message_;
    std::string activation_code_;
    bool has_new_version_ = false;
    bool has_mqtt_config_ = false;
    bool has_websocket_config_ = false;
    bool has_server_time_ = false;
    bool has_activation_code_ = false;
    bool has_serial_number_ = false;
    bool has_activation_challenge_ = false;
    bool has_device_sdk_config_ = false;
    std::string current_version_;
    std::string firmware_version_;
    std::string firmware_url_;
#if CONFIG_CONNECTION_TYPE_NERTC
    std::string firmware_md5_;
#endif
    std::string activation_challenge_;
    std::string device_sdk_config_;
    std::string serial_number_;
    int activation_timeout_ms_ = 30000;
    int agent_interrupt_mode_ = -1; //0:不打断，1:开始说话打断，2:结束说话打断, 3:打断词打断
    bool support_air_music_player = false;
    bool support_air_music_in_4G = false;
    std::function<void(int progress, size_t speed)> upgrade_callback_;
    std::vector<int> ParseVersion(const std::string& version);
    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);
    std::string GetActivationPayload();
    std::unique_ptr<Http> SetupHttp();
};

#endif // _OTA_H
