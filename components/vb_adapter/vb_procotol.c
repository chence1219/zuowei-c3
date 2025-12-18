#include "vb_protocol.h"

#include <string.h>
#include "FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "vb_cmd.h"

// 帧格式与原 vb6824 模块保持一致：
//  head: 0x55AA (大端)
//  len : 数据长度
//  cmd : 命令字
//  data[len]
//  sum : 从 head 开始到 data 末尾的 8 位累加和

#define FRAME_MIN_LEN   (7)         // 最小帧长度：head(2)+len(2)+cmd(2)+sum(1)
#define FRAME_MAX_LEN   (512 + 7)   // 最大帧长度（根据原代码）

typedef struct {
    uint16_t head;
    uint16_t len;
    uint16_t cmd;
} __attribute__((packed)) vb_frame_head_t;

typedef struct {
    uint8_t sum;
} __attribute__((packed)) vb_frame_tail_t;

typedef struct {
    uint16_t head;
    uint16_t len;
    uint16_t cmd;
    uint8_t  data[0];
} __attribute__((packed)) vb_frame_t;

#define SWAP_16(x)          (uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))

static inline int __sum_bytes(const uint8_t *bytes, uint16_t size)
{
    int sum = 0;
    for (uint16_t i = 0; i < size; i++) {
        sum += bytes[i];
    }
    return sum;
}

#define SUM8(bytes, size)   (__sum_bytes((const uint8_t *)(bytes), (uint16_t)(size)) % 256)

#define FRAME_HEAD_VALUE    (SWAP_16(0x55AA))
#define FRAME_DATA_LEN(h)   (SWAP_16((h)->len))
#define FRAME_CHECK(h, t)   (SUM8((h), (uint16_t)(sizeof(vb_frame_head_t) + SWAP_16((h)->len))) == (t)->sum)


// 底层发送回调（组帧后由下层实际发送）
static vb_protocol_send_cb_t s_send_cb      = NULL;
static void                 *s_arg          = NULL;

extern vb_cmd_evt_t __start_vb_cmd_evt[];
extern vb_cmd_evt_t __stop_vb_cmd_evt[];

// 阻塞发送上下文（仅允许单次阻塞调用）
typedef struct {
    volatile int  in_use;
    uint16_t      wait_cmd;
    uint8_t      *buf;
    uint16_t      len;
    SemaphoreHandle_t done_sem;
} vb_protocol_sync_ctx_t;

static vb_protocol_sync_ctx_t s_sync_ctx = {0};

// 协议层解析完成后转给应用层的回调桥接
static void vb_frame_cb(uint16_t cmd, uint8_t *data, uint16_t len)
{
    for (uint8_t* t = (uint8_t*)__start_vb_cmd_evt; t < (uint8_t*)__stop_vb_cmd_evt; t+=sizeof(vb_cmd_evt_t))
    {
        vb_cmd_evt_t *p = (vb_cmd_evt_t*)t;
        
        // ESP_LOGI(TAG, "VB_CMD:%04x, evt_id:%04x", cmd, p->cmd);
        if (p->cmd == cmd && p->evt_cb != NULL)
        {
            p->evt_cb(data, len, s_arg);
            break;
        }
    }
}

void vb_protocol_init(vb_protocol_send_cb_t cb, void *arg)
{
    s_send_cb     = cb;
    s_arg = arg;

    if (s_sync_ctx.done_sem == NULL) {
        s_sync_ctx.done_sem = xSemaphoreCreateBinary();
        configASSERT(s_sync_ctx.done_sem != NULL);
    }
    s_sync_ctx.in_use  = 0;
    s_sync_ctx.buf     = NULL;
    s_sync_ctx.len     = 0;
}


void vb_protocol_send(uint16_t cmd, const uint8_t *data, uint16_t len)
{
    if (!s_send_cb) {
        return; // 未注册底层发送实现
    }
    if (len > 512) {
        return; // 超过协议最大负载
    }

    uint16_t frame_len = (uint16_t)(sizeof(vb_frame_head_t) + len + sizeof(vb_frame_tail_t));
    if (frame_len > FRAME_MAX_LEN) {
        return;
    }

    uint8_t buf[FRAME_MAX_LEN] = {0};
    vb_frame_t *frame = (vb_frame_t *)buf;
    frame->head = FRAME_HEAD_VALUE;
    frame->len  = SWAP_16(len);
    frame->cmd  = SWAP_16(cmd);

    if (data && len > 0) {
        memcpy(frame->data, data, len);
    }

    vb_frame_tail_t *tail = (vb_frame_tail_t *)&buf[sizeof(vb_frame_head_t) + len];
    tail->sum = (uint8_t)SUM8(buf, (uint16_t)(sizeof(vb_frame_head_t) + len));

    s_send_cb(buf, frame_len, s_arg);
}

void vb_protocol_input(uint8_t *data, uint16_t len)
{
    static uint16_t s_tmp_len = 0;
    static uint8_t  s_tmp_buf[FRAME_MAX_LEN * 2] = {0};

    uint8_t  *parse_data = NULL;
    uint16_t  parse_len  = 0;

    if (s_tmp_len == 0 && len >= FRAME_MIN_LEN) {
        // 当前没有残留数据，且本次长度足够，直接在当前 buffer 上解析
        parse_data = data;
        parse_len  = len;
    } else {
        // 需要把新数据拼到临时 buffer 后面
        uint16_t copy_len = len;
        if (s_tmp_len + len > sizeof(s_tmp_buf)) {
            copy_len = sizeof(s_tmp_buf) - s_tmp_len;
        }
        memcpy(s_tmp_buf + s_tmp_len, data, copy_len);
        s_tmp_len  += copy_len;
        parse_data  = s_tmp_buf;
        parse_len   = s_tmp_len;
    }

re_parse:
    if (parse_len < FRAME_MIN_LEN) {
        // 数据太少，等下一包
        return;
    }

    for (uint16_t i = 0; i < parse_len; i++) {
        uint16_t left_len = parse_len - i;
        if (left_len < FRAME_MIN_LEN) {
            // 剩余数据不足一帧最小长度，缓存起来
            memmove(s_tmp_buf, &parse_data[i], left_len);
            s_tmp_len = left_len;
            return;
        }

        vb_frame_head_t *head = (vb_frame_head_t *)&parse_data[i];
        if (head->head != FRAME_HEAD_VALUE) {
            // 不是帧头，继续找
            continue;
        }

        uint16_t data_len = FRAME_DATA_LEN(head);
        uint16_t frame_len = (uint16_t)(sizeof(vb_frame_head_t) + data_len + sizeof(vb_frame_tail_t));
        if (data_len > 512) {
            // 非法长度，丢弃这个字节继续
            continue;
        }

        if (left_len < frame_len) {
            // 数据不完整，缓存从本帧开始的剩余数据，等待下一包补齐
            memmove(s_tmp_buf, &parse_data[i], left_len);
            s_tmp_len = left_len;
            return;
        }

        vb_frame_tail_t *tail = (vb_frame_tail_t *)&parse_data[i + sizeof(vb_frame_head_t) + data_len];
        if (!FRAME_CHECK(head, tail)) {
            // 校验失败，跳过这个字节继续向后找头
            continue;
        }

        // 到这里说明有一帧完整有效数据
        vb_frame_t *frame = (vb_frame_t *)&parse_data[i];
        uint16_t cmd = SWAP_16(frame->cmd);
        uint8_t *payload = frame->data;

        // 优先唤醒阻塞等待的调用者
        if (s_sync_ctx.in_use && cmd == s_sync_ctx.wait_cmd) {
            uint8_t *copy = NULL;
            if (data_len > 0) {
                copy = (uint8_t *)malloc(data_len);
                if (!copy) {
                    // malloc 失败，通知上层 len=0
                    s_sync_ctx.buf = NULL;
                    s_sync_ctx.len = 0;
                } else {
                    memcpy(copy, payload, data_len);
                    s_sync_ctx.buf = copy;
                    s_sync_ctx.len = data_len;
                }
            } else {
                s_sync_ctx.buf = NULL;
                s_sync_ctx.len = 0;
            }

            if (s_sync_ctx.done_sem) {
                xSemaphoreGive(s_sync_ctx.done_sem);
            }
        } else {
            // 正常异步分发
            vb_frame_cb(cmd, payload, data_len);
        }

        // 处理剩余数据：把本帧之后的内容搬到缓冲区开头，继续解析
        uint16_t remain = (uint16_t)(left_len - frame_len);
        if (remain > 0) {
            memmove(s_tmp_buf, &parse_data[i + frame_len], remain);
            s_tmp_len = remain;
            parse_data = s_tmp_buf;
            parse_len  = s_tmp_len;
            goto re_parse;
        } else {
            // 正好解析完
            s_tmp_len = 0;
            return;
        }
    }
}

int vb_protocol_send_block(uint16_t cmd,
                           const uint8_t *data,
                           uint16_t len,
                           uint8_t **out_buf,
                           uint16_t *out_len,
                           uint32_t timeout_ms)
{
    if (!out_buf || !out_len) {
        return -3;
    }
    if (!s_send_cb) {
        return -3;
    }

    // 不可重入：已经有一次阻塞调用在等待时直接返回
    if (s_sync_ctx.in_use) {
        return -2;
    }

    s_sync_ctx.in_use   = 1;
    s_sync_ctx.wait_cmd = cmd;
    s_sync_ctx.buf      = NULL;
    s_sync_ctx.len      = 0;

    if (cmd == VB_CMD_SEND_GET_WAKEUP_WORD)
    {
        s_sync_ctx.wait_cmd = VB_CMD_RECV_WAKEUP_WORD;
    }
    

    // 清理信号量残留
    if (s_sync_ctx.done_sem) {
        while (xSemaphoreTake(s_sync_ctx.done_sem, 0) == pdPASS) {
            // drain
        }
    }

    // 发送请求帧
    vb_protocol_send(cmd, data, len);

    // 等待响应
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    if (xSemaphoreTake(s_sync_ctx.done_sem, ticks) != pdPASS) {
        // 超时
        if (s_sync_ctx.buf) {
            free(s_sync_ctx.buf);
            s_sync_ctx.buf = NULL;
        }
        s_sync_ctx.len   = 0;
        s_sync_ctx.in_use = 0;
        return -1;
    }

    // 正常收到响应
    *out_buf = s_sync_ctx.buf;  // 交给调用方 free()
    *out_len = s_sync_ctx.len;

    int ret = s_sync_ctx.len;

    s_sync_ctx.buf    = NULL;
    s_sync_ctx.len    = 0;
    s_sync_ctx.in_use = 0;

    return ret;
}


