#include "vb_transport.h"

#include <string.h>
#include "FreeRTOSConfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "driver/uart.h"

static const char *TAG = "vb_transport";

#define UART_NUM    CONFIG_VB_UART_PORT
#define UART_QUEUE_SIZE         CONFIG_VB_UART_QUEUE_SIZE
#define UART_RX_BUFFER_SIZE     CONFIG_VB_UART_RX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE     CONFIG_VB_UART_TX_BUFFER_SIZE
#define UART_BAUD_RATE          CONFIG_VB_UART_BAUD_RATE
#define UART_TASK_STACK_SIZE     CONFIG_VB_UART_TASK_STACK_SIZE

static uart_port_t s_uart_port = UART_NUM_MAX;
static gpio_num_t s_tx_pin = GPIO_NUM_NC;
static gpio_num_t s_rx_pin = GPIO_NUM_NC;
static QueueHandle_t s_uart_queue = NULL;
static vb_transport_rx_cb_t s_rx_cb = NULL;
static void *s_rx_cb_arg = NULL;
static bool s_uart_inited = false;

/**
 * @brief UART接收任务
 */
static void vb_transport_task(void *arg)
{
    uart_event_t event;
    uint8_t temp_buf[512];  // 临时接收缓冲区

    while (true) {
        // 等待UART事件
        if (xQueueReceive(s_uart_queue, &event, pdMS_TO_TICKS(4))) {
            switch (event.type) {
                case UART_DATA: {
                    // 读取数据
                    int len = uart_read_bytes(UART_NUM, temp_buf, sizeof(temp_buf), 0);
                    if (len > 0 && s_rx_cb) {
                        s_rx_cb(temp_buf, len, s_rx_cb_arg);
                    }
                    break;
                }
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO overflow, flushing UART.");
                    uart_flush_input(UART_NUM);
                    xQueueReset(s_uart_queue);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring buffer full, flushing UART.");
                    uart_flush_input(UART_NUM);
                    xQueueReset(s_uart_queue);
                    break;
                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void vb_transport_init(gpio_num_t tx, gpio_num_t rx, vb_transport_rx_cb_t rx_cb, void *rx_cb_arg)
{
    s_rx_cb = rx_cb;
    s_rx_cb_arg = rx_cb_arg;
    s_tx_pin = tx;
    s_rx_pin = rx;

    // 上电先拉低 TX 唤醒从机
    vb_transport_wake_pulse();

    // 配置UART参数
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    // 中断分配标志
    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    // 安装UART驱动
    uart_driver_install(UART_NUM, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE,
                        UART_QUEUE_SIZE, &s_uart_queue, intr_alloc_flags);
    
    // 配置UART参数
    uart_param_config(UART_NUM, &uart_config);
    
    // 设置UART引脚
    uart_set_pin(UART_NUM, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 创建UART接收任务
    xTaskCreate(vb_transport_task, "vb_transport_task", UART_TASK_STACK_SIZE, NULL, 9, NULL);
    s_uart_inited = true;
}

void vb_transport_wake_pulse(void)
{
    if (s_tx_pin == GPIO_NUM_NC) {
        return;
    }

    // 暂时接管 TX，引脚拉低 10ms
    gpio_reset_pin(s_tx_pin);
    gpio_set_direction(s_tx_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(s_tx_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 恢复 UART 功能：若驱动已初始化则立即设置；否则稍后 init 会再绑定
    if (s_uart_inited) {
        uart_set_pin(UART_NUM, s_tx_pin, s_rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
}



int vb_transport_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return -1;
    }

    int ret = uart_write_bytes(UART_NUM, data, len);
    return ret;
}

