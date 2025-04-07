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
#ifdef CONFIG_USE_AUDIO_CODEC_ENCODE_OPUS
    virtual int Read(uint8_t* dest, int samples) override;
#endif
    virtual int Write(const int16_t* data, int samples) override;
#ifdef CONFIG_USE_AUDIO_CODEC_DECODE_OPUS
    virtual int Write(uint8_t* opus, int samples) override;
#endif
    std::function<void(std::string)> on_wake_up_;
    bool frist_volume_is_set = false;
public:
    VbAduioCodec(gpio_num_t tx, gpio_num_t rx);
    void OnWakeUp(std::function<void(std::string)> callback);
    void SetOutputVolume(int volume) override;
    virtual void Start() override;
#ifdef CONFIG_USE_AUDIO_CODEC_ENCODE_OPUS
    virtual bool InputData(std::vector<uint8_t>& opus) override;
#endif
    virtual void EnableInput(bool enable) override; 
    virtual void EnableOutput(bool enable) override; 
};

#endif