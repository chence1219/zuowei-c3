#ifndef _OPUS_CODEC_H_
#define _OPUS_CODEC_H_

#include <functional>
#include <vector>
#include <cstdint>

#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>
#endif

class OpusCodec {
public:
    OpusCodec();
    virtual ~OpusCodec();

    virtual void EncodeConfig(int sample_rate, int channels, int duration_ms = 60);
    virtual void Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample = -1);
    virtual void EncodeResetState();
    
    virtual void DecodeConfig(int sample_rate, int channels, int duration_ms = 60);
    virtual bool Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample = -1);
    virtual void DecodeResetState();

private:
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
    int encode_sample_rate_;
    int decode_sample_rate_;
    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;
    int encode_resample_ = -1;
    int decode_resample_ = -1;
    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;
#endif
};

#endif // _OPUS_DECODER_WRAPPER_H_
