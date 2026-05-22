#pragma once
#include "esp_blufi.h"
#include "esp_blufi_api.h"

#ifdef __cplusplus
extern "C"
{
#endif
#define BLUFI_EXAMPLE_TAG "DOIT_BLUFI"
#define BLUFI_INFO(fmt, ...) ESP_LOGI(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...) ESP_LOGE(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)

    void doit_blufi_init(void);
    void doit_blufi_deinit(void);
    void doit_blufi_start_adv(void);
    void doit_blufi_stop_adv(void);
    esp_err_t doit_blufi_send_code(uint8_t *code);
    int esp_blufi_gap_register_callback(void);
    esp_err_t esp_blufi_host_init(void);
    esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
    esp_err_t esp_blufi_host_deinit(void);
    esp_err_t esp_blufi_controller_init(void);
    esp_err_t esp_blufi_controller_deinit(void);
    bool doit_blufi_is_init(void);

#ifdef __cplusplus
}
#endif