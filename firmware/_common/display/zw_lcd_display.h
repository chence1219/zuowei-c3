#include "display/lcd_display.h"

#include <cstring>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "settings.h"
#include "esp_lvgl_port.h"

struct AngleMap {
    int a_0;
    int a_90;
    int a_180;
    int a_270;
};

class ZwLcdDisplay : public SpiLcdDisplay {
protected:
    int rotation_angle_ = 0;
    AngleMap angle_map_ = {0, 90, 180, 270};
    lv_obj_t* splash_cont_ = nullptr;
    lv_obj_t* splash_img_ = nullptr;

public:
    ZwLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts, AngleMap angle_map)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, fonts), angle_map_(angle_map){

        Settings settings("display", false);
        rotation_angle_ = settings.GetInt("rotation_angle", 0);

        SetRotationAngle(rotation_angle_);

        // 状态栏图标垂直居中
        lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

#ifdef CONFIG_UI_USE_SPLASH_SCREEN
        if (lvgl_port_lock(2000)) {
            lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
          auto splash_cont = lv_obj_create(lv_layer_top());
          lv_obj_set_size(splash_cont, width, height);
          lv_obj_center(splash_cont);
          lv_obj_clear_flag(splash_cont, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_set_style_border_width(splash_cont, 0, 0);
          lv_obj_set_style_bg_color(splash_cont, lv_color_black(), 0);
          splash_cont_ = splash_cont;

          auto image = lv_image_create(splash_cont);
          LV_IMG_DECLARE(startup_white_180);
          lv_image_set_src(image, &startup_white_180);
          lv_obj_center(image);
        splash_img_ = image;

          auto timer = lv_timer_create(
              [](lv_timer_t *timer) {
                // auto cont = (lv_obj_t *)lv_timer_get_user_data(timer);
                auto this_ = static_cast<ZwLcdDisplay *>(lv_timer_get_user_data(timer));
                lv_obj_remove_flag(this_->container_, LV_OBJ_FLAG_HIDDEN);
                // lv_obj_delete(this_->splash_img_);
                // this_->splash_img_ = nullptr;
                lv_obj_add_flag(this_->splash_cont_, LV_OBJ_FLAG_HIDDEN);
              },
              2000, this);
          lv_timer_set_repeat_count(timer, 1);
          lvgl_port_unlock();
        }
#endif
    
    };

    int GetRotationAngle() { return rotation_angle_; };

    void SetRotationAngle(int rotation_angle) {
        if(rotation_angle != 0 && rotation_angle != 90 && rotation_angle != 180 && rotation_angle != 270){
            return;
        }
        int angle = -1;
        if(rotation_angle == 0){angle = angle_map_.a_0;}
        if(rotation_angle == 90){angle = angle_map_.a_90;}
        if(rotation_angle == 180){angle = angle_map_.a_180;}
        if(rotation_angle == 270){angle = angle_map_.a_270;}

        if(angle == 0){
            lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_0);
            lv_display_set_offset(lv_display_get_default(), 0, 0);
        }else if(angle == 90){
            lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_270);
            lv_display_set_offset(lv_display_get_default(), 0, 0);
        }else if(angle == 180){
            lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_180);
            lv_display_set_offset(lv_display_get_default(), 0, 80);
        }else if(angle == 270){
            lv_display_set_rotation(lv_display_get_default(), LV_DISP_ROTATION_90);
            lv_display_set_offset(lv_display_get_default(), 80, 0); 
        }else{
            return;
        }

        if(rotation_angle_ != rotation_angle){
            rotation_angle_ = rotation_angle;
            Settings settings("display", true);
            settings.SetInt("rotation_angle", rotation_angle);
        }
    }

    // virtual void SetChatMessage(const char *role, const char *content) {
    //   DisplayLockGuard lock(this);
    //   if (chat_message_label_ == nullptr) {
    //     return;
    //   }
    //   if (strlen(content) > 150) {
    //     // 创建一个新的字符串，长度为150+3个字符(...) + 1个结束符
    //     char truncated_content[154]; // 100 + 3个'.' + '\0'
    //     strncpy(truncated_content, content, 150);
    //     truncated_content[150] = '\0'; // 确保字符串正确结束
    //     strcat(truncated_content, "...");
    //     lv_label_set_text(chat_message_label_, truncated_content);
    //   } else {
    //     lv_label_set_text(chat_message_label_, content);
    //   }
    // }
};
