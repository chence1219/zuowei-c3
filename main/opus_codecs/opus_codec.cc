#include "opus_codec.h"
#include <esp_log.h>
#include <cstring>

#include "board.h"
#include "audio_codec.h"

#include "esp_log.h"

#define TAG "OpusCodec"


OpusCodec::OpusCodec() {
   
}

OpusCodec::~OpusCodec() {
    
}

void OpusCodec::EncodeConfig(int sample_rate, int channels, int duration_ms){
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
    if(opus_encoder_ != nullptr){
        // opus_encoder_.reset() //使用reset释放后重新分配可能存在分配失败问题
        // opus_encoder_ = std::make_unique<OpusEncoderWrapper>(sample_rate, channels, duration_ms);
        opus_encoder_->Config(sample_rate, channels, duration_ms);
    }else{
        opus_encoder_ = std::make_unique<OpusEncoderWrapper>(sample_rate, channels, duration_ms);
    }

#ifdef CONFIG_IDF_TARGET_ESP32C3
    opus_encoder_->SetComplexity(3);
#else
    auto& board = Board::GetInstance();
    if (board.GetBoardType() == "ml307") {
        ESP_LOGI(TAG, "ML307 board detected, setting opus encoder complexity to 5");
        opus_encoder_->SetComplexity(5);
    } else {
        ESP_LOGI(TAG, "WiFi board detected, setting opus encoder complexity to 3");
        opus_encoder_->SetComplexity(3);
    }
#endif

    encode_sample_rate_ = sample_rate;
#endif
}

void OpusCodec::Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample) {
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
    if(opus_encoder_ == nullptr){
        ESP_LOGI(TAG, "Encode not config");
        return;
    }
    
    if (resample != -1) {
        if(encode_resample_ != resample){
            encode_resample_ = resample;
            input_resampler_.Configure(resample, encode_sample_rate_);
            reference_resampler_.Configure(resample, encode_sample_rate_);
        }
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec->input_channels() == 2) {
            auto mic_channel = std::vector<int16_t>(pcm.size() / 2);
            auto reference_channel = std::vector<int16_t>(pcm.size() / 2);
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {
                mic_channel[i] = pcm[j];
                reference_channel[i] = pcm[j + 1];
            }
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
            pcm.resize(resampled_mic.size() + resampled_reference.size());
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                pcm[j] = resampled_mic[i];
                pcm[j + 1] = resampled_reference[i];
            }
        } else {
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(pcm.size()));
            input_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
    }

    opus_encoder_->Encode(std::move(pcm), handler);
#endif
}

void OpusCodec::EncodeResetState(){
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS 
    if(opus_encoder_ == nullptr){
        ESP_LOGI(TAG, "Encode not config");
        return;
    }
    opus_encoder_->ResetState();
#endif
}

void OpusCodec::DecodeConfig(int sample_rate, int channels, int duration_ms){
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
    if(opus_decoder_ != nullptr){
        opus_decoder_.reset();
    }
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, channels, duration_ms);

    decode_sample_rate_ = sample_rate;
#endif
}


bool OpusCodec::Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample) {
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
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

void OpusCodec::DecodeResetState() {
#ifdef CONFIG_OPUS_CODEC_TYPE_ESP_OPUS
    if(opus_decoder_ == nullptr){
        ESP_LOGI(TAG, "Decode not config");
        return;
    }
    opus_decoder_->ResetState();
#endif
}
