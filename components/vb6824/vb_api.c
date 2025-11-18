#include "vb6824.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "vb_api";

#define UART_NUM                    CONFIG_VB6824_UART_PORT
gpio_num_t s_gpio_tx = GPIO_NUM_NC;
gpio_num_t s_gpio_rx = GPIO_NUM_NC;

void *s_api_evt_cb_arg = NULL;
vb_api_event_cb_t s_api_event_cb = NULL;

typedef enum
{
    VB6824_CMD_SYS_KEEPALIVE = 0x0501,
    VB6824_CMD_SYS_MODE     = 0x0502,

    VB6824_CMD_GET_FFT = 0x0302,

    VB_CMD_GET_MODE = 0x0403,
    VB_CMD_SET_MODE = 0x0303,
    
    VB_CMD_GET_PLAY_STATUS = 0x0404,
    VB_CMD_SET_PLAY_STATUS = 0x0304,
    VB_CMD_SET_NEXT_PREV = 0x0305,
    VB_CMD_GET_MUSIC_LIST = 0x0406,
    VB_CMD_SET_MUSIC_BY_INDEX = 0x0306,

    VB_CMD_SYS_MODE_CHANGE = 0x0502,
    VB_CMD_SYS_PLAY_STATUS_CHANGE = 0x0503,
    VB_CMD_SYS_BT_STATUS_CHANGE = 0X0504,
    VB_CMD_SYS_TITLE = 0x0505,
    VB_CMD_SYS_LYRC = 0x0506,
    VB_CMD_SYS_TIME = 0x0507,
}vb6824_cmd_t;

typedef struct 
{
    vb6824_cmd_t cmd;
    void (*handle)(uint8_t *data, uint16_t len);
}_event_handle_t;

vb_music_list_cb_t g_music_list_cb = NULL;
static void *g_voice_fft_cb_arg = NULL;
static vb_voice_fft_cb_t g_voice_fft_cb = NULL;
static uint32_t s_last_keepalive_tick = 0;

extern void __frame_send(vb6824_cmd_t cmd, uint8_t *data, uint16_t len);
extern int frame_send_block(vb6824_cmd_t cmd,
                     const uint8_t *data,
                     uint16_t len,
                     uint8_t **out_buf,
                     uint16_t *out_len,
                     uint32_t timeout_ms);

void vb_music_play(uint8_t play){
    uint8_t action = play ? 1 : 0;
    __frame_send(VB_CMD_SET_PLAY_STATUS, &action, 1);
}

void vb_music_next_priv(uint8_t next){
    uint8_t action = next ? 1 : 0;
    __frame_send(VB_CMD_SET_NEXT_PREV, &action, 1);
}

void vb_music_set_mode(vb_music_mode_t mode){
    uint8_t m = (uint8_t)mode;
    __frame_send(VB_CMD_SET_MODE, &m, 1);
}

void vb_music_play_by_index(uint32_t index){
    uint32_t i = index;
    __frame_send(VB_CMD_SET_MUSIC_BY_INDEX, (uint8_t*)&i, sizeof(i));
}

uint8_t vb_music_get_play_status(){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    uint8_t status = 0;
    frame_send_block(VB_CMD_GET_PLAY_STATUS, NULL, 0, &recv_buf, &recv_len, 100);
    if (recv_len != 0)
    {
        status = recv_buf[0];
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return status;
}

vb_music_mode_t vb_music_get_mode(){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    vb_music_mode_t mode = VB_MUSIC_MODE_BT;
    frame_send_block(VB_CMD_GET_MODE, NULL, 0, &recv_buf, &recv_len, 100);
    if (recv_len != 0)
    {
        mode = recv_buf[0];
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return mode;
}

void vb6824_register_voice_fft_cb(vb_voice_fft_cb_t cb, void *arg){
    g_voice_fft_cb = cb;
    g_voice_fft_cb_arg = arg;
}


static void _wakeup_701()
{
    // 1. 先把 TX 引脚从 UART 功能“抢过来”，配置成普通 GPIO 输出
    //    gpio_pad_select_gpio 在 IDF 新版本里可以不调用，直接用 gpio_set_direction 也行
    gpio_reset_pin(s_gpio_tx);                    // 解除旧功能，包括 UART
    gpio_set_direction(s_gpio_tx, GPIO_MODE_OUTPUT);
    gpio_set_level(s_gpio_tx, 0);                 // 拉低

    // 2. 延时 low_ms 毫秒
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. 恢复为 UART TX 功能
    //    重新绑定 TX/RX 到 UART
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, s_gpio_tx, s_gpio_rx,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static void _get_fft_handle(uint8_t *data, uint16_t len){
    int16_t *fft_data = (int16_t *)data;
    if(g_voice_fft_cb){
        g_voice_fft_cb(fft_data, len/sizeof(int16_t), g_voice_fft_cb_arg);
    }
}

static void _on_keepalive_handle(uint8_t *data, uint16_t len){
    s_last_keepalive_tick = xTaskGetTickCount()/portTICK_PERIOD_MS;
}

static void _get_music_list_handle(uint8_t *data, uint16_t len){
    uint32_t total = *((uint32_t*)data);
    uint32_t index = *((uint32_t*)(data + 4));
    uint32_t count = *((uint32_t*)(data + 8));
    char* names[10] = {0};
    uint8_t *p = data + 12;
    for(uint32_t i = 0; i < count; i++){
        uint8_t name_len = p[0];
        names[i] = (char*)&p[1];
        p += name_len + 1;
        if (p>= data+len)
        {
            break;
        }
    }
    if (g_music_list_cb)
    {
        g_music_list_cb(total, index, count, (const char**)names);
    }
}

static void _on_play_status_handle(uint8_t *data, uint16_t len){
    if (s_api_event_cb)
    {
        s_api_event_cb(VB_EVT_PLAY_STATUS_CHANGE, data, s_api_evt_cb_arg);
    }
    
}

static void _on_music_title_handle(uint8_t *data, uint16_t len){
    if (s_api_event_cb)
    {
        s_api_event_cb(VB_EVT_MUSIC_TITLE, data, s_api_evt_cb_arg);
    }
    
}
static void _on_music_lyrc_handle(uint8_t *data, uint16_t len){
    if (s_api_event_cb)
    {
        s_api_event_cb(VB_EVT_MUSIC_LYRC, data, s_api_evt_cb_arg);
    }
    
}

static void _on_music_time_handle(uint8_t *data, uint16_t len){
    if (s_api_event_cb)
    {
        s_api_event_cb(VB_EVT_MUSIC_TIME, data, s_api_evt_cb_arg);
    }
}

static void _on_play_mode_handle(uint8_t *data, uint16_t len){
    if (s_api_event_cb)
    {
        s_api_event_cb(VB_EVT_MODE_CHANGE, data, s_api_evt_cb_arg);
    }
}


static _event_handle_t s_event_handle[] = {
    {VB_CMD_GET_MUSIC_LIST, _get_music_list_handle},
    {VB6824_CMD_GET_FFT, _get_fft_handle},
    {VB6824_CMD_SYS_KEEPALIVE, _on_keepalive_handle},
    {VB_CMD_SYS_MODE_CHANGE, _on_play_mode_handle},
    {VB_CMD_SYS_PLAY_STATUS_CHANGE, _on_play_status_handle},
    {VB_CMD_SYS_TITLE, _on_music_title_handle},
    {VB_CMD_SYS_LYRC, _on_music_lyrc_handle},
    {VB_CMD_SYS_TIME, _on_music_time_handle},
    // {VB_CMD_SYS_BT_STATUS_CHANGE, _on_keepalive_handle},
};

void vb_api_recv_handler(uint16_t cmd, uint8_t *data, uint16_t len){
    for (size_t i = 0; i < sizeof(s_event_handle) / sizeof(s_event_handle[0]); i++){        
        if(s_event_handle[i].cmd == cmd){
            if(s_event_handle[i].handle){
                s_event_handle[i].handle(data, len);
            }
            break;
        }
    }
}

int vb_get_music_list_cb(uint32_t index, uint32_t count, vb_music_list_cb_t cb){
    g_music_list_cb = cb;
    uint32_t pack[2] = {index, count};
    __frame_send(VB_CMD_GET_MUSIC_LIST, (uint8_t*)pack, sizeof(pack));
    return 0;
}

int vb_get_music_list_block(uint32_t index, uint32_t count, const char** names){
    uint32_t pack[2] = {index, count};
    // 初始化一个锁
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    frame_send_block(VB_CMD_GET_MUSIC_LIST, (uint8_t*)pack, sizeof(pack), &recv_buf, &recv_len, 1000);
    if(recv_buf && recv_len > 0){
        free(recv_buf);
    }
    return recv_len;
}

static void _vb_api_task(void *arg){
    uint32_t tick = 0;
    while (1)
    {
        tick += 100;
        uint32_t now = (uint32_t)xTaskGetTickCount()/portTICK_PERIOD_MS;
        if (s_last_keepalive_tick!=0 && now - s_last_keepalive_tick > 3000)
        {
            _wakeup_701();
            s_last_keepalive_tick = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 100ms
        if (tick % 1000 == 0)
        {
            __frame_send(VB6824_CMD_SYS_KEEPALIVE, NULL, 0);
        }

    }
}

void vb_api_register_evt(vb_api_event_cb_t cb, void *arg){
    s_api_event_cb = cb;
    s_api_evt_cb_arg = arg;
}

void vb_api_init(gpio_num_t tx, gpio_num_t rx){
    s_gpio_tx = tx;
    s_gpio_rx = rx;
    gpio_set_direction(tx, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(tx, GPIO_PULLDOWN_ONLY);
    gpio_set_level(tx, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(tx, 0);
    xTaskCreate(_vb_api_task, "vbapi", 4096, NULL, 2, NULL);
}