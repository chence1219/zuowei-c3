#include "only_dec_opus_code.h"
#include <esp_log.h>
#include <cstring>

#include "esp_log.h"

#define TAG "NoOpusCodec"

OnlyDecOpusCodec::OnlyDecOpusCodec() {
   
}

OnlyDecOpusCodec::~OnlyDecOpusCodec() {
#ifdef CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
    opus_decoder_.reset();
#endif
}

void OnlyDecOpusCodec::EncodeConfig(int sample_rate, int channels, int duration_ms){

}

void OnlyDecOpusCodec::Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample) {
    
    std::vector<uint8_t> opus(pcm.size()*2);
    memcpy(opus.data(), pcm.data(), opus.size());
    
    if (handler != nullptr) {
        handler(std::move(opus));
    }
}

void OnlyDecOpusCodec::EncodeResetState() {
 
}

void OnlyDecOpusCodec::DecodeConfig(int sample_rate, int channels, int duration_ms){
#ifdef CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
    if(opus_decoder_ != nullptr){
        opus_decoder_.reset();
    }
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, channels, duration_ms);

    decode_sample_rate_ = sample_rate;
#endif
}

bool OnlyDecOpusCodec::Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample) {
#ifdef CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
    if(opus_decoder_ == nullptr){
        ESP_LOGI(TAG, "Decode not config");
        return false;
    }

    if(!opus_decoder_->Decode(std::move(opus), pcm)){
        return false;
    }

    if (resample != -1) {
        if(decode_resample_ != resample){
            decode_resample_ = resample;
            output_resampler_.Configure(decode_sample_rate_, resample);
        }
        int target_size = output_resampler_.GetOutputSamples(pcm.size());
        std::vector<int16_t> resampled(target_size);
        output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
        pcm = std::move(resampled);
    }

    return true;
#else
    return false;
#endif
}

void OnlyDecOpusCodec::DecodeResetState() {
#ifdef CONFIG_OPUS_CODEC_TYPE_ONLY_DECODE
    if(opus_decoder_ == nullptr){
        ESP_LOGI(TAG, "Decode not config");
        return;
    }

    opus_decoder_->ResetState();
#endif
}
