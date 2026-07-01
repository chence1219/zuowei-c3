#pragma once

#include "application.h"
#include "assets/lang_config.h"
#include "esp_log.h"
#include "mcp_server.h"
#include "settings.h"

#define TAG "McpAecController"

class McpAecController {
public:
  McpAecController() {}
  ~McpAecController() {}

  static McpAecController &GetInstance() {
    static McpAecController instance;
    return instance;
  }

  void Init() {
    auto settings = Settings("aec_mode", true);
#ifdef CONFIG_DEVICE_AEC_DEFAULT_OFF
    auto aec_enable = settings.GetBool("enable", false);
#else
    auto aec_enable = settings.GetBool("enable", true);
#endif
    if (aec_enable) {
      Application::GetInstance().SetAecMode(AecMode::kAecOnDeviceSide);
    } else {
      Application::GetInstance().SetAecMode(AecMode::kAecOff);
    }
    McpServer::GetInstance().AddTool(
        "self.set_aec_onoff",
        "设置是否开启实时打断。(true为开启实时打断模式, "
        "false为唤醒词打断模式。)当用户说开启实时打断模式时为true, "
        "当用户说关闭实时打断或者切换到唤醒词打断模式时为false",
        PropertyList({Property("onoff", kPropertyTypeBoolean)}),
        [](const PropertyList &properties) -> ReturnValue {
          auto onoff = properties["onoff"].value<bool>();
          Application::GetInstance().PlaySound(
              onoff ? Lang::Sounds::OGG_REALTIME : Lang::Sounds::OGG_WAKEWORD);
          Application::GetInstance().SetAecMode(
              onoff ? kAecOnDeviceSide : kAecOff, true);
          auto settings = Settings("aec_mode", true);
          settings.SetBool("enable", onoff);
          return true;
        });
    McpServer::GetInstance().AddTool(
        "self.get_aec_onoff", "获取是否开启实时打断。", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          return Application::GetInstance().GetAecMode() == kAecOnDeviceSide;
        });
  }
};