#include "music_player.h"
#include "music_player_view.h"
#include <vector>
#include <cstring>
#include <cmath>
#include "mcp_server.h"
#include "application.h"
// 工具：把字节流拆成 16-bit 采样
static void deinterleave_int16(const uint8_t* src, int16_t* dstL, int16_t* dstR, int samples) {
    const int16_t* s = reinterpret_cast<const int16_t*>(src);
    for (int i = 0; i < samples; ++i) {
        dstL[i] = s[i * 2];
        dstR[i] = s[i * 2 + 1];
    }
}

// 工具：单声道 → 目标采样率
static std::vector<int16_t> resample(const std::vector<int16_t>& in,
                                      int inRate, int outRate) {
    double ratio = static_cast<double>(inRate) / outRate;
    int outLen = static_cast<int>(std::ceil(in.size() / ratio));
    std::vector<int16_t> out(outLen);

    for (int i = 0; i < outLen; ++i) {
        double srcIdx = i * ratio;
        int   idx0    = static_cast<int>(srcIdx);
        int   idx1    = std::min(idx0 + 1, static_cast<int>(in.size()) - 1);
        double frac   = srcIdx - idx0;
        out[i] = static_cast<int16_t>(std::round(
                    in[idx0] * (1.0 - frac) + in[idx1] * frac));
    }
    return out;
}

// 主接口
void TransferInpuAudioIntoGivenParamAudio(const char* input_data,
                                           int input_len,
                                           int input_sample,
                                           int input_channel,
                                           int out_putchannel,
                                           int out_sample,
                                           std::vector<int16_t>& out_put_samples)
{
    out_put_samples.clear();
    if (!input_data || input_len <= 0) return;

    // 1. 字节流 → 16-bit 采样
    int samples_in = input_len / 2;          // 16-bit 每采样 2 字节
    std::vector<int16_t> pcm_in(samples_in);
    std::memcpy(pcm_in.data(), input_data, input_len);

    // 2. 通道处理：先统一到单声道
    std::vector<int16_t> mono;
    if (input_channel == 2) {
        // 立体声 → 单声道 (L+R)/2
        int frameCount = samples_in / 2;
        mono.resize(frameCount);
        const int16_t* p = pcm_in.data();
        for (int i = 0; i < frameCount; ++i)
            mono[i] = static_cast<int16_t>((static_cast<int32_t>(p[i*2]) + p[i*2+1]) >> 1);
    } else {
        mono = std::move(pcm_in);
    }

    // 3. 采样率转换
    std::vector<int16_t> resampled = resample(mono, input_sample, out_sample);

    // 4. 通道展开：单声道 → 立体声（如果需要）
    if (out_putchannel == 2) {
        int frames = resampled.size();
        out_put_samples.resize(frames * 2);
        for (int i = 0; i < frames; ++i) {
            out_put_samples[i*2]   = resampled[i];
            out_put_samples[i*2+1] = resampled[i];
        }
    } else {
        out_put_samples = std::move(resampled);
    }
}

int DataOutCb(unsigned char*data, int data_size, void *arg){
    auto player = static_cast<MusicPlayer*>(arg);
    return player->DataCallback(data, data_size);
}

void InfoCb(int sample_rate, int channels, int bits, void *arg){
    auto player = static_cast<MusicPlayer*>(arg);
    player->InfoCallback(sample_rate, channels, bits);
}

void PlayStateCb(music_player_state_t state, void *arg){
    auto player = static_cast<MusicPlayer*>(arg);
    player->PlayStateCallback(state);
}

MusicPlayer::MusicPlayer(){
    
}

void MusicPlayer::InfoCallback(int sample_rate, int channels, int bits){
    sample_rate_ = sample_rate;
    channels_ = channels;
    bits_ = bits;
}

void MusicPlayer::PlayStateCallback(music_player_state_t state){
    bool should_play_next = false;
    {
        std::lock_guard<std::mutex> lock(call_back_mutex_);
        current_state_ = state;
        ESP_LOGI("MusicPlayer", "Music player state changed: %d", state);
        should_play_next = (state == MUSIC_PLAYER_STATE_FINISHED || state == MUSIC_PLAYER_STATE_ERROR) &&
            !CurrentPlayingLastMusic() && is_air_music_playing_;
    }

    MusicPlayerView::GetInstance().OnPlayStateChanged(state);

    if (should_play_next) {
        PlayNextAirMusic();
    }
}

int MusicPlayer::DataCallback(uint8_t *data, int data_size){
    if(codec_ == nullptr){
        return -1;
    }
    std::vector<int16_t> pcm;
    //convert data to int16_t pcm
    TransferInpuAudioIntoGivenParamAudio(reinterpret_cast<const char*>(data),
                                          data_size,
                                          sample_rate_,
                                          channels_,
                                          codec_->output_channels(),
                                          codec_->output_sample_rate(),
                                          pcm);
    if (!codec_->output_enabled()) {
        codec_->EnableOutput(true);
    }
    codec_->OutputData(pcm);
    audio_service_->UpdateLastOutputTime();
    return data_size;
}


int MusicPlayer::Initialize(AudioCodec* codec, AudioService* audio_service, std::string sd_card_music_path) {
    if(initialed){
        return 0;
    }
    initialed = true;
    if(codec_ == nullptr){
        codec_ = codec;
        audio_service_ = audio_service;
        // mp3_player_init(DataOutCb, InfoCb, PlayStateCb, this);
        mp3_online_player_.Mp3OnlinePlayerInit(DataOutCb, InfoCb, PlayStateCb, this);
    }
    MusicPlayerView::GetInstance().Initialize();
    if(!sd_card_music_path.empty())
    {
        music_list_manager_.InitLocalMusicListAuto(sd_card_music_path);
    }
    //add mcp tools
    McpServer::GetInstance().AddTool("self.music_player.play_online_music",
        "播放在线音乐，根据索引从缓存列表或在线播放",
        PropertyList({
            Property("index", kPropertyTypeInteger)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string param = "index";
            ESP_LOGI("MusicPlayer", "Received play_online_music tool call");
            try{
                int play_index = properties[param].value<int>();
                if(music_list_manager_.PlayCachedAirMusicByIndex(play_index)){
                    if(PlayCurrentAirMusic()){
                        Application::GetInstance().Close();
                        return R"({"operator":"success","message":"Music playing from cached list"})";;
                    }
                }
                if(PlayAirMusicByIndex(play_index)){
                    Application::GetInstance().Close();
                    return R"({"operator":"success","message":"Music playing"})";;
                }
                if(!music_list_manager_.IsAirMusicListEmpty() && PlayAirMusicByIndex(0)){
                    Application::GetInstance().Close();
                    return R"({"operator":"success","message":"Music playing, but play_index out of range, play the first music"})";
                }
                return R"({"operator":"fail","message":"No music found at given index"})";
            }
            catch(...){
                return R"({"operator":"fail","message":"Invalid play_index"})";
            }
        });

    McpServer::GetInstance().AddTool("self.music_player.stop_music",
        "停止当前正在播放的音乐",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI("MusicPlayer", "Received stop_music tool call");
            this->StopAirPlay();
            return R"({"operator":"success","message":"Music stopped"})";
        });

    McpServer::GetInstance().AddTool("self.music_player.play_next",
        "播放下一首音乐，切换到缓存列表中的下一首",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI("MusicPlayer", "Received play_next tool call");
            if (this->PlayNextAirMusic()) {
                Application::GetInstance().Close();
                return R"({"operator":"success","message":"Playing next music"})";
            }
            return R"({"operator":"fail","message":"No next music available"})";
        });

    McpServer::GetInstance().AddTool("self.music_player.play_previous",
        "播放上一首音乐，切换到缓存列表中的上一首",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI("MusicPlayer", "Received play_previous tool call");
            if (this->PlayPreviousAirMusic()) {
                Application::GetInstance().Close();
                return R"({"operator":"success","message":"Playing previous music"})";
            }
            return R"({"operator":"fail","message":"No previous music available"})";
        });
    return 0;
}

void MusicPlayer::Play(const char* mp3_path){
    if(!initialed){
        return;
    }
    ESP_LOGI("MusicPlayer", "Playing music: %s", mp3_path);
    //这里判断MP3_path 是不是以http开头或者https开头，如果是则认为是在线音乐，否则认为是本地音乐
    is_air_music_playing_ = (strncmp(mp3_path, "http://", 7) == 0 || strncmp(mp3_path, "https://", 8) == 0);
    if(codec_ == nullptr){
        return;
    }
    if (is_air_music_playing_) {
        // mp3_online_player_.StartStreaming(mp3_path);
        struct Params{
            Mp3OnlinePlayer* ptr;
            std::string url;
        };
        Params* input = new Params;
        input->ptr = &mp3_online_player_;
        input->url = std::string(mp3_path);
        xTaskCreate(
        [](void* arg){
            Params* p = (Params*)arg;
            Mp3OnlinePlayer* palyer = p->ptr;
            palyer->StartStreaming(p->url);
            delete p;
            vTaskDelete(NULL);
        }, "tmp_play", 2 * 1024, input, 8,  NULL); 
    } else {
        // mp3_player_play(mp3_path, false, this);
    }
}

MusicProgress MusicPlayer::GetProgress() {
    MusicProgress progress = {0, 0};
    if (!initialed) return progress;
    if (is_air_music_playing_) {
        progress.duration_ms = mp3_online_player_.GetDurationMs();
        progress.position_ms = mp3_online_player_.GetPositionMs();
    } else {
        // progress.duration_ms = mp3_player_get_duration();
        // progress.position_ms = mp3_player_get_position();
    }
    return progress;
}

void MusicPlayer::UpdateAirMusicListAndPlay(const std::vector<MusicInfo>& music_list, bool play_now){
    if(!initialed){
        return;
    }
    if(play_now){
        music_list_manager_.ClearCacheAirMusicList();
        music_list_manager_.SetAirMusicList(music_list);
        bool res = PlayCurrentAirMusic();
        if(res){
            Application::GetInstance().Close();
        }
    }
    else{
        music_list_manager_.CacheAirMusicList(music_list);

    }
}

MusicPlayingInfo MusicPlayer::GetCurrentMusicPlayingInfo() {
    MusicPlayingInfo info = {};
    if (!initialed) return info;

    MusicInfo music_info = music_list_manager_.GetCurrentAirMusic();
    info.name   = music_info.name;
    info.album  = music_info.album;
    info.artist = music_info.artist;
    info.uri    = music_info.uri;

    info.duration_ms = mp3_online_player_.GetDurationMs();
    info.position_ms = mp3_online_player_.GetPositionMs();

    return info;
}