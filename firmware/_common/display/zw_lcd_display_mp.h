#include <esp_lvgl_port.h>
#include "core/lv_obj_pos.h"
#include "core/lv_obj_style_gen.h"
#include "display/lcd_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "misc/lv_text.h"
#include "settings.h"
#include "widgets/label/lv_label.h"

#define TAG "ZwLcdDisplayMp"

struct AngleMap {
  int a_0;
  int a_90;
  int a_180;
  int a_270;
};
LV_IMG_DECLARE(qrcode);
class ZwLcdDisplayMp : public LcdDisplay {
protected:
  int rotation_angle_ = 0;
  AngleMap angle_map_ = {0, 90, 180, 270};
  lv_obj_t *qrcode_image_ = nullptr;
  lv_obj_t *qrcode_container = nullptr;

public:
  ZwLcdDisplayMp(esp_lcd_panel_io_handle_t panel_io,
                 esp_lcd_panel_handle_t panel, int width, int height,
                 int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                 bool swap_xy, DisplayFonts fonts, AngleMap angle_map)
      : LcdDisplay(panel_io, panel, fonts, width, height),
        angle_map_(angle_map) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
      esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

#if CONFIG_SPIRAM
    // lv image cache, currently only PNG is supported
    size_t psram_size_mb = esp_psram_get_size() / 1024 / 1024;
    if (psram_size_mb >= 8) {
      lv_image_cache_resize(2 * 1024 * 1024, true);
      ESP_LOGI(TAG, "Use 2MB of PSRAM for image cache");
    } else if (psram_size_mb >= 2) {
      lv_image_cache_resize(512 * 1024, true);
      ESP_LOGI(TAG, "Use 512KB of PSRAM for image cache");
    }
#endif

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 20;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 15),
        .double_buffer = false,
        .trans_size = 0,
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
    if (display_ == nullptr) {
      ESP_LOGE(TAG, "Failed to add display");
      return;
    }

    if (offset_x != 0 || offset_y != 0) {
      lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();

    DisplayLockGuard lock(this);
    qrcode_container = lv_obj_create(NULL);
    lv_obj_set_size(qrcode_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(qrcode_container, 0, 0);
    lv_obj_set_style_border_width(qrcode_container, 0, 0);
    // lv_obj_set_scrollbar_mode(qrcode_container, LV_SCROLLBAR_MODE_OFF);

    qrcode_image_ = lv_image_create(qrcode_container);
    lv_image_set_src(qrcode_image_, &qrcode);
    lv_obj_align(qrcode_image_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(qrcode_image_, 200, 200);

    lv_obj_t *qrcode_label = lv_label_create(qrcode_container);
    lv_label_set_text(qrcode_label, "请扫码进入微信小程序中添加设备");
    lv_obj_align(qrcode_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(qrcode_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(qrcode_label, fonts_.text_font, 0);
    lv_obj_set_width(qrcode_label, width-8);
    lv_obj_set_style_text_align(qrcode_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(qrcode_label, LV_LABEL_LONG_MODE_SCROLL);

    // lv_scr_load(qrcode_container);
    Settings settings("display", false);
    rotation_angle_ = settings.GetInt("rotation_angle", 0);

    SetRotationAngle(rotation_angle_);
  };
  void show_qrcode() {
    DisplayLockGuard lock(this);
    if (qrcode_container != nullptr) {
      lv_scr_load(qrcode_container);
    }
  }

  int GetRotationAngle() { return rotation_angle_; };

  void SetRotationAngle(int rotation_angle) {
    if (rotation_angle != 0 && rotation_angle != 90 && rotation_angle != 180 &&
        rotation_angle != 270) {
      return;
    }
    int angle = -1;
    if (rotation_angle == 0) {
      angle = angle_map_.a_0;
    }
    if (rotation_angle == 90) {
      angle = angle_map_.a_90;
    }
    if (rotation_angle == 180) {
      angle = angle_map_.a_180;
    }
    if (rotation_angle == 270) {
      angle = angle_map_.a_270;
    }

    if (angle == 0) {
      lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_0);
      lv_display_set_offset(lv_display_get_default(), 0, 0);
    } else if (angle == 90) {
      lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_270);
      lv_display_set_offset(lv_display_get_default(), 0, 0);
    } else if (angle == 180) {
      lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_180);
      lv_display_set_offset(lv_display_get_default(), 0, 80);
    } else if (angle == 270) {
      lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_90);
      lv_display_set_offset(lv_display_get_default(), 80, 0);
    } else {
      return;
    }

    if (rotation_angle_ != rotation_angle) {
      rotation_angle_ = rotation_angle;
      Settings settings("display", true);
      settings.SetInt("rotation_angle", rotation_angle);
    }
  }
};
