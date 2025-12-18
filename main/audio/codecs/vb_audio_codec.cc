#include "vb_audio_codec.h"

#include "vb_adapter.h"
#include "vb_audio.h"
#include "vb_cmd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "vbAudioCodec";

vbAudioCodec::vbAudioCodec(gpio_num_t tx, gpio_num_t rx)
{
    input_sample_rate_  = 16 * 1000;
    output_sample_rate_ = 16 * 1000;

    // 初始化适配层（内部含协议/传输），音频回调不需要额外上报
    vb_adapter_init(tx, rx, nullptr, nullptr);
    vb_audio_init();
}


void vbAudioCodec::Start() {
    // 默认启用输入输出
    EnableInput(true);
    EnableOutput(true);
}

void vbAudioCodec::SetOutputVolume(int volume){
    vb_audio_set_volume((uint8_t)volume);
    AudioCodec::SetOutputVolume(volume);
}

#ifdef CONFIG_USE_AUDIO_CODEC_ENCODE_OPUS
bool vbAudioCodec::InputData(std::vector<uint8_t>& opus) {
    opus.resize(40);
    int samples = Read((uint8_t *)opus.data(), opus.size());
    if (samples > 0) {
        return true;
    }
    return false;
}
#endif

void vbAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    vb_audio_enable_input(enable);
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void vbAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    vb_audio_enable_output(enable);
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}

int vbAudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        int read_len = vb_audio_read((uint8_t *)dest, 2 * samples);
        if (read_len > 0) {
            return read_len / 2;
        }
    }
    return 0;
}

#ifdef CONFIG_USE_AUDIO_CODEC_ENCODE_OPUS
int vbAudioCodec::Read(uint8_t* dest, int samples) {
    if (input_enabled_) {
        int read_len = vb_audio_read(dest, samples);
        if (read_len > 0) {
            return read_len;
        }
    }
    return 0;
}
#endif

int vbAudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        if(!first_volume_set_){
            first_volume_set_ = true;
            SetOutputVolume(output_volume_);
        }
        vb_audio_write((uint8_t *)data, 2 * samples);
        return samples;
    }
    return 0;
}

#ifdef CONFIG_USE_AUDIO_CODEC_DECODE_OPUS
int vbAudioCodec::Write(uint8_t* opus, int samples) {
    if (output_enabled_) {
        if(!first_volume_set_){
            first_volume_set_ = true;
            SetOutputVolume(output_volume_);
        }
        vb_audio_write(opus, samples);
        return samples;
    }
    return 0;
}
#endif


