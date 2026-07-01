#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"
#include "settings.h"

#include <esp_log.h>
#include <string>

// #include "../firmware/_common/display/zw_lcd_display.h"

#define TAG "ZwScreen"

namespace iot {

// 这里仅定义 ZwScreen 的属性和方法，不包含具体的实现
class ZwScreen : public Thing {
public:
    ZwScreen() : Thing("ZwScreen", "这是一个屏幕") {
        // 定义设备的属性
        properties_.AddStringProperty("theme", "主题", [this]() -> std::string {
            auto theme = Board::GetInstance().GetDisplay()->GetTheme();
            return theme;
        });

        properties_.AddNumberProperty("brightness", "当前亮度百分比", [this]() -> int {
            // 这里可以添加获取当前亮度的逻辑
            auto backlight = Board::GetInstance().GetBacklight();
            return backlight ? backlight->brightness() : 100;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTheme", "设置屏幕主题", ParameterList({
            Parameter("theme_name", "主题模式, light 或 dark", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            std::string theme_name = static_cast<std::string>(parameters["theme_name"].string());
            auto display = Board::GetInstance().GetDisplay();
            if (display) {
                display->SetTheme(theme_name);
            }
        });
        
        methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({
            Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            uint8_t brightness = static_cast<uint8_t>(parameters["brightness"].number());
            auto backlight = Board::GetInstance().GetBacklight();
            if (backlight) {
                backlight->SetBrightness(brightness, true);
            }
        });

        properties_.AddNumberProperty("angle", "当前旋转角度", [this]() -> int {
            ZwLcdDisplay* display = static_cast<ZwLcdDisplay*>(Board::GetInstance().GetDisplay());
            if (!display) {
                return 0;
            }
            return display->GetRotationAngle();
        });

        methods_.AddMethod("SetAngle", "设置旋转角度", ParameterList({
            Parameter("angle", "0, 90, 180, 270", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            int angle = parameters["angle"].number();
            ZwLcdDisplay* display = static_cast<ZwLcdDisplay*>(Board::GetInstance().GetDisplay());
            if (display) {
                display->SetRotationAngle(angle);
            }
        });
    }
};

} // namespace iot