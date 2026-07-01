#include "weather_service.hpp"
#include <cstring>
#include <esp_log.h>
#include <sys/time.h>
#include "board.h"
#include "network_interface.h"
#include "http.h"
#include "system_info.h"
#include "xinzhi_weather_api.h"

static const unsigned int lunar_info[] = {
  0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
  0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
  0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
  0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
  0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
  0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5d0, 0x14573, 0x052d0, 0x0a9a8, 0x0e950, 0x06aa0,
  0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
  0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b5a0, 0x195a6,
  0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
  0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
  0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
  0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
  0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
  0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
  0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0
};

static const char* lunar_month_names[] = {
  "正月", "二月", "三月", "四月", "五月", "六月",
  "七月", "八月", "九月", "十月", "冬月", "腊月"
};

static const char* lunar_day_names[] = {
  "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
  "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
  "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"
};

static const char* zodiac_names[] = {
  "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"
};

static int xinzhi_http_bridge(const char *url, const char **headers, size_t header_count,
                               char *buf, size_t buf_size, int timeout_ms) {
  auto& board = Board::GetInstance();
  NetworkInterface* network = board.GetNetwork();
  if (network == nullptr) {
    return -1;
  }
  auto http = network->CreateHttp(0);
  if (http == nullptr) {
    return -1;
  }
  http->SetTimeout(timeout_ms);
  for (size_t i = 0; i < header_count; i++) {
    std::string header(headers[i]);
    auto sep = header.find(':');
    if (sep != std::string::npos) {
      http->SetHeader(header.substr(0, sep), header.substr(sep + 1));
    }
  }
  if (!http->Open("GET", url)) {
    return -1;
  }
  if (http->GetStatusCode() != 200) {
    http->Close();
    return -1;
  }
  std::string body = http->ReadAll();
  http->Close();
  size_t len = body.size();
  if (len > buf_size - 1) {
    len = buf_size - 1;
  }
  memcpy(buf, body.c_str(), len);
  buf[len] = '\0';
  return (int)len;
}

WeatherService* WeatherService::instance_ = nullptr;

WeatherService* WeatherService::GetInstance() {
  if (instance_ == nullptr) {
    instance_ = new WeatherService();
  }
  return instance_;
}

WeatherService::WeatherService() = default;

void WeatherService::Init() {
  ESP_LOGI(TAG, "Initializing WeatherService");
  weather_data_mutex_ = xSemaphoreCreateMutex();
  if (weather_data_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create weather data mutex");
    return;
  }
  UpdateCurrentTime();
  xinzhi_weather_init(xinzhi_http_bridge);
  ESP_LOGI(TAG, "WeatherService initialized");
}

void WeatherService::StartWeatherUpdateTimer() {
  if (weather_update_timer_ != nullptr) {
    return;
  }
  esp_timer_create_args_t timer_args = {
    .callback = &WeatherUpdateTimerCallback,
    .arg = this,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "weather_update"
  };
  esp_err_t err = esp_timer_create(&timer_args, &weather_update_timer_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
    return;
  }
  err = esp_timer_start_periodic(weather_update_timer_, WEATHER_UPDATE_INTERVAL_SEC * 1000000ULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
  }
}

void WeatherService::StopWeatherUpdateTimer() {
  if (weather_update_timer_ != nullptr) {
    esp_timer_stop(weather_update_timer_);
    esp_timer_delete(weather_update_timer_);
    weather_update_timer_ = nullptr;
  }
}

void WeatherService::WeatherUpdateTimerCallback(void* arg) {
  auto* service = static_cast<WeatherService*>(arg);
  if (service != nullptr) {
    service->FetchWeatherWithRetry();
  }
}

void WeatherService::OnLockScreenVisible() {
  StartWeatherUpdateTimer();

  if (!weather_fetched_) {
    ESP_LOGI(TAG, "First lock screen, fetching weather");
    weather_fetched_ = true;
    FetchWeatherWithRetry();
    return;
  }

  time_t now;
  time(&now);
  double elapsed = difftime(now, last_update_time_);
  if (elapsed < WEATHER_UPDATE_INTERVAL_SEC) {
    ESP_LOGI(TAG, "Weather fresh (%.0fs ago), skip fetch", elapsed);
    return;
  }

  ESP_LOGI(TAG, "Weather stale (%.0fs ago), fetching", elapsed);
  FetchWeatherWithRetry();
}

void WeatherService::OnLockScreenHidden() {
  StopWeatherUpdateTimer();
}

void WeatherService::FetchWeatherWithRetry() {
  for (int i = 0; i < WEATHER_MAX_RETRY_COUNT; i++) {
    if (FetchWeatherFromAPI()) {
      return;
    }
    if (i + 1 < WEATHER_MAX_RETRY_COUNT) {
      ESP_LOGW(TAG, "Retry in %d ms...", WEATHER_RETRY_INTERVAL_MS);
      vTaskDelay(pdMS_TO_TICKS(WEATHER_RETRY_INTERVAL_MS));
    }
  }
  ESP_LOGE(TAG, "Failed after %d attempts", WEATHER_MAX_RETRY_COUNT);
}

bool WeatherService::FetchWeatherFromAPI() {
  std::string mac = SystemInfo::GetMacAddress();
  xinzhi_weather_data_t xinzhi_data;
  int ret = xinzhi_weather_fetch(mac.c_str(), &xinzhi_data);
  if (ret != 0) {
    ESP_LOGE(TAG, "xinzhi_weather_fetch failed: %d", ret);
    return false;
  }

  if (!xinzhi_data.valid) {
    ESP_LOGE(TAG, "Invalid weather data");
    return false;
  }

  WeatherData new_data;
  new_data.weather = xinzhi_data.weather_text;
  new_data.temperature = xinzhi_data.temperature;
  new_data.city = xinzhi_data.location_name;
  new_data.report_time = xinzhi_data.last_update;

  if (weather_data_mutex_ != nullptr && xSemaphoreTake(weather_data_mutex_, portMAX_DELAY) == pdTRUE) {
    weather_data_ = new_data;
    time(&last_update_time_);
    xSemaphoreGive(weather_data_mutex_);
    ESP_LOGI(TAG, "Weather updated: %s, %d°C, %s", new_data.weather.c_str(),
             new_data.temperature, new_data.report_time.c_str());
  }
  return true;
}

WeatherData WeatherService::GetWeatherData() const {
  WeatherData data;
  if (weather_data_mutex_ != nullptr && xSemaphoreTake(weather_data_mutex_, portMAX_DELAY) == pdTRUE) {
    data = weather_data_;
    xSemaphoreGive(weather_data_mutex_);
  }
  return data;
}

void WeatherService::UpdateCurrentTime() const {
  time(&cached_time_);
  localtime_r(&cached_time_, &cached_timeinfo_);
}

std::string WeatherService::GetCurrentTime() const {
  UpdateCurrentTime();
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", cached_timeinfo_.tm_hour, cached_timeinfo_.tm_min);
  return buf;
}

std::string WeatherService::GetCurrentDate() const {
  UpdateCurrentTime();
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d月%02d日", cached_timeinfo_.tm_mon + 1, cached_timeinfo_.tm_mday);
  return buf;
}

std::string WeatherService::GetCurrentWeekday() const {
  UpdateCurrentTime();
  static const char* kNames[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
  int w = cached_timeinfo_.tm_wday;
  return (w >= 0 && w <= 6) ? kNames[w] : "星期?";
}

LunarDate WeatherService::GetLunarDate() const {
  UpdateCurrentTime();
  return ConvertToLunar(cached_timeinfo_.tm_year + 1900, cached_timeinfo_.tm_mon + 1, cached_timeinfo_.tm_mday);
}

std::string WeatherService::GetWeather() const {
  return GetWeatherData().weather;
}

std::string WeatherService::GetTemperature() const {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d°C", GetWeatherData().temperature);
  return buf;
}

LunarDate WeatherService::ConvertToLunar(int year, int month, int day) const {
  LunarDate lunar;
  lunar.year = year;
  lunar.month = 1;
  lunar.day = day;
  lunar.is_leap_month = false;
  lunar.zodiac = zodiac_names[(year - 4) % 12];

  int offset = year - 1900;
  if (offset < 0 || offset >= (int)(sizeof(lunar_info) / sizeof(lunar_info[0]))) {
    if (lunar.month >= 1 && lunar.month <= 12) {
      lunar.lunar_month_name = lunar_month_names[lunar.month - 1];
    }
    if (lunar.day >= 1 && lunar.day <= 30) {
      lunar.lunar_day_name = lunar_day_names[lunar.day - 1];
    }
    return lunar;
  }

  int days = offset * 365 + (offset - 1) / 4 - (offset - 1) / 100 + (offset - 1) / 400;

  int day_count = day;
  static const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int i = 1; i < month; i++) {
    day_count += days_in_month[i];
  }
  if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
    if (month > 2) {
      day_count++;
    }
  }

  days += day_count;
  days -= 30;

  int lunar_year = 1900;
  int i;
  for (i = 0; i < 201; i++) {
    int year_days = GetLunarYearDays(1900 + i);
    if (days < year_days) {
      break;
    }
    days -= year_days;
  }

  lunar_year += i;
  lunar.year = lunar_year;
  lunar.zodiac = zodiac_names[(lunar_year - 4) % 12];

  int leap_month = GetLeapMonth(lunar_year);
  int lunar_month = 1;
  bool is_leap = false;

  for (int m = 1; m <= 12; m++) {
    int month_days = GetLunarMonthDays(lunar_year, m);

    if (leap_month > 0 && m == leap_month && !is_leap) {
      int leap_days = GetLeapMonthDays(lunar_year);
      if (days < leap_days) {
        lunar_month = m;
        is_leap = true;
        break;
      }
      days -= leap_days;
      is_leap = true;
      m--;
      continue;
    }

    if (days < month_days) {
      lunar_month = m;
      break;
    }

    days -= month_days;
    if (leap_month == m && !is_leap) {
      is_leap = true;
    } else {
      lunar_month++;
    }
  }

  lunar.month = lunar_month;
  lunar.is_leap_month = is_leap;
  lunar.day = days + 1;

  if (lunar.month >= 1 && lunar.month <= 12) {
    int month_idx = lunar.month - 1;
    if (lunar.is_leap_month) {
      if (lunar.month == 1) {
        lunar.lunar_month_name = "闰正月";
      } else if (lunar.month == 11) {
        lunar.lunar_month_name = "闰冬月";
      } else if (lunar.month == 12) {
        lunar.lunar_month_name = "闰腊月";
      } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "闰%s", lunar_month_names[month_idx]);
        lunar.lunar_month_name = buf;
      }
    } else {
      lunar.lunar_month_name = lunar_month_names[month_idx];
    }
  }

  if (lunar.day >= 1 && lunar.day <= 30) {
    lunar.lunar_day_name = lunar_day_names[lunar.day - 1];
  }

  return lunar;
}

int WeatherService::GetLunarMonthDays(int lunar_year, int lunar_month) const {
  int offset = lunar_year - 1900;
  if (offset < 0 || offset >= (int)(sizeof(lunar_info) / sizeof(lunar_info[0]))) {
    return 29;
  }
  return (lunar_info[offset] & (1 << (16 - lunar_month))) ? 30 : 29;
}

int WeatherService::GetLunarYearDays(int lunar_year) const {
  int offset = lunar_year - 1900;
  if (offset < 0 || offset >= (int)(sizeof(lunar_info) / sizeof(lunar_info[0]))) {
    return 354;
  }
  int sum = 348;
  unsigned int info = lunar_info[offset];
  for (int i = 0x8000; i > 0x8; i >>= 1) {
    sum += (info & i) ? 1 : 0;
  }
  return sum + GetLeapMonthDays(lunar_year);
}

int WeatherService::GetLeapMonth(int lunar_year) const {
  int offset = lunar_year - 1900;
  if (offset < 0 || offset >= (int)(sizeof(lunar_info) / sizeof(lunar_info[0]))) {
    return 0;
  }
  return lunar_info[offset] & 0xf;
}

int WeatherService::GetLeapMonthDays(int lunar_year) const {
  int leap_month = GetLeapMonth(lunar_year);
  if (leap_month == 0) {
    return 0;
  }
  int offset = lunar_year - 1900;
  if (offset < 0 || offset >= (int)(sizeof(lunar_info) / sizeof(lunar_info[0]))) {
    return 29;
  }
  return (lunar_info[offset] & 0x10000) ? 30 : 29;
}
