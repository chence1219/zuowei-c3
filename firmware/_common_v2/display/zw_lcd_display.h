#include "display/lcd_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "settings.h"

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

public:
    ZwLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy, AngleMap angle_map)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy), angle_map_(angle_map){

        Settings settings("display", false);
        rotation_angle_ = settings.GetInt("rotation_angle", 0);

        SetRotationAngle(rotation_angle_);
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
};
