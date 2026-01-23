#include "doit_blufi_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdbool.h>

#define NVS_NAMESPACE "blufi"
#define KEY_SSID "ssid"
#define KEY_PWD "pwd"
#define KEY_OTA_URL "ota_url"
#define KEY_HAS_CONFIG "has_cfg"

/* ---------- 私有辅助 ---------- */
static inline void nvs_set_blob_static(const char *key, const void *value, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_set_blob(h, key, value, len);
        nvs_commit(h);
        nvs_close(h);
    }
}

static inline size_t nvs_get_blob_static(const char *key, void *buf, size_t buf_len)
{
    nvs_handle_t h;
    size_t len = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK)
    {
        if (nvs_get_blob(h, key, buf, &buf_len) == ESP_OK)
            len = buf_len;
        nvs_close(h);
    }
    return len;
}

/* ---------- 对外接口 ---------- */
void blufi_storage_write_has_config(bool v)
{
    nvs_set_blob_static(KEY_HAS_CONFIG, &v, sizeof(v));
}

bool blufi_storage_read_has_config(void)
{
    bool v = false;
    nvs_get_blob_static(KEY_HAS_CONFIG, &v, sizeof(v));
    return v;
}

void blufi_storage_write_wifi_ssid(const char *ssid)
{
    if (ssid)
        nvs_set_blob_static(KEY_SSID, ssid, strlen(ssid) + 1);
}

void blufi_storage_read_wifi_ssid(char *ssid)
{
    if (ssid)
        nvs_get_blob_static(KEY_SSID, ssid, 32); // 32 == sizeof(wifi_config_t.sta.ssid)
}

size_t blufi_storage_read_wifi_ssid_length(void)
{
    return nvs_get_blob_static(KEY_SSID, NULL, 0);
}

void blufi_storage_write_wifi_password(const char *pwd)
{
    if (pwd)
        nvs_set_blob_static(KEY_PWD, pwd, strlen(pwd) + 1);
}

void blufi_storage_read_wifi_password(char *pwd)
{
    if (pwd)
        nvs_get_blob_static(KEY_PWD, pwd, 64); // 64 == sizeof(wifi_config_t.sta.password)
}

size_t blufi_storage_read_wifi_password_length(void)
{
    return nvs_get_blob_static(KEY_PWD, NULL, 0);
}

void blufi_storage_write_ota_url(const char *url)
{
    if (url)
        nvs_set_blob_static(KEY_OTA_URL, url, strlen(url) + 1);
}

void blufi_storage_read_ota_url(char *url)
{
    if (url)
        nvs_get_blob_static(KEY_OTA_URL, url, 256); // 根据业务实际大小调整
}

size_t blufi_storage_read_ota_url_length(void)
{
    return nvs_get_blob_static(KEY_OTA_URL, NULL, 0);
}