#ifndef __VB_API_H__
#define __VB_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum{
    VB_MUSIC_MODE_BT = 0,
    VB_MUSIC_MODE_TF,
    VB_MUSIC_MODE_MAX,
} vb_music_mode_t;

typedef enum{
    VB_EVT_KEEPALIVE,
    VB_EVT_MODE_CHANGE,
    VB_EVT_PLAY_STATUS_CHANGE,
    VB_EVT_BT_STATUS_CHANGE,
    VB_EVT_MUSIC_TITLE,
    VB_EVT_MUSIC_LYRC,
    VB_EVT_MUSIC_TIME,
} vb_api_event_t;

typedef void (*vb_music_list_cb_t)(uint32_t total, uint32_t index, uint32_t count, const char** names);
typedef void (*vb_api_event_cb_t)(vb_api_event_t code, void *data, void *arg);

/**
 * @brief 控制音乐播放或暂停
 *
 * 该函数用于控制音乐播放状态。
 *
 * @param play 控制标志：1 = 播放，0 = 暂停
 */
void vb_music_play(uint8_t play);

/**
 * @brief 上一首或下一首音乐
 * @param next 控制标志：1 = 下一首，0 = 上一首
 */
void vb_music_next_priv(uint8_t next);

void vb_music_set_mode(vb_music_mode_t mode);

vb_music_mode_t vb_music_get_mode(); 

uint8_t vb_music_get_play_status();

/**
 * @brief 获取音乐列表
 */
int vb_get_music_list(uint32_t index, uint32_t count, const char** names);

int vb_get_music_list_cb(uint32_t index, uint32_t count, vb_music_list_cb_t cb);

void vb_api_register_evt(vb_api_event_cb_t cb, void *arg);

void vb_music_play_by_index(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // __VB_API_H__