// earth_light.h
#ifndef _EARTH_LIGHT_H_
#define _EARTH_LIGHT_H_

#include "config.h"
#include "freertos/idf_additions.h"
#include "hal/ledc_types.h"
#include "led/led.h"
#include "mcp_server.h"
#include <atomic>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <led_strip.h>
#include <mutex>
#include <vector>

#define DEFAULT_BRIGHTNESS 70
#define DEFAULT_USE_MODE 0

#define LEDC_LS_TIMER LEDC_TIMER_1
#define LEDC_LS_MODE LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH0_CHANNEL LEDC_CHANNEL_1
#define LEDC_LS_CH1_CHANNEL LEDC_CHANNEL_2

#define TAG "EarthLightLED"

enum EarthLightMode {
  kEarthLightModeOff = 0,
  kEarthLightModeOutside = 1,
  kEarthLightModeBoth = 2,
  kEarthLightModeBothBreathe = 3,
  kEarthLightModeInside = 4,
};

class EarthLightLED : public Led {

public:
  static EarthLightLED &GetInstance() {
    static EarthLightLED instance =
        EarthLightLED(EARTH_LED_PWM_GPIO, LED_PWM_GPIO, 0);
    return instance;
  }

  EarthLightLED(gpio_num_t inside_gpio, gpio_num_t outside_gpio,
                int output_invert = 0) {
    ledc_timer_config_t ledc_timer_inside = {};
    ledc_timer_config_t ledc_timer_outside = {};
    ledc_timer_inside.duty_resolution =
        LEDC_TIMER_9_BIT;                        // resolution of PWM duty
    ledc_timer_inside.freq_hz = 78176;           // frequency of PWM signal
    ledc_timer_inside.speed_mode = LEDC_LS_MODE; // timer mode
    ledc_timer_inside.timer_num = LEDC_LS_TIMER; // timer index
    ledc_timer_inside.clk_cfg = LEDC_AUTO_CLK;   // Auto select the source clock
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_inside));

    ledc_timer_outside.duty_resolution =
        LEDC_TIMER_9_BIT;                         // resolution of PWM duty
    ledc_timer_outside.freq_hz = 5000;            // frequency of PWM signal
    ledc_timer_outside.speed_mode = LEDC_LS_MODE; // timer mode
    ledc_timer_outside.timer_num = LEDC_TIMER_2;  // timer index
    ledc_timer_outside.clk_cfg = LEDC_AUTO_CLK; // Auto select the source clock
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_outside));

    inside_ledc_.channel = LEDC_LS_CH0_CHANNEL;
    inside_ledc_.duty = 0;
    inside_ledc_.gpio_num = inside_gpio;
    inside_ledc_.speed_mode = LEDC_LS_MODE;
    inside_ledc_.hpoint = 0;
    inside_ledc_.timer_sel = LEDC_LS_TIMER;
    inside_ledc_.flags.output_invert = output_invert & 0x01;
    inside_ledc_.intr_type = LEDC_INTR_DISABLE;
    ledc_channel_config(&inside_ledc_);

    outside_ledc_.channel = LEDC_LS_CH1_CHANNEL;
    outside_ledc_.duty = 0;
    outside_ledc_.gpio_num = outside_gpio;
    outside_ledc_.speed_mode = LEDC_LS_MODE;
    outside_ledc_.hpoint = 0;
    outside_ledc_.timer_sel = LEDC_TIMER_2;
    outside_ledc_.intr_type = LEDC_INTR_DISABLE;
    ledc_channel_config(&outside_ledc_);

#ifdef REMEMBER_LIGHT
    RestoreLightState();
#endif
    OutsideLight_on();
    InsideLight_on();

    xTaskCreate(
        [](void *arg) {
          auto self = (EarthLightLED *)arg;
          while (1) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (self->outside_light_enable_ == false) {
              self->OutsideLight_off();
              vTaskDelay(pdMS_TO_TICKS(500));
              continue;
            }
            if (self->has_breathe) {
              self->EarthLightLED_Breathe(LEDC_LS_CH1_CHANNEL, 10, 512);
            }
          }
          vTaskDelete(NULL);
        },
        "earth task", 1024 * 5, this, 5, NULL);

    McpServer::GetInstance().AddTool(
        "self.earth_lamp.is_earth_light_on",
        "Checks if the Earth lamp is on. Returns true if it is on, false "
        "otherwise. 地球灯是在外框内，磁悬浮的球体灯，也可以称作内灯",
        PropertyList(), [](const PropertyList &properties) -> ReturnValue {
          return EarthLightLED::GetInstance().InsideLight_get_onoff();
        });

    McpServer::GetInstance().AddTool(
        "self.earth_lamp.is_outside_light_on",
        "Checks if the outside light is on. Returns true if it is on, false "
        "otherwise. 外灯是在外框上的灯，也可以称作氛围灯",
        PropertyList(), [](const PropertyList &properties) -> ReturnValue {
          return EarthLightLED::GetInstance().OutsideLight_get_onoff();
        });

    McpServer::GetInstance().AddTool(
        "self.earth_lamp.earth_light_onoff",
        "打开地球灯（打开内灯），用户使用以下任意命令时打开：打开地球灯、打开球"
        "体灯、开球体灯、开启地球灯；关闭地球灯（关闭内灯），用户使用以下任意命"
        "令时关闭：关闭地球灯、关闭球体灯、关掉地球灯、关掉球体灯。参数onoff为t"
        "rue时"
        "打开，false时关闭",
        PropertyList({Property("onoff", kPropertyTypeBoolean)}),
        [](const PropertyList &properties) -> ReturnValue {
          auto onoff = properties["onoff"].value<bool>();
          if (onoff) {
            EarthLightLED::GetInstance().InsideLight_enable(true);
            EarthLightLED::GetInstance().InsideLight_on();
          } else {
            EarthLightLED::GetInstance().InsideLight_enable(false);
            EarthLightLED::GetInstance().InsideLight_off();
          }
#ifdef REMEMBER_LIGHT
          EarthLightLED::GetInstance().SaveLightState();
#endif
          return true;
        });

    McpServer::GetInstance().AddTool(
        "self.earth_lamp.outside_light_onoff",
        "打开外灯（打开外框灯，打开氛围灯），用户使用以下任意命"
        "令时打开：打开外灯、打开氛围灯、开启外灯、开启氛围灯；关闭外灯（关闭外"
        "框灯，关闭氛围灯），用户使用以下任意命令时关闭：关闭外"
        "灯、关闭氛围灯、关掉外灯、关掉氛围灯。参数onoff为true时打开，false时关"
        "闭",
        PropertyList({Property("onoff", kPropertyTypeBoolean)}),
        [](const PropertyList &properties) -> ReturnValue {
          auto onoff = properties["onoff"].value<bool>();
          if (onoff) {
            EarthLightLED::GetInstance().OutsideLight_enable(true);
            EarthLightLED::GetInstance().OutsideLight_on();
          } else {
            EarthLightLED::GetInstance().OutsideLight_enable(false);
            EarthLightLED::GetInstance().OutsideLight_off();
          }
#ifdef REMEMBER_LIGHT
          EarthLightLED::GetInstance().SaveLightState();
#endif
          return true;
        });
  }

  virtual ~EarthLightLED() {}

  uint8_t Get_EarthLight_Mode();
  std::string Get_EarthLight_Mode_string();
  void InsideLight_off();
  void OutsideLight_off();
  void InsideLight_on();
  void OutsideLight_on();
  bool InsideLight_get_onoff();
  bool OutsideLight_get_onoff();
  void InsideLight_enable(bool enable);
  void OutsideLight_enable(bool enable);

  void Set_EarthLight_Mode(EarthLightMode mode);
  void EarthLightLED_Breathe(ledc_channel_t channel, uint32_t min_duty,
                             uint32_t max_duty);
  virtual void OnStateChanged();

#ifdef REMEMBER_LIGHT
  void SaveLightState();
  void RestoreLightState();
#endif

private:
  ledc_channel_config_t inside_ledc_ = {0};
  ledc_channel_config_t outside_ledc_ = {0};
  uint8_t light_mode = 0;

  uint8_t has_breathe = 0;

  uint16_t curr_inside_duty_ = 0;
  uint16_t curr_outside_duty_ = 0;

  bool inside_light_enable_ = true;
  bool outside_light_enable_ = true;

  uint32_t breathe_duty_ = 512;
  int8_t breathe_direction_ = -1;
};

#endif // _EARTH_LIGHT_H_