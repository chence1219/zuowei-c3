#ifndef __VB_ADAPTER_H__
#define __VB_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driver/gpio.h"
#include "vb_cmd.h"
#include "vb_audio.h"
#include "../priv_inc/vb_event.h"
#include "../priv_inc/vb_api.h"

typedef struct
{
    uint32_t total;
    uint32_t index;
    uint32_t count;
    const char *names[];
}vb_music_list_t;


typedef enum{
    VB_EVT_WAKE_WORD,
    VB_EVT_VOL_CHANGE,
    VB_EVT_MUSIC_LIST,
    VB_EVT_MODE_CHANGE,
    VB_EVT_STATUS_CHANGE,
    VB_EVT_MUSIC_TITLE,
    VB_EVT_MUSIC_LYRC,
    VB_EVT_MUSIC_TIME,
    VB_EVT_PLAY_INDEX,
    VB_EVT_PHONE_CALL,
    VB_EVT_PHONE_CALL_HANGUP,
    VB_EVT_FFT, 
    VB_EVT_BATTERY_LEVEL,
    VB_EVT_CHARGE_STATUS,
}VB_EVT_CODE;

// 闹钟数据传输结构 (6字节) - 通用格式，对方主机不需要知道内部结构体
typedef struct {
    uint8_t index;      // 闹钟索引
    uint8_t sw;         // 开关状态 (0:关, 1:开)
    uint8_t mode;       // 重复模式 (见alarm.h中的E_ALARM_MODE_xxx)
    uint8_t hour;       // 时 (0-23)
    uint8_t min;        // 分 (0-59)
    uint8_t sec;        // 秒 (0-59)
} __attribute__((packed)) alarm_transfer_t;

/**
 * @brief 应用层收到一帧协议数据的回调
 *
 * 与协议层的 cmd/data/len 含义一致，但对上层屏蔽了内部实现。
 */
typedef void (*vb_adapter_frame_cb_t)(uint16_t cmd, uint8_t *data, uint16_t len, void *arg);

/**
 * @brief 适配层初始化：串口底层 + 协议解析
 *
 * @param tx       UART TX 引脚
 * @param rx       UART RX 引脚
 * @param cb       解析成功后上报的回调
 * @param cb_arg   透传给回调的用户参数
 */
void vb_adapter_init(gpio_num_t tx, gpio_num_t rx, vb_adapter_frame_cb_t cb, void *cb_arg);

void vb_adapter_power_off();

char * vb_adapter_get_wake_word();


#ifdef __cplusplus
}
#endif

#endif // __VB_ADAPTER_H__


