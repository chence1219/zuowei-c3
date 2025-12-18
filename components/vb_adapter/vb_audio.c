#include <string.h>
#include "vb_protocol.h"
#include "vb_cmd.h"
#include "stdbool.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/ringbuf.h"

#define TAG "vb_audio"

#define RECV_BUF_LENGTH CONFIG_VB_AUDIO_INPUT_FRAME_LEN*30
#define SEND_BUF_LENGTH CONFIG_VB_AUDIO_OUTPUT_FRAME_LEN*10
#define AUDIO_SEND_CHENK_MS      20

// 音频输入输出允许
static bool s_output_enable = false;
static bool s_input_enable = false;

static RingbufHandle_t s_rx_ringbuffer = NULL;
static RingbufHandle_t s_tx_ringbuffer = NULL;


void vb_audio_set_volume(uint8_t volume){
    uint8_t vol = (uint8_t)((int)(volume * 31) / 100);
    vb_protocol_send(VB_CMD_SEND_VOLUME, &vol, 1);
}

uint16_t vb_audio_read(uint8_t *data, uint16_t size){
    size_t item_size = 0;
    uint32_t timeout = 20;
    uint32_t start_time = xTaskGetTickCount()/portTICK_PERIOD_MS;
    while (s_input_enable)
    {
        char *item = (char *)xRingbufferReceive(s_rx_ringbuffer, &item_size, pdMS_TO_TICKS(2));
        if (item)
        {
            if (size < item_size)
            {
                ESP_LOGE(TAG, "size is too small, item_size = %d, size = %d", item_size, size);
                item_size = 0;
            }else{
                memcpy(data, item, item_size);
            }
            vRingbufferReturnItem(s_rx_ringbuffer, (void *)item);
            break;
        }
        if(timeout != -1 && ((xTaskGetTickCount()/portTICK_PERIOD_MS) - start_time) > timeout){
            break;
        }else{
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    return item_size;
}

int vb_audio_input(uint8_t *data, uint16_t len, void *arg){
    if(s_input_enable){
        while (xRingbufferSend(s_rx_ringbuffer, (void *)data, len, 0) != pdTRUE) {
            size_t item_size = 0;
            // ESP_LOGE(TAG, "xRingbufferSend failed");
            uint8_t *item = (uint8_t *)xRingbufferReceive(s_rx_ringbuffer, &item_size, 0);
            if (item != NULL) {
                vRingbufferReturnItem(s_rx_ringbuffer, (void *)item);
            } else {
                break;
            }
        }
    }
    return 0;
}
VB_REGIST_CMD_EVT(VB_CMD_RECV_AUDIO, vb_audio_input);

void vb_audio_write(uint8_t *data, uint16_t len){
    if(s_output_enable){
        xRingbufferSend(s_tx_ringbuffer, (void *)data, len, portMAX_DELAY);
    }
}

void vb_audio_enable_input(bool enable){
    s_input_enable = enable;
}

void vb_audio_enable_output(bool enable){
    s_output_enable = enable;
}


#if CONFIG_VB_SEND_USE_TASK
void __send_task(void *arg) {
    TickType_t last_time = xTaskGetTickCount();
    while (1)
    {
        if(s_output_enable){
            size_t item_size = 0;
// #if defined(CONFIG_VB6824_TYPE_OPUS_16K_20MS)
//             uint8_t *item = (uint8_t *)xRingbufferReceive(s_tx_ringbuffer, &item_size, portMAX_DELAY);
// #else
            uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(s_tx_ringbuffer, &item_size, portMAX_DELAY, CONFIG_VB_AUDIO_OUTPUT_FRAME_LEN);
// #endif
            if (item != NULL) {
                if(s_output_enable == false){
                    vRingbufferReturnItem(s_tx_ringbuffer, (void *)item);
                    goto clear_rbuffer;
                }
                TickType_t now_time = xTaskGetTickCount();
                if((now_time - last_time) >= pdMS_TO_TICKS(AUDIO_SEND_CHENK_MS)){
                    last_time = xTaskGetTickCount();
                }
                vb_protocol_send(VB_CMD_SEND_AUDIO, (uint8_t *)item, item_size);
                vRingbufferReturnItem(s_tx_ringbuffer, (void *)item);
                vTaskDelayUntil(&last_time, pdMS_TO_TICKS(AUDIO_SEND_CHENK_MS));
clear_rbuffer:
                if(s_output_enable == false){
                    while(1){
                        size_t item_size = 0;
            // #if defined(CONFIG_VB6824_TYPE_OPUS_16K_20MS)
            //             uint8_t *item = (uint8_t *)xRingbufferReceive(s_tx_ringbuffer, &item_size, 0);
            // #else
                        uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(s_tx_ringbuffer, &item_size, 0, CONFIG_VB_AUDIO_OUTPUT_FRAME_LEN);
            // #endif
                        if (item != NULL) {
                            vRingbufferReturnItem(s_tx_ringbuffer, (void *)item);
                        }else{
                            break;
                        }
                    }
                    continue;
                }
            }
        }else{
            vTaskDelay(10);
        }
    }
}
#endif

void vb_audio_init(){
    s_rx_ringbuffer = xRingbufferCreate(RECV_BUF_LENGTH, RINGBUF_TYPE_NOSPLIT);
    s_tx_ringbuffer = xRingbufferCreate(SEND_BUF_LENGTH, RINGBUF_TYPE_BYTEBUF);
#if CONFIG_VB_SEND_USE_TASK
    xTaskCreate(__send_task, "__send_task", CONFIG_VB_SEND_TASK_STACK_SIZE, NULL, 9, NULL);
#else
    esp_timer_handle_t send_timer = NULL;
    esp_timer_create_args_t timer_args = {
        .callback = __send_timer_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "vb_send",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &send_timer);
    if(send_timer){
        esp_timer_start_periodic(send_timer, AUDIO_SEND_CHENK_MS*1000);
    }else{
        ESP_LOGE(TAG, "send_timer is null");
    }
#endif

}