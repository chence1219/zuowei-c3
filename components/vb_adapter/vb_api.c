#include "vb_api.h"
#include "vb_cmd.h"
#include "vb_protocol.h"
#include "vb_adapter.h"
#include "vb_event.h"
#include "esp_log.h"
#include "vb_adapter.h"
#include <string.h>
#include "time.h"
#include "freertos/FreeRTOS.h"

#define TAG "vb_api"


vb_music_list_cb_t g_music_list_cb = NULL;

static bool _check_is_wake_word(char *word){
    char *wake_word = vb_adapter_get_wake_word();
    if (word == NULL || wake_word == NULL)
    {
        return false;
    }
    return true;

}

//来电
int _on_phone_call(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_PHONE_CALL, data, len);   
    return 0;
}

int _on_phone_call_hangup(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_PHONE_CALL_HANGUP, data, len);   
    return 0;
}

void vb_api_phone_call_answer(bool answer){
    uint8_t action = answer ? 1 : 0;
    vb_protocol_send(VB_CMD_SYS_PHONE_CALL_ANSWER, &action, 1);
}

int _get_fft_handle(uint8_t *data, uint16_t len, void *arg){
    int16_t *fft_data = (int16_t *)data;
    // ESP_LOGI(TAG, "fft data: %d", len/sizeof(int16_t));
    vb_event_emit(VB_EVT_FFT, fft_data, len/sizeof(int16_t));
    return 0;
}


int _on_mode_change(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_MODE_CHANGE, data, len);   
    return 0;
}

int _on_status_change(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_STATUS_CHANGE, data, len);   
    return 0;
}

int _on_index_change(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_PLAY_INDEX, data, len);   
    return 0;
}

int _on_music_title(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_MUSIC_TITLE, data, len);   
    return 0;
}

int _on_music_lyrc(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_MUSIC_LYRC, data, len);   
    return 0;
}


int _on_music_time(uint8_t *data, uint16_t len, void *arg){
    vb_event_emit(VB_EVT_MUSIC_TIME, data, len);   
    return 0;
}


/* 标记为 used，避免因未被直接调用而在链接阶段被丢弃 */
int _on_wake_word(uint8_t *data, uint16_t len, void *arg){
    ESP_LOGI(TAG, "[%s]: %s", __func__, data);
    if ( _check_is_wake_word((char *)data))
    {
        vb_event_emit(VB_EVT_WAKE_WORD, (void*)data, len);
    }
    return 0;
}

int _on_battery_level(uint8_t *data, uint16_t len, void *arg){
    int level = *((int*)data);
    vb_event_emit(VB_EVT_BATTERY_LEVEL, &level, sizeof(level));
    return 0;
}

static int _on_charge_state(uint8_t *data, uint16_t len, void *arg){
    int level = (int)(*data);
    vb_event_emit(VB_EVT_CHARGE_STATUS, &level, sizeof(level));
    return 0;
}

void vb_api_set_music_next_prev(uint8_t next){
    uint8_t action = next ? 1 : 0;
    vb_protocol_send(VB_CMD_SET_NEXT_PREV, &action, 1);
}

void vb_api_set_music_play(uint8_t play){
    uint8_t action = play ? 1 : 0;
    vb_protocol_send(VB_CMD_SET_PLAY_STATUS, &action, 1);
}

void vb_api_set_music_mode(vb_music_mode_t mode){
    uint8_t m = (uint8_t)mode;
    vb_protocol_send(VB_CMD_SET_MODE, &m, 1);
}

vb_music_mode_t vb_api_get_music_mode(void){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    vb_music_mode_t mode = VB_MUSIC_MODE_BT;
    vb_protocol_send_block(VB_CMD_GET_MODE, NULL, 0, &recv_buf, &recv_len, 100);
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

void vb_set_sys_time(){
    // 获取ESP32系统时间，并通过vb_time_t结构体打印/设置
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    vb_time_t t;
    t.year = timeinfo.tm_year + 1900;
    t.month = timeinfo.tm_mon + 1;
    t.day = timeinfo.tm_mday;
    t.hour = timeinfo.tm_hour;
    t.min = timeinfo.tm_min;
    t.sec = timeinfo.tm_sec;

    // 这里只是打印出来，如果需要设置，可以通过vb_protocol_send发送
    ESP_LOGI(TAG, "ESP32 System Time: %04lu-%02lu-%02lu %02lu:%02lu:%02lu\n",
        t.year, t.month, t.day, t.hour, t.min, t.sec);
    vb_protocol_send(VB_CMD_SET_TIME, (uint8_t*)&t, sizeof(vb_time_t));
}

int vb_api_get_time(vb_time_t *t){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    if (t == NULL)
    {
        return -1;
    }
    
    vb_protocol_send_block(VB_CMD_GET_TIME, NULL, 0, &recv_buf, &recv_len, 100);
    if (recv_len == sizeof(vb_time_t))
    {
        memcpy(t, recv_buf, sizeof(vb_time_t));
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return recv_len == sizeof(vb_time_t)?0:-2;
}

uint8_t vb_api_get_play_status(void){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    uint8_t status = 0;
    vb_protocol_send_block(VB_CMD_GET_PLAY_STATUS, NULL, 0, (uint8_t**)&recv_buf, &recv_len, 100);
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

void vb_api_music_play_by_index(uint32_t index){
    uint32_t i = index;
    vb_protocol_send(VB_CMD_SET_MUSIC_BY_INDEX, (uint8_t*)&i, sizeof(i));
}

uint32_t vb_api_get_play_index(void){
    uint32_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    uint32_t index = 0;
    vb_protocol_send_block(VB_CMD_GET_PLAY_INDEX, NULL, 0, (uint8_t**)&recv_buf, &recv_len, 100);
    if (recv_len != 0)
    {
        index = *((uint32_t*)recv_buf);
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return index;
}

int vb_api_music_list_req(uint32_t index, uint32_t count, vb_music_list_cb_t cb){
    g_music_list_cb = cb;
    uint32_t pack[2] = {index, count};
    vb_protocol_send(VB_CMD_GET_MUSIC_LIST, (uint8_t*)pack, sizeof(pack));
    return 0;
}

int _get_music_list_handle(uint8_t *data, uint16_t len, void *arg){
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
    return 0;
}

int vb_api_get_battery_level(void){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    int level = -1;
    vb_protocol_send_block(VB_CMD_SYS_BATTERY_LEVEL, NULL, 0, &recv_buf, &recv_len, 100);
    if (recv_len != 0)
    {
        level = *((int*)recv_buf);
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return level;
}

int vb_api_get_charge_state(void){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    int status = -1;
    vb_protocol_send_block(VB_CMD_SYS_CHARGE_STATUS, NULL, 0, &recv_buf, &recv_len, 100);
    if (recv_len != 0)
    {
        status = (int)(*recv_buf);
    }
    if (recv_buf)
    {
        free(recv_buf);
    }
    return status;
}

void vb_api_set_tone_index(vb_tone_index_t index){
    uint32_t i = (uint32_t)index;
    vb_protocol_send(VB_CMD_SET_TONE_INDEX, (uint8_t*)&i, sizeof(index));
}

void vb_api_power_off(){
    uint8_t *recv_buf = NULL;
    uint16_t recv_len = 0;
    int retry = 5;
    while (retry-- > 0 && vb_protocol_send_block(VB_CMD_SYS_POWER_OFF, NULL, 0, &recv_buf, &recv_len, 100)<0){
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if(recv_buf){
        free(recv_buf);
    }
    return;
}

void vb_api_ans_enable(uint8_t en){
    uint8_t enable = en;
    vb_protocol_send(VB_CMD_SET_ANS, (uint8_t*)&enable, sizeof(enable));
}

void vb_api_init(){
    
}
