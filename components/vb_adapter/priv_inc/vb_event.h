#ifndef __VB_EVENT_H__
#define __VB_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 事件回调函数类型
 *
 * @param event_id  事件 ID（由调用方自定义）
 * @param data      事件关联数据指针
 * @param len       数据长度（字节数）
 * @param user_arg  注册时传入的用户参数
 */
typedef void (*vb_event_cb_t)(uint32_t event_id, void *data, uint16_t len, void *user_arg);

/**
 * @brief 事件系统初始化（创建内部锁）
 */
void vb_event_init(void);

/**
 * @brief 注册事件回调（线程安全）
 *
 * 同一个 event_id 支持注册多个回调。
 *
 * @param event_id  事件 ID
 * @param cb        回调函数
 * @param user_arg  透传给回调的用户参数
 * @return 0 成功，负数表示失败（如表满）
 */
int vb_event_register(uint32_t event_id, vb_event_cb_t cb, void *user_arg);

/**
 * @brief 注销事件回调（线程安全）
 *
 * 会移除所有匹配 event_id + cb + user_arg 的回调。
 *
 * @return 移除的回调数量
 */
int vb_event_unregister(uint32_t event_id, vb_event_cb_t cb, void *user_arg);

/**
 * @brief 触发事件（线程安全）
 *
 * 会调用所有已注册的该 event_id 的回调。
 *
 * @param event_id  事件 ID
 * @param data      事件数据指针（可为 NULL）
 * @param len       数据长度（字节数，可为 0）
 */
void vb_event_emit(uint32_t event_id, void *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // __VB_EVENT_H__


