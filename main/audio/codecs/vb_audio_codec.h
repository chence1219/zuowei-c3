#ifndef _VB_AUDIO_CODEC_H_
#define _VB_AUDIO_CODEC_H_

#include "audio_codec.h"
#include <driver/gpio.h>

class vbAudioCodec : public AudioCodec {
public:
    vbAudioCodec(gpio_num_t tx, gpio_num_t rx);

    void SetOutputVolume(int volume) override;
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    void Start() override;

    // PCM 读写
    
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Read(uint8_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
    virtual bool InputData(std::vector<uint8_t>& opus) override;
private:
    bool first_volume_set_ = false;
};

#endif // _VB_AUDIO_CODEC_H_


