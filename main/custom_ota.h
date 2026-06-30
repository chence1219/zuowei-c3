#ifndef _CUSTOM_OTA_H
#define _CUSTOM_OTA_H

#include <functional>
#include <string>
#include <map>

#include "ota.h"

class CustomOta : public Ota {
public:
    CustomOta();
    ~CustomOta();

    virtual esp_err_t CheckVersion();
    virtual std::string GetCheckVersionUrl();
    bool forced() { return forced_; };
private:
    bool forced_ = false;
};

#endif // _OTA_H
