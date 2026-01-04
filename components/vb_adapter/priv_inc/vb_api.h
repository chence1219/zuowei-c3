#ifndef __VB_API_H__
#define __VB_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum{
    VB_MUSIC_MODE_BT = 0,
    VB_MUSIC_MODE_TF,
    VB_MUSIC_MODE_MAX,
} vb_music_mode_t;

typedef enum{
    VB_TONE_POWER_ON = 0x00,
    VB_TONE_POWER_OFF = 0x01
}vb_tone_index_t;

typedef struct{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
}vb_time_t;

typedef void (*vb_music_list_cb_t)(uint32_t total, uint32_t index, uint32_t count, const char** names);

void vb_api_init();

int vb_api_music_list_req(uint32_t index, uint32_t count, vb_music_list_cb_t cb);

void vb_api_music_play_by_index(uint32_t index);

uint32_t vb_api_get_play_index(void);

uint8_t vb_api_get_play_status(void);

vb_music_mode_t vb_api_get_music_mode(void);

void vb_api_set_music_mode(vb_music_mode_t mode);

void vb_api_set_music_play(uint8_t play);

void vb_api_set_music_next_prev(uint8_t next);

void vb_api_power_off();

void vb_api_set_tone_index(vb_tone_index_t index);

void vb_api_phone_call_answer(bool answer);

int vb_api_get_battery_level(void);

int vb_api_get_charge_state(void);

int vb_api_get_time(vb_time_t *t);

void vb_set_sys_time();

// int vb_alarm_set(alarm_transfer_t *alarm);
// int vb_alarm_add(alarm_transfer_t *alarm);
// int vb_alarm_del(uint8_t index);
// int vb_alarm_get_info(alarm_transfer_t **pAlarm);


#ifdef __cplusplus
}
#endif

#endif