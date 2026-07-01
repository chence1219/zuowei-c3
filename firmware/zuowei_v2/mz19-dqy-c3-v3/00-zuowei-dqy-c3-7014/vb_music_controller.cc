#include "vb_music_controller.hpp"
#include "mz19-dqy-c3-v3.h"

void VbMusicContorller::Init() {
    McpServer::GetInstance().AddTool(
        "self.bl_music.prev", "蓝牙音乐，播放上一首歌", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          xTaskCreate(
              [](void *arg) {
                for (int i = 0; i < 100; i++) {
                  if (Application::GetInstance().GetDeviceState() !=
                          DeviceState::kDeviceStateSpeaking &&
                      Application::GetInstance().GetDeviceState() !=
                          DeviceState::kDeviceStateListening) {
                    break;
                  } else if (Application::GetInstance().GetDeviceState() ==
                             DeviceState::kDeviceStateListening) {
                    Application::GetInstance().ToggleChatState();
                  }
                  vTaskDelay(pdMS_TO_TICKS(200));
                }
                vb_api_set_music_next_prev(0);
                vTaskDelete(NULL);
              },
              "play_next", 1024, NULL, 5, NULL);
          return true;
        });
    McpServer::GetInstance().AddTool(
        "self.bl_music.next", "蓝牙音乐，播放下一首歌", PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
          xTaskCreate(
              [](void *arg) {
                for (int i = 0; i < 100; i++) {
                  if (Application::GetInstance().GetDeviceState() !=
                          DeviceState::kDeviceStateSpeaking &&
                      Application::GetInstance().GetDeviceState() !=
                          DeviceState::kDeviceStateListening) {
                    break;
                  } else if (Application::GetInstance().GetDeviceState() ==
                             DeviceState::kDeviceStateListening) {
                    Application::GetInstance().ToggleChatState();
                  }
                  vTaskDelay(pdMS_TO_TICKS(200));
                }
                vb_api_set_music_next_prev(1);
                vTaskDelete(NULL);
              },
              "play_next", 1024, NULL, 5, NULL);

          return true;
        });
    McpServer::GetInstance().AddTool(
        "self.bl_music.set_play_or_pause",
        "蓝牙音乐，播放/暂停，参数play_pause为true时播放，为false时暂停/停止",
        PropertyList({Property("play_pause", kPropertyTypeBoolean)}),
        [this](const PropertyList &properties) -> ReturnValue {
          auto play_pause = properties["play_pause"].value<bool>();
          temp_play_pause = play_pause;
          ESP_LOGI(TAG, "play_pause: %d");

          xTaskCreate(
              [](void *arg) {
                auto self = (VbMusicContorller *)arg;
                // 等于暂停的时候，先关掉音频
                if (self->temp_play_pause == false) {
                  for (int i = 0; i < 100; i++) {
                    if (Application::GetInstance().GetDeviceState() !=
                            DeviceState::kDeviceStateSpeaking &&
                        Application::GetInstance().GetDeviceState() !=
                            DeviceState::kDeviceStateListening) {
                      break;
                    } else if (Application::GetInstance().GetDeviceState() ==
                               DeviceState::kDeviceStateListening) {
                      Application::GetInstance().ToggleChatState();
                    }
                    vTaskDelay(pdMS_TO_TICKS(200));
                  }
                }
                auto board = (Mz01C3Lcd *)(&Board::GetInstance());
                board->should_resume_bl_play_ =
                    false; // 不管怎么样，都不能恢复播放
                vb_api_set_music_play(self->temp_play_pause); // 播放/暂停
                vTaskDelete(NULL);
              },
              "play_next", 1024, this, 5, NULL);
          return true;
        });
}
