#include "xinzhi_weather_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <esp_log.h>

static const char *TAG = "xinzhi_weather";

static xinzhi_http_get_cb_t s_http_get = NULL;

int xinzhi_weather_init(xinzhi_http_get_cb_t http_get_cb) {
    if (http_get_cb == NULL) {
        ESP_LOGE(TAG, "http_get_cb is NULL");
        return -1;
    }
    s_http_get = http_get_cb;
    ESP_LOGI(TAG, "initialized");
    return 0;
}

static int do_fetch(const char *api_key, const char *mac, xinzhi_weather_data_t *data) {
    if (s_http_get == NULL) {
        ESP_LOGE(TAG, "not initialized, call xinzhi_weather_init() first");
        return -1;
    }
    if (data == NULL) {
        ESP_LOGE(TAG, "data is NULL");
        return -1;
    }
    if (mac == NULL || mac[0] == '\0') {
        ESP_LOGE(TAG, "mac is required");
        return -1;
    }

    memset(data, 0, sizeof(*data));

    const char *key = (api_key != NULL) ? api_key : XINZHI_API_KEY_DEFAULT;

    char url[512];
    snprintf(url, sizeof(url),
             "%s%s?key=%s&mac=%s",
             XINZHI_API_HOST, XINZHI_API_PATH, key, mac);

    ESP_LOGI(TAG, "GET %s", url);

    char buf[XINZHI_RESPONSE_BUF_SIZE];
    const char *headers[] = {"Accept: application/json"};
    int n = s_http_get(url, headers, 1, buf, sizeof(buf), XINZHI_API_TIMEOUT_MS);
    if (n < 0) {
        ESP_LOGE(TAG, "HTTP GET failed: %d", n);
        return -2;
    }
    buf[(n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1] = '\0';
    ESP_LOGI(TAG, "response (%d bytes): %s", n, buf);

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse failed");
        return -3;
    }

    int ret = -3;
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (results == NULL || !cJSON_IsArray(results) || cJSON_GetArraySize(results) < 1) {
        ESP_LOGE(TAG, "no 'results' array");
        goto cleanup;
    }

    cJSON *item = cJSON_GetArrayItem(results, 0);

    cJSON *loc = cJSON_GetObjectItem(item, "location");
    if (loc) {
        cJSON *v;
        if ((v = cJSON_GetObjectItem(loc, "id")) && cJSON_IsString(v))
            snprintf(data->location_id, sizeof(data->location_id), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(loc, "name")) && cJSON_IsString(v))
            snprintf(data->location_name, sizeof(data->location_name), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(loc, "country")) && cJSON_IsString(v))
            snprintf(data->country, sizeof(data->country), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(loc, "path")) && cJSON_IsString(v))
            snprintf(data->path, sizeof(data->path), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(loc, "timezone")) && cJSON_IsString(v))
            snprintf(data->timezone, sizeof(data->timezone), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(loc, "timezone_offset")) && cJSON_IsString(v))
            snprintf(data->timezone_offset, sizeof(data->timezone_offset), "%s", v->valuestring);
    }

    cJSON *now = cJSON_GetObjectItem(item, "now");
    if (now) {
        cJSON *v;
        if ((v = cJSON_GetObjectItem(now, "text")) && cJSON_IsString(v))
            snprintf(data->weather_text, sizeof(data->weather_text), "%s", v->valuestring);
        if ((v = cJSON_GetObjectItem(now, "code")) && cJSON_IsString(v))
            data->weather_code = atoi(v->valuestring);
        if ((v = cJSON_GetObjectItem(now, "temperature")) && cJSON_IsString(v))
            data->temperature = atoi(v->valuestring);
    }

    cJSON *upd = cJSON_GetObjectItem(item, "last_update");
    if (upd && cJSON_IsString(upd))
        snprintf(data->last_update, sizeof(data->last_update), "%s", upd->valuestring);

    data->valid = (data->weather_code >= 0);
    ret = data->valid ? 0 : -3;

cleanup:
    cJSON_Delete(root);
    return ret;
}

int xinzhi_weather_fetch(const char *mac, xinzhi_weather_data_t *data) {
    return do_fetch(NULL, mac, data);
}

int xinzhi_weather_fetch_ex(const char *api_key, const char *mac, xinzhi_weather_data_t *data) {
    return do_fetch(api_key, mac, data);
}

static const xinzhi_weather_type_t s_code_map[] = {
    XINZHI_WEATHER_SUNNY,               /*  0: 晴（国内城市白天晴） */
    XINZHI_WEATHER_CLEAR,               /*  1: 晴（国内城市夜晚晴） */
    XINZHI_WEATHER_FAIR,                /*  2: 晴（国外城市白天晴） */
    XINZHI_WEATHER_FAIR,                /*  3: 晴（国外城市夜晚晴） */
    XINZHI_WEATHER_CLOUDY,              /*  4: 多云 */
    XINZHI_WEATHER_PARTLY_CLOUDY,       /*  5: 晴间多云 */
    XINZHI_WEATHER_PARTLY_CLOUDY,       /*  6: 晴间多云 */
    XINZHI_WEATHER_MOSTLY_CLOUDY,       /*  7: 大部多云 */
    XINZHI_WEATHER_MOSTLY_CLOUDY,       /*  8: 大部多云 */
    XINZHI_WEATHER_OVERCAST,            /*  9: 阴 */
    XINZHI_WEATHER_SHOWER,              /* 10: 阵雨 */
    XINZHI_WEATHER_THUNDERSHOWER,       /* 11: 雷阵雨 */
    XINZHI_WEATHER_THUNDERSHOWER_HAIL,  /* 12: 雷阵雨伴有冰雹 */
    XINZHI_WEATHER_LIGHT_RAIN,          /* 13: 小雨 */
    XINZHI_WEATHER_MODERATE_RAIN,       /* 14: 中雨 */
    XINZHI_WEATHER_HEAVY_RAIN,         /* 15: 大雨 */
    XINZHI_WEATHER_STORM,              /* 16: 暴雨 */
    XINZHI_WEATHER_HEAVY_STORM,        /* 17: 大暴雨 */
    XINZHI_WEATHER_SEVERE_STORM,       /* 18: 特大暴雨 */
    XINZHI_WEATHER_ICE_RAIN,           /* 19: 冻雨 */
    XINZHI_WEATHER_SLEET,              /* 20: 雨夹雪 */
    XINZHI_WEATHER_SNOW_FLURRY,        /* 21: 阵雪 */
    XINZHI_WEATHER_LIGHT_SNOW,         /* 22: 小雪 */
    XINZHI_WEATHER_MODERATE_SNOW,      /* 23: 中雪 */
    XINZHI_WEATHER_HEAVY_SNOW,         /* 24: 大雪 */
    XINZHI_WEATHER_SNOWSTORM,          /* 25: 暴雪 */
    XINZHI_WEATHER_DUST,               /* 26: 浮尘 */
    XINZHI_WEATHER_SAND,               /* 27: 扬沙 */
    XINZHI_WEATHER_DUSTSTORM,          /* 28: 沙尘暴 */
    XINZHI_WEATHER_SANDSTORM,          /* 29: 强沙尘暴 */
    XINZHI_WEATHER_FOGGY,              /* 30: 雾 */
    XINZHI_WEATHER_HAZE,               /* 31: 霾 */
    XINZHI_WEATHER_WINDY,              /* 32: 风 */
    XINZHI_WEATHER_BLUSTERY,           /* 33: 大风 */
    XINZHI_WEATHER_HURRICANE,          /* 34: 飓风 */
    XINZHI_WEATHER_TROPICAL_STORM,     /* 35: 热带风暴 */
    XINZHI_WEATHER_TORNADO,            /* 36: 龙卷风 */
    XINZHI_WEATHER_COLD,               /* 37: 冷 */
    XINZHI_WEATHER_HOT,                /* 38: 热 */
};

#define CODE_MAP_SIZE (sizeof(s_code_map) / sizeof(s_code_map[0]))

xinzhi_weather_type_t xinzhi_code_to_weather_type(int code) {
    if (code >= 0 && code < (int)CODE_MAP_SIZE) {
        return s_code_map[code];
    }
    return XINZHI_WEATHER_UNKNOWN;
}

static const char *s_type_str[XINZHI_WEATHER_COUNT] = {
    "晴", "晴", "晴", "多云", "晴间多云", "大部多云",
    "阴", "阵雨", "雷阵雨", "雷阵雨冰雹",
    "小雨", "中雨", "大雨", "暴雨", "大暴雨", "特大暴雨",
    "冻雨", "雨夹雪", "阵雪", "小雪", "中雪", "大雪", "暴雪",
    "浮尘", "扬沙", "沙尘暴", "强沙尘暴",
    "雾", "霾", "风", "大风", "飓风", "热带风暴", "龙卷风",
    "冷", "热", "未知",
};

const char *xinzhi_weather_type_to_str(xinzhi_weather_type_t type) {
    if (type >= 0 && type < XINZHI_WEATHER_COUNT) {
        return s_type_str[type];
    }
    return s_type_str[XINZHI_WEATHER_UNKNOWN];
}
