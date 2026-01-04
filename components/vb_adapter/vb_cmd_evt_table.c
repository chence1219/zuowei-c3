#include "vb_protocol.h"
#include "vb_cmd.h"

int _on_phone_call(uint8_t *data, uint16_t len, void *arg);
int _on_phone_call_hangup(uint8_t *data, uint16_t len, void *arg);
int _get_fft_handle(uint8_t *data, uint16_t len, void *arg);
int _on_mode_change(uint8_t *data, uint16_t len, void *arg);
int _on_status_change(uint8_t *data, uint16_t len, void *arg);
int _on_index_change(uint8_t *data, uint16_t len, void *arg);
int _on_music_title(uint8_t *data, uint16_t len, void *arg);
int _on_music_lyrc(uint8_t *data, uint16_t len, void *arg);
int _on_music_time(uint8_t *data, uint16_t len, void *arg);
int _on_wake_word(uint8_t *data, uint16_t len, void *arg);
int _on_battery_level(uint8_t *data, uint16_t len, void *arg);
int _get_music_list_handle(uint8_t *data, uint16_t len, void *arg);
int _get_volume_handle(uint8_t *data, uint16_t len, void *arg);
int vb_audio_input(uint8_t *data, uint16_t len, void *arg);
int __on_keepalive(uint8_t *data, uint16_t len, void *arg);

const vb_cmd_evt_t g_vb_cmd_evt_table[] = {
    { VB_CMD_SYS_PHONE_NUMBER, _on_phone_call },
    { VB_CMD_SYS_PHONE_CALL_HANGUP, _on_phone_call_hangup },
    { VB_CMD_GET_FFT, _get_fft_handle },
    { VB_CMD_SYS_MODE_CHANGE, _on_mode_change },
    { VB_CMD_SYS_PLAY_STATUS_CHANGE, _on_status_change },
    { VB_CMD_SYS_PLAY_INDEX, _on_index_change },
    { VB_CMD_SYS_TITLE, _on_music_title },
    { VB_CMD_SYS_LYRIC, _on_music_lyrc },
    { VB_CMD_SYS_TIME, _on_music_time },
    { VB_CMD_RECV_CTL, _on_wake_word },
    { VB_CMD_SYS_BATTERY_LEVEL, _on_battery_level },
    { VB_CMD_SYS_CHARGE_STATUS, _on_battery_level },
    { VB_CMD_GET_MUSIC_LIST, _get_music_list_handle },
    { VB_CMD_GET_VOLUME, _get_volume_handle },
    { VB_CMD_RECV_AUDIO, vb_audio_input },
    { VB_CMD_SYS_KEEPALIVE, __on_keepalive },
};

const size_t g_vb_cmd_evt_table_size = sizeof(g_vb_cmd_evt_table) / sizeof(g_vb_cmd_evt_table[0]);
