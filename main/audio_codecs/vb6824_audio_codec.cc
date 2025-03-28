#include "vb6824_audio_codec.h"

#include <cstring>
#include <cmath>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "settings.h"
#include <esp_timer.h>

#include "esp_log.h"

#include "vb6824.h"

static const char *TAG = "VbAduioCodec";

#define VB_PLAY_SAMPLE_RATE     16 * 1000
#define VB_RECO_SAMPLE_RATE     16 * 1000

// #ifdef CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
// IRAM_ATTR void VbAduioCodec::ready() {
// #else
void VbAduioCodec::ready() {
// #endif
    if (on_input_ready_) {
        on_input_ready_();
    }
    if (on_output_ready_) {
        on_output_ready_();
    }
}

void VbAduioCodec::WakeUp(std::string command) {
    if (on_wake_up_) {
        on_wake_up_(command);
    }
}

void VbAduioCodec::OnWakeUp(std::function<void(std::string)> callback) {
    on_wake_up_ = callback;
}

VbAduioCodec::VbAduioCodec(gpio_num_t tx, gpio_num_t rx) {

    input_sample_rate_ = VB_RECO_SAMPLE_RATE;
    output_sample_rate_ = VB_RECO_SAMPLE_RATE;

    vb6824_init(tx, rx);

    vb6824_register_voice_command_cb([](char *command, uint16_t len, void *arg){
        auto this_ = (VbAduioCodec*)arg;
        this_->WakeUp(command);
    }, this);
}

void VbAduioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);

    EnableInput(true);
    EnableOutput(true);

    if(ready_timer_ == nullptr){
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto this_ = (VbAduioCodec*)arg;
                this_->ready();
            },
            .arg = this,
// #ifdef CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
            // .dispatch_method = ESP_TIMER_ISR,
// #else
            .dispatch_method = ESP_TIMER_TASK,
// #endif
            .name = "vb_ready",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&timer_args, &ready_timer_);
    }
    if(ready_timer_){
        esp_timer_start_periodic(ready_timer_, 20*1000);
    }
}

#if (defined(CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE) || defined(CONFIG_OPUS_CODEC_TYPE_NO_DECODE))
bool VbAduioCodec::InputData(std::vector<int16_t>& data) {
    data.resize(20);
    int samples = Read(data.data(), data.size());
    if (samples > 0) {
        return true;
    }
    return false;
}

void VbAduioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}
#endif

void VbAduioCodec::SetOutputVolume(int volume){
    vb6824_audio_set_output_volume(volume);
    AudioCodec::SetOutputVolume(volume);
}

void VbAduioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void VbAduioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}

void VbAduioCodec::OnInputReady(std::function<bool()> callback) {
    on_input_ready_ = callback;
}

void VbAduioCodec::OnOutputReady(std::function<bool()> callback) {
    on_output_ready_ = callback;
}

int VbAduioCodec::Read(int16_t* dest, int samples) {
    int read_len = vb6824_audio_read((uint8_t *)dest, 2 * samples);
    return read_len / 2;
}

int VbAduioCodec::Write(const int16_t* data, int samples) {
    if(frist_volume_is_set == false){
        frist_volume_is_set = true;
        SetOutputVolume(output_volume_);
    }
    vb6824_audio_write((uint8_t *)data, 2 * samples);
    return samples;
}