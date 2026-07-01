#pragma once

#include "application.h"
#include "config.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_style.h"
#include "core/lv_obj_style_gen.h"
#include "display/lcd_display.h"
#include "esp_lvgl_port.h"
#include <cstring>

#include "lvgl_theme.h"
#include "misc/lv_color.h"
#include "settings.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "font_awesome.h"
#include "font_emoji.h"
#include "widgets/bar/lv_bar.h"

#define TAG "XjLcdDisplay"

LV_FONT_DECLARE(qrcode_sibo);

struct AngleMap {
  int a_0;
  int a_90;
  int a_180;
  int a_270;
};

class XjLcdDisplay : public LcdDisplay {
protected:
  int rotation_angle_ = 0;
  AngleMap angle_map_ = {0, 90, 180, 270};
  lv_obj_t *progress_bar_ = nullptr;
  lv_obj_t *progress_label_left_ = nullptr;
  lv_obj_t *progress_label_right_ = nullptr;

public:
  XjLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
               int width, int height, int offset_x, int offset_y, bool mirror_x,
               bool mirror_y, bool swap_xy, AngleMap angle_map)
      : LcdDisplay(panel_io, panel, width, height), angle_map_(angle_map) {

    // draw white
    {
      std::vector<uint16_t> buffer(width_, 0x0000);
      for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
      }
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_affinity = -1;
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 60;
    port_cfg.task_max_sleep_ms = 2000;
    // port_cfg.task_stack = 3328;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(2 * width_),
        .double_buffer = false,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation =
            {
                .swap_xy = swap_xy,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .swap_bytes = 1,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);

    if (offset_x != 0 || offset_y != 0) {
      lv_display_set_offset(display_, offset_x, offset_y);
    }
    SetupUI();

    // current_theme_name_是当前以获取到的设置，恢复主题为当前设置的主题
    SetTheme(current_theme_);

    DisplayLockGuard lock(this);

    // 状态栏图标垂直居中
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_hor(status_bar_, 40, 0);

    lv_obj_t *cont = lv_obj_create(content_);
    // lv_obj_remove_style_all(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_size(cont, 250, 20);
    lv_obj_align_to(cont, chat_message_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *progress_bar = lv_bar_create(cont);
    lv_obj_set_size(progress_bar, 120, 10);
    lv_obj_center(progress_bar);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    progress_bar_ = progress_bar;

    lv_obj_t *progress_label_left = lv_label_create(cont);
    lv_label_set_text(progress_label_left, "");
    lv_obj_set_align(progress_label_left, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(progress_label_left, LV_OBJ_FLAG_HIDDEN);
    progress_label_left_ = progress_label_left;

    lv_obj_t *progress_label_right = lv_label_create(cont);
    lv_label_set_text(progress_label_right, "");
    lv_obj_set_align(progress_label_right, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(progress_label_right, LV_OBJ_FLAG_HIDDEN);
    progress_label_right_ = progress_label_right;
  };

  const char *FormatTime(uint32_t seconds) {
    static char time_buffer_[16];
    sprintf(time_buffer_, "%02d:%02d", (int)(seconds / 60),
            (int)(seconds - (seconds / 60) * 60));
    return time_buffer_;
  }

  void SetMusicProgress(int progress, int total) {
    if (progress_bar_ == nullptr) {
      return;
    }
    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
      return;
    }
    DisplayLockGuard lock(this);
    lv_obj_clear_flag(progress_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(progress_label_left_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(progress_label_right_, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_range(progress_bar_, 0, total);
    lv_bar_set_value(progress_bar_, progress, LV_ANIM_ON);
    lv_label_set_text(progress_label_left_, FormatTime(progress));
    lv_label_set_text(progress_label_right_, FormatTime(total));
    if (GetTheme() == nullptr) {
      return;
    }
    if (GetTheme()->name() == "light" || GetTheme()->name() == "Light") {
      lv_obj_set_style_text_color(progress_label_left_, lv_color_black(), 0);
      lv_obj_set_style_text_color(progress_label_right_, lv_color_black(), 0);
    } else {
      lv_obj_set_style_text_color(progress_label_left_, lv_color_white(), 0);
      lv_obj_set_style_text_color(progress_label_right_, lv_color_white(), 0);
    }
    auto color = LvglThemeManager::GetInstance()
                     .GetTheme(GetTheme()->name())
                     ->text_color();
    lv_obj_set_style_text_color(progress_label_left_, color, 0);
    lv_obj_set_style_text_color(progress_label_right_, color, 0);
  }

  void HideMusicProgress() {
    if (progress_bar_ == nullptr) {
      return;
    }
    DisplayLockGuard lock(this);
    lv_obj_add_flag(progress_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_label_left_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_label_right_, LV_OBJ_FLAG_HIDDEN);
  }

  virtual void SetChatMessage(const char *role, const char *content) override {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
      return;
    }

    if (!(strlen(content) >= strlen("**系统**") &&
          strncmp(content, "**系统**", strlen("**系统**")) == 0)) {
      lv_label_set_text(chat_message_label_, content);
    }
  }

  virtual void SetEmotion(const char *emotion) override {
    DisplayLockGuard lock(this);
    if (emoji_label_ == nullptr) {
      return;
    }
    if (Application::GetInstance().GetDeviceState() ==
        kDeviceStateWifiConfiguring) {
      DisplayLockGuard lock(this);
      lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
      lv_image_set_src(emoji_image_, &qrcode_sibo);
      return;
    }

    struct Emotion {
      const char *icon;
      const char *text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},   {"🙂", "happy"},   {"😆", "laughing"},
        {"😂", "funny"},     {"😔", "sad"},     {"😠", "angry"},
        {"😭", "crying"},    {"😍", "loving"},  {"😳", "embarrassed"},
        {"😯", "surprised"}, {"😱", "shocked"}, {"🤔", "thinking"},
        {"😉", "winking"},   {"😎", "cool"},    {"😌", "relaxed"},
        {"🤤", "delicious"}, {"😘", "kissy"},   {"😏", "confident"},
        {"😴", "sleepy"},    {"😜", "silly"},   {"🙄", "confused"}};

    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(
        emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion &e) { return e.text == emotion_view; });

    lv_obj_set_style_text_font(emoji_label_, font_emoji_64_init(), 0);
    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    if (it != emotions.end()) {
      lv_label_set_text(emoji_label_, it->icon);
    } else {
      lv_label_set_text(emoji_label_, "😶");
    }
  }
};
