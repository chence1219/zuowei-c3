
#include "iot/thing.h"
#include "application.h"
#include <esp_log.h>

#define TAG "LowPower"

namespace iot {

// 这里仅定义 LowPowerIot 的属性和方法，不包含具体的实现
class LowPower : public Thing {
private:
    bool auto_poweroff_ = true;

public:
    LowPower() : Thing("LowPower", "低功耗管理，可以配置自动关机") {
        
        Settings settings("low_power", false);
        auto_poweroff_ = settings.GetInt("auto_poweroff", 1)?true:false;

        // 定义设备的属性
        properties_.AddBooleanProperty("auto_poweroff", "自动关机，开启的话会在长时间没有使用的时候自动关机", [this]() -> bool {
            return auto_poweroff_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetAutoPoweroff", "设置是否自动关机", ParameterList({
            Parameter("enable", "是否开启", kValueTypeBoolean, true),
        }), [this](const ParameterList& parameters) {
            bool enable = parameters["enable"].boolean();
            ESP_LOGI(TAG, "SetAutoPoweroff: %d", enable);
            auto_poweroff_ = enable;

            Settings settings("low_power", true);
            settings.SetInt("auto_poweroff", auto_poweroff_?1:0);
        });
    }

    bool IsAutoPoweroff(void){
        return auto_poweroff_;
    }
};

} // namespace iot

// DECLARE_THING(LowPower);