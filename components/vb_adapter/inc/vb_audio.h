#ifndef __VB_AUDIO_H__
#define __VB_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化 VB 音频子系统（仅负责协议侧的收发缓存等）
 *
 * - 创建内部接收 ringbuffer
 * - 其他底层 UART/协议初始化由 vb_adapter 负责
 */
void vb_audio_init(void);

/**
 * @brief 设置从机播放音量（0~100）
 *
 * 内部会按比例映射到从机支持的 0~31 级音量。
 */
void vb_audio_set_volume(uint8_t volume);


/**
 * @brief 获取从机播放音量（0~100） 
 */
uint8_t vb_audio_get_volume(void);

/**
 * @brief 从 VB 音频输入队列读取一帧数据
 *
 * @param data  目标缓冲区
 * @param size  缓冲区大小（字节）
 * @return 实际读取的字节数，0 表示无数据或超时
 */
uint16_t vb_audio_read(uint8_t *data, uint16_t size);

/**
 * @brief 发送一帧音频数据到 VB 从机
 *
 * @param data  音频帧数据
 * @param len   数据长度（字节）
 */
void vb_audio_write(uint8_t *data, uint16_t len);

/**
 * @brief 控制音频输入/输出使能
 */
void vb_audio_enable_input(bool enable);
void vb_audio_enable_output(bool enable);

// /**
//  * @brief 将从机上行的音频数据写入内部接收队列
//  *
//  * 该接口通常仅由适配层在接收到 VB_CMD_RECV_AUDIO 时调用，
//  * 应用层不需要直接使用。
//  */
// void vb_audio_input(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // __VB_AUDIO_H__


