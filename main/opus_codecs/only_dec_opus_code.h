#ifndef _ONLY_DEC_OPUS_CODEC_H_
#define _ONLY_DEC_OPUS_CODEC_H_

#include "opus_codec.h"

#include "sdkconfig.h"

#include <vector>
#include <cstdint>

#if CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
#include <memory>
#include <functional>
#include <opus_decoder.h>
#include <opus_resampler.h>
#endif

class OnlyDecOpusCodec : public OpusCodec {
    
public:
    OnlyDecOpusCodec();
    virtual ~OnlyDecOpusCodec();

    virtual void EncodeConfig(int sample_rate, int channels, int duration_ms = 60) override;
    virtual void Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample = -1) override;
    virtual void EncodeResetState() override;

    virtual void DecodeConfig(int sample_rate, int channels, int duration_ms = 60) override;
    virtual bool Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample = -1) override;
    virtual void DecodeResetState() override;

private:
#if CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
    int decode_sample_rate_;
    OpusResampler output_resampler_;
    int decode_resample_ = -1;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;
#endif
};

#endif // _OPUS_DECODER_WRAPPER_H_
