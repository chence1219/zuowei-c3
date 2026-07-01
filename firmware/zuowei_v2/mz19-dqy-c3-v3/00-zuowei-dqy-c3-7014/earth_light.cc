// earth_light.cc
#include "earth_light.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "application.h"
#include "settings.h"

void EarthLightLED::Set_EarthLight_Mode(EarthLightMode mode)
{   
    light_mode = mode;
}

uint8_t EarthLightLED::Get_EarthLight_Mode()
{
    return light_mode;
}

std::string EarthLightLED::Get_EarthLight_Mode_string()
{
    // 优化: 直接返回字符串，避免额外操作
    return "mode:" + std::to_string(light_mode);
}

void EarthLightLED::InsideLight_enable(bool enable){
    inside_light_enable_ = enable;
}

void EarthLightLED::OutsideLight_enable(bool enable){
    outside_light_enable_ = enable;
}

void EarthLightLED::InsideLight_off()
{
    if(curr_inside_duty_ == 0){
        return;
    }
    curr_inside_duty_ = 0;
    
    ESP_ERROR_CHECK(ledc_set_duty(inside_ledc_.speed_mode, inside_ledc_.channel, curr_inside_duty_));
    ESP_ERROR_CHECK(ledc_update_duty(inside_ledc_.speed_mode, inside_ledc_.channel));
}

void EarthLightLED::InsideLight_on()
{
    if(inside_light_enable_==false){
        // 如果是未启用状态，则强制关闭
        if(curr_inside_duty_ == 210){
            InsideLight_off();
        }
        return;
    }
    if(curr_inside_duty_ == 210){
        return;
    }
    curr_inside_duty_ = 210;

    ESP_ERROR_CHECK(ledc_set_duty(inside_ledc_.speed_mode, inside_ledc_.channel, curr_inside_duty_));
    ESP_ERROR_CHECK(ledc_update_duty(inside_ledc_.speed_mode, inside_ledc_.channel));
}

bool EarthLightLED::InsideLight_get_onoff(){
    return curr_inside_duty_ > 0;
}

bool EarthLightLED::OutsideLight_get_onoff(){
    return curr_outside_duty_ > 0;
}

void EarthLightLED::OutsideLight_off()
{
    if(curr_outside_duty_ == 0){
        return;
    }
    curr_outside_duty_ = 0;

    ESP_ERROR_CHECK(ledc_set_duty(outside_ledc_.speed_mode, outside_ledc_.channel, curr_outside_duty_));
    ESP_ERROR_CHECK(ledc_update_duty(outside_ledc_.speed_mode, outside_ledc_.channel));
}

void EarthLightLED::OutsideLight_on()
{
    if(outside_light_enable_==false){
        // 如果是未启用状态，则强制关闭
        if(curr_outside_duty_ == 512){
            OutsideLight_off();
        }
        return;
    }
    if(curr_outside_duty_ == 512){
        return;
    }
    curr_outside_duty_ = 512;

    ESP_ERROR_CHECK(ledc_set_duty(outside_ledc_.speed_mode, outside_ledc_.channel, curr_outside_duty_));
    ESP_ERROR_CHECK(ledc_update_duty(outside_ledc_.speed_mode, outside_ledc_.channel));
}

void EarthLightLED::EarthLightLED_Breathe(ledc_channel_t channel, uint32_t min_duty, uint32_t max_duty)
{
    curr_outside_duty_ = breathe_duty_;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, curr_outside_duty_));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));

    if (breathe_duty_ < 64) {
        breathe_duty_ += breathe_direction_ * 2;
    } else if (breathe_duty_ < 128) {
        breathe_duty_ += breathe_direction_ * 5;
    } else if (breathe_duty_ < 256) {
        breathe_duty_ += breathe_direction_ * 7;
    } else {
        breathe_duty_ += breathe_direction_ * 10;
    }

    if (breathe_duty_ <= min_duty) {
        breathe_duty_ = min_duty;
        breathe_direction_ = 1;
    } else if (breathe_duty_ >= max_duty) {
        breathe_duty_ = max_duty;
        breathe_direction_ = -1;
    }
}

#ifdef REMEMBER_LIGHT
void EarthLightLED::SaveLightState() {
    Settings settings("light", true);
    settings.SetBool("inside_enable", inside_light_enable_);
    settings.SetBool("outside_enable", outside_light_enable_);
    ESP_LOGI(TAG, "Light state saved: inside=%d, outside=%d",
             inside_light_enable_, outside_light_enable_);
}

void EarthLightLED::RestoreLightState() {
    Settings settings("light", false);
    inside_light_enable_ = settings.GetBool("inside_enable", true);
    outside_light_enable_ = settings.GetBool("outside_enable", true);
    ESP_LOGI(TAG, "Light state restored: inside=%d, outside=%d",
             inside_light_enable_, outside_light_enable_);
}
#endif

void EarthLightLED::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting:
            break;
        case kDeviceStateWifiConfiguring:
            break;
        case kDeviceStateIdle:
            has_breathe = 0;
            OutsideLight_on();
            break;
        case kDeviceStateConnecting:
            break;
        case kDeviceStateListening:
            has_breathe = 1;
            break;
        case kDeviceStateSpeaking:
            has_breathe = 0;
            OutsideLight_on();
            break;
        case kDeviceStateUpgrading:
            has_breathe = 1;
            break;
        case kDeviceStateActivating:
            break;
        default:
            ESP_LOGE(TAG, "Unknown gpio led event: %d", device_state);
            return;
    }
}