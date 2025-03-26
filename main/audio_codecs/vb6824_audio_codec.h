#ifndef _VB6824_AUDIO_CODEC_H
#define _VB6824_AUDIO_CODEC_H

#include "audio_codec.h"
#include <driver/gpio.h>
#include <esp_timer.h>

#include <functional>

#include "freertos/timers.h"

class VbAduioCodec : public AudioCodec {
private:
    void ready();
    void WakeUp(std::string command);
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
    std::function<void(std::string)> on_wake_up_;
    std::function<bool()> on_input_ready_;
    std::function<bool()> on_output_ready_;
    esp_timer_handle_t power_save_timer_ = nullptr;
    bool frist_volume_is_set = false;
public:
    VbAduioCodec(gpio_num_t tx, gpio_num_t rx);
    void OnWakeUp(std::function<void(std::string)> callback);
    void SetOutputVolume(int volume) override;
    virtual void OnOutputReady(std::function<bool()> callback) override;
    virtual void OnInputReady(std::function<bool()> callback) override;
    virtual void Start() override;
#if (defined(CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE))
    virtual void OutputData(std::vector<int16_t>& data) override;
    virtual bool InputData(std::vector<int16_t>& data) override;
#endif
    virtual void EnableInput(bool enable) override; 
    virtual void EnableOutput(bool enable) override; 
};

#endif