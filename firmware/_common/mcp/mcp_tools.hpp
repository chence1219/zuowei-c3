#include "../display/zw_lcd_display.h"
// #include "../iot/alarm.h"
// #include "mcp_alarm.hpp"
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "esp_log.h"
#include "mcp_server.h"

#define TAG "McpTools"

class McpTools {
  // AlarmManager *alarm_manager;
  std::function<void(int time, const std::string &name)> on_alarm_;
  // lv_obj_t *alarm_popup_ = nullptr;
  // lv_obj_t *alarm_label_ = nullptr;
  // lv_timer_t *alarm_play_timer = nullptr;
  // int alarm_cnt = -1;

  // void InitAlarm() {
  //   alarm_popup_ = lv_obj_create(lv_screen_active());
  //   lv_obj_set_scrollbar_mode(alarm_popup_, LV_SCROLLBAR_MODE_OFF);
  //   // lv_obj_set_size(alarm_popup_, LV_HOR_RES * 0.9,
  //   //                 font_noto_thin_no_arab_16_1.line_height * 5);
  //   lv_obj_set_size(alarm_popup_, LV_HOR_RES * 0.9, 16 * 5);
  //   // lv_obj_align(alarm_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
  //   lv_obj_center(alarm_popup_);
  //   lv_obj_set_style_bg_color(alarm_popup_, lv_color_black(), 0);
  //   lv_obj_set_style_radius(alarm_popup_, 10, 0);
  //   alarm_label_ = lv_label_create(alarm_popup_);
  //   lv_label_set_text(alarm_label_, "");
  //   lv_obj_set_style_text_color(alarm_label_, lv_color_white(), 0);
  //   lv_obj_center(alarm_label_);

  //   lv_obj_add_flag(alarm_popup_, LV_OBJ_FLAG_HIDDEN);

  //   alarm_play_timer = lv_timer_create(
  //       [](lv_timer_t *timer) {
  //         McpTools *mcp_tools =
  //             static_cast<McpTools *>(lv_timer_get_user_data(timer));
  //         ESP_LOGW(TAG, "lv_timer_cb: %d", mcp_tools->alarm_cnt);
  //         auto &app = Application::GetInstance();
  //         mcp_tools->alarm_cnt++;
  //         if (app.GetDeviceState() == kDeviceStateIdle &&
  //             mcp_tools->alarm_cnt <= 5) {
  //           auto codec = Board::GetInstance().GetAudioCodec();
  //           codec->EnableOutput(true);
  //           app.PlaySound(Lang::Sounds::OGG_ALARM_RING);
  //         } else {
  //           mcp_tools->StopAlarm();
  //         }
  //       },
  //       4500, this);
  //   lv_timer_pause(alarm_play_timer);

  //   alarm_manager->OnAlarm([this](int time, const std::string &name) {
  //     ESP_LOGI(TAG, "OnAlarm: %d, alarm_name: %s", time, name.c_str());
  //     if (on_alarm_) {
  //       on_alarm_(time, name);
  //     }

  //     auto &app = Application::GetInstance();
  //     app.Close();

  //     app.Schedule([this, name]() {
  //       auto &app = Application::GetInstance();
  //       if (app.GetDeviceState() == kDeviceStateIdle) {
  //         StartAlarm(name);
  //       } else {
  //         app.Schedule([this, name]() {
  //           auto &app = Application::GetInstance();
  //           if (app.GetDeviceState() == kDeviceStateIdle) {
  //             StartAlarm(name);
  //           }
  //         });
  //       }
  //     });
  //   });
  // }
  // void
  // OnAlarm(std::function<void(int time, const std::string &name)> callback) {
  //   on_alarm_ = callback;
  // }

public:
  McpTools(){};
  ~McpTools(){};
  static McpTools *GetInstance() {
    static McpTools instance;
    return &instance;
  }

  void McpToolsInit() {
    // alarm_manager = new AlarmManager();
    // InitAlarm();

    McpServer::GetInstance().AddTool(
        "self.screen.set_angle",
        "Sets the angle of the screen. Parameters: angle (0, 90, 180, 270)",
        PropertyList({Property("angle", kPropertyTypeInteger, 0, 270)}),
        [](const PropertyList &properties) -> ReturnValue {
          Board &board = Board::GetInstance();
          int angle = properties["angle"].value<int>();
          if (angle % 90 != 0 || angle < 0 || angle > 270) {
            return "{\"success\": false, \"message\": \"Invalid angle value\"}";
          }
          ZwLcdDisplay *display = (ZwLcdDisplay *)(board.GetDisplay());
          if (!display) {
            return "{\"success\": false, \"message\": \"Display not found\"}";
          }
          display->SetRotationAngle(angle);
          ESP_LOGI("McpTools", "Set screen angle to %d degrees", angle);
          return true;
        });

    McpServer::GetInstance().AddTool(
        "self.screen.get_angle", "Gets the current angle of the screen.",
        PropertyList(), [](const PropertyList &properties) -> ReturnValue {
          Board &board = Board::GetInstance();
          ZwLcdDisplay *display = (ZwLcdDisplay *)(board.GetDisplay());
          if (!display) {
            return "{\"success\": false, \"message\": \"Display not found\"}";
          }
          int angle = display->GetRotationAngle();
          ESP_LOGI("McpTools", "Current screen angle is %d degrees", angle);
          return angle;
        });
    McpServer::GetInstance().AddTool(
        "self.low_power.set_auto_poweroff",
        "Sets whether to enable automatic power off. Parameters: enable "
        "(true/false)",
        PropertyList({Property("enable", kPropertyTypeBoolean)}),
        [](const PropertyList &properties) -> ReturnValue {
          bool enable = properties["enable"].value<bool>();
          Settings settings("low_power", true);
          settings.SetInt("auto_poweroff", enable ? 1 : 0);
          return true;
        });
    McpServer::GetInstance().AddTool(
        "self.low_power.get_auto_poweroff",
        "Gets whether automatic power off is enabled.", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          Settings settings("low_power", false);
          auto auto_poweroff_ =
              settings.GetInt("auto_poweroff", 1) ? true : false;
          ESP_LOGI("McpTools", "Auto power off is %s",
                   auto_poweroff_ ? "enabled" : "disabled");
          return auto_poweroff_;
        });
    // McpServer::GetInstance().AddTool(
    //     "self.alarm.set_alarm",
    //     "Sets an alarm. Parameters: seconds_from_now "
    //     "(int), alarm_name (string)",
    //     PropertyList({Property("seconds_from_now", kPropertyTypeInteger),
    //                   Property("alarm_name", kPropertyTypeString)}),
    //     [this](const PropertyList &properties) -> ReturnValue {
    //       int seconds_from_now = properties["seconds_from_now"].value<int>();
    //       std::string alarm_name =
    //           properties["alarm_name"].value<std::string>();
    //       alarm_manager->SetAlarm(seconds_from_now, alarm_name);
    //       return true;
    //     });
    // McpServer::GetInstance().AddTool(
    //     "self.alarm.get_alarms_status",
    //     "Gets the current status of all alarms.", PropertyList(),
    //     [this](const PropertyList &properties) -> ReturnValue {
    //       std::string status = alarm_manager->GetAlarmsStatus();
    //       ESP_LOGI("McpTools", "Current alarms status: %s", status.c_str());
    //       return status;
    //     });

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

  bool GetIsAutoPoweroff() {
    Settings settings("low_power", false);
    auto auto_poweroff_ = settings.GetInt("auto_poweroff", 1) ? true : false;
    ESP_LOGI("McpTools", "Auto power off is %s",
             auto_poweroff_ ? "enabled" : "disabled");
    return auto_poweroff_;
  }

  // bool IsAlarm(void) { return (alarm_cnt > -1); }
  // void StartAlarm(const std::string &name) {
  //   ESP_LOGW(TAG, "StartAlarm");
  //   alarm_cnt = 0;
  //   auto display = Board::GetInstance().GetDisplay();
  //   {
  //     DisplayLockGuard lock(display);
  //     lv_label_set_text(this->alarm_label_, name.c_str());
  //     lv_obj_remove_flag(this->alarm_popup_, LV_OBJ_FLAG_HIDDEN);
  //   }
  //   display->SetStatus("提醒");
  //   if (lv_timer_get_paused(this->alarm_play_timer)) {
  //     lv_timer_ready(this->alarm_play_timer);
  //     lv_timer_resume(this->alarm_play_timer);
  //   }
  // }

  // void StopAlarm(void) {
  //   ESP_LOGW(TAG, "StopAlarm");
  //   if (alarm_cnt > -1) {
  //     alarm_cnt = -1;
  //     DisplayLockGuard lock(Board::GetInstance().GetDisplay());
  //     lv_timer_pause(alarm_play_timer);
  //     lv_obj_add_flag(alarm_popup_, LV_OBJ_FLAG_HIDDEN);
  //   }
  // }
};
