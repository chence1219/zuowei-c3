/*
 * @Author: HoGC 
 * @Date: 2022-10-26 11:30:08 
 * @Last Modified by:   HoGC 
 * @Last Modified time: 2022-10-26 11:30:08 
 */
#ifndef __FRAME_PARSER_H__
#define __FRAME_PARSER_H__

#include <stdint.h>
#include <stdbool.h>

static inline uint32_t fp_sum(const uint8_t* bytes, uint16_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return sum;
}

#define FRAME_SUM(bytes, len)   (fp_sum(bytes, len))
#define FRAME_SWAP_16(x)        ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))
#define FRAME_SWAP_32(x)        ((((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF000000) >> 24))

#define FRAME_PARSER_HEAD        0xAA55

typedef struct{
    uint16_t head;
    uint16_t len;
    uint16_t cmd;
}__attribute__ ((packed))frame_parser_head_t;

typedef struct{
    uint8_t sum;
}__attribute__ ((packed))frame_parser_last_t;

#define FRAME_MIN_LEN          (sizeof(frame_parser_head_t) + 2 + sizeof(frame_parser_last_t))
#define FRAME_MAX_LEN          (sizeof(frame_parser_head_t) + 512 + sizeof(frame_parser_last_t))
#define FRAME_DATA_LEN(h)      (FRAME_SWAP_16(h->len)) 
#define FRAME_CHECK(h, e)      ((FRAME_SUM((uint8_t *)h, sizeof(frame_parser_head_t) + FRAME_DATA_LEN(h))%256) == e->sum)

bool frame_parser_init(uint32_t size);
void frame_parser_reset(void);
uint32_t frame_parser_add_buf(uint8_t *buf, uint32_t len);
bool frame_parser_get_frame(uint8_t *frame, uint32_t *len);

#endif