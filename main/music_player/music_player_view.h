#pragma once

#include <esp_timer.h>
#include <lvgl.h>

#include "mp3_online_player.h"

class LcdDisplay;

class MusicPlayerView {
public:
    static MusicPlayerView& GetInstance() {
        static MusicPlayerView instance;
        return instance;
    }

    void Initialize();
    void OnPlayStateChanged(music_player_state_t state);

private:
    MusicPlayerView() = default;
    ~MusicPlayerView();
    MusicPlayerView(const MusicPlayerView&) = delete;
    MusicPlayerView& operator=(const MusicPlayerView&) = delete;

    bool EnsureAttached();
    void ApplyTheme();
    void Refresh();
    void Hide();
    void StartRefreshTimer();
    void StopRefreshTimer();

    static void RefreshTimerCallback(void* arg);

private:
    LcdDisplay* display_ = nullptr;
    lv_obj_t* music_info_panel_ = nullptr;
    lv_obj_t* music_name_label_ = nullptr;
    lv_obj_t* music_meta_label_ = nullptr;
    lv_obj_t* music_progress_bar_ = nullptr;
    lv_obj_t* music_time_label_ = nullptr;
    esp_timer_handle_t refresh_timer_ = nullptr;
};
