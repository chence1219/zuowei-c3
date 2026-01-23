#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void blufi_storage_write_has_config(bool v);
    bool blufi_storage_read_has_config(void);

    void blufi_storage_write_wifi_ssid(const char *ssid);
    void blufi_storage_read_wifi_ssid(char *ssid);
    size_t blufi_storage_read_wifi_ssid_length(void);

    void blufi_storage_write_wifi_password(const char *pwd);
    void blufi_storage_read_wifi_password(char *pwd);
    size_t blufi_storage_read_wifi_password_length(void);

    void blufi_storage_write_ota_url(const char *url);
    void blufi_storage_read_ota_url(char *url);
    size_t blufi_storage_read_ota_url_length(void);

#ifdef __cplusplus
}
#endif