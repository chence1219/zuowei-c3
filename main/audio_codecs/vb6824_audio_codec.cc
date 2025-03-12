#include "vb6824_audio_codec.h"

#include <cstring>
#include <cmath>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "settings.h"

extern "C" {
#include "frame_parser.h"
}
#include "esp_log.h"

static const char *TAG = "vb6824";

#define UART_API_UART_NUM    UART_NUM_1
#define MAX_UART_USER_LEN    2048
#define UART_RX_BUFFER_SIZE  (MAX_UART_USER_LEN * 2)
#define UART_TX_BUFFER_SIZE  (MAX_UART_USER_LEN * 2)

#define UART_QUEUE_SIZE      32

#if defined(CONFIG_AUDIO_CODEC_VB6824_TYPE_OPUS_16K_20MS)
#define AUDIO_RECV_CHENK_LEN     40
#define AUDIO_SEND_CHENK_LEN     40
#define AUDIO_SEND_CHENK_MS      20
#elif defined(CONFIG_AUDIO_CODEC_VB6824_TYPE_OPUS_16K_20MS_PCM_16K)
#define AUDIO_RECV_CHENK_LEN     40
#define AUDIO_SEND_CHENK_LEN     320
#define AUDIO_SEND_CHENK_MS      10
#else
#define AUDIO_RECV_CHENK_LEN     512
// #define AUDIO_SEND_CHENK_LEN     480
// #define AUDIO_SEND_CHENK_MS      15
#define AUDIO_SEND_CHENK_LEN     320
#define AUDIO_SEND_CHENK_MS      10
#endif

#define SEND_BUF_LENGTH         AUDIO_SEND_CHENK_LEN*28
#define RECV_BUF_LENGTH         AUDIO_RECV_CHENK_LEN*15

#define VB_PLAY_SAMPLE_RATE     16 * 1000
#define VB_RECO_SAMPLE_RATE     16 * 1000
 
typedef struct{
    uint16_t head;
    uint16_t len;
    uint16_t cmd;
    uint8_t data[0];
}__attribute__ ((packed))vb6824_frame_t;


#pragma GCC diagnostic ignored "-Wswitch"
void VbAduioCodec::uart_event_task() {
    uart_event_t event;
    uint8_t temp_buf[1024];  // 中间缓冲

    for (;;) {
        // 等待队列事件
        if (xQueueReceive(uartQueue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:{
                    // 尝试多次读取，直到本次事件里可读字节消耗完为止
                    int len = 0;
                    do{
                        len = uart_read_bytes(UART_API_UART_NUM, temp_buf, sizeof(temp_buf), 0);
                        if (len > 0) {
                            // ESP_LOGI(TAG, "UART read: %d", len);
                            vb6824_parse(temp_buf, len);
                        }
                    }while (len);
                    }
                    break;

                case UART_FIFO_OVF:{
                    
                    ESP_LOGW(TAG, "HW FIFO overflow, flushing UART.");
                    uart_flush_input(UART_API_UART_NUM);
                    xQueueReset(uartQueue);
                }
                    break;

                case UART_BUFFER_FULL:{
                    ESP_LOGW(TAG, "Ring buffer full, flushing UART.");
                    uart_flush_input(UART_API_UART_NUM);
                    xQueueReset(uartQueue);
                    }
                    break;

                default:
                    // ESP_LOGI(TAG, "UART event: %d", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void VbAduioCodec::OnWakeUp(std::function<void(std::string)> callback) {
    on_wake_up_ = callback;
}

void VbAduioCodec::vb6824_parse(uint8_t *data, uint16_t len){
    frame_parser_add_buf(data, len);

    uint32_t get_len = 0;
    uint8_t get_buf[FRAME_MAX_LEN] = {0};
    while(frame_parser_get_frame(get_buf, &get_len)){
        // ESP_LOGI(TAG, "frame_parser_get_frame get_len: %ld", get_len);

        vb6824_frame_t *frame = (vb6824_frame_t *)get_buf;
        frame->len = FRAME_SWAP_16(frame->len);
        frame->cmd = FRAME_SWAP_16(frame->cmd);

        vb6824_recv_cb((vb6824_cmd_t)frame->cmd, frame->data, frame->len);
    }
}

void VbAduioCodec::vb6824_recv_cb(vb6824_cmd_t cmd, uint8_t *data, uint16_t len){

    if(cmd == VB6824_CMD_RECV_PCM){
        if (rbuffer_used_size(play_buf) < AUDIO_SEND_CHENK_LEN)
        {
            rbuffer_push(recv_buf, data, len, 1);
        }
    }else if (cmd == VB6824_CMD_RECV_CTL)
    {
        if(strncmp("你好小智", (char*)data, len) == 0 || strncmp("开始配网", (char*)data, len) == 0){
            std::string command(reinterpret_cast<char*>(data), len);
            if (on_wake_up_) {
                on_wake_up_(command);
            }
        }
        ESP_LOGI(TAG, "vb6824_recv cmd: %04x, len: %d :%.*s", cmd, len, len, data);
    }
}


void VbAduioCodec::uart_init(gpio_num_t tx, gpio_num_t rx){

    uart_config_t uart_config = {0};
    uart_config.baud_rate = 2000000;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE; // 如需RTS/CTS，改为 UART_HW_FLOWCTRL_CTS_RTS
    uart_config.rx_flow_ctrl_thresh = 0;             // (若开启RTS则需要设置触发阈值)   
    // uart_config.source_clk = UART_SCLK_APB; // 使用 APB 时钟源

    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    // 安装驱动，增大 rx_buffer_size
    uart_driver_install(UART_API_UART_NUM,
                        UART_RX_BUFFER_SIZE,
                        UART_TX_BUFFER_SIZE,
                        UART_QUEUE_SIZE, &uartQueue, intr_alloc_flags);

    uart_param_config(UART_API_UART_NUM, &uart_config);
    uart_set_pin(UART_API_UART_NUM, tx, rx,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 创建事件处理任务
    xTaskCreate(
        [](void* arg) {
        auto this_ = (VbAduioCodec*)arg;
        this_->uart_event_task();
#ifdef CONFIG_IDF_TARGET_ESP32C2
        }, "uart_event_task", 3072, this, 7, NULL);
#else
        }, "uart_event_task", 4096, this, 7, NULL);
#endif
}

void VbAduioCodec::vb6824_send(vb6824_cmd_t cmd, uint8_t *data, uint16_t len){
    uint16_t packet_len = 0;
    uint8_t packet[AUDIO_SEND_CHENK_LEN+7];

    int16_t idx = 0;
    uint16_t send_len = len;

    while (idx < len)
    {
        vb6824_frame_t *frame = (vb6824_frame_t *)packet;
        frame->head = FRAME_PARSER_HEAD;
        frame->len = FRAME_SWAP_16(send_len);
        frame->cmd = FRAME_SWAP_16(cmd);
        
        memcpy(frame->data, data + idx, (send_len>(len-idx))?(len-idx):send_len);
        idx += send_len;

        packet_len = 6 + send_len + 1;

        uint8_t checksum = 0;
        for (size_t i = 0; i < packet_len - 1; i++) {
            checksum += packet[i];
        }
        packet[packet_len - 1] = checksum;
        uart_write_bytes(UART_API_UART_NUM, packet, packet_len);
        // uart_flush(UART_API_UART_NUM);
    }
}

void VbAduioCodec::vb_thread() {
    uint8_t playing = 0;
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t)*AUDIO_SEND_CHENK_LEN);
    TickType_t xLastWakeTime = xTaskGetTickCount(); // 获取当前的 Tick 计数
    TickType_t xLastSendTime = xTaskGetTickCount(); // 获取当前的 Tick 计数
    TickType_t xLastReadyTime = xTaskGetTickCount(); // 获取当前的 Tick 计数
    while (1)
    {
        if(xLastWakeTime - xLastSendTime >= pdMS_TO_TICKS(AUDIO_SEND_CHENK_MS)){
            xLastSendTime = xLastWakeTime;
            if (rbuffer_used_size(play_buf) >= AUDIO_SEND_CHENK_LEN){
                rbuffer_pop(play_buf, (void*)data, AUDIO_SEND_CHENK_LEN);
                vb6824_send(VB6824_CMD_SEND_PCM, data, AUDIO_SEND_CHENK_LEN);
            }
        }
        if(xLastWakeTime - xLastReadyTime >= pdMS_TO_TICKS(5)){
            xLastReadyTime = xLastWakeTime;
            if (rbuffer_available_size(play_buf) > 10*AUDIO_RECV_CHENK_LEN){
                if(on_output_ready_) on_output_ready_();
            }
#ifdef CONFIG_AUDIO_CODEC_VB6824_TYPE_PCM_16K
            if (rbuffer_used_size(recv_buf) >= 2*960) {
#else
            if (rbuffer_used_size(recv_buf) >= 40) {
#endif
                if (on_input_ready_) {
                    on_input_ready_();
                }
            }
        }
        
        TickType_t delay1 = (xLastSendTime + AUDIO_SEND_CHENK_MS) - xLastWakeTime;
        TickType_t delay2 = (xLastReadyTime + 5) - xLastWakeTime;
        TickType_t delay = delay1<delay2?delay1:delay2;
        if(delay){
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(delay));
        }
    }
}

VbAduioCodec::VbAduioCodec(gpio_num_t tx, gpio_num_t rx) {
    uart_init(tx, rx);
    recv_buf = rbuffer_create(RECV_BUF_LENGTH);
    play_buf = rbuffer_create(SEND_BUF_LENGTH);

    input_sample_rate_ = VB_RECO_SAMPLE_RATE;
    output_sample_rate_ = VB_RECO_SAMPLE_RATE;

    frame_parser_init(FRAME_MAX_LEN*2);

    ESP_LOGI(TAG, "VbAduioCodec initialized");
}

void VbAduioCodec::Start() {
    ESP_LOGI(TAG, "Start");
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    EnableInput(true);
    EnableOutput(true);

    // uint8_t group = 0;
    // vb6824_send(VB6824_CMD_SEND_CTL, &group, 1);
    SetOutputVolume(80);

    xTaskCreate([](void *arg){
        VbAduioCodec * this_ = (VbAduioCodec *)arg;
        this_->vb_thread();
    }, "vb_thread", 2048, this, 8, NULL);
}

#ifndef CONFIG_AUDIO_CODEC_VB6824_TYPE_PCM_16K
bool VbAduioCodec::InputData(std::vector<int16_t>& data) {
    data.resize(20);
    int samples = Read(data.data(), data.size());
    if (samples > 0) {
        return true;
    }
    return false;
}

void VbAduioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}
#endif

void VbAduioCodec::SetOutputVolume(int volume){
    AudioCodec::SetOutputVolume(volume);
    uint8_t vol = (uint8_t)(volume * 31 / 100);
    vb6824_send(VB6824_CMD_SEND_VOLUM, &vol, 1);
}


void VbAduioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void VbAduioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}

void VbAduioCodec::OnInputReady(std::function<bool()> callback) {
    on_input_ready_ = callback;
}

void VbAduioCodec::OnOutputReady(std::function<bool()> callback) {
    on_output_ready_ = callback;
}

int VbAduioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        uint32_t len = rbuffer_used_size(recv_buf);
        if (samples < len/sizeof(int16_t))
        {
            rbuffer_pop(recv_buf, (void*)dest, samples*2);
            return samples;
        }else{
            rbuffer_pop(recv_buf, (void*)dest, len);
            return len/2;
        }
    }
    return samples;
}

int VbAduioCodec::Write(const int16_t* data, int samples) {
    
    if (output_enabled_) {
        while (rbuffer_available_size(play_buf) < 2*samples) {
            vTaskDelay(pdTICKS_TO_MS(2));
        }
        uint32_t push_len = rbuffer_push(play_buf, (uint8_t *)data, 2*samples, 1);
    }
    return samples;
}