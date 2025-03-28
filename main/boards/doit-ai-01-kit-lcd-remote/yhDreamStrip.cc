#include "yhDreamStrip.h"

#include "esp_log.h"

#define TAG "LedStripControl"

void YHDreamStrip::onoff(const std::string& onoff){
    for (auto& fun : remoteFunList) {
        if (fun.funName == onoff) {
            fun.func(fun.code);
            auto now = std::chrono::steady_clock::now();
            lastSetColorTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            return;
        }
    }
}

void YHDreamStrip::setColor(const std::string& color){
    for (auto& fun : remoteFunList) {
        if (fun.funName == color) {
            fun.func(fun.code);
            auto now = std::chrono::steady_clock::now();
            lastSetColorTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            return;
        }
    }
}

void YHDreamStrip::setBright(const std::string& act){
    for (auto& fun : remoteFunList) {
        if (fun.funName == act) {
            fun.func(fun.code);
            return;
        }
    }
}

void YHDreamStrip::setMode(const std::string& mode){
    for (auto& fun : remoteFunList) {
        if (fun.funName == mode) {
            fun.func(fun.code);
            auto now = std::chrono::steady_clock::now();
            lastSetColorTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            return;
        }
    }
}

void YHDreamStrip::setEmotion(const char* emoji){
    auto now = std::chrono::steady_clock::now();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (nowMs - lastSetColorTimeMs <= 1000)
    {
        return;
    }
    
    struct Emotion {
        std::string color;    // 表情名称
        std::string emoji; // 描述
    };
    std::vector<Emotion> emotions = {
        {"红色灯光", "angry"},               // 生气
        {"黄色灯光", "happy"},               // 快乐
        {"黄色灯光", "laughing"},            // 大笑
        {"橙色灯光", "funny"},               // 有趣
        {"蓝色灯光", "sad"},                 // 悲伤
        {"蓝色灯光", "crying"},              // 哭泣
        {"红色灯光", "loving"},              // 爱
        {"橙色灯光", "embarrassed"},         // 尴尬
        {"青色灯光", "surprised"},           // 惊讶
        {"紫色灯光", "shocked"},             // 震惊
        {"绿色灯光", "thinking"},            // 思考
        {"绿色灯光", "winking"},             // 眨眼
        {"蓝色灯光", "cool"},                // 酷
        {"绿色灯光", "relaxed"},             // 放松
        {"黄色灯光", "delicious"},           // 美味
        {"红色灯光", "kissy"},               // 吻
        {"紫色灯光", "confident"},           // 自信
        {"蓝色灯光", "sleepy"},              // 睡意
        {"橙色灯光", "silly"},               // 傻乎乎
        {"紫色灯光", "confused"}             // 困惑
    };
    // 遍历并输出每个表情
    for (const auto& emotion : emotions) {
        if (emotion.emoji == emoji)
        {
            for (auto& fun : remoteFunList) {
                if (fun.funName == emotion.color) {
                    ESP_LOGW(TAG, "set emoji:%s", emotion.color.c_str());
                    fun.func(fun.code);
                    return;
                }
            }
        }
    }
}

void YHDreamStrip::sendGapData(uint8_t code){
    gapData[9]++;
    gapData[11] = code;
    remote.start_adv(gapData, gapDateLen, 1000);
}

YHDreamStrip::YHDreamStrip()
    :Thing("LedStripControl", "LED 灯带控制,支持红色、绿色、蓝色、橙色、青色、紫色、白色、黄色"){
    auto now = std::chrono::steady_clock::now();
    lastSetColorTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    // 定义设备可以被远程执行的指令
    methods_.AddMethod("onOrOff", "打开或关闭灯光", ParameterList({
        Parameter("onoff", "动作+'灯光'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
                       {
                        onoff(parameters["onoff"].string()); 
                        ESP_LOGI(TAG, "LedStripControl: %s", parameters["onoff"].string().data());});
    
    methods_.AddMethod("SetColor", "设置灯光颜色、支持红色、绿色、蓝色、橙色、青色、紫色、白色、黄色", ParameterList({
        Parameter("color", "字符串'颜色'+'灯光'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
    {
        setColor(parameters["color"].string()); 
        ESP_LOGW(TAG, "LedStripControl:%s", parameters["color"].string().data()); });
    
    methods_.AddMethod("SetBrigbt", "调节灯光亮度,只支持调亮或调暗", ParameterList({
        Parameter("action", "字符串'调亮'/'调暗'+'灯光'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
    {
        setColor(parameters["action"].string()); 
        ESP_LOGW(TAG, "LedStripControl:%s", parameters["action"].string().data()); });
    
    methods_.AddMethod("SetMode", "调节灯光模式,只支持七彩或音乐", ParameterList({
        Parameter("action", "字符串'七彩'/'音乐'+'模式'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
    {
        setMode(parameters["action"].string()); 
        ESP_LOGW(TAG, "REMOTE SetBrigbt:%s", parameters["action"].string().data()); });

    methods_.AddMethod("SetSence", "当用户说切换场景时触发,比如切换场景，换个场景这种", ParameterList({
        Parameter("action", "字符串'切换场景'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
    {
        setMode(parameters["action"].string()); 
        ESP_LOGW(TAG, "REMOTE SetSence:%s", parameters["action"].string().data()); });
   
   
    methods_.AddMethod("SetEmotions", "当用户描述自己的情绪时你根据用户情绪选一个合适的氛围灯颜色，支持红色、绿色、蓝色、橙色、青色、紫色、白色、黄色", ParameterList({
        Parameter("color", "字符串'颜色'+'灯光'", kValueTypeString, true)
    }), [this](const ParameterList &parameters)
    {
        setColor(parameters["color"].string()); 
        ESP_LOGW(TAG, "REMOTE SetBrigbt:%s", parameters["color"].string().data()); });
}