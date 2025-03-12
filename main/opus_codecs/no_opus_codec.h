#ifndef _NO_OPUS_CODEC_H_
#define _NO_OPUS_CODEC_H_

#include "opus_codec.h"

#include <vector>
#include <cstdint>

class NoOpusCodec : public OpusCodec {
    
public:
    NoOpusCodec();
    virtual ~NoOpusCodec();

    virtual void EncodeConfig(int sample_rate, int channels, int duration_ms = 60) override;
    virtual void Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample = -1) override;
    virtual void EncodeResetState() override;

    virtual void DecodeConfig(int sample_rate, int channels, int duration_ms = 60) override;
    virtual bool Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample = -1) override;
    virtual void DecodeResetState() override;

private:
};

#endif // _OPUS_DECODER_WRAPPER_H_
