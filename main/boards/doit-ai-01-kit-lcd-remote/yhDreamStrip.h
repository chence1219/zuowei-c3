#ifndef _YH_DREAM_STRIP_H_
#define _YH_DREAM_STRIP_H_

#include "iot/thing.h"
#include "gapRemote.h"

#include <chrono>

using namespace iot;
class YHDreamStrip: public Thing{
private:
    int64_t lastSetColorTimeMs;
    struct RemotFun
    {
        std::string funName;
        uint8_t code;
        std::function<void(uint8_t)> func; // 使用 std::function 存储函数
    };
    const uint8_t gapDateLen = 12;
    uint8_t gapData[12] = {
        0x0B,       // 数据长度
        0xFF,       // 厂商自定义类型
        0x07, 0x66, // 厂商 ID
        0x01,       // 协议
        0xFF,       // 标志
        0x95, 0x03, // 设备 ID
        0xFF,       // 组 ID
        0x00,       // 序列号
        0x01,       // 命令
        0x01        // 参数
    };
    std::vector<RemotFun> remoteFunList = {
        {"打开灯光", 0x08, [this](uint8_t code){sendGapData(code);}},
        {"关闭灯光", 0x02, [this](uint8_t code){sendGapData(code);}},
        {"调亮灯光", 0x14, [this](uint8_t code){sendGapData(code);}},
        {"调暗灯光", 0x0e, [this](uint8_t code){sendGapData(code);}},
        {"七彩模式", 0x03, [this](uint8_t code){sendGapData(code);}},
        {"音乐模式", 0x13, [this](uint8_t code){sendGapData(code);}},
        {"白色灯光", 0x0b, [this](uint8_t code){sendGapData(code);}},
        {"黄色灯光", 0x16, [this](uint8_t code){sendGapData(code);}},
        {"红色灯光", 0x15, [this](uint8_t code){sendGapData(code);}},
        {"绿色灯光", 0x0f, [this](uint8_t code){sendGapData(code);}},
        {"蓝色灯光", 0x09, [this](uint8_t code){sendGapData(code);}},
        {"橙色灯光", 0x08, [this](uint8_t code){sendGapData(code);}},
        {"青色灯光", 0x10, [this](uint8_t code){sendGapData(code);}},
        {"紫色灯光", 0x0a, [this](uint8_t code){sendGapData(code);}},
        {"切换场景", 0x04, [this](uint8_t code){sendGapData(code);}}
    };

    GapRemote& remote = GapRemote::GetInstance();
    void sendGapData(uint8_t code);
public:
    static YHDreamStrip& GetInstance() {
        static YHDreamStrip instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    YHDreamStrip(const YHDreamStrip&) = delete;
    YHDreamStrip& operator=(const YHDreamStrip&) = delete;
    explicit YHDreamStrip();
    void setColor(const std::string &color);
    void onoff(const std::string& onoff);
    void setMode(const std::string& mode);
    void setBright(const std::string& act);
    void setEmotion(const char* emoji);

};



#endif