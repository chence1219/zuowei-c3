#include "mz19-dqy-c3-v3.h"
#include "config.h"
#include "device_state.h"
#include "display.h"
#include "display/lv_display.h"
#include "misc/lv_color.h"
#include "widgets/label/lv_label.h"
#include "lock_screen.hpp"

#define TAG "ZwMz01C3Lcd"

namespace {
bool HasRealDisplay(const Display *display) {
  return display != nullptr &&
         dynamic_cast<const NoDisplay *>(display) == nullptr;
}

XjLcdDisplay *AsXjDisplay(Display *display) {
  return dynamic_cast<XjLcdDisplay *>(display);
}
}  // namespace

class ZwMz01C3Lcd : public Mz01C3Lcd {
public:
  ZwMz01C3Lcd() {}
  ~ZwMz01C3Lcd() {}
};

DECLARE_BOARD(ZwMz01C3Lcd);

// Mz01C3Lcd implementation

#if defined(CONFIG_USE_DEVICE_AEC)
void Mz01C3Lcd::ToggleRealtimeChatMode() {
  auto &app = Application::GetInstance();
  app.SetAecMode((app.GetAecMode() == kAecOff) ? kAecOnDeviceSide : kAecOff);
  Settings settings("realtime", true);
  settings.SetInt("enable", (app.GetAecMode() == kAecOff) ? 0 : 1);
  ESP_LOGI(TAG, "set realtime enable: %d",
           (app.GetAecMode() == kAecOff) ? 0 : 1);
  app.Schedule([]() {
    auto &app = Application::GetInstance();
    // app.Close();
    app.Schedule([]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateIdle) {
        if (app.GetAecMode() == kAecOff) {
          // display->ShowNotification(Lang::Strings::WAKEUP_WORD_ABORT,
          // 1000);
          Application::GetInstance().PlaySound(Lang::Sounds::OGG_WAKEWORD);
        } else {
          // display->ShowNotification(Lang::Strings::REALTIME_ABORT, 1000);
          Application::GetInstance().PlaySound(Lang::Sounds::OGG_REALTIME);
        }
      } else {
        app.Schedule([]() {
          auto &app = Application::GetInstance();
          if (app.GetAecMode() == kAecOff) {
            // display->ShowNotification(Lang::Strings::WAKEUP_WORD_ABORT,
            // 1000);
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_WAKEWORD);
          } else {
            // display->ShowNotification(Lang::Strings::REALTIME_ABORT, 1000);
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_REALTIME);
          }
        });
      }
    });
  });
}
#endif

void Mz01C3Lcd::ForceIdle() {
  auto state = Application::GetInstance().GetDeviceState();
  if (state == kDeviceStateListening) {
    Application::GetInstance().ToggleChatState();
  } else if (state == kDeviceStateSpeaking) {
    Application::GetInstance().AbortSpeaking(kAbortReasonNone);
    Application::GetInstance().Close();
    // Application::GetInstance().SetDeviceState(kDeviceStateIdle);
  }
  // auto state = Application::GetInstance().GetDeviceState();
  // if (state == kDeviceStateListening) {
  //   Application::GetInstance().ToggleChatState();
  // } else if (state == kDeviceStateSpeaking) {
  //   Application::GetInstance().SetDeviceState(kDeviceStateIdle);
  //   Application::GetInstance().Schedule(
  //       []() { Application::GetInstance().AbortSpeaking(kAbortReasonNone);
  //       });
  //   Application::GetInstance().Close();
  // }
}

void Mz01C3Lcd::InitializeButtons() {
  if (boot_button_) {
    boot_button_->OnClick([this]() {
      auto &app = Application::GetInstance();

      if (LockScreen::GetInstance()->IsVisible()) {
        LockScreen::GetInstance()->Hide();
        LockScreen::GetInstance()->StartIdleTimer();
        return;
      }

      if (McpAlarm::GetInstance()->IsAlarm()) {
        McpAlarm::GetInstance()->StopAlarm();
      }

      if (app.GetDeviceState() == kDeviceStateWifiConfiguring &&
          !SsidManager::GetInstance().GetSsidList().empty()) {
        app.Reboot();
      }
      app.ToggleChatState();
    });
    boot_button_->OnPressRepeaDone([this](uint16_t count) {
      if (LockScreen::GetInstance()->IsVisible()) {
        LockScreen::GetInstance()->Hide();
        LockScreen::GetInstance()->StartIdleTimer();
        return;
      }

      if (count == 1) {
        return;
      }

      // if (audio_codec_->InOtaMode(1) == true) {
      //     ESP_LOGI(TAG, "OTA mode, do not enter chat");
      //     return;
      // }

      // #if defined(CONFIG_VB6824_OTA_SUPPORT) && CONFIG_VB6824_OTA_SUPPORT
      // == 1
      //             if(count >= 7){
      //                 if(GetBoardCfg()->GetFunEnable(kFunEnableVb6824OTA)){
      //                     int ret = audio_codec_->OtaStart(0);
      //                     if(ret == vbAudioCodec::OTA_ERR_NOT_SUPPORT){
      //                         ESP_LOGW(TAG, "Please enable
      //                         VB6824_OTA_SUPPORT");
      //                     }
      //                 }
      //             }else
      // #endif

#ifdef CONFIG_USE_CUSTOM_OTA
      if (count >= 5) {
        if (kDeviceStateUpgrading !=
            Application::GetInstance().GetDeviceState()) {
          auto &app = Application::GetInstance();
          app.StartCheckNewVersionForCustom();
        }

      } else
#endif
          if (count >= 3) {
        ResetWifiConfiguration();
      }else if (count >= 2) {
        auto state = Application::GetInstance().GetDeviceState();
        if (LockScreen::GetInstance()->IsVisible() == false &&
            (state == kDeviceStateIdle || state == kDeviceStateListening ||
             state == kDeviceStateSpeaking)) {
          LockScreen::GetInstance()->Show();
          LockScreen::GetInstance()->StopIdleTimer();
        }
      }
    });
  }
}

void Mz01C3Lcd::InitializeSpi() {
  ESP_LOGI(TAG, "Display disabled, skipping SPI init");
}

void Mz01C3Lcd::InitializeLcdDisplay() {
  if (display_ == nullptr) {
    display_ = new NoDisplay();
  }
  ESP_LOGI(TAG, "Display disabled, using NoDisplay");
}

void Mz01C3Lcd::DisableDisplayHardware() {
  backlight_ = nullptr;

  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << LCD_BL_GPIO) | (1ULL << LCD_RESET_GPIO);
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  gpio_set_level(LCD_BL_GPIO, DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 1 : 0);
  gpio_set_level(LCD_RESET_GPIO, 0);
  ESP_LOGI(TAG, "Display hardware disabled: backlight off, reset held low");
}

Mz01C3Lcd::Mz01C3Lcd() {
  if (BOOT_BUTTON_GPIO != -1) {
    boot_button_ = new Button(BOOT_BUTTON_GPIO, false, 3000);
  }

  settings_7014_ = new Settings("7014", true);

  InitializeLcdDisplay();
  DisableDisplayHardware();

  audio_codec_ = new vbAudioCodec(CODEC_TX_GPIO, CODEC_RX_GPIO);
  led_ = new SingleLed(RGB_DI_GPIO);

#if defined(CONFIG_USE_DEVICE_AEC)
  Settings settings("realtime", false);
#ifdef CONFIG_DEVICE_AEC_DEFAULT_OFF
  bool realtime_chat_enabled = settings.GetInt("enable", false);
#else
  bool realtime_chat_enabled = settings.GetInt("enable", true);
#endif
  auto &app = Application::GetInstance();
  if (realtime_chat_enabled) {
    app.SetAecMode(kAecOnDeviceSide, false);
  } else {
    app.SetAecMode(kAecOff, false);
  }
// #else
//     if (GetBoardCfg()->GetFunEnable(kFunEnableDeviceAce)) {
//       ESP_LOGW(TAG, "CONFIG_USE_DEVICE_AEC not enable");
//     }
#endif

  McpAlarm::GetInstance()->Init();
  McpAlarm::GetInstance()->SetOnAlarmCallback(
      [this](int time, const std::string &name) {
        std::string text;
        if (name == "") {
          text = "闹钟提醒";
        } else {
          char buf[128];
          snprintf(buf, sizeof(buf), "闹钟提醒：%s", name.c_str());
          text = buf;
        }
        is_alarm_ringing_ = true;
        Application::GetInstance().Schedule([this, text]() {
          LockScreen::GetInstance()->StopIdleTimer();
          if (LockScreen::GetInstance()->IsVisible()) {
            LockScreen::GetInstance()->Hide();
          }
          if (HasRealDisplay(this->display_)) {
            DisplayLockGuard lock(this->display_);
            ToastOpen(60000, text.c_str());
          }
          if (vb_api_get_play_status() == 1) {
            should_resume_bl_play_from_alarm_ = true;
            should_resume_bl_play_ = false;
            vb_api_set_music_play(0);
          }
        });
      });
  McpAlarm::GetInstance()->SetOnAlarmStopCallback(
      [this]() {
        is_alarm_ringing_ = false;
        Application::GetInstance().Schedule([this]() {
          if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle) {
            LockScreen::GetInstance()->StartIdleTimer();
          }
          if (should_resume_bl_play_from_alarm_) {
            should_resume_bl_play_from_alarm_ = false;
            vb_api_set_music_play(1);
          }
          if (HasRealDisplay(this->display_)) {
            DisplayLockGuard lock(this->display_);
            ToastForceClose();
          }
        });
      });
  McpOtaChecker::GetInstance()->Init();
  McpAecController::GetInstance().Init();
  VbMusicContorller::GetInstance().Init();

  EarthLightLED::GetInstance();

  if (HasRealDisplay(display_)) {
    WeatherService::GetInstance()->Init();
    LockScreen::GetInstance()->Init(display_);
  } else {
    ESP_LOGI(TAG, "Display disabled, skipping weather and lock screen init");
  }

  InitializeButtons();

  // music_lyric_timer_ = xTimerCreate(
  //     "music_lyric_timer", pdMS_TO_TICKS(300), pdTRUE, this,
  //     [](TimerHandle_t xTimer) {
  //       auto this_ = (Mz01C3Lcd *)pvTimerGetTimerID(xTimer);
  //       vb_music_
  //     }
  // )

  vb_event_register(
      VB_EVT_MUSIC_TITLE,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;
        ESP_LOGI(TAG, "music title: %s, len:%d", (const char *)data, len);
        this_->music_name = std::string((const char *)data);
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
          return;
        }
        if (this_->display_ == nullptr) {
          return;
        }
        this_->display_->SetStatus((const char *)data);
      },
      this);

  vb_event_register(
      VB_EVT_MUSIC_LYRC,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;
        ESP_LOGI(TAG, "music lyric: %s, len:%d", (const char *)data, len);
        this_->music_lyric = std::string((const char *)data);
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
          return;
        }
        if (this_->display_ == nullptr) {
          return;
        }
        this_->display_->SetChatMessage("system", (const char *)data);
      },
      this);

  vb_event_register(
      VB_EVT_MUSIC_TIME,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;
        ESP_LOGI(TAG, "music_time:%d, len:%d", *(uint32_t *)data, len);
        this_->play_time = *(uint32_t *)data;
        this_->play_duration = ((uint32_t *)data)[1];
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
          return;
        }
        auto display = AsXjDisplay(this_->display_);
        if (display == nullptr) {
          return;
        }
        LockScreen::GetInstance()->StopIdleTimer();
        display->SetStatus(this_->music_name.c_str());
        display->SetMusicProgress(this_->play_time, this_->play_duration);
      },
      this);

  vb_event_register(
      VB_EVT_WAKE_WORD,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;

        ESP_LOGI(TAG, "raw: %.*s, wake word: %.*s, cmp: %d, len:%d", len, data,
                 len, vb_adapter_get_wake_word(),
                 strncmp((const char *)data, vb_adapter_get_wake_word(), len),
                 len);
        // printf("data: ");
        // for (int i = 0; i < len; i++) {
        //   printf("%02x", ((char *)data)[i]);
        //   printf(" ");
        // }
        // printf("\n");
        // printf("wake word: ");
        // char *wake_word = vb_adapter_get_wake_word();
        // for (int i = 0; i < len; i++) {
        //   printf("%02x", ((char *)wake_word)[i]);
        //   printf(" ");
        // }
        // printf("\n");

        if (strncmp((const char *)data, vb_adapter_get_wake_word(), len - 1) ==
            0) {
          ESP_LOGI(TAG, "wake word detected");

          if (LockScreen::GetInstance()->IsVisible()) {
            LockScreen::GetInstance()->Hide();
            LockScreen::GetInstance()->StartIdleTimer();
          }

          auto state = Application::GetInstance().GetDeviceState();
          if (state == kDeviceStateUnknown || state == kDeviceStateStarting ||
              state == kDeviceStateWifiConfiguring) {
            if (!HasRealDisplay(this_->display_)) {
              return;
            }
            DisplayLockGuard lock(this_->display_);
            this_->ToastOpen(6000, "唤醒词已检测到!", lv_color_make(0, 255, 0));
          } else if (state == kDeviceStateIdle ||
                     state == kDeviceStateListening ||
                     state == kDeviceStateSpeaking) {
#ifdef CONFIG_LANGUAGE_ZH_CN
            Application::GetInstance().WakeWordInvoke("你好");
#else
            Application::GetInstance().WakeWordInvoke("");
#endif
          }
        }
        // else if (command == "开始配网") {
        //   this_->ResetWifiConfiguration();
        // }
      },
      this);

#if (USE_7014_BL_AUDIO_MIX == false)
  vb_event_register(
      VB_EVT_STATUS_CHANGE,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;
        uint8_t status = *(uint8_t *)data;
        if (status == 1) { // 如果蓝牙音乐点击了播放，小智就恢复成idle状态
          this_->ForceIdle();
          LockScreen::GetInstance()->StopIdleTimer();
          if(LockScreen::GetInstance()->IsVisible()){
            LockScreen::GetInstance()->Hide();
          }
          ESP_LOGI(TAG, "bl play");
        } else {
          ESP_LOGI(TAG, "bl pause");
          auto display = AsXjDisplay(this_->display_);
          if (display != nullptr) {
            display->HideMusicProgress();
          }
          LockScreen::GetInstance()->StartIdleTimer();
          if(Application::GetInstance().GetDeviceState() == kDeviceStateIdle){
            this_->display_->SetChatMessage("system","");
          }
        }
      },
      this);

  vb_event_register(
      VB_EVT_VOL_CHANGE,
      [](uint32_t event_id, void *data, uint16_t len, void *user_arg) {
        auto this_ = (Mz01C3Lcd *)user_arg;
        uint8_t vol = *(uint8_t *)data;
        ESP_LOGI(TAG, "volume change: %d", vol);
#if (USE_7014_SEPARATE_BL_AND_XIAOZHI_VOLUME)
        // 在listening和speaking状态下，保存小智音量
        auto chat_state = Application::GetInstance().GetDeviceState();
        if (chat_state == kDeviceStateListening ||
            chat_state == kDeviceStateSpeaking) {
          Settings settings("audio", true);
          settings.SetInt("output_volume", vol);
          if (settings.GetInt("output_volume", vol) != vol) {
            Board::GetInstance().GetAudioCodec()->SetOutputVolume(
                vol); // 为了同步codec对象内的音量
          }
        } else {
          // 在其他状态下，保存蓝牙音量
          if (this_->settings_7014_ != nullptr) {
            this_->settings_7014_->SetInt("bl_volume", vol);
          }
        }
#else
        // 如果不分开的话，则同步小智音量
        Settings settings("audio", true);
        auto xz_volume = settings.GetInt("output_volume", vol);
        if (xz_volume != vol) {
          Board::GetInstance().GetAudioCodec()->AudioCodec::SetOutputVolume(
              vol); // 使用的是AudioCodec基类的方法，这样就不会实际设置音量，防止重复设置
        }
#endif
      },
      this);
#endif

  //     audio_codec_->OnWakeUp([this](std::string command) {
  //       ESP_LOGI(TAG, "command: %s, wake word: %s", command.c_str(),
  //                vb6824_get_wakeup_word());
  //       if (command == std::string(vb6824_get_wakeup_word())) {
  //         ESP_LOGI(TAG, "wake word detected");
  //         auto state = Application::GetInstance().GetDeviceState();
  //         if (state == kDeviceStateUnknown || state ==
  //         kDeviceStateStarting
  //         ||
  //             state == kDeviceStateWifiConfiguring) {
  //           Application::GetInstance().PlaySound(
  //               Lang::Sounds::OGG_NET_DISCONNECT);
  //         } else if (state == kDeviceStateIdle ||
  //                    state == kDeviceStateListening ||
  //                    state == kDeviceStateSpeaking) {
  // #ifdef CONFIG_LANGUAGE_ZH_CN
  //           Application::GetInstance().WakeWordInvoke("你好");
  // #else
  //           Application::GetInstance().WakeWordInvoke("");
  // #endif
  //         }
  //       } else if (command == "开始配网") {
  //         ResetWifiConfiguration();
  //       }
  //     });

  if (wifi_config_mode_ == false &&
      !SsidManager::GetInstance().GetSsidList().empty()) {
#ifdef CONFIG_POWER_ON_SOUNDS
    if (boot_sound_timer_ == nullptr) {
      esp_timer_create_args_t timer_args = {
          .callback =
              [](void *arg) {
                auto this_ = (Mz01C3Lcd *)arg;
                esp_reset_reason_t reason = esp_reset_reason();
                if (reason == ESP_RST_POWERON || reason == ESP_RST_DEEPSLEEP) {
                  Application::GetInstance().PlaySound(
                      Lang::Sounds::OGG_POWER_ON);
                }
              },
          .arg = this,
          .dispatch_method = ESP_TIMER_TASK,
          .name = "boot_sound_timer",
          .skip_unhandled_events = true,
      };
      esp_timer_create(&timer_args, &boot_sound_timer_);
    }
    if (boot_sound_timer_) {
      esp_timer_start_once(boot_sound_timer_, 150 * 1000);
    }
#endif

#ifdef CONFIG_NET_STA_SOUNDS
    esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void *event_handler_arg, esp_event_base_t event_base,
           int32_t event_id, void *event_data) {
          static bool conn_status = false;
          if (event_id == WIFI_EVENT_STA_CONNECTED) {
            if (conn_status == false) {
              conn_status = true;
              Application::GetInstance().PlaySound(
                  Lang::Sounds::OGG_NET_CONNECT);
            }
          } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (conn_status == true) {
              conn_status = false;
              Application::GetInstance().PlaySound(
                  Lang::Sounds::OGG_NET_DISCONNECT);
            }
          }
        },
        NULL);
#endif
  }
}

AudioCodec *Mz01C3Lcd::GetAudioCodec() { return (AudioCodec *)audio_codec_; }

Display *Mz01C3Lcd::GetDisplay() { return display_; }

Backlight *Mz01C3Lcd::GetBacklight() { return backlight_; }

Led *Mz01C3Lcd::GetLed() { return led_; }

void Mz01C3Lcd::OnStateChanged() {
  EarthLightLED::GetInstance().OnStateChanged();
  auto state = Application::GetInstance().GetDeviceState();

  if (state == kDeviceStateIdle) {
    LockScreen::GetInstance()->StartIdleTimer();
  } else {
    LockScreen::GetInstance()->StopIdleTimer();
    if (LockScreen::GetInstance()->IsVisible()) {
      LockScreen::GetInstance()->Hide();
    }
  }

#if (USE_7014_BL_AUDIO_MIX == false)
  if (state == kDeviceStateIdle) {
    // 恢复成idle之后，则检查恢复蓝牙播放
    if (should_resume_bl_play_) {
      should_resume_bl_play_ = false;
      bool is_playing = vb_api_get_play_status();
      if (is_playing == false) {
        vb_api_set_music_play(1);
      }
    }
#if (USE_7014_SEPARATE_BL_AND_XIAOZHI_VOLUME)
    // 音量恢复成蓝牙音量
    if (settings_7014_ != nullptr) {
      uint8_t vol = settings_7014_->GetInt("bl_volume", 100);
      vb_audio_set_volume(vol);
    }
#endif
  }

  if (state != kDeviceStateIdle) {
    auto display = AsXjDisplay(GetDisplay());
    if (display) {
      display->HideMusicProgress();
      display->SetChatMessage("system","");
    }
  }

  // 若小智播放，则停止蓝牙播放，并且可以恢复
  if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
    if (vb_api_get_play_status() == 1) {
      should_resume_bl_play_ = true;
      vb_api_set_music_play(0);
    }
#if (USE_7014_SEPARATE_BL_AND_XIAOZHI_VOLUME)
    // 音量要变成小智音量
    Settings settings("audio", false);
    uint8_t output_volume_ = settings.GetInt("output_volume", 100);
    Board::GetInstance().GetAudioCodec()->SetOutputVolume(output_volume_);
#endif
  }
#endif
}

void Mz01C3Lcd::ToastOpen(uint32_t timeout_ms, const char *text) {
  if (!HasRealDisplay(GetDisplay()) || GetDisplay()->GetTheme() == nullptr) {
    return;
  }

  lv_color_t text_color = LvglThemeManager::GetInstance()
                              .GetTheme(GetDisplay()->GetTheme()->name())
                              ->text_color();
  ToastOpen(timeout_ms, text, text_color);
}

void Mz01C3Lcd::ToastOpen(uint32_t timeout_ms, const char *text, lv_color_t text_color) {
  if (!HasRealDisplay(display_)) {
    return;
  }
  if (toast_cont != NULL && toast_label != NULL) {
    return;
  }

  if (toast_timer_ == nullptr) {
    esp_timer_create_args_t timer_args = {
        .callback =
            [](void *arg) {
              Mz01C3Lcd *board = static_cast<Mz01C3Lcd *>(arg);
              Application::GetInstance().Schedule([board]() {
                if (!HasRealDisplay(board->display_)) {
                  return;
                }
                DisplayLockGuard lock(board->display_);
                board->ToastForceClose();
                ESP_LOGI(TAG, "________________ToastOpen timer ended");
              });
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "toast_timer",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &toast_timer_);
  }

  toast_cont = lv_obj_create(lv_layer_top());
  lv_obj_set_size(toast_cont, LV_HOR_RES * 2 / 3, LV_SIZE_CONTENT);
  lv_obj_align(toast_cont, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_remove_flag(toast_cont, LV_OBJ_FLAG_SCROLLABLE);

  toast_label = lv_label_create(toast_cont);
  lv_obj_set_width(toast_label, LV_HOR_RES * 2 / 3 - 16);
  lv_obj_center(toast_label);
  lv_label_set_text(toast_label, text);
  lv_obj_set_style_text_align(toast_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(toast_label, LV_LABEL_LONG_SCROLL);

  if (GetDisplay() == nullptr || GetDisplay()->GetTheme() == nullptr) {
    return;
  }

  if (GetDisplay()->GetTheme()->name() == "light" ||
      GetDisplay()->GetTheme()->name() == "Light") {
    lv_obj_set_style_bg_color(toast_cont, lv_color_white(), 0);
    lv_obj_set_style_text_color(toast_label, text_color, 0);
  } else {
    lv_obj_set_style_bg_color(toast_cont, lv_color_black(), 0);
    lv_obj_set_style_text_color(toast_label, text_color, 0);
  }

  auto font = LvglThemeManager::GetInstance()
                  .GetTheme(GetDisplay()->GetTheme()->name())
                  ->text_font()
                  ->font();
  auto bg_color = LvglThemeManager::GetInstance()
                      .GetTheme(GetDisplay()->GetTheme()->name())
                      ->background_color();
  lv_obj_set_style_text_font(toast_label, font, 0);
  lv_obj_set_style_bg_color(toast_cont, bg_color, 0);

  if (toast_timer_ != nullptr && timeout_ms > 0) {
    ESP_LOGI(TAG, "________________ToastOpen timer started, timeout=%d", timeout_ms);
    esp_timer_start_once(toast_timer_, timeout_ms * 1000);
  }
}

void Mz01C3Lcd::ToastForceClose() {
  if (!HasRealDisplay(display_)) {
    return;
  }

  // DisplayLockGuard lock(display_);

  if (toast_cont != NULL && toast_label != NULL) {
    lv_obj_delete(toast_cont);
    toast_cont = NULL;
    toast_label = NULL;
  }

  if (toast_timer_ != nullptr) {
    esp_timer_stop(toast_timer_);
  }
}
