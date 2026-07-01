#pragma once

// mcp_alarm.hpp
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mcp_server.h"
#include <atomic>
#include <inttypes.h>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <time.h>
#include <utility>
#include <vector>

#ifndef USE_NVS_ALARM_STORAGE
#define USE_NVS_ALARM_STORAGE 1 // 设置为 0 禁用 NVS 存储，1 启用
#endif
#if USE_NVS_ALARM_STORAGE
#include "settings.h"
#endif

#define TAG "McpAlarm"
#define MCP_ALARM_MAX_COUNT 10

enum class AlarmRepeatMode {
  ONCE,
  DAILY,
  MONTHLY_DAY,
  SPECIFIC_DATE,
  WEEKLY,
  MULTI_WEEKLY,
  YEARLY
};

struct Alarm {
  std::string name;
  time_t time; // 使用 time_t 支持长时间范围
  AlarmRepeatMode repeat_mode = AlarmRepeatMode::ONCE;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int day_of_month = 1;
  int year = 0;
  int month = 1;
  int day = 1;
  std::vector<int> weekdays;
};

class AlarmManager {
public:
  AlarmManager() {
    ESP_LOGI(TAG, "AlarmManager init");
    ring_flag = false;
    running_flag = false;

    // 创建精确触发定时器
    esp_timer_create_args_t timer_args = {
        .callback =
            [](void *arg) {
              AlarmManager *alarm_manager = (AlarmManager *)arg;
              alarm_manager->OnAlarm(); // 闹钟响了
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_timer"};
    esp_timer_create(&timer_args, &timer_);

    // 创建时间同步检查定时器
    esp_timer_create_args_t sync_check_args = {
        .callback =
            [](void *arg) {
              AlarmManager *alarm_manager = (AlarmManager *)arg;
              alarm_manager->CheckTimeSync(); // 检查时间同步
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sync_check_timer"};
    esp_timer_create(&sync_check_args, &sync_check_timer_);

    time_t now = time(NULL);
    last_checked_time_ = now;
#if USE_NVS_ALARM_STORAGE
    LoadAlarms(); // 从 NVS 加载闹钟（内部会判断时间是否同步）
#endif
    
    // 只有在时间已同步的情况下才清除过期闹钟
    if (now >= 1000000000) {  // 时间大于2001-09-09才认为已同步
      ClearOverdueAlarm(now);
    }

    // 启动时间同步检查定时器（每10秒检查一次）
    esp_timer_start_periodic(sync_check_timer_, 10000000); // 10秒 = 10,000,000 微秒
    ESP_LOGI(TAG, "Started time sync check timer (10s interval)");
  }

  ~AlarmManager() {
    if (timer_ != nullptr) {
      esp_timer_stop(timer_);
      esp_timer_delete(timer_);
    }
    if (sync_check_timer_ != nullptr) {
      esp_timer_stop(sync_check_timer_);
      esp_timer_delete(sync_check_timer_);
    }
  }

  // 设置闹钟
  void SetAlarm(int seconde_from_now, std::string alarm_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (alarms_.size() >= MCP_ALARM_MAX_COUNT) {
      ESP_LOGE(TAG, "Too many alarms (max %d)", MCP_ALARM_MAX_COUNT);
      return;
    }
    if (seconde_from_now <= 0) {
      ESP_LOGE(TAG, "Invalid alarm time");
      return;
    }

    Alarm alarm;
    alarm.name = alarm_name;
    time_t now = time(NULL);
    if (now > std::numeric_limits<time_t>::max() - seconde_from_now) {
      ESP_LOGE(TAG, "Alarm time overflow");
      return;
    }
    alarm.time = now + seconde_from_now;
    alarms_.push_back(alarm);

    Alarm *alarm_first = GetProximateAlarm(now);
    if (alarm_first != nullptr) {
      ESP_LOGI(TAG, "Alarm %s set at %"PRId64", now first %"PRId64, alarm.name.c_str(),
               (int64_t)alarm.time, (int64_t)alarm_first->time);
    }
    RearmNextAlarmLocked(now);
#if USE_NVS_ALARM_STORAGE
    SaveAlarms(); // 保存闹钟到 NVS
#endif
  }

  // 设置增强闹钟（支持重复模式）
  bool SetAlarm(const Alarm &alarm_template, Alarm *created_alarm = nullptr,
                std::string *error_reason = nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (alarms_.size() >= MCP_ALARM_MAX_COUNT) {
      ESP_LOGE(TAG, "Too many alarms (max %d)", MCP_ALARM_MAX_COUNT);
      if (error_reason) {
        *error_reason = "闹钟数量已达上限（最多 " +
                        std::to_string(MCP_ALARM_MAX_COUNT) + " 个）";
      }
      return false;
    }

    Alarm alarm = alarm_template;
    time_t now = time(NULL);
    alarm.time = CalculateNextTriggerTime(alarm, now);

    if ((alarm.repeat_mode == AlarmRepeatMode::ONCE ||
         alarm.repeat_mode == AlarmRepeatMode::SPECIFIC_DATE) &&
        alarm.time <= now) {
      ESP_LOGW(TAG, "One-time alarm %s time already passed, rejecting",
               alarm.name.c_str());
      if (error_reason) {
        *error_reason = "一次性闹钟时间已过";
      }
      return false;
    }

    alarms_.push_back(alarm);

    Alarm *alarm_first = GetProximateAlarm(now);
    if (alarm_first == nullptr) {
      ESP_LOGE(TAG, "No valid alarm found after adding");
      if (error_reason) {
        *error_reason = "未找到可触发的闹钟";
      }
      return false;
    }

    ESP_LOGI(TAG, "Alarm %s set at %" PRId64 ", mode %d, now first %" PRId64,
             alarm.name.c_str(), (int64_t)alarm.time,
             static_cast<int>(alarm.repeat_mode), (int64_t)alarm_first->time);
    {
      time_t trigger_time = alarm.time;
      struct tm *tm_info = localtime(&trigger_time);
      std::ostringstream weekdays_text;
      for (size_t i = 0; i < alarm.weekdays.size(); ++i) {
        weekdays_text << alarm.weekdays[i];
        if (i + 1 < alarm.weekdays.size()) {
          weekdays_text << ",";
        }
      }
      if (tm_info != nullptr) {
        ESP_LOGI(TAG,
                 "Alarm detail: name=%s, repeat=%d, trigger=%04d-%02d-%02d "
                 "%02d:%02d:%02d, "
                 "config{hour=%d,minute=%d,second=%d,day_of_month=%d,year=%d,"
                 "month=%d,day=%d,weekdays=[%s]}",
                 alarm.name.c_str(), static_cast<int>(alarm.repeat_mode),
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, alarm.hour,
                 alarm.minute, alarm.second, alarm.day_of_month, alarm.year,
                 alarm.month, alarm.day, weekdays_text.str().c_str());
      } else {
        ESP_LOGI(TAG,
                 "Alarm detail: name=%s, repeat=%d, trigger_ts=%" PRId64 ", "
                 "config{hour=%d,minute=%d,second=%d,day_of_month=%d,year=%d,"
                 "month=%d,day=%d,weekdays=[%s]}",
                 alarm.name.c_str(), static_cast<int>(alarm.repeat_mode),
                 (int64_t)alarm.time, alarm.hour, alarm.minute, alarm.second,
                 alarm.day_of_month, alarm.year, alarm.month, alarm.day,
                 weekdays_text.str().c_str());
      }
    }
    RearmNextAlarmLocked(now);
#if USE_NVS_ALARM_STORAGE
    SaveAlarms(); // 保存闹钟到 NVS
#endif
    if (created_alarm) {
      *created_alarm = alarm;
    }

    return true;
  }

  // 获取闹钟列表状态
  std::string GetAlarmsStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (alarms_.empty()) {
      return "{\"count\":0,\"alarms\":[]}";
    }

    time_t now = time(NULL);
    auto escape_json = [](const std::string &s) -> std::string {
      std::string out;
      out.reserve(s.size() + 8);
      for (char c : s) {
        switch (c) {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          out += c;
          break;
        }
      }
      return out;
    };

    std::ostringstream json;
    json << "{\"count\":" << alarms_.size() << ",\"alarms\":[";

    for (size_t i = 0; i < alarms_.size(); ++i) {
      const Alarm &alarm = alarms_[i];
      time_t alarm_time = static_cast<time_t>(alarm.time);
      struct tm *timeinfo = localtime(&alarm_time);

      json << "{" << "\"index\":" << i << "," << "\"name\":\""
           << escape_json(alarm.name) << "\","
           << "\"trigger_time\":{\"timestamp\":" << alarm.time << ","
           << "\"year\":" << (timeinfo->tm_year + 1900) << ","
           << "\"month\":" << (timeinfo->tm_mon + 1) << ","
           << "\"day\":" << timeinfo->tm_mday << ","
           << "\"hour\":" << timeinfo->tm_hour << ","
           << "\"minute\":" << timeinfo->tm_min << ","
           << "\"second\":" << timeinfo->tm_sec << ","
           << "\"weekday\":" << timeinfo->tm_wday << "},";

      int64_t seconds_until = (int64_t)(alarm.time - now);
      if (seconds_until > 0) {
        int64_t days = seconds_until / 86400;
        int64_t hours = (seconds_until % 86400) / 3600;
        int64_t minutes = (seconds_until % 3600) / 60;
        int64_t secs = seconds_until % 60;
        json << "\"time_remaining\":{\"seconds\":" << seconds_until << ","
             << "\"days\":" << days << "," << "\"hours\":" << hours << ","
             << "\"minutes\":" << minutes << "," << "\"seconds\":" << secs
             << "}," << "\"status\":\"pending\"";
      } else {
        json << "\"status\":\"expired\"";
      }

      json << ",\"repeat_mode\":\"";
      switch (alarm.repeat_mode) {
      case AlarmRepeatMode::ONCE:
        json << "once";
        break;
      case AlarmRepeatMode::DAILY:
        json << "daily";
        break;
      case AlarmRepeatMode::MONTHLY_DAY:
        json << "monthly_day";
        break;
      case AlarmRepeatMode::SPECIFIC_DATE:
        json << "specific_date";
        break;
      case AlarmRepeatMode::WEEKLY:
        json << "weekly";
        break;
      case AlarmRepeatMode::MULTI_WEEKLY:
        json << "multi_weekly";
        break;
      case AlarmRepeatMode::YEARLY:
        json << "yearly";
        break;
      }
      json << "\"";

      json << ",\"config\":{";
      json << "\"time\":\"" << (alarm.hour < 10 ? "0" : "") << alarm.hour << ":"
           << (alarm.minute < 10 ? "0" : "") << alarm.minute << ":"
           << (alarm.second < 10 ? "0" : "") << alarm.second << "\"";

      if (alarm.repeat_mode == AlarmRepeatMode::YEARLY) {
        json << ",\"month\":" << alarm.month << ",\"day\":" << alarm.day;
      }

      if (!alarm.weekdays.empty()) {
        json << ",\"weekdays\":[";
        for (size_t j = 0; j < alarm.weekdays.size(); ++j) {
          json << alarm.weekdays[j];
          if (j < alarm.weekdays.size() - 1)
            json << ",";
        }
        json << "]";
      }

      json << "}}";
      if (i < alarms_.size() - 1)
        json << ",";
    }

    json << "]}";
    return json.str();
  }

  void ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    alarms_.clear();
    StopActiveTimerLocked();
#if USE_NVS_ALARM_STORAGE
    SaveAlarms(); // 保存闹钟到 NVS
#endif
    ESP_LOGI(TAG, "All alarms cleared");
  }

  // 删除指定名称的闹钟
  bool DeleteAlarm(std::string alarm_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool found = false;
    bool was_next_alarm = false;
    time_t now = time(NULL);

    // 检查要删除的闹钟是否是下一个要触发的
    Alarm *next_alarm = GetProximateAlarm(now);
    if (next_alarm != nullptr && next_alarm->name == alarm_name) {
      was_next_alarm = true;
    }

    // 查找并删除闹钟
    for (auto it = alarms_.begin(); it != alarms_.end();) {
      if (it->name == alarm_name) {
        it = alarms_.erase(it);
        found = true;
        ESP_LOGI(TAG, "Alarm %s deleted", alarm_name.c_str());
        break; // 只删除第一个匹配的闹钟
      } else {
        it++;
      }
    }

    if (!found) {
      ESP_LOGW(TAG, "Alarm %s not found", alarm_name.c_str());
      return false;
    }

    if (was_next_alarm) {
      RearmNextAlarmLocked(now);
    }
#if USE_NVS_ALARM_STORAGE
    SaveAlarms(); // 保存闹钟到 NVS
#endif

    return true;
  }

  void
  OnAlarm(std::function<void(int time, const std::string &name)> callback) {
    callback_ = callback;
  }

  // 在系统时间同步/跳变后调用，避免因 wall clock 变化导致漏触发
  void NotifyTimeChanged() {
    std::lock_guard<std::mutex> lock(mutex_);
    time_t now = time(NULL);
    ESP_LOGI(TAG, "Time changed notification received, current time: %" PRId64, (int64_t)now);
    RearmNextAlarmLocked(now);
  }

  // 检查时间是否发生大幅跳变（用于检测NTP同步）
  void CheckTimeSync() {
    time_t now = time(NULL);
    time_t time_diff = now - last_checked_time_;

    // 检测时间大幅向前跳变（超过10秒，说明是从NVS或NTP同步了时间）
    // 正常情况下10秒内时间只应该前进10秒左右，如果超过太多说明发生了跳变
    bool is_big_jump = false;
    
    if (time_diff > 10) {
      // 时间向前跳变超过10秒
      is_big_jump = true;
      ESP_LOGI(TAG, "Detected time jump forward: %lld seconds (expected ~10), resyncing alarms",
               (long long)time_diff);
    }

    if (is_big_jump) {
      // 时间发生大幅跳变，需要重新计算闹钟
      ESP_LOGI(TAG, "Time sync detected, recalculating all alarm trigger times");
      
      {
        std::lock_guard<std::mutex> lock(mutex_);
        ESP_LOGI(TAG, "Acquired lock for time sync, updating %d alarms", (int)alarms_.size());
        
        // 重新计算所有闹钟的触发时间
        for (auto &alarm : alarms_) {
          time_t old_time = alarm.time;
          time_t new_time = CalculateNextTriggerTime(alarm, now);
          
          ESP_LOGI(TAG, "Alarm '%s': old=%lld, new=%lld, now=%lld",
                   alarm.name.c_str(), (long long)old_time, (long long)new_time, (long long)now);
          
          // 只更新未来的闹钟，保留已经过去但会重复的闹钟
          if (new_time > now) {
            alarm.time = new_time;
            ESP_LOGI(TAG, "Updated alarm '%s' trigger time: %lld -> %lld",
                     alarm.name.c_str(), (long long)old_time, (long long)new_time);
          } else {
            ESP_LOGW(TAG, "Alarm '%s' new_time <= now, keeping old time", alarm.name.c_str());
          }
        }
        
        ESP_LOGI(TAG, "Clearing overdue alarms...");
        // 重新清除过期闹钟并设置下一次闹钟
        ESP_LOGI(TAG, "Clearing overdue alarms...");
        // 重新清除过期闹钟并设置下一次闹钟
        ClearOverdueAlarmLocked(now);
        ESP_LOGI(TAG, "Setting next alarm...");
        RearmNextAlarmLocked(now);
      }
    }

    last_checked_time_ = now;
  }

private:
  // 清除过时的闹钟
  void ClearOverdueAlarm(time_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    ClearOverdueAlarmLocked(now);
  }

  // 清除过时的闹钟（假设已经持有锁）
  void ClearOverdueAlarmLocked(time_t now) {
    int erased_count = 0;
    for (auto it = alarms_.begin(); it != alarms_.end();) {
      if (it->time <= now) {
        ESP_LOGI(TAG, "Erasing overdue alarm '%s' (time=%lld, now=%lld)",
                 it->name.c_str(), (long long)it->time, (long long)now);
        it = alarms_.erase(it); // 删除过期的闹钟, 此时it指向下一个元素
        erased_count++;
      } else {
        it++;
      }
    }
    ESP_LOGI(TAG, "Cleared %d overdue alarms, remaining %d alarms", erased_count, (int)alarms_.size());
  }

  bool HasDueAlarmLocked(time_t now) {
    for (const auto &alarm : alarms_) {
      if (alarm.time <= now + kDueGraceSeconds) {
        return true;
      }
    }
    return false;
  }

  // 获取从现在开始第一个响的闹钟
  Alarm *GetProximateAlarm(time_t now) {
    Alarm *current_alarm_ = nullptr;
    for (auto &alarm : alarms_) {
      if (alarm.time >= now &&
          (current_alarm_ == nullptr || alarm.time < current_alarm_->time)) {
        current_alarm_ = &alarm; // 获取当前时间以后第一个发生的时钟句柄
      }
    }
    return current_alarm_;
  }

  void StopActiveTimerLocked() {
    if (timer_ != nullptr && running_flag) {
      ESP_LOGI(TAG, "Stopping active timer...");
      esp_timer_stop(timer_);
      running_flag = false;
      ESP_LOGI(TAG, "Timer stopped");
    }
  }

  void RearmNextAlarmLocked(time_t now) {
    ESP_LOGI(TAG, "RearmNextAlarmLocked: now=%lld", (long long)now);
    StopActiveTimerLocked();

    if (HasDueAlarmLocked(now)) {
      ESP_LOGI(TAG, "due alarm exists, trigger immediately");
      esp_timer_start_once(timer_, 1000);
      running_flag = true;
      ESP_LOGI(TAG, "Timer armed for immediate trigger (1ms)");
      return;
    }

    Alarm *next_alarm = GetProximateAlarm(now);
    if (next_alarm == nullptr) {
      ESP_LOGI(TAG, "no alarm now");
      return;
    }

    int64_t seconds_until = (int64_t)(next_alarm->time - now);
    int64_t delay_us = seconds_until <= 0 ? 1000 : seconds_until * 1000000;
    ESP_LOGI(TAG, "rearm next alarm '%s' in %lld seconds (delay_us=%lld)", 
             next_alarm->name.c_str(), (long long)seconds_until, (long long)delay_us);
    esp_timer_start_once(timer_, delay_us);
    running_flag = true;
    ESP_LOGI(TAG, "Timer armed successfully");
  }

  // 计算下一个触发时间
  time_t CalculateNextTriggerTime(const Alarm &alarm, time_t now) {
    struct tm *timeinfo = localtime(&now);
    struct tm next_time = *timeinfo;
    next_time.tm_sec = alarm.second;
    next_time.tm_min = alarm.minute;
    next_time.tm_hour = alarm.hour;

    switch (alarm.repeat_mode) {
    case AlarmRepeatMode::ONCE:
      // 使用指定的日期
      next_time.tm_year = alarm.year - 1900;
      next_time.tm_mon = alarm.month - 1;
      next_time.tm_mday = alarm.day;
      next_time.tm_hour = alarm.hour;
      next_time.tm_min = alarm.minute;
      next_time.tm_sec = alarm.second;
      break;

    case AlarmRepeatMode::DAILY: {
      // 计算今天的目标时间
      time_t today_target = mktime(&next_time);
      // 如果今天的这个时间已经过了，设置为明天
      if (today_target <= now) {
        next_time.tm_mday += 1;
      }
      break;
    }

    case AlarmRepeatMode::MONTHLY_DAY:
      // 下个月指定日期
      next_time.tm_mon += 1;
      next_time.tm_mday = alarm.day_of_month;
      break;

    case AlarmRepeatMode::SPECIFIC_DATE:
      // 指定日期
      next_time.tm_year = alarm.year - 1900;
      next_time.tm_mon = alarm.month - 1;
      next_time.tm_mday = alarm.day;
      break;

    case AlarmRepeatMode::WEEKLY: {
      // 每周指定星期几
      int target_weekday = alarm.weekdays.empty() ? 0 : alarm.weekdays[0];
      int days_until_target = (target_weekday - timeinfo->tm_wday + 7) % 7;
      if (days_until_target == 0) {
        days_until_target = 7; // 如果今天就是目标星期，则等到下周
      }
      next_time.tm_mday += days_until_target;
      break;
    }

    case AlarmRepeatMode::MULTI_WEEKLY: {
      // 多个星期几
      int min_days = 7;
      for (int wd : alarm.weekdays) {
        int days_until = (wd - timeinfo->tm_wday + 7) % 7;
        if (days_until == 0) {
          days_until = 7;
        }
        if (days_until < min_days) {
          min_days = days_until;
        }
      }
      next_time.tm_mday += min_days;
      break;
    }

    case AlarmRepeatMode::YEARLY: {
      // 每年指定月日
      next_time.tm_mon = alarm.month - 1;
      next_time.tm_mday = alarm.day;
      next_time.tm_hour = alarm.hour;
      next_time.tm_min = alarm.minute;
      next_time.tm_sec = alarm.second;

      // 检查今年的这个日期是否已经过了
      time_t this_year = mktime(&next_time);
      if (this_year <= now) {
        // 如果今年的已经过了，设置为明年
        next_time.tm_year += 1;
      }
      break;
    }
    }

    time_t result = mktime(&next_time);

    return result;
  }

  // 闹钟响了的处理函数
  void OnAlarm() {
    ring_flag = true;
    std::vector<std::pair<time_t, std::string>> due_alarms;
    time_t now = time(NULL);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto it = alarms_.begin(); it != alarms_.end();) {
        if (it->time <= now + kDueGraceSeconds) {
          due_alarms.emplace_back(it->time, it->name);
          if (it->repeat_mode == AlarmRepeatMode::ONCE ||
              it->repeat_mode == AlarmRepeatMode::SPECIFIC_DATE) {
            it = alarms_.erase(it);
            continue;
          }

          time_t next = CalculateNextTriggerTime(*it, now);
          while (next <= now) {
            next = CalculateNextTriggerTime(*it, now + 1);
          }
          it->time = next;
          ESP_LOGI(TAG, "Reschedule recurring alarm to %" PRId64,
                   (int64_t)it->time);
        }
        ++it;
      }

      RearmNextAlarmLocked(now);
    }

    if (due_alarms.empty()) {
      ESP_LOGW(TAG, "OnAlarm triggered but no due alarms found, skipping");
      return;
    }

    if (!callback_) {
      ESP_LOGW(TAG, "OnAlarm callback not set, skip %d due alarms",
               (int)due_alarms.size());
      return;
    }

    for (const auto &item : due_alarms) {
      int cb_time = 0;
      if (item.first > std::numeric_limits<int>::max()) {
        cb_time = std::numeric_limits<int>::max();
        ESP_LOGW(TAG, "alarm timestamp over int range, clamped");
      } else if (item.first < std::numeric_limits<int>::min()) {
        cb_time = std::numeric_limits<int>::min();
        ESP_LOGW(TAG, "alarm timestamp under int range, clamped");
      } else {
        cb_time = (int)item.first;
      }
      callback_(cb_time, item.second);
    }
  }

  // 闹钟是不是响了的标志位
  bool IsRing() { return ring_flag; };
  // 清除闹钟标志位
  void ClearRing() {
    ESP_LOGI("Alarm", "clear");
    ring_flag = false;
  };

  std::vector<Alarm> alarms_; // 闹钟列表
  std::mutex mutex_;          // 互斥锁
  esp_timer_handle_t timer_; // 精确触发定时器（用于短时间定时）
  esp_timer_handle_t sync_check_timer_; // 时间同步检查定时器
  time_t last_checked_time_; // 上次检查的时间

  std::atomic<bool> ring_flag{false};
  std::atomic<bool> running_flag{false};
  static constexpr time_t kDueGraceSeconds = 2;

  std::function<void(int time, const std::string &name)> callback_;

#if USE_NVS_ALARM_STORAGE
  // 保存闹钟列表到 NVS（紧凑二进制格式）
  void SaveAlarms() {
    if (alarms_.empty()) {
      Settings settings("mcp_alarm", true);
      settings.EraseKey("alarms");
      ESP_LOGI(TAG, "No alarms to save, erased NVS key");
      return;
    }

    // 计算所需缓冲区大小
    size_t total_size = 1; // count (1 byte)
    for (const auto &alarm : alarms_) {
      total_size += 1 + alarm.name.size();     // name_len + name
      total_size += 8;                         // time (time_t)
      total_size += 1;                         // repeat_mode
      total_size += 1;                         // hour
      total_size += 1;                         // minute
      total_size += 1;                         // second
      total_size += 1;                         // day_of_month
      total_size += 2;                         // year
      total_size += 1;                         // month
      total_size += 1;                         // day
      total_size += 1 + alarm.weekdays.size(); // weekdays_count + weekdays
    }

    // 分配缓冲区
    std::vector<uint8_t> buffer(total_size);
    size_t pos = 0;

    // 写入闹钟数量
    buffer[pos++] = static_cast<uint8_t>(alarms_.size());

    // 写入每个闹钟
    for (const auto &alarm : alarms_) {
      // name_len + name
      buffer[pos++] =
          static_cast<uint8_t>(std::min(alarm.name.size(), size_t(255)));
      for (size_t i = 0;
           i < std::min(alarm.name.size(), size_t(255)) && i < 255; i++) {
        buffer[pos++] = static_cast<uint8_t>(alarm.name[i]);
      }

      // time (8 bytes, little-endian)
      time_t t = alarm.time;
      for (int i = 0; i < 8; i++) {
        buffer[pos++] = static_cast<uint8_t>((t >> (i * 8)) & 0xFF);
      }

      // repeat_mode
      buffer[pos++] = static_cast<uint8_t>(alarm.repeat_mode);
      // hour
      buffer[pos++] = static_cast<uint8_t>(alarm.hour);
      // minute
      buffer[pos++] = static_cast<uint8_t>(alarm.minute);
      // second
      buffer[pos++] = static_cast<uint8_t>(alarm.second);
      // day_of_month
      buffer[pos++] = static_cast<uint8_t>(alarm.day_of_month);
      // year (2 bytes, little-endian)
      buffer[pos++] = static_cast<uint8_t>(alarm.year & 0xFF);
      buffer[pos++] = static_cast<uint8_t>((alarm.year >> 8) & 0xFF);
      // month
      buffer[pos++] = static_cast<uint8_t>(alarm.month);
      // day
      buffer[pos++] = static_cast<uint8_t>(alarm.day);
      // weekdays_count + weekdays
      buffer[pos++] =
          static_cast<uint8_t>(std::min(alarm.weekdays.size(), size_t(255)));
      for (size_t i = 0; i < std::min(alarm.weekdays.size(), size_t(255));
           i++) {
        buffer[pos++] = static_cast<uint8_t>(alarm.weekdays[i]);
      }
    }

    // 转换为 base64 以便安全存储（避免二进制数据问题）
    std::string encoded = Base64Encode(buffer.data(), buffer.size());
    Settings settings("mcp_alarm", true);
    settings.SetString("alarms", encoded);

    ESP_LOGI(TAG, "Saved %" PRIuPTR " alarms to NVS (%" PRIuPTR " bytes, encoded %" PRIuPTR " chars)",
             alarms_.size(), total_size, encoded.size());
  }

  // 从 NVS 加载闹钟列表（紧凑二进制格式）
  void LoadAlarms() {
    Settings settings("mcp_alarm", false);
    std::string encoded = settings.GetString("alarms", "");

    if (encoded.empty()) {
      ESP_LOGI(TAG, "No alarms found in NVS");
      return;
    }

    // Base64 解码
    std::vector<uint8_t> buffer = Base64Decode(encoded);
    if (buffer.empty()) {
      ESP_LOGE(TAG, "Failed to decode alarm data from NVS");
      return;
    }

    alarms_.clear();
    size_t pos = 0;

    // 读取闹钟数量
    if (pos >= buffer.size()) {
      ESP_LOGE(TAG, "Invalid alarm data: missing count");
      return;
    }
    uint8_t count = buffer[pos++];

    // 读取每个闹钟
    for (uint8_t i = 0; i < count && pos < buffer.size(); i++) {
      Alarm alarm;

      // 读取 name_len + name
      if (pos >= buffer.size())
        break;
      uint8_t name_len = buffer[pos++];
      alarm.name.clear();
      for (uint8_t j = 0; j < name_len && pos < buffer.size(); j++) {
        alarm.name += static_cast<char>(buffer[pos++]);
      }

      // 读取 time (8 bytes)
      if (pos + 8 > buffer.size())
        break;
      time_t t = 0;
      for (int j = 0; j < 8; j++) {
        t |= static_cast<time_t>(buffer[pos++]) << (j * 8);
      }
      alarm.time = t;

      // 读取 repeat_mode
      if (pos >= buffer.size())
        break;
      alarm.repeat_mode = static_cast<AlarmRepeatMode>(buffer[pos++]);

      // 读取 hour
      if (pos >= buffer.size())
        break;
      alarm.hour = buffer[pos++];

      // 读取 minute
      if (pos >= buffer.size())
        break;
      alarm.minute = buffer[pos++];

      // 读取 second
      if (pos >= buffer.size())
        break;
      alarm.second = buffer[pos++];

      // 读取 day_of_month
      if (pos >= buffer.size())
        break;
      alarm.day_of_month = buffer[pos++];

      // 读取 year (2 bytes)
      if (pos + 2 > buffer.size())
        break;
      alarm.year = buffer[pos] | (buffer[pos + 1] << 8);
      pos += 2;

      // 读取 month
      if (pos >= buffer.size())
        break;
      alarm.month = buffer[pos++];

      // 读取 day
      if (pos >= buffer.size())
        break;
      alarm.day = buffer[pos++];

      // 读取 weekdays_count + weekdays
      if (pos >= buffer.size())
        break;
      uint8_t weekdays_count = buffer[pos++];
      alarm.weekdays.clear();
      for (uint8_t j = 0; j < weekdays_count && pos < buffer.size(); j++) {
        alarm.weekdays.push_back(buffer[pos++]);
      }

      alarms_.push_back(alarm);
    }

    ESP_LOGI(TAG, "Loaded %" PRIuPTR " alarms from NVS", alarms_.size());
    time_t now = time(NULL);
    
    // 检测是否时间是1970年附近（时间未同步）
    bool is_time_unsynced = (now < 1000000000); // 小于2001-09-09
    
    if (is_time_unsynced) {
      ESP_LOGW(TAG, "Time not synchronized yet (current: %" PRId64 "), skipping alarm calculation",
               (int64_t)now);
      ESP_LOGI(TAG, "Alarms will be recalculated after time sync");
      // 时间未同步时，不清除过期闹钟，也不重新设置定时器
      // 等待时间同步后由CheckTimeSync自动处理
      return;
    }
    
    // 时间已同步，正常处理闹钟
    ClearOverdueAlarm(now);
    RearmNextAlarmLocked(now);
  }

  // Base64 编码
  static std::string Base64Encode(const uint8_t *data, size_t len) {
    static const char *encode_table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
      uint32_t triple = (data[i] << 16) | (i + 1 < len ? data[i + 1] << 8 : 0) |
                        (i + 2 < len ? data[i + 2] : 0);

      result.push_back(encode_table[(triple >> 18) & 0x3F]);
      result.push_back(encode_table[(triple >> 12) & 0x3F]);
      result.push_back(encode_table[(triple >> 6) & 0x3F]);
      result.push_back(encode_table[triple & 0x3F]);
    }

    // 添加 padding
    if (len % 3 == 1) {
      result[result.size() - 2] = '=';
      result[result.size() - 1] = '=';
    } else if (len % 3 == 2) {
      result[result.size() - 1] = '=';
    }

    return result;
  }

  // Base64 解码
  static std::vector<uint8_t> Base64Decode(const std::string &encoded) {
    static const int decode_table[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1};

    std::vector<uint8_t> result;
    if (encoded.empty())
      return result;

    // 计算 padding
    size_t padding = 0;
    if (!encoded.empty() && encoded.back() == '=')
      padding++;
    if (encoded.size() > 1 && encoded[encoded.size() - 2] == '=')
      padding++;

    result.reserve((encoded.size() * 3) / 4 - padding);

    uint32_t buffer = 0;
    int bits = 0;

    for (char c : encoded) {
      if (c == '=')
        break;
      int val = decode_table[static_cast<uint8_t>(c)];
      if (val < 0)
        continue;

      buffer = (buffer << 6) | val;
      bits += 6;

      if (bits >= 8) {
        bits -= 8;
        result.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
      }
    }

    return result;
  }
#endif
};

class McpAlarm {
  AlarmManager *alarm_manager;
  std::function<void(int time, const std::string &name)> on_alarm_cb_;
  std::function<void()> on_alarm_stop_cb_;
  esp_timer_handle_t alarm_play_timer = nullptr;
  int alarm_cnt = -1;
  bool alarm_playing = false;

  void PlayAlarmOnceAsync() {
    if (alarm_playing) {
      return;
    }
    alarm_playing = true;
    auto &app = Application::GetInstance();
    app.Schedule([]() {
      auto &app = Application::GetInstance();
      auto codec = Board::GetInstance().GetAudioCodec();
      codec->EnableOutput(true);
      app.PlaySound(Lang::Sounds::OGG_ALARM_RING);
      McpAlarm::GetInstance()->alarm_playing = false;
    });
  }

  void InitAlarm() {
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = [](void *arg) {
      McpAlarm *mcp_alarm = static_cast<McpAlarm *>(arg);
      ESP_LOGW(TAG, "esp_timer_cb: %d", mcp_alarm->alarm_cnt);
      auto &app = Application::GetInstance();
      mcp_alarm->alarm_cnt++;
      if (app.GetDeviceState() == kDeviceStateIdle &&
          mcp_alarm->alarm_cnt <= 13) {
        // 使用非阻塞方式播放声音
        mcp_alarm->PlayAlarmOnceAsync();
      } else {
        mcp_alarm->StopAlarm();
        mcp_alarm->on_alarm_stop_cb_();
      }
    };
    timer_args.arg = this;
    timer_args.name = "alarm_play_timer";

    esp_timer_create(&timer_args, &alarm_play_timer);

    alarm_manager->OnAlarm([this](int time, const std::string &name) {
      ESP_LOGI(TAG, "OnAlarm: %d, alarm_name: %s", time, name.c_str());
      if (on_alarm_cb_) {
        on_alarm_cb_(time, name);
      }

      auto &app = Application::GetInstance();
      app.Close();

      app.Schedule([this, name]() {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateIdle) {
          StartAlarm(name);
        } else {
          app.Schedule([this, name]() {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
              StartAlarm(name);
            }
          });
        }
      });
    });
  }

public:
  McpAlarm() : alarm_playing(false) {}

  ~McpAlarm() {
    if (alarm_play_timer) {
      esp_timer_stop(alarm_play_timer);
      esp_timer_delete(alarm_play_timer);
      on_alarm_stop_cb_();
    }
  }

  static McpAlarm *GetInstance() {
    static McpAlarm instance;
    return &instance;
  }

  void SetOnAlarmCallback(
      std::function<void(int time, const std::string &name)> callback) {
    on_alarm_cb_ = callback;
  }

  void SetOnAlarmStopCallback(std::function<void()> callback) {
    on_alarm_stop_cb_ = callback;
  }

  void Init() {
    alarm_manager = new AlarmManager();
    InitAlarm();

    // McpServer::GetInstance().AddTool(
    //     "self.alarm.set_alarm",
    //     "Sets an alarm. Parameters: seconds_from_now "
    //     "(int), alarm_name (string)",
    //     PropertyList({Property("seconds_from_now", kPropertyTypeInteger),
    //                   Property("alarm_name", kPropertyTypeString)}),
    //     [this](const PropertyList &properties) -> ReturnValue {
    //       int seconds_from_now = properties["seconds_from_now"].value<int>();
    //       std::string alarm_name =
    //           properties["alarm_name"].value<std::string>();
    //       alarm_manager->SetAlarm(seconds_from_now, alarm_name);
    //       return true;
    //     });

    McpServer::GetInstance().AddTool(
        "self.alarm.get_alarms_status",
        "获取当前全部闹钟状态（JSON）。\n返回字段：\n- count：闹钟总数\n- "
        "alarms：闹钟数组，每项包含：\n  - index：闹钟索引（从 0 开始）\n  - "
        "name：闹钟名称\n  - trigger_time：触发时间（时间戳与年月日时分秒）\n  "
        "- time_remaining：剩余时间（仅 pending 状态）\n  - status：pending 或 "
        "expired\n  - repeat_mode：once / daily / monthly_day / specific_date "
        "/ weekly / multi_weekly / yearly\n  - "
        "config：原始配置（如时分秒、weekdays 等）",
        PropertyList(), [this](const PropertyList &properties) -> ReturnValue {
          std::string status = alarm_manager->GetAlarmsStatus();
          ESP_LOGI(TAG, "Current alarms status: %s", status.c_str());
          return status;
        });

    // 添加清除所有闹钟的工具
    McpServer::GetInstance().AddTool(
        "self.alarm.clear_all_alarms", "清空所有闹钟。", PropertyList(),
        [this](const PropertyList &properties) -> ReturnValue {
          alarm_manager->ClearAll();
          ESP_LOGI(TAG, "All alarms cleared via MCP command");
          return true;
        });

    // 添加删除指定闹钟的工具
    McpServer::GetInstance().AddTool(
        "self.alarm.delete_alarm",
        "按名称删除指定闹钟。\n参数：alarm_name（string，必填）",
        PropertyList({Property("alarm_name", kPropertyTypeString)}),
        [this](const PropertyList &properties) -> ReturnValue {
          std::string alarm_name =
              properties["alarm_name"].value<std::string>();
          bool success = alarm_manager->DeleteAlarm(alarm_name);
          if (success) {
            ESP_LOGI(TAG, "Alarm %s deleted via MCP command",
                     alarm_name.c_str());
          } else {
            ESP_LOGW(TAG, "Failed to delete alarm %s (not found)",
                     alarm_name.c_str());
          }
          return success;
        });

    // 添加获取当前日期时间的工具
    McpServer::GetInstance().AddTool(
        "self.alarm.get_current_datetime",
        "获取系统当前日期时间。\n返回字段：year、month、day、hour、minute、seco"
        "nd、weekday（0=周日，6=周六）。",
        PropertyList(), [this](const PropertyList &properties) -> ReturnValue {
          time_t now = time(NULL);
          struct tm *timeinfo = localtime(&now);
          char buffer[200];
          snprintf(buffer, sizeof(buffer),
                   "{\"year\":%d,\"month\":%d,\"day\":%d,\"hour\":%d,"
                   "\"minute\":%d,\"second\":%d,\"weekday\":%d}",
                   timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                   timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
                   timeinfo->tm_sec, timeinfo->tm_wday);
          std::string result(buffer);
          ESP_LOGI(TAG, "Current datetime: %s", result.c_str());
          return result;
        });

    // 添加增强的设置闹钟工具（支持重复模式）
    McpServer::GetInstance().AddTool(
        "self.alarm.set_alarm",
        "设置闹钟（支持重复模式）。\n建议先调用 "
        "self.alarm.get_current_datetime "
        "获取当前时间后再设置。\n\n必填参数：\n- hour：小时，0-23\n- "
        "minute：分钟，0-59\n- second：秒，0-59\n- repeat_mode：重复模式，取值 "
        "one of [once, daily, monthly_day, specific_date, weekly, "
        "multi_weekly, yearly]\n- alarm_name：闹钟名称\n\n按 repeat_mode "
        "的额外必填参数：\n- monthly_day：必须提供 day_of_month（1-31）\n- "
        "specific_date：必须提供 year、month、day（需为真实日期）\n- "
        "weekly：必须提供 weekdays，且只能 1 个值（0-6，0=周日）\n- "
        "multi_weekly：必须提供 weekdays，可多个值（逗号分隔，每个值 0-6）\n- "
        "yearly：必须提供 month、day（需为真实月日）\n\n说明：\n- "
        "once：默认使用“今天 + "
        "指定时分秒”；若今天该时间已过，自动顺延到明天同一时刻\n- "
        "若用户明确给了 year/month/day，则即使 repeat_mode=once "
        "也会按指定日期设置（等价于 specific_date）\n- "
        "若语义是“下个月/下下个月/明年/后年”这种一次性时间点，优先使用 "
        "specific_date，而不是 "
        "monthly_day/yearly\n- "
        "day_of_month/year/month/day 在接口内部默认是 "
        "-1，仅用于占位；实际是否有效由 repeat_mode 校验决定\n\n返回：JSON "
        "字符串。成功时包含 success=true 与 alarm 详情；失败时包含 "
        "success=false 与 message 失败原因。",
        PropertyList(
            {Property("hour", kPropertyTypeInteger),
             Property("minute", kPropertyTypeInteger),
             Property("second", kPropertyTypeInteger),
             Property("repeat_mode", kPropertyTypeString),
             Property("day_of_month", kPropertyTypeInteger, -1),
             Property("year", kPropertyTypeInteger, -1),
             Property("month", kPropertyTypeInteger, -1),
             Property("day", kPropertyTypeInteger, -1),
             Property("weekdays", kPropertyTypeString, std::string("")),
             Property("alarm_name", kPropertyTypeString)}),
        [this](const PropertyList &properties) -> ReturnValue {
          auto escape_json = [](const std::string &s) -> std::string {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
              switch (c) {
              case '\\':
                out += "\\\\";
                break;
              case '\"':
                out += "\\\"";
                break;
              case '\n':
                out += "\\n";
                break;
              case '\r':
                out += "\\r";
                break;
              case '\t':
                out += "\\t";
                break;
              default:
                out += c;
                break;
              }
            }
            return out;
          };
          auto repeat_mode_to_string =
              [](AlarmRepeatMode mode) -> const char * {
            switch (mode) {
            case AlarmRepeatMode::ONCE:
              return "once";
            case AlarmRepeatMode::DAILY:
              return "daily";
            case AlarmRepeatMode::MONTHLY_DAY:
              return "monthly_day";
            case AlarmRepeatMode::SPECIFIC_DATE:
              return "specific_date";
            case AlarmRepeatMode::WEEKLY:
              return "weekly";
            case AlarmRepeatMode::MULTI_WEEKLY:
              return "multi_weekly";
            case AlarmRepeatMode::YEARLY:
              return "yearly";
            }
            return "unknown";
          };
          auto make_result = [&](bool success, const std::string &message,
                                 const Alarm *alarm_info) -> std::string {
            std::ostringstream json;
            json << "{" << "\"success\":" << (success ? "true" : "false") << ","
                 << "\"message\":\"" << escape_json(message) << "\"";
            if (alarm_info != nullptr) {
              time_t alarm_time = alarm_info->time;
              struct tm *tm_info = localtime(&alarm_time);
              json << ",\"alarm\":{" << "\"name\":\""
                   << escape_json(alarm_info->name) << "\","
                   << "\"repeat_mode\":\""
                   << repeat_mode_to_string(alarm_info->repeat_mode) << "\","
                   << "\"trigger_time\":{"
                   << "\"timestamp\":" << (int64_t)alarm_info->time << ","
                   << "\"year\":" << (tm_info->tm_year + 1900) << ","
                   << "\"month\":" << (tm_info->tm_mon + 1) << ","
                   << "\"day\":" << tm_info->tm_mday << ","
                   << "\"hour\":" << tm_info->tm_hour << ","
                   << "\"minute\":" << tm_info->tm_min << ","
                   << "\"second\":" << tm_info->tm_sec << ","
                   << "\"weekday\":" << tm_info->tm_wday << "},"
                   << "\"config\":{" << "\"hour\":" << alarm_info->hour << ","
                   << "\"minute\":" << alarm_info->minute << ","
                   << "\"second\":" << alarm_info->second << ","
                   << "\"day_of_month\":" << alarm_info->day_of_month << ","
                   << "\"year\":" << alarm_info->year << ","
                   << "\"month\":" << alarm_info->month << ","
                   << "\"day\":" << alarm_info->day;
              if (!alarm_info->weekdays.empty()) {
                json << ",\"weekdays\":[";
                for (size_t i = 0; i < alarm_info->weekdays.size(); ++i) {
                  json << alarm_info->weekdays[i];
                  if (i + 1 < alarm_info->weekdays.size()) {
                    json << ",";
                  }
                }
                json << "]";
              }
              json << "}}";
            }
            json << "}";
            return json.str();
          };

          Alarm alarm;
          alarm.hour = properties["hour"].value<int>();
          alarm.minute = properties["minute"].value<int>();
          alarm.second = properties["second"].value<int>();
          alarm.name = properties["alarm_name"].value<std::string>();
          int input_year = properties["year"].value<int>();
          int input_month = properties["month"].value<int>();
          int input_day = properties["day"].value<int>();

          auto is_valid_date = [](int year, int month, int day) -> bool {
            if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) {
              return false;
            }
            struct tm check_tm = {};
            check_tm.tm_year = year - 1900;
            check_tm.tm_mon = month - 1;
            check_tm.tm_mday = day;
            check_tm.tm_hour = 12;
            if (mktime(&check_tm) == (time_t)-1) {
              return false;
            }
            return check_tm.tm_year == year - 1900 &&
                   check_tm.tm_mon == month - 1 && check_tm.tm_mday == day;
          };

          if (alarm.hour < 0 || alarm.hour > 23 || alarm.minute < 0 ||
              alarm.minute > 59 || alarm.second < 0 || alarm.second > 59) {
            ESP_LOGE(TAG, "Invalid time: %02d:%02d:%02d", alarm.hour,
                     alarm.minute, alarm.second);
            return make_result(
                false, "时间参数非法：hour/minute/second 超出范围", nullptr);
          }

          ESP_LOGI(TAG, "[set_alarm] Received parameters:");
          ESP_LOGI(TAG, "  alarm_name: %s", alarm.name.c_str());
          ESP_LOGI(TAG, "  time: %02d:%02d:%02d", alarm.hour, alarm.minute,
                   alarm.second);

          std::string repeat_mode_str =
              properties["repeat_mode"].value<std::string>();
          ESP_LOGI(TAG, "  repeat_mode: %s", repeat_mode_str.c_str());

          if (repeat_mode_str == "once") {
            bool has_any_date =
                (input_year != -1 || input_month != -1 || input_day != -1);
            bool has_full_date =
                (input_year != -1 && input_month != -1 && input_day != -1);
            if (has_any_date && !has_full_date) {
              return make_result(
                  false, "once 模式若提供日期，必须同时提供 year/month/day",
                  nullptr);
            }
            if (has_full_date) {
              if (!is_valid_date(input_year, input_month, input_day)) {
                return make_result(false, "once 模式提供的 year/month/day 非法",
                                   nullptr);
              }
              alarm.repeat_mode = AlarmRepeatMode::SPECIFIC_DATE;
              alarm.year = input_year;
              alarm.month = input_month;
              alarm.day = input_day;
              ESP_LOGI(TAG,
                       "  once with explicit date, use specific_date: "
                       "%04d-%02d-%02d",
                       alarm.year, alarm.month, alarm.day);
            } else {
              alarm.repeat_mode = AlarmRepeatMode::ONCE;
              time_t now = time(NULL);
              struct tm *timeinfo = localtime(&now);
              struct tm trigger_tm = *timeinfo;
              trigger_tm.tm_hour = alarm.hour;
              trigger_tm.tm_min = alarm.minute;
              trigger_tm.tm_sec = alarm.second;
              time_t trigger_time = mktime(&trigger_tm);
              if (trigger_time <= now) {
                trigger_tm.tm_mday += 1;
                trigger_time = mktime(&trigger_tm);
              }
              alarm.year = trigger_tm.tm_year + 1900;
              alarm.month = trigger_tm.tm_mon + 1;
              alarm.day = trigger_tm.tm_mday;
              ESP_LOGI(TAG, "  once date resolved to: %04d-%02d-%02d",
                       alarm.year, alarm.month, alarm.day);
            }
          } else if (repeat_mode_str == "daily") {
            alarm.repeat_mode = AlarmRepeatMode::DAILY;
          } else if (repeat_mode_str == "monthly_day") {
            alarm.repeat_mode = AlarmRepeatMode::MONTHLY_DAY;
            alarm.day_of_month = properties["day_of_month"].value<int>();
            if (alarm.day_of_month < 1 || alarm.day_of_month > 31) {
              ESP_LOGE(TAG, "monthly_day requires day_of_month in [1,31]");
              return make_result(
                  false, "monthly_day 模式要求 day_of_month 在 1-31", nullptr);
            }
            ESP_LOGI(TAG, "  day_of_month: %d", alarm.day_of_month);
          } else if (repeat_mode_str == "specific_date") {
            alarm.repeat_mode = AlarmRepeatMode::SPECIFIC_DATE;
            alarm.year = input_year;
            alarm.month = input_month;
            alarm.day = input_day;
            if (alarm.year < 1970 || alarm.month < 1 || alarm.month > 12 ||
                alarm.day < 1 || alarm.day > 31) {
              ESP_LOGE(TAG, "specific_date requires valid year/month/day");
              return make_result(
                  false, "specific_date 模式要求 year/month/day 合法", nullptr);
            }
            if (!is_valid_date(alarm.year, alarm.month, alarm.day)) {
              ESP_LOGE(TAG, "specific_date is not a real calendar date");
              return make_result(
                  false, "specific_date 日期不存在，请检查年月日", nullptr);
            }
            ESP_LOGI(TAG, "  specific_date: %04d-%02d-%02d", alarm.year,
                     alarm.month, alarm.day);
          } else if (repeat_mode_str == "weekly" ||
                     repeat_mode_str == "multi_weekly") {
            if (repeat_mode_str == "weekly") {
              alarm.repeat_mode = AlarmRepeatMode::WEEKLY;
            } else {
              alarm.repeat_mode = AlarmRepeatMode::MULTI_WEEKLY;
            }
            std::string weekdays_str =
                properties["weekdays"].value<std::string>();
            ESP_LOGI(TAG, "  weekdays (raw): %s", weekdays_str.c_str());
            if (weekdays_str.empty()) {
              ESP_LOGE(TAG, "%s requires weekdays", repeat_mode_str.c_str());
              return make_result(
                  false, repeat_mode_str + " 模式要求提供 weekdays", nullptr);
            }
            std::stringstream ss(weekdays_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
              int weekday = -1;
              try {
                weekday = std::stoi(item);
              } catch (...) {
                ESP_LOGE(TAG, "Invalid weekday token: %s", item.c_str());
                return make_result(
                    false, "weekdays 格式错误，必须是 0-6 的数字列表", nullptr);
              }
              if (weekday < 0 || weekday > 6) {
                ESP_LOGE(TAG, "weekday out of range [0,6]: %d", weekday);
                return make_result(false, "weekdays 超出范围，必须在 0-6",
                                   nullptr);
              }
              alarm.weekdays.push_back(weekday);
              ESP_LOGI(TAG, "    weekday: %d", weekday);
            }
            if (alarm.weekdays.empty()) {
              ESP_LOGE(TAG, "%s requires at least one weekday",
                       repeat_mode_str.c_str());
              return make_result(false,
                                 repeat_mode_str + " 模式至少需要一个 weekday",
                                 nullptr);
            }
            if (repeat_mode_str == "weekly" && alarm.weekdays.size() != 1) {
              ESP_LOGE(TAG, "weekly requires exactly one weekday");
              return make_result(false, "weekly 模式只能提供一个 weekday",
                                 nullptr);
            }
          } else if (repeat_mode_str == "yearly") {
            alarm.repeat_mode = AlarmRepeatMode::YEARLY;
            alarm.month = properties["month"].value<int>();
            alarm.day = properties["day"].value<int>();
            if (alarm.month < 1 || alarm.month > 12 || alarm.day < 1 ||
                alarm.day > 31) {
              ESP_LOGE(TAG, "yearly requires valid month/day");
              return make_result(false, "yearly 模式要求 month/day 合法",
                                 nullptr);
            }
            struct tm check_tm = {};
            check_tm.tm_year = 2000 - 1900;
            check_tm.tm_mon = alarm.month - 1;
            check_tm.tm_mday = alarm.day;
            check_tm.tm_hour = 12;
            if (mktime(&check_tm) == (time_t)-1 ||
                check_tm.tm_mon != alarm.month - 1 ||
                check_tm.tm_mday != alarm.day) {
              ESP_LOGE(TAG, "yearly month/day is not a real calendar date");
              return make_result(
                  false, "yearly 的月日组合不存在，请检查 month/day", nullptr);
            }
            ESP_LOGI(TAG, "  yearly: month=%d, day=%d", alarm.month, alarm.day);
          } else {
            ESP_LOGE(TAG, "Invalid repeat_mode: %s", repeat_mode_str.c_str());
            return make_result(false, "repeat_mode 非法", nullptr);
          }

          bool has_next_month_text =
              alarm.name.find("下个月") != std::string::npos ||
              alarm.name.find("下下个月") != std::string::npos;
          bool has_next_year_text =
              alarm.name.find("明年") != std::string::npos ||
              alarm.name.find("后年") != std::string::npos;

          if (alarm.repeat_mode == AlarmRepeatMode::MONTHLY_DAY &&
              has_next_month_text) {
            time_t now = time(NULL);
            struct tm *now_tm = localtime(&now);
            int month_offset =
                (alarm.name.find("下下个月") != std::string::npos) ? 2 : 1;
            int target_year = now_tm->tm_year + 1900;
            int target_month = now_tm->tm_mon + 1 + month_offset;
            while (target_month > 12) {
              target_month -= 12;
              target_year += 1;
            }
            if (!is_valid_date(target_year, target_month, alarm.day_of_month)) {
              return make_result(
                  false, "目标月份不存在该日期，请检查 day_of_month", nullptr);
            }
            alarm.repeat_mode = AlarmRepeatMode::SPECIFIC_DATE;
            alarm.year = target_year;
            alarm.month = target_month;
            alarm.day = alarm.day_of_month;
            ESP_LOGI(TAG,
                     "  normalize monthly_day -> specific_date: %04d-%02d-%02d",
                     alarm.year, alarm.month, alarm.day);
          }

          if (alarm.repeat_mode == AlarmRepeatMode::YEARLY &&
              has_next_year_text) {
            time_t now = time(NULL);
            struct tm *now_tm = localtime(&now);
            int year_offset =
                (alarm.name.find("后年") != std::string::npos) ? 2 : 1;
            int target_year = now_tm->tm_year + 1900 + year_offset;
            if (!is_valid_date(target_year, alarm.month, alarm.day)) {
              return make_result(
                  false, "目标年份不存在该日期，请检查 month/day", nullptr);
            }
            alarm.repeat_mode = AlarmRepeatMode::SPECIFIC_DATE;
            alarm.year = target_year;
            ESP_LOGI(TAG, "  normalize yearly -> specific_date: %04d-%02d-%02d",
                     alarm.year, alarm.month, alarm.day);
          }

          ESP_LOGI(TAG, "[set_alarm] Setting alarm: name=%s, mode=%d",
                   alarm.name.c_str(), static_cast<int>(alarm.repeat_mode));
          Alarm created_alarm;
          std::string error_reason;
          bool success =
              alarm_manager->SetAlarm(alarm, &created_alarm, &error_reason);
          if (success) {
            ESP_LOGI(TAG, "[set_alarm] Alarm %s set successfully",
                     alarm.name.c_str());
            return make_result(true, "闹钟设置成功", &created_alarm);
          } else {
            ESP_LOGE(TAG, "[set_alarm] Failed to set alarm %s",
                     alarm.name.c_str());
            if (error_reason.empty()) {
              error_reason = "闹钟设置失败";
            }
            return make_result(false, error_reason, nullptr);
          }
        });
  }

  bool IsAlarm(void) { return (alarm_cnt > -1); }

  void StartAlarm(const std::string &name) {
    ESP_LOGW(TAG, "StartAlarm");
    alarm_cnt = 0;
    alarm_playing = false; // 重置播放标志
    PlayAlarmOnceAsync();  // 首次立即响起

    if (alarm_play_timer) {
      esp_timer_stop(alarm_play_timer);
      esp_timer_start_periodic(alarm_play_timer,
                               4500000); // 4500ms in microseconds
    }
  }

  void StopAlarm(void) {
    ESP_LOGW(TAG, "StopAlarm");
    if (alarm_cnt > -1) {
      alarm_cnt = -1;
      alarm_playing = false; // 重置播放标志
      if (alarm_play_timer) {
        esp_timer_stop(alarm_play_timer);
        on_alarm_stop_cb_();
      }

      // 确保音频输出被禁用
      auto codec = Board::GetInstance().GetAudioCodec();
      if (codec) {
        codec->EnableOutput(false);
      }
    }
  }

  AlarmManager *GetAlarmManager() { return alarm_manager; }
};
