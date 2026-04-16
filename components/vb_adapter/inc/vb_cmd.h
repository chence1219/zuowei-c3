#ifndef __VB_CMD_H__
#define __VB_CMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


typedef enum {
    /* 基本音频 / 控制 / OTA 收发 */
    VB_CMD_RECV_AUDIO           = 0x2080,   /* 音频数据接收 */
    VB_CMD_RECV_CTL             = 0x0180,  /* 控制数据接收 */
    VB_CMD_RECV_WAKEUP_WORD     = 0x0280,  /* 唤醒词接收 */
    // VB_CMD_RECV_OTA             = 0x0105,  /* OTA数据接收 */

    VB_CMD_SEND_AUDIO           = 0x2081,  /* 音频数据发送 */
    VB_CMD_SEND_VOLUME          = 0x0203,  /* 音量数据发送 */
    VB_CMD_SEND_OTA             = 0x0205,  /* OTA数据发送 */
    VB_CMD_SEND_GET_WAKEUP_WORD = 0x0207,  /* 唤醒词发送 */
    VB_CMD_SEND_DEEP_SLEEP      = 0x0208,  /* 深度睡眠数据发送 */

    /* 系统 & 功能控制相关 */
    VB_CMD_SYS_KEEPALIVE        = 0x0501,  /* 心跳包 */
    VB_CMD_SYS_MODE             = 0x0502,  /* 模式 */
    VB_CMD_SYS_POWER_OFF        = 0x0509,  /* 关机 */

    VB_CMD_GET_FFT              = 0x0302,  /* FFT */
    VB_CMD_GET_MODE             = 0x0403,  /* 模式 */
    VB_CMD_SET_MODE             = 0x0303,  /* 模式 */
    VB_CMD_GET_PLAY_STATUS      = 0x0404,  /* 播放状态 */
    VB_CMD_SET_PLAY_STATUS      = 0x0304,  /* 播放状态 */
    VB_CMD_SET_NEXT_PREV        = 0x0305,  /* 下一首 */
    VB_CMD_GET_MUSIC_LIST       = 0x0406,  /* 音乐列表 */
    VB_CMD_SET_MUSIC_BY_INDEX   = 0x0306,  /* 音乐索引 */
    VB_CMD_SET_TONE_INDEX       = 0x0307,  /* 音调索引 */
    VB_CMD_GET_PLAY_INDEX       = 0x0407,  /* 播放索引 */
    VB_CMD_GET_VOLUME           = 0x0408,  /* 播放索引 */
    VB_CMD_SET_TIME             = 0x0309,  /* 设置时间 */
    VB_CMD_GET_TIME             = 0x0409,  /* 获取时间 */
    /* 系统事件上报（模式/状态/歌词等） */
    VB_CMD_SYS_MODE_CHANGE      = 0x0502,  /* 原 VB_CMD_SYS_MODE_CHANGE，与 VB_CMD_SYS_MODE 同值 */
    VB_CMD_SYS_PLAY_STATUS_CHANGE = 0x0503,/* 原 VB_CMD_SYS_PLAY_STATUS_CHANGE */
    VB_CMD_SYS_BT_STATUS_CHANGE = 0x0504,  /* 原 VB_CMD_SYS_BT_STATUS_CHANGE */
    VB_CMD_SYS_TITLE            = 0x0505,  /* 原 VB_CMD_SYS_TITLE */
    VB_CMD_SYS_LYRIC            = 0x0506,  /* 原 VB_CMD_SYS_LYRC */
    VB_CMD_SYS_TIME             = 0x0507,  /* 原 VB_CMD_SYS_TIME */
    VB_CMD_SYS_PLAY_INDEX       = 0x0508,  /* 原 VB_CMD_SYS_PLAY_INDEX */
    VB_CMD_SYS_PHONE_NUMBER     = 0x0510,  /* 来电号码 */
    VB_CMD_SYS_PHONE_CALL_ANSWER= 0x0511,  /* 接听来电 */
    VB_CMD_SYS_PHONE_CALL_HANGUP= 0x0512,  /* 挂断来电 */
    VB_CMD_SYS_BATTERY_LEVEL    = 0x0513,  /* 电池电量 */
    VB_CMD_SYS_CHARGE_STATUS    = 0x0514,  /* 充电状态改变*/

    VB_CMD_GET_ALARMS           = 0x0415,  /* 获取闹钟列表 */
    VB_CMD_SET_ALARM            = 0x0315,  /* 设置闹钟 */
    VB_CMD_DEL_ALARM            = 0x0316,  /* 删除闹钟 */
    VB_CMD_SET_ANS              = 0x0317,  /* 设置ANS */
} vb_cmd_t;

#ifdef __cplusplus
}
#endif

#endif // __VB_CMD_H__


