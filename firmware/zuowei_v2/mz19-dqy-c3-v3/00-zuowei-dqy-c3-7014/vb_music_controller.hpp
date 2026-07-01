#pragma once

#include "application.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mcp_server.h"
#include "vb_adapter.h"

// Forward declaration to avoid circular dependency
class Mz01C3Lcd;

#define TAG "VbMusicContorller"

class VbMusicContorller {
private:
  bool temp_play_pause;

public:
  VbMusicContorller() {}
  ~VbMusicContorller() {}

  static VbMusicContorller &GetInstance() {
    static VbMusicContorller controller;
    return controller;
  }

  void Init();
};