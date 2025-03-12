#ifndef _VB6824_AUDIO_CODEC_H
#define _VB6824_AUDIO_CODEC_H

#include "audio_codec.h"
#include <driver/gpio.h>

#include <functional>

#include "rbuffer.h"

#include "freertos/timers.h"

typedef enum
{
    VB6824_CMD_RECV_PCM = 0x2080,
    VB6824_CMD_RECV_CTL = 0x0180,
    VB6824_CMD_SEND_PCM = 0x2081,
    VB6824_CMD_SEND_PCM_EOF = 0x0204,
    VB6824_CMD_SEND_CTL = 0x0202,
    VB6824_CMD_SEND_VOLUM = 0x0203,
    VB6824_CMD_SEND_NULL = 0xFFFF,
}vb6824_cmd_t;

class VbAduioCodec : public AudioCodec {
private:
    rbuffer_handle_t play_buf;
    rbuffer_handle_t recv_buf;
    QueueHandle_t play_queue;
    QueueHandle_t uartQueue;
    
    void vb_thread();
    void uart_event_task();
    void uart_init(gpio_num_t tx, gpio_num_t rx);
    void vb6824_parse(uint8_t *data, uint16_t len);
    void vb6824_send(vb6824_cmd_t cmd, uint8_t *data, uint16_t len);
    void vb6824_recv_cb(vb6824_cmd_t cmd, uint8_t *data, uint16_t len);
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
    std::function<void(std::string)> on_wake_up_;
    std::function<bool()> on_input_ready_;
    std::function<bool()> on_output_ready_;
    uint8_t *play_buf_u8;
public:
    VbAduioCodec(gpio_num_t tx, gpio_num_t rx);
    void OnWakeUp(std::function<void(std::string)> callback);
    void SetOutputVolume(int volume) override;
    virtual void OnOutputReady(std::function<bool()> callback) override;
    virtual void OnInputReady(std::function<bool()> callback) override;
    virtual void Start() override;
#ifndef CONFIG_AUDIO_CODEC_VB6824_TYPE_PCM_16K
    virtual void OutputData(std::vector<int16_t>& data) override;
    virtual bool InputData(std::vector<int16_t>& data) override;
#endif
    virtual void EnableInput(bool enable) override; 
    virtual void EnableOutput(bool enable) override; 
};

#endif