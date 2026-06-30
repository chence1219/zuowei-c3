#include "music_player_view.h"

#include "board.h"
#include "lcd_display.h"
#include "lvgl_theme.h"
#include "music_player.h"

#include <esp_err.h>
#include <esp_log.h>

#include <cstdio>
#include <string>

namespace {
constexpr uint64_t kMusicRefreshIntervalUs = 1000000;
const char* TAG = "MusicPlayerView";

std::string BuildMetaText(const MusicPlayingInfo& info) {
    if (!info.artist.empty() && !info.album.empty()) {
        return info.artist + " · " + info.album;
    }
    if (!info.artist.empty()) {
        return info.artist;
    }
    if (!info.album.empty()) {
        return info.album;
    }
    return "";
}

void FormatTimeText(char* buffer, size_t buffer_size, unsigned int position_ms, unsigned int duration_ms) {
    unsigned int position_sec = position_ms / 1000;
    unsigned int duration_sec = duration_ms / 1000;
    snprintf(buffer, buffer_size, "%02u:%02u / %02u:%02u",
        position_sec / 60, position_sec % 60,
        duration_sec / 60, duration_sec % 60);
}
}  // namespace

MusicPlayerView::~MusicPlayerView() {
    StopRefreshTimer();
    if (refresh_timer_ != nullptr) {
        esp_timer_delete(refresh_timer_);
        refresh_timer_ = nullptr;
    }
}

void MusicPlayerView::Initialize() {
    if (refresh_timer_ == nullptr) {
        esp_timer_create_args_t refresh_timer_args = {
            .callback = &MusicPlayerView::RefreshTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "music_view",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&refresh_timer_args, &refresh_timer_));
    }

    EnsureAttached();
    OnPlayStateChanged(MusicPlayer::GetInstance().GetPlayerState());
}

void MusicPlayerView::OnPlayStateChanged(music_player_state_t state) {
    if (state == MUSIC_PLAYER_STATE_PLAYING) {
        if (!EnsureAttached()) {
            return;
        }
        Refresh();
        StartRefreshTimer();
        return;
    }

    StopRefreshTimer();
    Hide();
}

bool MusicPlayerView::EnsureAttached() {
    auto* display = dynamic_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
    if (display == nullptr) {
        return false;
    }

    display_ = display;
    if (music_info_panel_ != nullptr) {
        return true;
    }

    DisplayLockGuard lock(display_);
    if (music_info_panel_ != nullptr) {
        return true;
    }

    auto* lvgl_theme = dynamic_cast<LvglTheme*>(display_->GetTheme());
    if (lvgl_theme == nullptr) {
        return false;
    }

    lv_obj_t* parent = lv_screen_active();
    if (parent == nullptr) {
        return false;
    }

    auto text_font = lvgl_theme->text_font()->font();

    music_info_panel_ = lv_obj_create(parent);
    lv_obj_set_style_layout(music_info_panel_, LV_LAYOUT_NONE, 0);
    lv_obj_add_flag(music_info_panel_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(music_info_panel_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_width(music_info_panel_, LV_PCT(100));
    lv_obj_set_height(music_info_panel_, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(music_info_panel_, 0, 0);
    lv_obj_set_style_border_width(music_info_panel_, 0, 0);
    lv_obj_set_style_pad_all(music_info_panel_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_row(music_info_panel_, lvgl_theme->spacing(2), 0);
    lv_obj_set_flex_flow(music_info_panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(music_info_panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(music_info_panel_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(music_info_panel_, LV_OBJ_FLAG_HIDDEN);

    music_name_label_ = lv_label_create(music_info_panel_);
    lv_obj_set_width(music_name_label_, LV_PCT(100));
    lv_label_set_long_mode(music_name_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(music_name_label_, "");

    music_meta_label_ = lv_label_create(music_info_panel_);
    lv_obj_set_width(music_meta_label_, LV_PCT(100));
    lv_label_set_long_mode(music_meta_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(music_meta_label_, "");

    lv_obj_t* progress_row = lv_obj_create(music_info_panel_);
    lv_obj_set_width(progress_row, LV_PCT(100));
    lv_obj_set_height(progress_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(progress_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(progress_row, 0, 0);
    lv_obj_set_style_pad_all(progress_row, 0, 0);
    lv_obj_set_flex_flow(progress_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(progress_row, lvgl_theme->spacing(2), 0);
    lv_obj_set_scrollbar_mode(progress_row, LV_SCROLLBAR_MODE_OFF);

    music_progress_bar_ = lv_bar_create(progress_row);
    lv_obj_set_flex_grow(music_progress_bar_, 1);
    lv_obj_set_height(music_progress_bar_, 4);
    lv_bar_set_range(music_progress_bar_, 0, 1000);
    lv_bar_set_value(music_progress_bar_, 0, LV_ANIM_OFF);

    music_time_label_ = lv_label_create(progress_row);
    lv_obj_set_style_text_font(music_time_label_, text_font, 0);
    lv_label_set_text(music_time_label_, "00:00 / 00:00");

    ApplyTheme();
    return true;
}

void MusicPlayerView::ApplyTheme() {
    if (display_ == nullptr || music_info_panel_ == nullptr) {
        return;
    }

    auto* lvgl_theme = dynamic_cast<LvglTheme*>(display_->GetTheme());
    if (lvgl_theme == nullptr) {
        return;
    }

    auto text_font = lvgl_theme->text_font()->font();
    lv_color_t meta_color = lv_color_mix(lvgl_theme->text_color(), lvgl_theme->background_color(), LV_OPA_50);

    lv_obj_set_style_bg_color(music_info_panel_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_bg_opa(music_info_panel_, LV_OPA_70, 0);

    lv_obj_set_style_text_font(music_name_label_, text_font, 0);
    lv_obj_set_style_text_color(music_name_label_, lvgl_theme->text_color(), 0);

    lv_obj_set_style_text_font(music_meta_label_, text_font, 0);
    lv_obj_set_style_text_color(music_meta_label_, meta_color, 0);

    lv_obj_set_style_text_font(music_time_label_, text_font, 0);
    lv_obj_set_style_text_color(music_time_label_, lvgl_theme->text_color(), 0);

    lv_obj_set_style_bg_color(music_progress_bar_, lvgl_theme->border_color(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(music_progress_bar_, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_progress_bar_, lvgl_theme->text_color(), LV_PART_INDICATOR);
}

void MusicPlayerView::Refresh() {
    if (!EnsureAttached()) {
        return;
    }

    auto& player = MusicPlayer::GetInstance();
    if (player.GetPlayerState() != MUSIC_PLAYER_STATE_PLAYING) {
        Hide();
        return;
    }

    MusicPlayingInfo info = player.GetCurrentMusicPlayingInfo();

    DisplayLockGuard lock(display_);
    if (music_info_panel_ == nullptr) {
        return;
    }

    ApplyTheme();
    lv_obj_remove_flag(music_info_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(music_name_label_, info.name.empty() ? "..." : info.name.c_str());

    std::string meta_text = BuildMetaText(info);
    lv_label_set_text(music_meta_label_, meta_text.c_str());

    int progress_value = 0;
    if (info.duration_ms > 0) {
        uint64_t scaled = static_cast<uint64_t>(info.position_ms) * 1000 / info.duration_ms;
        if (scaled > 1000) {
            scaled = 1000;
        }
        progress_value = static_cast<int>(scaled);
    }
    lv_bar_set_value(music_progress_bar_, progress_value, LV_ANIM_OFF);

    char time_text[24];
    FormatTimeText(time_text, sizeof(time_text), info.position_ms, info.duration_ms);
    lv_label_set_text(music_time_label_, time_text);
}

void MusicPlayerView::Hide() {
    if (display_ == nullptr || music_info_panel_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(display_);
    if (music_info_panel_ != nullptr) {
        lv_obj_add_flag(music_info_panel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MusicPlayerView::StartRefreshTimer() {
    if (refresh_timer_ == nullptr) {
        return;
    }

    esp_err_t err = esp_timer_start_periodic(refresh_timer_, kMusicRefreshIntervalUs);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to start refresh timer: %s", esp_err_to_name(err));
    }
}

void MusicPlayerView::StopRefreshTimer() {
    if (refresh_timer_ == nullptr) {
        return;
    }

    esp_err_t err = esp_timer_stop(refresh_timer_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to stop refresh timer: %s", esp_err_to_name(err));
    }
}

void MusicPlayerView::RefreshTimerCallback(void* arg) {
    auto* view = static_cast<MusicPlayerView*>(arg);
    view->Refresh();
}
