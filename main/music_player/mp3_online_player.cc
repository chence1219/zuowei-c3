#include "mp3_online_player.h"
#include "board.h"
#include "audio/audio_codec.h"


#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#include <cJSON.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype> // 为isdigit函数
#include <thread> // 为线程ID比较
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "MP3_ONLINE_PLAYER"


Mp3OnlinePlayer::Mp3OnlinePlayer() : is_playing_(false), is_downloading_(false),
                           play_thread_(), download_thread_(), audio_buffer_(), buffer_mutex_(),
                           buffer_cv_(), buffer_size_(0), mp3_decoder_(nullptr), mp3_frame_info_(),
                           mp3_decoder_initialized_(false),
                           position_ms_(0), duration_ms_(0), content_length_bytes_(0)
{
    ESP_LOGI(TAG, "Music player initialized with default spectrum display mode");
    InitializeMp3Decoder();
}

Mp3OnlinePlayer::~Mp3OnlinePlayer()
{
    ESP_LOGI(TAG, "Destroying music player - stopping all operations");

    // 停止所有操作
    is_downloading_ = false;
    is_playing_ = false;

    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    // 等待下载线程结束，设置5秒超时
    if (download_thread_.joinable())
    {
        ESP_LOGI(TAG, "Waiting for download thread to finish (timeout: 5s)");
        auto start_time = std::chrono::steady_clock::now();

        // 等待线程结束
        bool thread_finished = false;
        while (!thread_finished)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - start_time)
                               .count();

            if (elapsed >= 5)
            {
                ESP_LOGW(TAG, "Download thread join timeout after 5 seconds");
                break;
            }

            // 再次设置停止标志，确保线程能够检测到
            is_downloading_ = false;

            // 通知条件变量
            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                buffer_cv_.notify_all();
            }

            // 检查线程是否已经结束
            if (!download_thread_.joinable())
            {
                thread_finished = true;
            }

            // 定期打印等待信息
            if (elapsed > 0 && elapsed % 1 == 0)
            {
                ESP_LOGI(TAG, "Still waiting for download thread to finish... (%ds)", (int)elapsed);
            }
        }

        if (download_thread_.joinable())
        {
            download_thread_.join();
        }
        ESP_LOGI(TAG, "Download thread finished");
    }

    // 等待播放线程结束，设置3秒超时
    if (play_thread_.joinable())
    {
        ESP_LOGI(TAG, "Waiting for playback thread to finish (timeout: 3s)");
        auto start_time = std::chrono::steady_clock::now();

        bool thread_finished = false;
        while (!thread_finished)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - start_time)
                               .count();

            if (elapsed >= 3)
            {
                ESP_LOGW(TAG, "Playback thread join timeout after 3 seconds");
                break;
            }

            // 再次设置停止标志
            is_playing_ = false;

            // 通知条件变量
            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                buffer_cv_.notify_all();
            }

            // 检查线程是否已经结束
            if (!play_thread_.joinable())
            {
                thread_finished = true;
            }
        }

        if (play_thread_.joinable())
        {
            play_thread_.join();
        }
        ESP_LOGI(TAG, "Playback thread finished");
    }

    // 清理缓冲区和MP3解码器
    ClearAudioBuffer();
    CleanupMp3Decoder();

    ESP_LOGI(TAG, "Music player destroyed successfully");
}

void Mp3OnlinePlayer::Mp3OnlinePlayerInit(mp3_player_output_cb_t output_cb, mp3_player_info_cb_t info_cb, mp3_player_event_cb_t event_cb, void *output_cb_arg)
{
    output_cb_ = output_cb;
    info_cb_ = info_cb;
    event_cb_ = event_cb;
    user_context_ = output_cb_arg;
}

namespace {
constexpr int kMaxConsecutiveDecodeErrors = 64;
}


unsigned int Mp3OnlinePlayer::GetPositionMs() const
{
    if (!is_playing_) return 0;
    int64_t pos = position_ms_.load();
    if (duration_ms_ > 0 && pos > duration_ms_) pos = duration_ms_;
    return static_cast<unsigned int>(pos);
}

unsigned int Mp3OnlinePlayer::GetDurationMs() const
{
    return static_cast<unsigned int>(duration_ms_);
}

// 开始流式播放
bool Mp3OnlinePlayer::StartStreaming(const std::string &music_url)
{
    std::lock_guard<std::mutex> lock(thread_control_mutex_);
    need_info_cb_ = true;
    if (music_url.empty())
    {
        ESP_LOGE(TAG, "Music URL is empty");
        return false;
    }

    ESP_LOGD(TAG, "Starting streaming for URL: %s", music_url.c_str());

    // 停止之前的播放和下载
    is_downloading_ = false;
    is_playing_ = false;
    try{
        // 等待之前的线程完全结束
        if (download_thread_.joinable())
        {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                buffer_cv_.notify_all(); // 通知线程退出
            }
            download_thread_.join();
        }
        if (play_thread_.joinable())
        {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                buffer_cv_.notify_all(); // 通知线程退出
            }
            play_thread_.join();
        }
    }catch(const std::exception& e){
        ESP_LOGE(TAG, "Thread join exception: %s", e.what());
        // 如果 join 失败，说明对象状态已乱，强制 detach 丢弃
        if (download_thread_.joinable()) download_thread_.detach();
        if (play_thread_.joinable()) play_thread_.detach();
    }
    

    // 清空缓冲区
    ClearAudioBuffer();

    // 配置线程栈大小以避免栈溢出
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 8192 * 2; // 8KB栈大小
    cfg.prio = 16;          // 中等优先级，确保不会被其他高优先级任务频繁抢占导致调度问题
    cfg.thread_name = "audio_stream_download";
    cfg.stack_alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    esp_pthread_set_cfg(&cfg);

    // 开始下载线程
    is_downloading_ = true;
    download_thread_ = std::thread(&Mp3OnlinePlayer::DownloadAudioStream, this, music_url);

    // 开始播放线程（会等待缓冲区有足够数据）
    is_playing_ = true;
    cfg.thread_name = "audio_stream_play";
    esp_pthread_set_cfg(&cfg);
    play_thread_ = std::thread(&Mp3OnlinePlayer::PlayAudioStream, this);

    ESP_LOGI(TAG, "Streaming threads started successfully");

    return true;
}

// 停止流式播放
bool Mp3OnlinePlayer::StopStreaming()
{
    std::lock_guard<std::mutex> lock(thread_control_mutex_);
    ESP_LOGI(TAG, "Stopping music streaming - current state: downloading=%d, playing=%d",
             is_downloading_.load(), is_playing_.load());

    // 检查是否有流式播放正在进行
    if (!is_playing_ && !is_downloading_)
    {
        ESP_LOGW(TAG, "No streaming in progress");
        return true;
    }

    // 停止下载和播放标志
    is_downloading_ = false;
    is_playing_ = false;

    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    // 等待线程结束（避免重复代码，让StopStreaming也能等待线程完全停止）
    if (download_thread_.joinable())
    {
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread joined in StopStreaming");
    }

    // 等待播放线程结束，使用更安全的方式
    if (play_thread_.joinable())
    {
        // 先设置停止标志
        is_playing_ = false;

        // 通知条件变量，确保线程能够退出
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }

        // 使用超时机制等待线程结束，避免死锁
        bool thread_finished = false;
        int wait_count = 0;
        const int max_wait = 100; // 最多等待1秒

        while (!thread_finished && wait_count < max_wait)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;

            // 检查线程是否仍然可join
            if (!play_thread_.joinable())
            {
                thread_finished = true;
                break;
            }
        }

        if (play_thread_.joinable())
        {
            if (wait_count >= max_wait)
            {
                ESP_LOGW(TAG, "Play thread join timeout, detaching thread");
                play_thread_.detach();
            }
            else
            {
                play_thread_.join();
                ESP_LOGI(TAG, "Play thread joined in StopStreaming");
            }
        }
    }
    ESP_LOGI(TAG, "Music streaming stop signal sent");
    if(player_state_ != music_player_state_t::MUSIC_PLAYER_STATE_NONE){
        player_state_ = music_player_state_t::MUSIC_PLAYER_STATE_NONE;
        event_cb_(music_player_state_t::MUSIC_PLAYER_STATE_NONE, user_context_);
    }
    return true;
}

// 从 Content-Range 头解析总大小
// 格式: "bytes 0-262143/12345678"
int64_t Mp3OnlinePlayer::ParseContentRangeTotal(const std::string& content_range) {
    auto pos = content_range.find('/');
    if (pos == std::string::npos) return -1;
    std::string total_str = content_range.substr(pos + 1);
    if (total_str == "*") return -1;  // 未知大小
    return std::stoll(total_str);
}

// 分片 Range 下载后续数据（复用调用方的栈 buffer，避免栈溢出）
void Mp3OnlinePlayer::DownloadWithRange(const std::string& music_url, size_t total_size, char* buffer, size_t buffer_len) {
    size_t current_offset = RANGE_CHUNK_SIZE;  // 首片已在 DownloadAudioStream 中读取

    while (is_downloading_ && is_playing_ && current_offset < total_size) {
        // 等待 buffer 消耗到低水位再下载下一片
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this] {
                return buffer_size_ < BUFFER_LOW_THRESHOLD || !is_downloading_;
            });
            if (!is_downloading_) break;
        }

        // 计算本片范围
        size_t range_end = std::min(current_offset + RANGE_CHUNK_SIZE - 1, total_size - 1);

        // 创建新 HTTP 请求
        auto network = Board::GetInstance().GetNetwork();
        auto http = network->CreateHttp(0);

        std::ostringstream range_header;
        range_header << "bytes=" << current_offset << "-" << range_end;
        http->SetHeader("Range", range_header.str());

        if (!http->Open("GET", music_url)) {
            ESP_LOGE(TAG, "Range request failed to connect, offset=%d", (int)current_offset);
            break;
        }

        int status_code = http->GetStatusCode();
        if (status_code != 206) {
            ESP_LOGW(TAG, "Range request returned %d (expected 206), stopping range download", status_code);
            http->Close();
            break;
        }

        ESP_LOGI(TAG, "Range chunk: bytes %d-%d/%d", (int)current_offset, (int)range_end, (int)total_size);

        // 读取本片数据 (4KB 循环)
        while (is_downloading_ && is_playing_) {
            int bytes_read = http->Read(buffer, buffer_len);
            if (bytes_read < 0) {
                ESP_LOGE(TAG, "Range read error: %d", bytes_read);
                break;
            }
            if (bytes_read == 0) break;

            uint8_t *chunk_data = (uint8_t *)heap_caps_malloc(bytes_read, MALLOC_CAP_SPIRAM);
            if (!chunk_data) {
                ESP_LOGE(TAG, "Failed to allocate memory for range chunk");
                break;
            }
            memcpy(chunk_data, buffer, bytes_read);

            // 等待缓冲区有空间
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                buffer_cv_.wait(lock, [this] {
                    return buffer_size_ < MAX_BUFFER_SIZE || !is_downloading_;
                });

                if (is_downloading_) {
                    audio_buffer_.push(AudioChunk(chunk_data, bytes_read));
                    buffer_size_ += bytes_read;
                    buffer_cv_.notify_one();
                } else {
                    heap_caps_free(chunk_data);
                    break;
                }
            }
        }

        http->Close();
        current_offset = range_end + 1;
    }

    is_downloading_ = false;
    // 通知播放线程下载完成
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    ESP_LOGI(TAG, "Range download finished, total_size=%d", (int)total_size);
}

// 流式下载音频数据
void Mp3OnlinePlayer::DownloadAudioStream(const std::string &music_url)
{
    ESP_LOGD(TAG, "Starting audio stream download from: %s", music_url.c_str());

    // 验证URL有效性
    if (music_url.empty() || music_url.find("http") != 0)
    {
        ESP_LOGE(TAG, "Invalid URL format: %s", music_url.c_str());
        is_downloading_ = false;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        return;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    // 携带 Range 头探测服务器是否支持分片下载
    bool use_range = false;
    if (!force_no_range_) {
        std::ostringstream first_range;
        first_range << "bytes=0-" << (RANGE_CHUNK_SIZE - 1);
        http->SetHeader("Range", first_range.str());
    }

    if (!http->Open("GET", music_url))
    {
        ESP_LOGE(TAG, "Failed to connect to music stream URL");
        is_downloading_ = false;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        return;
    }

    int status_code = http->GetStatusCode();
    if (status_code != 200 && status_code != 206)
    {
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        http->Close();
        is_downloading_ = false;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        return;
    }

    ESP_LOGI(TAG, "Started downloading audio stream, status: %d", status_code);

    // 解析总大小
    content_length_bytes_ = 0;
    size_t total_file_size = 0;

    if (status_code == 206) {
        // 服务器支持 Range，从 Content-Range 解析总大小
        std::string content_range = http->GetResponseHeader("Content-Range");
        ESP_LOGI(TAG, "Content-Range: %s", content_range.c_str());
        int64_t parsed_total = ParseContentRangeTotal(content_range);
        if (parsed_total > 0) {
            total_file_size = static_cast<size_t>(parsed_total);
            content_length_bytes_ = parsed_total;
            use_range = true;
            ESP_LOGI(TAG, "Range supported, total file size: %d bytes", (int)total_file_size);
        } else {
            ESP_LOGW(TAG, "Failed to parse Content-Range, falling back to streaming");
        }
    }

    if (!use_range) {
        // 不支持 Range（返回 200）或解析失败，使用 Content-Length
        size_t body_len = http->GetBodyLength();
        if (body_len > 0) {
            content_length_bytes_ = static_cast<int64_t>(body_len);
            ESP_LOGI(TAG, "Content-Length: %d bytes", (int)content_length_bytes_);
        }
    }

    // 读取首片（或全部）数据
    const size_t chunk_size = 4096;
    char buffer[chunk_size];
    size_t total_downloaded = 0;

    while (is_downloading_ && is_playing_)
    {
        int bytes_read = http->Read(buffer, chunk_size);
        if (bytes_read < 0)
        {
            ESP_LOGE(TAG, "Failed to read audio data: error code %d", bytes_read);
            break;
        }
        if (bytes_read == 0)
        {
            if (use_range) {
                ESP_LOGI(TAG, "First range chunk downloaded, total: %d bytes", (int)total_downloaded);
            } else {
                ESP_LOGI(TAG, "Audio stream download completed, total: %d bytes", (int)total_downloaded);
            }
            break;
        }

        // 尝试检测文件格式（检查文件头）
        if (total_downloaded == 0 && bytes_read >= 4)
        {
            if (memcmp(buffer, "ID3", 3) == 0)
            {
                ESP_LOGI(TAG, "Detected MP3 file with ID3 tag");
            }
            else if (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0)
            {
                ESP_LOGI(TAG, "Detected MP3 file header");
            }
            else if (memcmp(buffer, "RIFF", 4) == 0)
            {
                ESP_LOGI(TAG, "Detected WAV file");
            }
            else if (memcmp(buffer, "fLaC", 4) == 0)
            {
                ESP_LOGI(TAG, "Detected FLAC file");
            }
            else if (memcmp(buffer, "OggS", 4) == 0)
            {
                ESP_LOGI(TAG, "Detected OGG file");
            }
            else
            {
                ESP_LOGI(TAG, "Unknown audio format, first 4 bytes: %02X %02X %02X %02X",
                         (unsigned char)buffer[0], (unsigned char)buffer[1],
                         (unsigned char)buffer[2], (unsigned char)buffer[3]);
            }
        }

        // 创建音频数据块
        uint8_t *chunk_data = (uint8_t *)heap_caps_malloc(bytes_read, MALLOC_CAP_SPIRAM);
        if (!chunk_data)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for audio chunk");
            break;
        }
        memcpy(chunk_data, buffer, bytes_read);

        // 等待缓冲区有空间
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this]
                            { return buffer_size_ < MAX_BUFFER_SIZE || !is_downloading_; });

            if (is_downloading_)
            {
                audio_buffer_.push(AudioChunk(chunk_data, bytes_read));
                buffer_size_ += bytes_read;
                total_downloaded += bytes_read;

                // 通知播放线程有新数据
                buffer_cv_.notify_one();

                if (total_downloaded % (256 * 1024) == 0)
                { // 每256KB打印一次进度
                    ESP_LOGI(TAG, "Downloaded %d bytes, buffer size: %d", (int)total_downloaded, (int)buffer_size_);
                }
            }
            else
            {
                heap_caps_free(chunk_data);
                break;
            }
        }

        // 非 Range 模式节流：避免连续高速下载挤占 WiFi 带宽
        if (!use_range) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    http->Close();

    // Range 模式：首片读完后继续分片下载后续数据
    if (use_range && is_downloading_ && is_playing_ && total_file_size > RANGE_CHUNK_SIZE) {
        DownloadWithRange(music_url, total_file_size, buffer, chunk_size);
        return;  // DownloadWithRange 内部已处理 is_downloading_ 和通知
    }

    is_downloading_ = false;

    // 通知播放线程下载完成
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    ESP_LOGI(TAG, "Audio stream download thread finished");
}

// 流式播放音频数据
void Mp3OnlinePlayer::PlayAudioStream()
{
    ESP_LOGI(TAG, "Starting audio stream playback");

    // 初始化时间跟踪变量
    total_frames_decoded_ = 0;
    duration_ms_ = 0;
    position_ms_ = 0;
    if (!mp3_decoder_initialized_)
    {
        ESP_LOGE(TAG, "MP3 decoder not initialized");
        is_playing_ = false;
        return;
    }

    // 等待缓冲区有足够数据开始播放
    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        buffer_cv_.wait(lock, [this]
                        { return buffer_size_ >= MIN_BUFFER_SIZE || !is_downloading_; });

        if (audio_buffer_.empty() && !is_downloading_)
        {
            ESP_LOGW(TAG, "Download stopped before enough audio was buffered");
            is_playing_ = false;
            return;
        }
    }

    ESP_LOGI(TAG, "Starting playback with buffer size: %d", buffer_size_);

    size_t total_played = 0;
    uint8_t *mp3_input_buffer = nullptr;
    int bytes_left = 0;
    uint8_t *read_ptr = nullptr;
    int consecutive_decode_errors = 0;

    // 分配MP3输入缓冲区
    mp3_input_buffer = (uint8_t *)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
    if (!mp3_input_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate MP3 input buffer");
        is_playing_ = false;
        return;
    }

    // 标记是否已经处理过ID3标签
    bool id3_processed = false;

    while (is_playing_)
    {
        // 如果需要更多MP3数据，从缓冲区读取
        if (bytes_left < 4096)
        { // 保持至少4KB数据用于解码
            AudioChunk chunk;

            // 从缓冲区获取音频数据
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (audio_buffer_.empty())
                {
                    if (!is_downloading_)
                    {
                        // 下载完成且缓冲区为空，播放结束
                        if(player_state_ != music_player_state_t::MUSIC_PLAYER_STATE_FINISHED){
                            player_state_ = music_player_state_t::MUSIC_PLAYER_STATE_FINISHED;
                            event_cb_(music_player_state_t::MUSIC_PLAYER_STATE_FINISHED, user_context_);
                        }
                        ESP_LOGI(TAG, "Playback finished, total played: %d bytes", total_played);
                        break;
                    }
                    // 等待新数据
                    buffer_cv_.wait(lock, [this]
                                    { return !audio_buffer_.empty() || !is_downloading_; });
                    if (audio_buffer_.empty())
                    {
                        continue;
                    }
                }

                chunk = audio_buffer_.front();
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;

                // 通知下载线程缓冲区有空间
                buffer_cv_.notify_one();
            }

            // 将新数据添加到MP3输入缓冲区
            if (chunk.data && chunk.size > 0)
            {
                // 移动剩余数据到缓冲区开头
                if (bytes_left > 0 && read_ptr != mp3_input_buffer)
                {
                    memmove(mp3_input_buffer, read_ptr, bytes_left);
                }

                // 检查缓冲区空间
                size_t space_available = 8192 - bytes_left;
                size_t copy_size = std::min(chunk.size, space_available);

                // 复制新数据
                memcpy(mp3_input_buffer + bytes_left, chunk.data, copy_size);
                bytes_left += copy_size;
                read_ptr = mp3_input_buffer;

                // 检查并跳过ID3标签（仅在开始时处理一次）
                if (!id3_processed && bytes_left >= 10)
                {
                    size_t id3_skip = SkipId3Tag(read_ptr, bytes_left);
                    if (id3_skip > 0)
                    {
                        read_ptr += id3_skip;
                        bytes_left -= id3_skip;
                        ESP_LOGI(TAG, "Skipped ID3 tag: %u bytes", (unsigned int)id3_skip);
                    }
                    id3_processed = true;
                }

                // 释放chunk内存
                heap_caps_free(chunk.data);
            }
        }

        // 尝试找到MP3帧同步
        int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (sync_offset < 0)
        {
            ESP_LOGW(TAG, "No MP3 sync word found, skipping %d bytes", bytes_left);
            bytes_left = 0;
            continue;
        }

        // 跳过到同步位置
        if (sync_offset > 0)
        {
            read_ptr += sync_offset;
            bytes_left -= sync_offset;
        }

        // 解码MP3帧
        int16_t pcm_buffer[2304];
        int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);

        if (decode_result == 0)
        {
            consecutive_decode_errors = 0;
            // 解码成功，获取帧信息
            MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
            if(duration_ms_ == 0 && mp3_frame_info_.bitrate > 0 && content_length_bytes_ > 0){
                // 估算总时长
                duration_ms_ = (content_length_bytes_ * 8 * 1000) / (mp3_frame_info_.bitrate) - 2000; // 减去2秒的预估开头和结尾误差
                ESP_LOGI(TAG, "Estimated total duration: %d ms", (int)duration_ms_);
            }
            total_frames_decoded_++;

            // 基本的帧信息有效性检查，防止除零错误
            if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0)
            {
                ESP_LOGW(TAG, "Invalid frame info: rate=%d, channels=%d, skipping",
                         mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                continue;
            }
            if(need_info_cb_){
                need_info_cb_ = false;
                info_cb_(mp3_frame_info_.samprate, mp3_frame_info_.nChans, mp3_frame_info_.bitrate, user_context_);
            }

            // 计算当前帧的持续时间(毫秒)
            int frame_duration_ms = (mp3_frame_info_.outputSamps * 1000) /
                                    (mp3_frame_info_.samprate * mp3_frame_info_.nChans);

            // 更新当前播放时间
            position_ms_ += frame_duration_ms;

            // 播放to do
            output_cb_((uint8_t*)pcm_buffer, 2 * mp3_frame_info_.outputSamps, user_context_);
            if(player_state_ != music_player_state_t::MUSIC_PLAYER_STATE_PLAYING){
                player_state_ = music_player_state_t::MUSIC_PLAYER_STATE_PLAYING;
                event_cb_(music_player_state_t::MUSIC_PLAYER_STATE_PLAYING, user_context_);
            }
            taskYIELD();  // 主动让出 CPU，给同优先级或更高优先级任务运行机会
        }
        else
        {
            // 解码失败
            consecutive_decode_errors++;
            if (consecutive_decode_errors == 1 ||
                consecutive_decode_errors % 8 == 0 ||
                consecutive_decode_errors >= kMaxConsecutiveDecodeErrors)
            {
                ESP_LOGW(TAG, "MP3 decode failed with error: %d (consecutive=%d)",
                         decode_result, consecutive_decode_errors);
            }

            if (consecutive_decode_errors >= kMaxConsecutiveDecodeErrors)
            {
                ESP_LOGE(TAG, "Too many consecutive MP3 decode errors, stop playback");
                if (player_state_ != music_player_state_t::MUSIC_PLAYER_STATE_ERROR)
                {
                    player_state_ = music_player_state_t::MUSIC_PLAYER_STATE_ERROR;
                    if (event_cb_)
                    {
                        event_cb_(music_player_state_t::MUSIC_PLAYER_STATE_ERROR, user_context_);
                    }
                }
                is_downloading_ = false;
                is_playing_ = false;
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex_);
                    buffer_cv_.notify_all();
                }
                break;
            }
            // 跳过一些字节继续尝试
            if (bytes_left > 1)
            {
                read_ptr++;
                bytes_left--;
            }
            else
            {
                bytes_left = 0;
            }
        }
    }

    // 清理
    if (mp3_input_buffer)
    {
        heap_caps_free(mp3_input_buffer);
    }

    // 播放结束时进行基本清理，但不调用StopStreaming避免线程自我等待
    ESP_LOGI(TAG, "Audio stream playback finished, total played: %d bytes", total_played);
    ESP_LOGI(TAG, "Performing basic cleanup from play thread");

    // 停止播放标志
    is_playing_ = false;
}

// 清空音频缓冲区
void Mp3OnlinePlayer::ClearAudioBuffer()
{
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    while (!audio_buffer_.empty())
    {
        AudioChunk chunk = audio_buffer_.front();
        audio_buffer_.pop();
        if (chunk.data)
        {
            heap_caps_free(chunk.data);
        }
    }

    buffer_size_ = 0;
    ESP_LOGI(TAG, "Audio buffer cleared");
}

// 初始化MP3解码器
bool Mp3OnlinePlayer::InitializeMp3Decoder()
{
    mp3_decoder_ = MP3InitDecoder();
    if (mp3_decoder_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        mp3_decoder_initialized_ = false;
        return false;
    }

    mp3_decoder_initialized_ = true;
    ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    return true;
}

// 清理MP3解码器
void Mp3OnlinePlayer::CleanupMp3Decoder()
{
    if (mp3_decoder_ != nullptr)
    {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
    }
    mp3_decoder_initialized_ = false;
    ESP_LOGI(TAG, "MP3 decoder cleaned up");
}

// 跳过MP3文件开头的ID3标签
size_t Mp3OnlinePlayer::SkipId3Tag(uint8_t *data, size_t size)
{
    if (!data || size < 10)
    {
        return 0;
    }

    // 检查ID3v2标签头 "ID3"
    if (memcmp(data, "ID3", 3) != 0)
    {
        return 0;
    }

    // 计算标签大小（synchsafe integer格式）
    uint32_t tag_size = ((uint32_t)(data[6] & 0x7F) << 21) |
                        ((uint32_t)(data[7] & 0x7F) << 14) |
                        ((uint32_t)(data[8] & 0x7F) << 7) |
                        ((uint32_t)(data[9] & 0x7F));

    // ID3v2头部(10字节) + 标签内容
    size_t total_skip = 10 + tag_size;

    // 确保不超过可用数据大小
    if (total_skip > size)
    {
        total_skip = size;
    }

    ESP_LOGI(TAG, "Found ID3v2 tag, skipping %u bytes", (unsigned int)total_skip);
    return total_skip;
}