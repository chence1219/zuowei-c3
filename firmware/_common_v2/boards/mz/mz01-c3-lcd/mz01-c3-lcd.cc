#include "config.h"
#include "../firmware/_common/boards/mz/mz01-c3-lcd/mz01-c3-lcd.h"

#define TAG "MzMz01C3Lcd"

class MzMz01C3Lcd : public Mz01C3Lcd {
public:
    MzMz01C3Lcd(){

    }
    ~MzMz01C3Lcd(){

    }
};

DECLARE_BOARD(MzMz01C3Lcd);
