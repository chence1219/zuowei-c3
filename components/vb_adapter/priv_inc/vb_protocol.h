#ifndef __VB_PROTOCOL_H__
#define __VB_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint16_t cmd;
    int (*evt_cb)(uint8_t *data, uint16_t len, void *arg);
}vb_cmd_evt_t;

#define VB_REGIST_CMD_EVT(command, cb) \
vb_cmd_evt_t command##_evt __attribute__((section("vb_cmd_evt"), used)) = { \
        .cmd = command, \
        .evt_cb = cb, \
    }




/**
 * @brief 底层发送回调（协议层组帧后调用）
 */
typedef void (*vb_protocol_send_cb_t)(const uint8_t *frame, uint16_t len, void *arg);

void vb_protocol_init(vb_protocol_send_cb_t cb, void *arg);

void vb_protocol_input(uint8_t *data, uint16_t len);

void vb_protocol_send(uint16_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief 发送一帧并阻塞等待对应命令字的响应
 *
 * @param cmd        要发送的命令字（主机字节序）
 * @param data       待发送的数据指针（可为 NULL）
 * @param len        待发送数据长度
 * @param out_buf    输出参数，指向接收到的响应数据缓冲区（由本函数 malloc，调用方负责 free）
 * @param out_len    输出参数，接收到的数据长度
 * @param timeout_ms 超时时间（毫秒），0 表示不等待
 *
 * @return >0: 实际接收到的字节数
 *         0 : 无有效数据（对端返回空负载或内部出错）
 *        -1 : 超时未收到响应
 *        -2 : 已有其他阻塞请求在等待中（不可重入）
 *        -3 : 参数错误或内部错误
 */
int vb_protocol_send_block(uint16_t cmd,
                           const uint8_t *data,
                           uint16_t len,
                           uint8_t **out_buf,
                           uint16_t *out_len,
                           uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // __VB_PROTOCOL_H__


