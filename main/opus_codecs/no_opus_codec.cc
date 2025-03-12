#include "no_opus_codec.h"
#include <esp_log.h>
#include <cstring>

NoOpusCodec::NoOpusCodec() {
   
}

NoOpusCodec::~NoOpusCodec() {
    
}

void NoOpusCodec::EncodeConfig(int sample_rate, int channels, int duration_ms){

}

void NoOpusCodec::Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler, int resample) {
    
    std::vector<uint8_t> opus(pcm.size()*2);
    memcpy(opus.data(), pcm.data(), opus.size());
    
    if (handler != nullptr) {
        handler(std::move(opus));
    }
}

void NoOpusCodec::EncodeResetState() {
 
}


void NoOpusCodec::DecodeConfig(int sample_rate, int channels, int duration_ms){

}

bool NoOpusCodec::Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm, int resample) {

    pcm.resize(opus.size()/2);
    memcpy(pcm.data(), opus.data(), pcm.size());

    return true;
}

void NoOpusCodec::DecodeResetState() {
    
}