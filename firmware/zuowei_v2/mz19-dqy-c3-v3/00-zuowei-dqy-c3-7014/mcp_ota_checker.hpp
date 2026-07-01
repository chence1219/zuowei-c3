#pragma once

// mcp_ota_checker.hpp
#include "application.h"
#include "esp_log.h"
#include "mcp_server.h"

#define TAG "McpOtaChecker"

class McpOtaChecker {
public:
  McpOtaChecker() {}

  ~McpOtaChecker() {}

  static McpOtaChecker *GetInstance() {
    static McpOtaChecker instance;
    return &instance;
  }

  void Init() {
    McpServer::GetInstance().AddTool(
        "self.get_current_version", "Gets the current version.", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          auto ota = Application::GetInstance().GetCustomOta();
          ota.CheckVersion();
          auto version = ota.GetCurrentVersion();
          ESP_LOGI(TAG, "Current version: %s", version.c_str());
          return "{\"version\": \"" + version + "\"}";
        });

    McpServer::GetInstance().AddTool(
        "self.get_new_version", "Gets the new version.", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          auto ota = Application::GetInstance().GetCustomOta();
          ota.CheckVersion();
          auto new_version = ota.GetFirmwareVersion();
          ESP_LOGI(TAG, "New version: %s", new_version.c_str());
          return "{\"new_version\": \"" + new_version + "\"}";
        });

    McpServer::GetInstance().AddTool(
        "self.upgrade",
        "Starts the OTA upgrade process. This will check for updates and "
        "perform the upgrade if a new version is available.",
        PropertyList(), [](const PropertyList &properties) -> ReturnValue {
          Application::GetInstance().StartCheckNewVersionForCustom();
          return true;
        });
  }
};