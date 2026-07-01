#ifndef __XINZHI_WEATHER_API_H__
#define __XINZHI_WEATHER_API_H__

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XINZHI_API_KEY_DEFAULT    "sk_4pGFCQ-EgpHZcpQFjNMyh7xYp6YEGyRW"
#define XINZHI_API_HOST           "http://8.166.128.230:3100"
#define XINZHI_API_PATH           "/api/weather"
#define XINZHI_API_TIMEOUT_MS     10000
#define XINZHI_RESPONSE_BUF_SIZE  1024

typedef enum {
    XINZHI_WEATHER_SUNNY,
    XINZHI_WEATHER_CLEAR,
    XINZHI_WEATHER_FAIR,
    XINZHI_WEATHER_CLOUDY,
    XINZHI_WEATHER_PARTLY_CLOUDY,
    XINZHI_WEATHER_MOSTLY_CLOUDY,
    XINZHI_WEATHER_OVERCAST,
    XINZHI_WEATHER_SHOWER,
    XINZHI_WEATHER_THUNDERSHOWER,
    XINZHI_WEATHER_THUNDERSHOWER_HAIL,
    XINZHI_WEATHER_LIGHT_RAIN,
    XINZHI_WEATHER_MODERATE_RAIN,
    XINZHI_WEATHER_HEAVY_RAIN,
    XINZHI_WEATHER_STORM,
    XINZHI_WEATHER_HEAVY_STORM,
    XINZHI_WEATHER_SEVERE_STORM,
    XINZHI_WEATHER_ICE_RAIN,
    XINZHI_WEATHER_SLEET,
    XINZHI_WEATHER_SNOW_FLURRY,
    XINZHI_WEATHER_LIGHT_SNOW,
    XINZHI_WEATHER_MODERATE_SNOW,
    XINZHI_WEATHER_HEAVY_SNOW,
    XINZHI_WEATHER_SNOWSTORM,
    XINZHI_WEATHER_DUST,
    XINZHI_WEATHER_SAND,
    XINZHI_WEATHER_DUSTSTORM,
    XINZHI_WEATHER_SANDSTORM,
    XINZHI_WEATHER_FOGGY,
    XINZHI_WEATHER_HAZE,
    XINZHI_WEATHER_WINDY,
    XINZHI_WEATHER_BLUSTERY,
    XINZHI_WEATHER_HURRICANE,
    XINZHI_WEATHER_TROPICAL_STORM,
    XINZHI_WEATHER_TORNADO,
    XINZHI_WEATHER_COLD,
    XINZHI_WEATHER_HOT,
    XINZHI_WEATHER_UNKNOWN,
    XINZHI_WEATHER_COUNT
} xinzhi_weather_type_t;
typedef struct {
    char location_id[32];       /* 地点ID */
    char location_name[64];     /* 地点名称，如"深圳" */
    char country[16];           /* 国家代码 */
    char path[128];             /* 地点路径，如"深圳,深圳,广东,中国" */
    char timezone[64];          /* 时区 */
    char timezone_offset[16];   /* 时区偏移 */

    char weather_text[32];      /* 天气描述，如"晴" */
    int weather_code;           /* 天气code */
    int temperature;            /* 温度 */

    char last_update[64];       /* 最后更新时间 */
    bool valid;                 /* 数据是否有效 */
} xinzhi_weather_data_t;

xinzhi_weather_type_t xinzhi_code_to_weather_type(int code);

const char *xinzhi_weather_type_to_str(xinzhi_weather_type_t type);

/**
 * @brief HTTP GET 回调函数类型
 *
 * @param url        完整的请求 URL
 * @param headers    额外的 HTTP 头（可为 NULL）
 * @param header_count headers 数量
 * @param buf        用于存储响应体的缓冲区
 * @param buf_size   缓冲区大小
 * @param timeout_ms 超时时间（毫秒）
 * @return >=0 实际读取的字节数, <0 表示错误
 */
typedef int (*xinzhi_http_get_cb_t)(const char *url,
                                     const char **headers,
                                     size_t header_count,
                                     char *buf,
                                     size_t buf_size,
                                     int timeout_ms);

/**
 * @brief 初始化心知天气 API
 *
 * @param http_get_cb HTTP GET 回调函数，由调用方注入具体的 HTTP 实现
 * @return 0 成功, -1 参数无效
 */
int xinzhi_weather_init(xinzhi_http_get_cb_t http_get_cb);

/**
 * @brief 获取当前天气（吉米兔天气 API）
 *
 * 服务端通过设备 MAC 地址实时定位设备所在城市。
 *
 * @param mac  设备 MAC 地址，格式 "XX:XX:XX:XX:XX:XX"（半角冒号分隔，大小写均可），不可为 NULL
 * @param data 输出的天气数据
 * @return 0 成功, -1 未初始化或参数无效, -2 HTTP 请求失败, -3 JSON 解析失败
 */
int xinzhi_weather_fetch(const char *mac, xinzhi_weather_data_t *data);

/**
 * @brief 获取当前天气（可自定义 API key）
 *
 * @param api_key 吉米兔 API key，传 NULL 使用默认 key
 * @param mac     设备 MAC 地址，格式同 xinzhi_weather_fetch()，不可为 NULL
 * @param data    输出的天气数据
 * @return 0 成功, -1 未初始化或参数无效, -2 HTTP 请求失败, -3 JSON 解析失败
 */
int xinzhi_weather_fetch_ex(const char *api_key,
                             const char *mac,
                             xinzhi_weather_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __XINZHI_WEATHER_API_H__ */
