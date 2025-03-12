/*
 * @Author: HoGC 
 * @Date: 2022-10-26 11:30:06 
 * @Last Modified by:   HoGC 
 * @Last Modified time: 2022-10-26 11:30:06 
 */
#include "frame_parser.h"

#include <stdio.h>

#include "rbuffer.h"

static rbuffer_handle_t g_rbuffer;

bool frame_parser_init(uint32_t size){
    g_rbuffer = rbuffer_create(size);
    if(NULL == g_rbuffer){
        return false;
    }
    return true;
}

void frame_parser_reset(void){
    rbuffer_reset(g_rbuffer);
}

uint32_t frame_parser_add_buf(uint8_t *buf, uint32_t len){
    if(NULL == g_rbuffer){
        return 0;
    }
    return rbuffer_push(g_rbuffer, buf, len, false);
}

bool frame_parser_get_frame(uint8_t *frame, uint32_t *len){
    if(NULL == g_rbuffer){
        return false;
    }

    while (rbuffer_used_size(g_rbuffer) >= FRAME_MIN_LEN)
    {
        frame_parser_head_t *head = NULL;
        frame_parser_last_t *last = NULL;
        uint32_t try_len = 0;
        uint32_t try_index = 0;
        uint32_t remove_len = 0;
        uint32_t frame_len = 0;
        uint32_t start_index = 0;
        uint8_t buf[FRAME_MAX_LEN] = {0};

        try_len = rbuffer_used_size(g_rbuffer);
        rbuffer_get_head_index(g_rbuffer, &try_index);
        while (try_len > sizeof(frame_parser_head_t))
        {
            rbuffer_get_buffer(g_rbuffer, try_index, buf, sizeof(frame_parser_head_t));
            head =  (frame_parser_head_t *)buf;
            if(FRAME_PARSER_HEAD == head->head){
                break;
            }else{
                rbuffer_discard(g_rbuffer, 1);
            }
            try_len--;
            try_index++;
        }
        
        if(try_len < (sizeof(frame_parser_head_t) + FRAME_DATA_LEN(head)) + sizeof(frame_parser_last_t)){
            remove_len += sizeof(head->head);
            try_len -= sizeof(head->head);
            try_index += sizeof(head->head);
            if(try_len >= FRAME_MIN_LEN){
                while (try_len > sizeof(frame_parser_head_t))
                {
                    rbuffer_get_buffer(g_rbuffer, try_index, buf, sizeof(frame_parser_head_t));
                    head =  (frame_parser_head_t *)buf;
                    if(FRAME_PARSER_HEAD == head->head){
                        break;
                    }
                    try_len--;
                    try_index++;
                    remove_len++;
                }
                if(try_len <= sizeof(frame_parser_head_t)){
                    return false;
                }
            }else{
                return false;
            }
        }

        start_index = try_index;
        frame_len = (sizeof(frame_parser_head_t) + FRAME_DATA_LEN(head));
        try_len -= (sizeof(frame_parser_head_t) + FRAME_DATA_LEN(head));
        try_index += (sizeof(frame_parser_head_t) + FRAME_DATA_LEN(head));

        if(try_len >= sizeof(frame_parser_last_t)){
            frame_len += sizeof(frame_parser_last_t);
            if(frame_len > FRAME_MAX_LEN){
                rbuffer_discard(g_rbuffer, 2);
                return false;
            }
#ifdef FRAME_CHECK
            rbuffer_get_buffer(g_rbuffer, start_index, buf, frame_len);
            last = (frame_parser_last_t *)(buf + frame_len - sizeof(frame_parser_last_t));
            if(FRAME_CHECK(head, last)){
#else
            if(1){
#endif
                rbuffer_get_buffer(g_rbuffer, start_index, frame, frame_len);
                *len = frame_len;
                rbuffer_discard(g_rbuffer, frame_len);
                if(remove_len){
                   rbuffer_discard(g_rbuffer, remove_len); 
                }
                return true;
            }else{
                rbuffer_discard(g_rbuffer, 2);
            }
        }else{
            return false;
        }
    }
    return false;
}