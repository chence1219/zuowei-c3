#ifndef _TYPE_H_
#define _TYPE_H_

typedef enum {
    APP_TYPE_BLUFI = 1,
    APP_TYPE_OTA = 2,
} AppType_t;

// 全局 app 类型，在 blufi_app.cc 中定义
extern AppType_t g_app;

#endif // _TYPE_H_