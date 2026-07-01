#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define BOOT_BUTTON_GPIO GPIO_NUM_9

#define CODEC_TX_GPIO GPIO_NUM_12
#define CODEC_RX_GPIO GPIO_NUM_13

#define CONFIG_POWER_ON_SOUNDS
#define CONFIG_NET_STA_SOUNDS

#define RGB_DI_GPIO GPIO_NUM_8
#define LED_PWM_GPIO GPIO_NUM_10
#define EARTH_LED_PWM_GPIO GPIO_NUM_7

#define LCD_TE_GPIO GPIO_NUM_6
#define LCD_BL_GPIO GPIO_NUM_5
#define LCD_CS_GPIO GPIO_NUM_4
#define LCD_CLK_GPIO GPIO_NUM_3
#define LCD_RESET_GPIO GPIO_NUM_2
#define LCD_SDA_GPIO GPIO_NUM_1
#define LCD_CD_GPIO GPIO_NUM_0

#define DISPLAY_INVERT_COLOR true
#define DISPLAY_SWAP_XY true
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_WIDTH 296
#define DISPLAY_HEIGHT 240
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// 7014的蓝牙和音频混合
#define USE_7014_BL_AUDIO_MIX false

// 7014的蓝牙音量和小智音量分开保存，默认调整蓝牙音量，只有小智在listening和speaking状态下调整小智音量
// 切换模式时：
// 切换成idle: 音量改为蓝牙音量
// 切换到listen/speaking: 音量改为小智音量
// 音量改变时：
// 若是listen/speaking：记录小智音量
// 若是idle：记录蓝牙音量
#define USE_7014_SEPARATE_BL_AND_XIAOZHI_VOLUME false

// 是否记忆灯光
#define REMEMBER_LIGHT false

#endif // _BOARD_CONFIG_H_
