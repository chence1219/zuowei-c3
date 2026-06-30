#include "nertc_external_osal.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/idf_additions.h>

namespace {
constexpr const char* TAG = "NeRtcExternalOsal";
constexpr int kMaxCondWaiters = 128;
constexpr int kThreadStopTimeoutMs = 1000;
constexpr EventBits_t kExternalThreadWakeBit = BIT0;
constexpr EventBits_t kExternalThreadStopBit = BIT1;
constexpr uint32_t kWaitForeverMs = 0xFFFFFFFFu;
}

NeRtcExternalOsal* NeRtcExternalOsal::instance_ = nullptr;

NeRtcExternalOsal* NeRtcExternalOsal::GetInstance() {
    if (instance_ == nullptr) {
        instance_ = new NeRtcExternalOsal();
    }
    return instance_;
}

void NeRtcExternalOsal::DestroyInstance() {
    delete instance_;
    instance_ = nullptr;
}

NeRtcExternalOsal::NeRtcExternalOsal() {
    handle_ = {
        .create_thread = CreateThread,
        .destroy_thread = DestroyThread,
        .thread_is_current = IsCurrentThread,
        .thread_wait = WaitThread,
        .thread_notify = NotifyThread,

        .create_timer = CreateTimer,
        .start_timer = StartTimer,
        .stop_timer = StopTimer,
        .destroy_timer = DestroyTimer,

        .sleep_ms = SleepMs,
        .get_timestamp_ms = GetTimestampMs,
        .log_write = LogWrite,

        .create_mutex = CreateMutex,
        .destroy_mutex = DestroyMutex,
        .lock_mutex = LockMutex,
        .unlock_mutex = UnlockMutex,

        .create_cond = CreateCond,
        .destroy_cond = DestroyCond,
        .wait_cond = WaitCond,
        .notify_one_cond = NotifyOneCond,
        .notify_all_cond = NotifyAllCond,
    };

    ESP_LOGI(TAG, "Create NeRtcExternalOsal instance");
}

NeRtcExternalOsal::~NeRtcExternalOsal() {
}

nertc_ext_thread_handle_t NeRtcExternalOsal::CreateThread(const char* name,
                                                           int stack_size,
                                                           int priority,
                                                           bool prefer_external_memory,
                                                           nertc_ext_thread_entry_func entry,
                                                           void* user_data) {
    if (entry == nullptr) {
        return nullptr;
    }

    auto* ctx = new ExternalThreadCtx();
    ctx->entry = entry;
    ctx->user_data = user_data;
    ctx->event_group = xEventGroupCreate();
    if (ctx->event_group == nullptr) {
        ESP_LOGW(TAG, "CreateThread event group create failed");
    } else {
        xEventGroupClearBits(ctx->event_group, kExternalThreadWakeBit | kExternalThreadStopBit);
    }

    const char* task_name = (name && name[0] != '\0') ? name : "nertc_ext_thread";
    const uint32_t task_stack_size = (stack_size > 0) ? static_cast<uint32_t>(stack_size) : 4096;
    UBaseType_t task_priority = (priority > 0) ? static_cast<UBaseType_t>(priority) : tskIDLE_PRIORITY + 1;

    BaseType_t ret = pdFAIL;
    TaskHandle_t task_handle = nullptr;
    if (prefer_external_memory) {
        ret = xTaskCreatePinnedToCoreWithCaps(
            ThreadEntry,
            task_name,
            task_stack_size,
            ctx,
            task_priority,
            &task_handle,
            tskNO_AFFINITY,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ret != pdPASS) {
            ESP_LOGW(TAG, "CreateThread SPIRAM alloc failed, fallback to internal: %s", task_name);
        }
    }

    if (ret != pdPASS) {
        ret = xTaskCreate(ThreadEntry, task_name, task_stack_size, ctx, task_priority, &task_handle);
    }
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "CreateThread failed: %s", task_name);
        delete ctx;
        return nullptr;
    }
    ctx->task_handle.store(task_handle, std::memory_order_release);
    return static_cast<nertc_ext_thread_handle_t>(ctx);
}

void NeRtcExternalOsal::DestroyThread(nertc_ext_thread_handle_t handle) {
    if (handle == nullptr) {
        return;
    }

    auto* ctx = static_cast<ExternalThreadCtx*>(handle);
    TaskHandle_t task = ctx->task_handle.load(std::memory_order_acquire);
    if (task != nullptr) {
        if (ctx->event_group != nullptr) {
            xEventGroupSetBits(ctx->event_group, kExternalThreadStopBit);
        }
        if (xTaskGetCurrentTaskHandle() == task) {
            ctx->task_handle.store(nullptr, std::memory_order_release);
            if (ctx->event_group != nullptr) {
                vEventGroupDelete(ctx->event_group);
                ctx->event_group = nullptr;
            }
            delete ctx;
            vTaskDelete(nullptr);
            return;
        }

        int waited_ms = 0;
        while (ctx->task_handle.load(std::memory_order_acquire) != nullptr && waited_ms < kThreadStopTimeoutMs) {
            vTaskDelay(pdMS_TO_TICKS(10));
            waited_ms += 10;
        }

        task = ctx->task_handle.exchange(nullptr, std::memory_order_acq_rel);
        if (task != nullptr) {
            ESP_LOGW(TAG, "DestroyThread timeout, force delete task: %p", task);
            vTaskDelete(task);
        }
    }
    if (ctx->event_group != nullptr) {
        vEventGroupDelete(ctx->event_group);
        ctx->event_group = nullptr;
    }
    delete ctx;
}

bool NeRtcExternalOsal::IsCurrentThread(nertc_ext_thread_handle_t handle) {
    if (handle == nullptr) {
        return false;
    }

    auto* ctx = static_cast<ExternalThreadCtx*>(handle);
    TaskHandle_t task = ctx->task_handle.load(std::memory_order_acquire);
    return (task != nullptr) && (xTaskGetCurrentTaskHandle() == task);
}

void NeRtcExternalOsal::WaitThread(nertc_ext_thread_handle_t handle, uint32_t timeout_ms) {
    if (handle == nullptr) {
        return;
    }

    auto* ctx = static_cast<ExternalThreadCtx*>(handle);
    if (ctx->event_group == nullptr) {
        if (timeout_ms != kWaitForeverMs && timeout_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(timeout_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        return;
    }

    TickType_t wait_ticks = 0;
    if (timeout_ms == kWaitForeverMs) {
        wait_ticks = portMAX_DELAY;
    } else if (timeout_ms > 0) {
        wait_ticks = pdMS_TO_TICKS(timeout_ms);
        if (wait_ticks == 0) {
            wait_ticks = 1;
        }
    }

    xEventGroupWaitBits(ctx->event_group,
                        kExternalThreadWakeBit | kExternalThreadStopBit,
                        pdTRUE,
                        pdFALSE,
                        wait_ticks);
}

void NeRtcExternalOsal::NotifyThread(nertc_ext_thread_handle_t handle) {
    if (handle == nullptr) {
        return;
    }

    auto* ctx = static_cast<ExternalThreadCtx*>(handle);
    if (ctx->event_group != nullptr) {
        xEventGroupSetBits(ctx->event_group, kExternalThreadWakeBit);
    }
}

nertc_ext_timer_handle_t NeRtcExternalOsal::CreateTimer(const char* name,
                                                         int interval_ms,
                                                         nertc_ext_timer_callback_func callback,
                                                         void* user_data) {
    if (callback == nullptr || interval_ms <= 0) {
        return nullptr;
    }

    auto* ctx = new ExternalTimerCtx();
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->interval_ms = interval_ms;

    esp_timer_create_args_t timer_args = {
        .callback = TimerEntry,
        .arg = ctx,
        .dispatch_method = ESP_TIMER_TASK,
        .name = (name && name[0] != '\0') ? name : "nertc_ext_timer",
        .skip_unhandled_events = true,
    };

    esp_err_t err = esp_timer_create(&timer_args, &ctx->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CreateTimer failed: %s", esp_err_to_name(err));
        delete ctx;
        return nullptr;
    }

    return static_cast<nertc_ext_timer_handle_t>(ctx);
}

bool NeRtcExternalOsal::StartTimer(nertc_ext_timer_handle_t handle) {
    if (handle == nullptr) {
        return false;
    }
    auto* ctx = static_cast<ExternalTimerCtx*>(handle);
    esp_err_t err = esp_timer_start_periodic(ctx->timer, static_cast<uint64_t>(ctx->interval_ms) * 1000);
    return (err == ESP_OK);
}

bool NeRtcExternalOsal::StopTimer(nertc_ext_timer_handle_t handle) {
    if (handle == nullptr) {
        return false;
    }
    auto* ctx = static_cast<ExternalTimerCtx*>(handle);
    esp_err_t err = esp_timer_stop(ctx->timer);
    return (err == ESP_OK || err == ESP_ERR_INVALID_STATE);
}

void NeRtcExternalOsal::DestroyTimer(nertc_ext_timer_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    auto* ctx = static_cast<ExternalTimerCtx*>(handle);
    esp_timer_stop(ctx->timer);
    esp_timer_delete(ctx->timer);
    delete ctx;
}

void NeRtcExternalOsal::SleepMs(int ms) {
    if (ms <= 0) {
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

int64_t NeRtcExternalOsal::GetTimestampMs(void) {
    return esp_timer_get_time() / 1000;
}

void NeRtcExternalOsal::LogWrite(int level, const char* tag, const char* message) {
    const char* log_tag = (tag && tag[0] != '\0') ? tag : "NERTC";
    const char* log_msg = (message != nullptr) ? message : "";
    switch (level) {
        case 1:
            ESP_LOGE(log_tag, "%s", log_msg);
            break;
        case 2:
            ESP_LOGW(log_tag, "%s", log_msg);
            break;
        case 3:
            ESP_LOGI(log_tag, "%s", log_msg);
            break;
        default:
            ESP_LOGI(log_tag, "%s", log_msg);
            break;
    }
}

nertc_ext_mutex_handle_t NeRtcExternalOsal::CreateMutex() {
    return static_cast<nertc_ext_mutex_handle_t>(xSemaphoreCreateMutex());
}

void NeRtcExternalOsal::DestroyMutex(nertc_ext_mutex_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    vSemaphoreDelete(static_cast<SemaphoreHandle_t>(handle));
}

bool NeRtcExternalOsal::LockMutex(nertc_ext_mutex_handle_t handle) {
    if (handle == nullptr) {
        return false;
    }
    return xSemaphoreTake(static_cast<SemaphoreHandle_t>(handle), portMAX_DELAY) == pdTRUE;
}

void NeRtcExternalOsal::UnlockMutex(nertc_ext_mutex_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(handle));
}

nertc_ext_cond_handle_t NeRtcExternalOsal::CreateCond() {
    auto* ctx = new ExternalCondCtx();
    ctx->sem = xSemaphoreCreateCounting(kMaxCondWaiters, 0);
    if (ctx->sem == nullptr) {
        delete ctx;
        return nullptr;
    }
    return static_cast<nertc_ext_cond_handle_t>(ctx);
}

void NeRtcExternalOsal::DestroyCond(nertc_ext_cond_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    auto* ctx = static_cast<ExternalCondCtx*>(handle);
    if (ctx->sem) {
        vSemaphoreDelete(ctx->sem);
    }
    delete ctx;
}

void NeRtcExternalOsal::WaitCond(nertc_ext_cond_handle_t cond_handle,
                                 nertc_ext_mutex_handle_t mutex_handle,
                                 uint32_t timeout_ms) {
    if (cond_handle == nullptr || mutex_handle == nullptr) {
        return;
    }

    auto* cond = static_cast<ExternalCondCtx*>(cond_handle);
    auto mutex = static_cast<SemaphoreHandle_t>(mutex_handle);

    cond->waiters.fetch_add(1, std::memory_order_relaxed);
    xSemaphoreGive(mutex);
    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    xSemaphoreTake(cond->sem, ticks);
    xSemaphoreTake(mutex, portMAX_DELAY);
    cond->waiters.fetch_sub(1, std::memory_order_relaxed);
}

void NeRtcExternalOsal::NotifyOneCond(nertc_ext_cond_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    auto* cond = static_cast<ExternalCondCtx*>(handle);
    if (cond->waiters.load(std::memory_order_relaxed) > 0) {
        xSemaphoreGive(cond->sem);
    }
}

void NeRtcExternalOsal::NotifyAllCond(nertc_ext_cond_handle_t handle) {
    if (handle == nullptr) {
        return;
    }
    auto* cond = static_cast<ExternalCondCtx*>(handle);
    int waiter_count = cond->waiters.load(std::memory_order_relaxed);
    for (int i = 0; i < waiter_count; ++i) {
        xSemaphoreGive(cond->sem);
    }
}

void NeRtcExternalOsal::ThreadEntry(void* arg) {
    auto* ctx = static_cast<ExternalThreadCtx*>(arg);
    if (ctx && ctx->entry) {
        ctx->entry(ctx->user_data);
    }
    if (ctx) {
        ctx->task_handle.store(nullptr, std::memory_order_release);
    }
    vTaskDelete(nullptr);
}

void NeRtcExternalOsal::TimerEntry(void* arg) {
    auto* ctx = static_cast<ExternalTimerCtx*>(arg);
    if (ctx && ctx->callback) {
        ctx->callback(ctx->user_data);
    }
}
