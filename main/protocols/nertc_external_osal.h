#ifndef _NERTC_EXTERNAL_OSAL_H_
#define _NERTC_EXTERNAL_OSAL_H_

#include <atomic>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "nertc_sdk_ext_osal.h"

class NeRtcExternalOsal {
public:
    static NeRtcExternalOsal* GetInstance();
    static void DestroyInstance();

    nertc_sdk_ext_osal_handle_t* GetHandle() { return &handle_; }

private:
    NeRtcExternalOsal();
    ~NeRtcExternalOsal();

private:
    struct ExternalThreadCtx {
        std::atomic<TaskHandle_t> task_handle{nullptr};
        EventGroupHandle_t event_group = nullptr;
        nertc_ext_thread_entry_func entry = nullptr;
        void* user_data = nullptr;
    };

    struct ExternalTimerCtx {
        esp_timer_handle_t timer = nullptr;
        nertc_ext_timer_callback_func callback = nullptr;
        void* user_data = nullptr;
        int interval_ms = 0;
    };

    struct ExternalCondCtx {
        SemaphoreHandle_t sem = nullptr;
        std::atomic<int> waiters{0};
    };

private:
    static nertc_ext_thread_handle_t CreateThread(const char* name,
                                                  int stack_size,
                                                  int priority,
                                                  bool prefer_external_memory,
                                                  nertc_ext_thread_entry_func entry,
                                                  void* user_data);
    static void DestroyThread(nertc_ext_thread_handle_t handle);
    static bool IsCurrentThread(nertc_ext_thread_handle_t handle);
    static void WaitThread(nertc_ext_thread_handle_t handle, uint32_t timeout_ms);
    static void NotifyThread(nertc_ext_thread_handle_t handle);

    static nertc_ext_timer_handle_t CreateTimer(const char* name,
                                                int interval_ms,
                                                nertc_ext_timer_callback_func callback,
                                                void* user_data);
    static bool StartTimer(nertc_ext_timer_handle_t handle);
    static bool StopTimer(nertc_ext_timer_handle_t handle);
    static void DestroyTimer(nertc_ext_timer_handle_t handle);

    static void SleepMs(int ms);
    static int64_t GetTimestampMs(void);
    static void LogWrite(int level, const char* tag, const char* message);

    static nertc_ext_mutex_handle_t CreateMutex();
    static void DestroyMutex(nertc_ext_mutex_handle_t handle);
    static bool LockMutex(nertc_ext_mutex_handle_t handle);
    static void UnlockMutex(nertc_ext_mutex_handle_t handle);

    static nertc_ext_cond_handle_t CreateCond();
    static void DestroyCond(nertc_ext_cond_handle_t handle);
    static void WaitCond(nertc_ext_cond_handle_t cond_handle,
                         nertc_ext_mutex_handle_t mutex_handle,
                         uint32_t timeout_ms);
    static void NotifyOneCond(nertc_ext_cond_handle_t handle);
    static void NotifyAllCond(nertc_ext_cond_handle_t handle);

    static void ThreadEntry(void* arg);
    static void TimerEntry(void* arg);

private:
    static NeRtcExternalOsal* instance_;
    nertc_sdk_ext_osal_handle_t handle_;
};

#endif
