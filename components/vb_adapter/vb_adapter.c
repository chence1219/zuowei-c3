#include "vb_adapter.h"

#include "vb_transport.h"
#include "vb_protocol.h"
#include "vb_cmd.h"
#include "vb_audio.h"
#include "vb_api.h"
#include "esp_log.h"
#include "string.h"
#include "FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "vb_adapter";

static char *s_wake_word = NULL;
static uint8_t s_power_off_flag = 0;

char * vb_adapter_get_wake_word(){
    return s_wake_word;
}

// transport 的接收回调：把串口原始数据交给协议层
static void vb_adapter_rx_cb(uint8_t *data, uint16_t len, void *arg)
{
    (void)arg;
    vb_protocol_input(data, len);
}

// 协议层发送回调：把组好的帧交给 transport
static void vb_adapter_send_cb(const uint8_t *frame, uint16_t len, void *arg)
{
    (void)arg;
    vb_transport_send(frame, len);
}

// 最近一次收到对端 KEEPALIVE 的 tick
static TickType_t s_last_keepalive_tick = 0;



int __on_keepalive(uint8_t *data, uint16_t len, void *arg)
{
    (void)data;
    (void)len;
    (void)arg;
    s_last_keepalive_tick = xTaskGetTickCount();
    return 0;
}

// 每秒发送一次 KEEPALIVE，3 秒未收到则尝试唤醒从机
static void vb_adapter_keepalive_task(void *arg)
{
    (void)arg;
    const TickType_t tick_1s = pdMS_TO_TICKS(1000);
    const TickType_t timeout = pdMS_TO_TICKS(3000);

    s_last_keepalive_tick = xTaskGetTickCount();

    while (1)
    {
        // 尝试获取唤醒词，连续2次获取不到则拉低TX口唤醒一次
        static int wakeup_fail_count = 0;
        uint8_t *wakeup_word = NULL;
        uint16_t wakeup_word_len = 0;
        int ret = vb_protocol_send_block(VB_CMD_SEND_GET_WAKEUP_WORD, NULL, 0, &wakeup_word, &wakeup_word_len, 100);

        if (wakeup_word) {
            if (s_wake_word)
            {
                free(s_wake_word);
                s_wake_word = NULL;
            }
            
            // s_wake_word = strdup((char *)wakeup_word);
            s_wake_word = malloc(wakeup_word_len + 1);
            memcpy(s_wake_word, wakeup_word, wakeup_word_len);
            s_wake_word[wakeup_word_len] = '\0';
            free(wakeup_word);
            ESP_LOGI(TAG, "Get wake word: %s", s_wake_word);
            break;
        }

        if (ret != 0 || wakeup_word_len == 0) {
            wakeup_fail_count++;
            if (wakeup_fail_count >= 2) {
                // 连续5次获取不到，拉低TX唤醒
                vb_transport_wake_pulse();
                wakeup_fail_count = 0;
            }
        } else {
            wakeup_fail_count = 0;
        }
    }
    

    while (!s_power_off_flag) {
        vb_protocol_send(VB_CMD_SYS_KEEPALIVE, NULL, 0);

        TickType_t now = xTaskGetTickCount();
        if ((now - s_last_keepalive_tick) > timeout) {
            // 超过 3 秒未收到对端 KEEPALIVE，拉 TX 低唤醒从机
            vb_transport_wake_pulse();
            s_last_keepalive_tick = now;
        }

        vTaskDelay(tick_1s);
    }
    
    vTaskDelete(NULL);
}

void vb_adapter_power_off(){
    s_power_off_flag = 1;
    vb_api_power_off();
}

void vb_adapter_init(gpio_num_t tx, gpio_num_t rx, vb_adapter_frame_cb_t cb, void *cb_arg)
{
    (void)cb_arg; // 当前先不透传，后续如有需要可扩展上下文结构

    vb_audio_init();

    // 注册协议层回调（通过桥接函数再转给应用层）
    vb_protocol_init(vb_adapter_send_cb, (void *)cb);

    // 初始化 transport，并把数据入口指向协议解析
    vb_transport_init(tx, rx, vb_adapter_rx_cb, NULL);

    vb_api_init();

    // 启动保活任务
    xTaskCreate(vb_adapter_keepalive_task, "vb_keepalive", 2048, NULL, 5, NULL);
}
