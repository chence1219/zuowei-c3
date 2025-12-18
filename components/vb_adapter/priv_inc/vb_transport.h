#ifndef __VB_TRANSPORT_H__
#define __VB_TRANSPORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driver/gpio.h"

typedef void (*vb_transport_rx_cb_t)(uint8_t *data, uint16_t len, void *arg);

void vb_transport_init(gpio_num_t tx, gpio_num_t rx, vb_transport_rx_cb_t rx_cb, void *rx_cb_arg);

int vb_transport_send(const uint8_t *data, uint16_t len);

void vb_transport_flush_rx(void);

/**
 * @brief 将 TX 拉低 10ms 用于唤醒对端，然后恢复为 UART 功能
 */
void vb_transport_wake_pulse(void);

#ifdef __cplusplus
}
#endif

#endif // __VB_TRANSPORT_H__


