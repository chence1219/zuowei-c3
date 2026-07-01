#pragma once

#include <string>
#include <time.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "WeatherService"
#define WEATHER_UPDATE_INTERVAL_SEC 3600
#define WEATHER_RETRY_INTERVAL_MS (10 * 1000)
#define WEATHER_MAX_RETRY_COUNT 10

struct WeatherData {
  std::string city;
  std::string weather;
  int temperature;
  std::string report_time;

  bool IsValid() const {
    return !weather.empty() && !report_time.empty();
  }
};

struct LunarDate {
  int year;
  int month;
  int day;
  bool is_leap_month;
  std::string zodiac;
  std::string lunar_month_name;
  std::string lunar_day_name;

  LunarDate() : year(0), month(0), day(0), is_leap_month(false) {}

  std::string ToString() const {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%s", lunar_month_name.c_str(), lunar_day_name.c_str());
    return buffer;
  }
};

class WeatherService {
public:
  static WeatherService* GetInstance();

  void Init();

  WeatherData GetWeatherData() const;
  std::string GetCurrentTime() const;
  std::string GetCurrentDate() const;
  std::string GetCurrentWeekday() const;
  LunarDate GetLunarDate() const;
  std::string GetWeather() const;
  std::string GetTemperature() const;

  void OnLockScreenVisible();
  void OnLockScreenHidden();

private:
  WeatherService();
  ~WeatherService() = default;

  WeatherService(const WeatherService&) = delete;
  WeatherService& operator=(const WeatherService&) = delete;

  void StartWeatherUpdateTimer();
  void StopWeatherUpdateTimer();
  static void WeatherUpdateTimerCallback(void* arg);
  void FetchWeatherWithRetry();
  bool FetchWeatherFromAPI();

  void UpdateCurrentTime() const;
  std::string GetWeekdayName(int tm_wday) const;
  LunarDate ConvertToLunar(int year, int month, int day) const;

  int GetLunarMonthDays(int lunar_year, int lunar_month) const;
  int GetLunarYearDays(int lunar_year) const;
  int GetLeapMonth(int lunar_year) const;
  int GetLeapMonthDays(int lunar_year) const;

private:
  static WeatherService* instance_;
  esp_timer_handle_t weather_update_timer_ = nullptr;
  SemaphoreHandle_t weather_data_mutex_ = nullptr;
  WeatherData weather_data_;
  time_t last_update_time_ = 0;
  bool weather_fetched_ = false;

  mutable struct tm cached_timeinfo_ = {};
  mutable time_t cached_time_ = 0;
};
