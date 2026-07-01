#pragma once

#include "application.h"
#include "display.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "lvgl.h"
#include "lvgl_theme.h"
#include "../../../_common/mcp/mcp_alarm.hpp"
#include "misc/lv_area.h"
#include "weather_service.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <time.h>

#define TAG "LockScreen"
#define LOCK_SCREEN_IDLE_TIMEOUT_US (5 * 1000 * 1000ULL)
#define LOCK_SCREEN_GESTURE_EDGE_HEIGHT 48
#define LOCK_SCREEN_SHOW_THRESHOLD 48
#define LOCK_SCREEN_HIDE_THRESHOLD 48
#define LOCK_SCREEN_TOUCH_LOG_INTERVAL_US (50 * 1000)

// Layout constants
#define MARGIN_TOP 18
#define MARGIN_BOTTOM 18
#define MARGIN_LEFT 18
#define MARGIN_RIGHT 18
#define LINE_GAP 8
#define LAYOUT_OFFSET_Y -10

// Color definitions
#define COLOR_WHITE 0xFFFFFF
#define COLOR_WEATHER 0x666666
#define COLOR_DATE 0xBEBEBE
#define COLOR_LUNAR 0x8C8C8C

LV_FONT_DECLARE(font_misans_heavy_time);
LV_FONT_DECLARE(font_misans_medium_wdr);
// LV_FONT_DECLARE(font_misans_medium_weather);
// LV_FONT_DECLARE(font_misans_regular_date);
// LV_FONT_DECLARE(font_misans_regular_date_and_region);

// 天气PNG图片声明
extern const lv_image_dsc_t png_sunny;
extern const lv_image_dsc_t png_cloudy;
extern const lv_image_dsc_t png_overcast;
extern const lv_image_dsc_t png_light_rain;
extern const lv_image_dsc_t png_moderate_rain;
extern const lv_image_dsc_t png_heavy_rain;
extern const lv_image_dsc_t png_thundershower;
extern const lv_image_dsc_t png_light_snow;
extern const lv_image_dsc_t png_moderate_snow;
extern const lv_image_dsc_t png_heavy_snow;
extern const lv_image_dsc_t png_sleet;
extern const lv_image_dsc_t png_fog;
extern const lv_image_dsc_t png_haze;
extern const lv_image_dsc_t png_sandstorm;

class LockScreen {
public:
  LockScreen()
      : display_(nullptr), lock_screen_obj_(nullptr), time_label_(nullptr),
        weather_temp_label_(nullptr), weather_icon_label_(nullptr),
        date_label_(nullptr), lunar_label_(nullptr),
        edge_gesture_obj_(nullptr), lock_screen_gesture_obj_(nullptr),
        idle_timer_(nullptr),
        update_timer_(nullptr), is_visible_(false), drag_start_y_(0),
        drag_start_obj_y_(0), drag_mode_(DragMode::kNone) {}

  ~LockScreen() {
    if (update_timer_) {
      esp_timer_stop(update_timer_);
      esp_timer_delete(update_timer_);
    }
    if (idle_timer_) {
      esp_timer_stop(idle_timer_);
      esp_timer_delete(idle_timer_);
    }
  }

  static LockScreen *GetInstance() {
    static LockScreen instance;
    return &instance;
  }

  void Init(Display *display) {
    // display_ = display;

    // esp_timer_create_args_t timer_args = {
    //     .callback = &LockScreen::IdleTimerCallback,
    //     .arg = this,
    //     .dispatch_method = ESP_TIMER_TASK,
    //     .name = "lock_screen_idle_timer",
    //     .skip_unhandled_events = true,
    // };
    // esp_timer_create(&timer_args, &idle_timer_);

    // {
    //   DisplayLockGuard lock(display_);
    //   EnsureGestureEdgeLocked();
    // }

    // ESP_LOGI(TAG, "Lock screen initialized");
  }

  void StartIdleTimer() {
    // if (idle_timer_) {
    //   esp_timer_stop(idle_timer_);
    //   esp_timer_start_once(idle_timer_, LOCK_SCREEN_IDLE_TIMEOUT_US);
    //   ESP_LOGI(TAG, "Idle timer started");
    // }
  }

  void StopIdleTimer() {
    // if (idle_timer_) {
    //   esp_timer_stop(idle_timer_);
    //   ESP_LOGI(TAG, "Idle timer stopped");
    // }
  }

  void Show() {
    // if (is_visible_) {
    //   if (lock_screen_obj_ != nullptr) {
    //     lv_obj_set_y(lock_screen_obj_, 0);
    //   }
    //   RequestUpdateDisplay();
    //   return;
    // }

    // if (McpAlarm::GetInstance()->IsAlarm()) {
    //   ESP_LOGI(TAG, "Skip showing lock screen while alarm is ringing");
    //   return;
    // }

    // if (display_ == nullptr) {
    //   ESP_LOGE(TAG, "Display not initialized");
    //   return;
    // }

    // Application::GetInstance().Close();

    // {
    //   DisplayLockGuard lock(display_);

    //   EnsureLockScreenLocked(0);
    //   is_visible_ = true;
    //   UpdateDisplayLocked();

    //   if (update_timer_ == nullptr) {
    //     esp_timer_create_args_t update_timer_args = {
    //         .callback = &LockScreen::UpdateTimeCallback,
    //         .arg = this,
    //         .dispatch_method = ESP_TIMER_TASK,
    //         .name = "lock_screen_time_update",
    //         .skip_unhandled_events = true,
    //     };
    //     esp_timer_create(&update_timer_args, &update_timer_);
    //   }
    //   esp_timer_stop(update_timer_);
    //   esp_timer_start_periodic(update_timer_, 1000000);

    //   ESP_LOGI(TAG, "Lock screen shown");
    // }
    // Application::GetInstance().Schedule([]() {
    //   vTaskDelay(pdMS_TO_TICKS(1000));
    //   WeatherService::GetInstance()->OnLockScreenVisible();
    // });
  }

  void Hide() {
    // if (!is_visible_ && lock_screen_obj_ == nullptr) {
    //   return;
    // }

    // if (display_ == nullptr) {
    //   ESP_LOGE(TAG, "Display not initialized");
    //   return;
    // }

    // DisplayLockGuard lock(display_);

    // if (update_timer_) {
    //   esp_timer_stop(update_timer_);
    // }
    // lv_async_call_cancel(&LockScreen::AsyncUpdateDisplay, this);
    // lv_async_call_cancel(&LockScreen::AsyncHide, this);
    // lv_anim_delete(lock_screen_obj_, NULL);

    // DeleteLockScreenLocked();

    // is_visible_ = false;
    // drag_mode_ = DragMode::kNone;
    // WeatherService::GetInstance()->OnLockScreenHidden();
    // ESP_LOGI(TAG, "Lock screen hidden");
  }

  bool IsVisible() const { return is_visible_; }

private:
  enum class DragMode {
    kNone,
    kOpening,
    kClosing,
  };

  Display *display_;
  lv_obj_t *lock_screen_obj_;
  lv_obj_t *time_label_;
  lv_obj_t *weather_temp_label_;
  lv_obj_t *weather_icon_label_;
  lv_obj_t *date_label_;
  lv_obj_t *lunar_label_;
  lv_obj_t *edge_gesture_obj_;
  lv_obj_t *lock_screen_gesture_obj_;
  esp_timer_handle_t idle_timer_;
  esp_timer_handle_t update_timer_;
  bool is_visible_;
  lv_coord_t drag_start_y_;
  lv_coord_t drag_start_obj_y_;
  DragMode drag_mode_;

  static void IdleTimerCallback(void *arg) {
    // LockScreen *lock_screen = static_cast<LockScreen *>(arg);
    // if (McpAlarm::GetInstance()->IsAlarm()) {
    //   ESP_LOGI(TAG, "Idle timeout reached during alarm, skipping lock screen");
    //   return;
    // }
    // auto state = Application::GetInstance().GetDeviceState();
    // if (state == kDeviceStateIdle) {
    //   ESP_LOGI(TAG, "Idle timeout reached, showing lock screen");
    //   Application::GetInstance().Schedule(
    //       [lock_screen]() { lock_screen->Show(); });
    // } else {
    //   ESP_LOGI(
    //       TAG,
    //       "Idle timeout reached but not in idle state, skipping lock screen");
    // }
  }

  static void UpdateTimeCallback(void *arg) {
    // LockScreen *lock_screen = static_cast<LockScreen *>(arg);
    // lock_screen->RequestUpdateDisplay();
  }

  static void AsyncUpdateDisplay(void *arg) {
    // LockScreen *lock_screen = static_cast<LockScreen *>(arg);
    // lock_screen->UpdateDisplay();
  }

  static void AsyncHide(void *arg) {
    // LockScreen *lock_screen = static_cast<LockScreen *>(arg);
    // lock_screen->Hide();
  }

  static void GestureEventCallback(lv_event_t *e) {
    // auto *lock_screen = static_cast<LockScreen *>(lv_event_get_user_data(e));
    // if (lock_screen == nullptr) {
    //   return;
    // }
    // lock_screen->HandleGestureEvent(e);
  }

  bool HasLockScreenUi() const {
    return lock_screen_obj_ != nullptr && time_label_ != nullptr &&
           weather_temp_label_ != nullptr && weather_icon_label_ != nullptr &&
           date_label_ != nullptr && lunar_label_ != nullptr;
  }

  bool CanOpenByGesture() const {
    // if (display_ == nullptr || is_visible_ || McpAlarm::GetInstance()->IsAlarm()) {
    //   return false;
    // }
    // return Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
    return false;
  }

  void RequestHide() {
    // lv_async_call_cancel(&LockScreen::AsyncHide, this);
    // lv_async_call(&LockScreen::AsyncHide, this);
  }

  void EnsureGestureEdgeLocked() {
    if (edge_gesture_obj_ != nullptr) {
      return;
    }

    // edge_gesture_obj_ = lv_obj_create(lv_layer_top());
    // lv_obj_set_size(edge_gesture_obj_, LV_HOR_RES, LOCK_SCREEN_GESTURE_EDGE_HEIGHT);
    // lv_obj_set_pos(edge_gesture_obj_, 0, 0);
    // lv_obj_set_style_bg_opa(edge_gesture_obj_, LV_OPA_TRANSP, 0);
    // lv_obj_set_style_border_width(edge_gesture_obj_, 0, 0);
    // lv_obj_set_style_pad_all(edge_gesture_obj_, 0, 0);
    // lv_obj_set_style_radius(edge_gesture_obj_, 0, 0);
    // lv_obj_remove_flag(edge_gesture_obj_, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_event_cb(edge_gesture_obj_, GestureEventCallback,
    //                     LV_EVENT_PRESSED, this);
    // lv_obj_add_event_cb(edge_gesture_obj_, GestureEventCallback,
    //                     LV_EVENT_PRESSING, this);
    // lv_obj_add_event_cb(edge_gesture_obj_, GestureEventCallback,
    //                     LV_EVENT_RELEASED, this);
    // lv_obj_add_event_cb(edge_gesture_obj_, GestureEventCallback,
    //                     LV_EVENT_PRESS_LOST, this);
  }

  void EnsureLockScreenLocked(lv_coord_t y) {
    // if (lock_screen_obj_ == nullptr) {
    //   lock_screen_obj_ = lv_obj_create(lv_layer_top());
    //   lv_obj_set_size(lock_screen_obj_, LV_HOR_RES, LV_VER_RES);
    //   lv_obj_set_style_border_width(lock_screen_obj_, 0, 0);
    //   lv_obj_set_style_pad_all(lock_screen_obj_, 0, 0);
    //   lv_obj_set_style_pad_left(lock_screen_obj_, MARGIN_LEFT, 0);
    //   lv_obj_remove_flag(lock_screen_obj_, LV_OBJ_FLAG_SCROLLABLE);
    //   lv_obj_set_style_bg_opa(lock_screen_obj_, LV_OPA_COVER, 0);
    //   lv_obj_set_style_bg_color(lock_screen_obj_, lv_color_black(), 0);
    //   CreateTimeLabel();
    //   CreateWeatherLabels();
    //   CreateDateLabels();

    //   lock_screen_gesture_obj_ = lv_obj_create(lock_screen_obj_);
    //   lv_obj_set_size(lock_screen_gesture_obj_, LV_HOR_RES, LV_VER_RES);
    //   lv_obj_set_pos(lock_screen_gesture_obj_, 0, 0);
    //   lv_obj_set_style_bg_opa(lock_screen_gesture_obj_, LV_OPA_TRANSP, 0);
    //   lv_obj_set_style_border_width(lock_screen_gesture_obj_, 0, 0);
    //   lv_obj_set_style_pad_all(lock_screen_gesture_obj_, 0, 0);
    //   lv_obj_set_style_radius(lock_screen_gesture_obj_, 0, 0);
    //   lv_obj_remove_flag(lock_screen_gesture_obj_, LV_OBJ_FLAG_SCROLLABLE);
    //   lv_obj_add_event_cb(lock_screen_gesture_obj_, GestureEventCallback,
    //                       LV_EVENT_PRESSED, this);
    //   lv_obj_add_event_cb(lock_screen_gesture_obj_, GestureEventCallback,
    //                       LV_EVENT_PRESSING, this);
    //   lv_obj_add_event_cb(lock_screen_gesture_obj_, GestureEventCallback,
    //                       LV_EVENT_RELEASED, this);
    //   lv_obj_add_event_cb(lock_screen_gesture_obj_, GestureEventCallback,
    //                       LV_EVENT_PRESS_LOST, this);
    // }

    // lv_obj_set_pos(lock_screen_obj_, 0, y);
  }

  void DeleteLockScreenLocked() {
    // if (lock_screen_obj_ != nullptr) {
    //   lv_obj_delete_async(lock_screen_obj_);
    //   lock_screen_obj_ = nullptr;
    // }
    // lock_screen_gesture_obj_ = nullptr;
    // time_label_ = nullptr;
    // weather_temp_label_ = nullptr;
    // weather_icon_label_ = nullptr;
    // date_label_ = nullptr;
    // lunar_label_ = nullptr;
  }

  void HandleGestureEvent(lv_event_t *e) {
    // lv_indev_t *indev = lv_event_get_indev(e);
    // if (indev == nullptr) {
    //   return;
    // }

    // lv_point_t point;
    // lv_indev_get_point(indev, &point);

    // lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    // lv_event_code_t code = lv_event_get_code(e);
    // int64_t now_us = esp_timer_get_time();
    // static int64_t last_touch_log_us = 0;

    // if ((code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
    //      code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) &&
    //     now_us - last_touch_log_us >= LOCK_SCREEN_TOUCH_LOG_INTERVAL_US) {
    //   last_touch_log_us = now_us;
    //   // ESP_LOGI(TAG, "lvgl touch code=%d x=%d y=%d target=%p visible=%d",
    //   //          code, point.x, point.y, target, is_visible_);
    // }

    // if (code == LV_EVENT_PRESSED) {
    //   if (target == edge_gesture_obj_ && CanOpenByGesture() &&
    //       point.y <= LOCK_SCREEN_GESTURE_EDGE_HEIGHT) {
    //     drag_mode_ = DragMode::kOpening;
    //     drag_start_y_ = point.y;
    //     drag_start_obj_y_ = -LV_VER_RES;
    //     EnsureLockScreenLocked(-LV_VER_RES);
    //     UpdateDisplayLocked();
    //     return;
    //   }

    //   if ((target == lock_screen_gesture_obj_ || target == lock_screen_obj_) &&
    //       is_visible_) {
    //     drag_mode_ = DragMode::kClosing;
    //     drag_start_y_ = point.y;
    //     drag_start_obj_y_ = lv_obj_get_y(lock_screen_obj_);
    //     return;
    //   }
    // }

    // if (code == LV_EVENT_PRESSING) {
    //   if (lock_screen_obj_ == nullptr) {
    //     return;
    //   }

    //   if (drag_mode_ == DragMode::kOpening) {
    //     lv_coord_t delta = point.y - drag_start_y_;
    //     if (delta < 0) {
    //       delta = 0;
    //     }
    //     lv_coord_t y = drag_start_obj_y_ + delta;
    //     if (y > 0) {
    //       y = 0;
    //     }
    //     lv_obj_set_y(lock_screen_obj_, y);
    //     return;
    //   }

    //   if (drag_mode_ == DragMode::kClosing) {
    //     lv_coord_t delta = point.y - drag_start_y_;
    //     lv_coord_t y = drag_start_obj_y_ + delta;
    //     if (y > 0) {
    //       y = 0;
    //     }
    //     if (y < -LV_VER_RES) {
    //       y = -LV_VER_RES;
    //     }
    //     lv_obj_set_y(lock_screen_obj_, y);
    //     return;
    //   }
    // }

    // if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    //   if (drag_mode_ == DragMode::kOpening) {
    //     if (lock_screen_obj_ != nullptr) {
    //       lv_coord_t revealed = LV_VER_RES + lv_obj_get_y(lock_screen_obj_);
    //       if (revealed >= LOCK_SCREEN_SHOW_THRESHOLD) {
    //         Application::GetInstance().Close();
    //         lv_obj_set_y(lock_screen_obj_, 0);
    //         is_visible_ = true;
    //         StopIdleTimer();
    //         UpdateDisplayLocked();
    //         if (update_timer_ == nullptr) {
    //           esp_timer_create_args_t update_timer_args = {
    //               .callback = &LockScreen::UpdateTimeCallback,
    //               .arg = this,
    //               .dispatch_method = ESP_TIMER_TASK,
    //               .name = "lock_screen_time_update",
    //               .skip_unhandled_events = true,
    //           };
    //           esp_timer_create(&update_timer_args, &update_timer_);
    //         }
    //         esp_timer_stop(update_timer_);
    //         esp_timer_start_periodic(update_timer_, 1000000);
    //         WeatherService::GetInstance()->OnLockScreenVisible();
    //         ESP_LOGI(TAG, "Lock screen shown by gesture");
    //       } else {
    //         DeleteLockScreenLocked();
    //       }
    //     }
    //   } else if (drag_mode_ == DragMode::kClosing) {
    //     if (lock_screen_obj_ != nullptr) {
    //       lv_coord_t moved_up = -lv_obj_get_y(lock_screen_obj_);
    //       if (moved_up >= LOCK_SCREEN_HIDE_THRESHOLD) {
    //         RequestHide();
    //         StartIdleTimer();
    //         ESP_LOGI(TAG, "Lock screen hidden by gesture");
    //       } else {
    //         lv_obj_set_y(lock_screen_obj_, 0);
    //       }
    //     }
    //   }

    //   drag_mode_ = DragMode::kNone;
    // }
  }

  const lv_image_dsc_t* GetWeatherIcon(const std::string& weather) {
    // if (weather.find("晴") != std::string::npos) {
      return &png_sunny;
    // } else if (weather.find("多云") != std::string::npos) {
    //   return &png_cloudy;
    // } else if (weather.find("阴") != std::string::npos) {
    //   return &png_overcast;
    // } else if (weather.find("小雨") != std::string::npos) {
    //   return &png_light_rain;
    // } else if (weather.find("中雨") != std::string::npos) {
    //   return &png_moderate_rain;
    // } else if (weather.find("大雨") != std::string::npos) {
    //   return &png_heavy_rain;
    // } else if (weather.find("雷阵雨") != std::string::npos) {
    //   return &png_thundershower;
    // } else if (weather.find("小雪") != std::string::npos) {
    //   return &png_light_snow;
    // } else if (weather.find("中雪") != std::string::npos) {
    //   return &png_moderate_snow;
    // } else if (weather.find("大雪") != std::string::npos) {
    //   return &png_heavy_snow;
    // } else if (weather.find("雨夹雪") != std::string::npos) {
    //   return &png_sleet;
    // } else if (weather.find("雾") != std::string::npos) {
    //   return &png_fog;
    // } else if (weather.find("霾") != std::string::npos) {
    //   return &png_haze;
    // } else if (weather.find("沙尘") != std::string::npos) {
    //   return &png_sandstorm;
    // }
    // return &png_sunny;
  }

  void CreateTimeLabel() {
    // time_label_ = lv_label_create(lock_screen_obj_);
    // lv_label_set_text(time_label_, "");
    // lv_obj_set_y(time_label_, MARGIN_TOP - LAYOUT_OFFSET_Y);
    // // lv_obj_set_align(time_label_, LV_ALIGN_TOP_LEFT);
    // lv_obj_set_style_pad_all(time_label_, 0, 0);
    // // lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_set_style_text_font(time_label_, &font_misans_heavy_time, 0);
    // lv_obj_set_style_text_color(time_label_, lv_color_hex(COLOR_WHITE), 0);
  }

  void CreateWeatherLabels() {
    // int y_weather = LV_VER_RES - MARGIN_BOTTOM - 13 - LINE_GAP - 15 - LINE_GAP -
    //                 20 - LAYOUT_OFFSET_Y - 20;

    // weather_temp_label_ = lv_label_create(lock_screen_obj_);
    // lv_label_set_text(weather_temp_label_, "");
    // lv_obj_set_y(weather_temp_label_, y_weather);
    // lv_obj_set_style_text_font(weather_temp_label_, &font_misans_medium_wdr,
    //                            0);
    // lv_obj_set_style_text_color(weather_temp_label_, lv_color_hex(COLOR_WHITE),
    //                              0);
    // lv_obj_add_flag(weather_temp_label_, LV_OBJ_FLAG_HIDDEN);

    // weather_icon_label_ = lv_image_create(lock_screen_obj_);
    // lv_image_set_src(weather_icon_label_, NULL);
    // lv_obj_set_align(weather_icon_label_, LV_ALIGN_BOTTOM_RIGHT);
    // lv_obj_set_style_pad_right(weather_icon_label_, 40, 0);
    // lv_obj_set_style_pad_bottom(weather_icon_label_, 40, 0);
    // lv_obj_add_flag(weather_icon_label_, LV_OBJ_FLAG_HIDDEN);
  }

  void CreateDateLabels() {
    // int y_solar =
    //     LV_VER_RES - MARGIN_BOTTOM - 13 - LINE_GAP - 15 - LAYOUT_OFFSET_Y - 20;
    // int y_lunar = LV_VER_RES - MARGIN_BOTTOM - 13 - LAYOUT_OFFSET_Y - 20;

    // date_label_ = lv_label_create(lock_screen_obj_);
    // lv_label_set_text(date_label_, "");
    // lv_obj_set_y(date_label_,  y_solar);
    // lv_obj_set_style_text_font(date_label_, &font_misans_medium_wdr, 0);
    // lv_obj_set_style_text_color(date_label_, lv_color_hex(COLOR_DATE), 0);

    // lunar_label_ = lv_label_create(lock_screen_obj_);
    // lv_label_set_text(lunar_label_, "");
    // lv_obj_set_y(lunar_label_,  y_lunar);
    // lv_obj_set_style_text_font(lunar_label_,
    //                            &font_misans_medium_wdr, 0);
    // lv_obj_set_style_text_color(lunar_label_, lv_color_hex(COLOR_LUNAR), 0);
  }

  void RequestUpdateDisplay() {
    // if (!is_visible_ || !HasLockScreenUi()) {
    //   return;
    // }
    // lv_async_call_cancel(&LockScreen::AsyncUpdateDisplay, this);
    // lv_async_call(&LockScreen::AsyncUpdateDisplay, this);
  }

  void UpdateDisplay() {
    // if (!is_visible_ || !HasLockScreenUi()) {
    //   return;
    // }

    // DisplayLockGuard lock(display_);
    // UpdateDisplayLocked();
  }

  void UpdateDisplayLocked() {
  //   if (!HasLockScreenUi()) {
  //     return;
  //   }

  //   auto weather_service = WeatherService::GetInstance();
  //   std::string time_str = weather_service->GetCurrentTime();
  //   std::string date_str = weather_service->GetCurrentDate();
  //   std::string weekday_str = weather_service->GetCurrentWeekday();
  //   std::string weather_str = weather_service->GetWeather();
  //   std::string temp_str = weather_service->GetTemperature();
  //   LunarDate lunar = weather_service->GetLunarDate();
  //   WeatherData weather_data = weather_service->GetWeatherData();

  //   if (time_str.empty()) {
  //     time_t now = 0;
  //     time(&now);
  //     struct tm timeinfo = {};
  //     localtime_r(&now, &timeinfo);
  //     char fallback_time[16];
  //     snprintf(fallback_time, sizeof(fallback_time), "%02d:%02d",
  //              timeinfo.tm_hour, timeinfo.tm_min);
  //     time_str = fallback_time;
  //   }

  //   lv_label_set_text(time_label_, time_str.c_str());

  //   bool has_weather = !weather_str.empty() && weather_str != "—" &&
  //                      !temp_str.empty() && temp_str != "—°C";

  //   if (has_weather) {
  //     char weather_temp[32];
  //     snprintf(weather_temp, sizeof(weather_temp), "%s | %s",
  //              weather_str.c_str(), temp_str.c_str());
  //     lv_label_set_text(weather_temp_label_, weather_temp);
  //     lv_obj_clear_flag(weather_temp_label_, LV_OBJ_FLAG_HIDDEN);

  //     lv_image_set_src(weather_icon_label_, GetWeatherIcon(weather_str));
  //     lv_obj_clear_flag(weather_icon_label_, LV_OBJ_FLAG_HIDDEN);
  //   } else {
  //     lv_label_set_text(weather_temp_label_, "");
  //     lv_obj_add_flag(weather_temp_label_, LV_OBJ_FLAG_HIDDEN);
  //     lv_image_set_src(weather_icon_label_, NULL);
  //     lv_obj_add_flag(weather_icon_label_, LV_OBJ_FLAG_HIDDEN);
  //   }

  //   char full_date[64];
  //   snprintf(full_date, sizeof(full_date), "%s %s", date_str.c_str(),
  //            weekday_str.c_str());
  //   lv_label_set_text(date_label_, full_date);

  //   std::string lunar_str = lunar.ToString();
  //   if (has_weather && !weather_data.city.empty()) {
  //     lunar_str += " · " + weather_data.city;
  //   }
  //   lv_label_set_text(lunar_label_, lunar_str.c_str());
  }
};
