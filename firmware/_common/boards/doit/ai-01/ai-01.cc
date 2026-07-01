
#include "config.h"
#include "../firmware/_common/boards/doit/ai-01/ai-01.h"

#define TAG "DoitAI01"

class DoitAI01 : public AI01 {
public:
    DoitAI01(){

    }
    ~DoitAI01(){

    }
};

DECLARE_BOARD(DoitAI01);
