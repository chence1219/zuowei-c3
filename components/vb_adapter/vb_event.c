#include "vb_event.h"

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// 为了简单与稳定，这里使用固定大小表，不做动态分配
#ifndef VB_EVENT_MAX_SLOTS
#define VB_EVENT_MAX_SLOTS 64
#endif

typedef struct {
    uint32_t      event_id;
    vb_event_cb_t cb;
    void         *user_arg;
    uint8_t       in_use;
} vb_event_slot_t;

static vb_event_slot_t   s_slots[VB_EVENT_MAX_SLOTS];
static SemaphoreHandle_t s_lock = NULL;

void vb_event_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }
}

int vb_event_register(uint32_t event_id, vb_event_cb_t cb, void *user_arg)
{
    if (!cb) {
        return -1;
    }
    if (!s_lock) {
        vb_event_init();
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    int ret = -2; // 默认：没有空位
    for (int i = 0; i < VB_EVENT_MAX_SLOTS; i++) {
        if (!s_slots[i].in_use) {
            s_slots[i].event_id = event_id;
            s_slots[i].cb       = cb;
            s_slots[i].user_arg = user_arg;
            s_slots[i].in_use   = 1;
            ret = 0;
            break;
        }
    }

    xSemaphoreGive(s_lock);
    return ret;
}

int vb_event_unregister(uint32_t event_id, vb_event_cb_t cb, void *user_arg)
{
    if (!cb || !s_lock) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    int removed = 0;
    for (int i = 0; i < VB_EVENT_MAX_SLOTS; i++) {
        if (s_slots[i].in_use &&
            s_slots[i].event_id == event_id &&
            s_slots[i].cb == cb &&
            s_slots[i].user_arg == user_arg) {
            s_slots[i].in_use = 0;
            removed++;
        }
    }

    xSemaphoreGive(s_lock);
    return removed;
}

void vb_event_emit(uint32_t event_id, void *data, uint16_t len)
{
    if (!s_lock) {
        return;
    }

    // 为避免在回调中再次注册/注销导致死锁，这里先复制一份快照再调用
    vb_event_slot_t snapshot[VB_EVENT_MAX_SLOTS];
    int count = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < VB_EVENT_MAX_SLOTS && count < VB_EVENT_MAX_SLOTS; i++) {
        if (s_slots[i].in_use && s_slots[i].event_id == event_id) {
            snapshot[count++] = s_slots[i];
        }
    }
    xSemaphoreGive(s_lock);

    for (int i = 0; i < count; i++) {
        if (snapshot[i].cb) {
            snapshot[i].cb(event_id, data, len, snapshot[i].user_arg);
        }
    }
}


