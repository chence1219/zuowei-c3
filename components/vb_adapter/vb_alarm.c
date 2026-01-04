#include "vb_adapter.h"
#include "vb_protocol.h"
#include "esp_log.h"

#define TAG "alarm"

int vb_alarm_set(alarm_transfer_t *alarm){
    int retry_time = 4;
    int ret = -1;
    uint8_t *recv = NULL;
    uint16_t recv_len = 0;
    
    do{
        ret = vb_protocol_send_block(VB_CMD_SET_ALARM, alarm, sizeof(alarm_transfer_t), &recv, &recv_len, 100);
    }while (ret < 0 && retry_time--);
    
    if (recv)
    {
        if (recv[0] == 0xff)
        {
            char *msg = (char*)&recv[1];
            ESP_LOGE(TAG, "set alarm fail : %s", msg);
        }else{
            alarm_transfer_t *a = (alarm_transfer_t*)&recv[1];
            ESP_LOGI("alarm add success: %02d:%02d:%02d onoff:%d index:%d", a->hour, a->min, a->sec, a->sec, a->index);
        }
        free(recv);    
    }else{
        ESP_LOGE(TAG, "set alarm fail : respone time out");
    }
    return ret;
}

int vb_alarm_add(alarm_transfer_t *alarm){
    int retry_time = 4;
    int ret = -1;
    uint8_t *recv = NULL;
    uint16_t recv_len = 0;
    alarm->index = 0xff;
    do{
        ret = vb_protocol_send_block(VB_CMD_SET_ALARM, alarm, sizeof(alarm_transfer_t), &recv, &recv_len, 100);
    }while (ret < 0 && retry_time--);
    
    if (recv)
    {
        if (recv[0] == 0xff)
        {
            char *msg = (char*)&recv[1];
            ESP_LOGE(TAG, "set alarm fail : %s", msg);
            free(recv);
            return -1;
        }else{
            alarm_transfer_t *a = (alarm_transfer_t*)&recv[1];
            ESP_LOGI(TAG, "alarm add success: %02d:%02d:%02d onoff:%d index:%d", a->hour, a->min, a->sec, a->sec, a->index);
        }
        free(recv);    
    }else{
        ESP_LOGE(TAG, "set alarm fail : respone time out");
    }
    return ret;
}

int vb_alarm_del(uint8_t index){
    uint8_t send = index;
    int retry_time = 4;
    int ret = -1;
    uint8_t *recv = NULL;
    uint16_t recv_len = 0;
    do{
        ret = vb_protocol_send_block(VB_CMD_GET_ALARMS, &send, sizeof(send), &recv, &recv_len, 100);
        if (ret>=0)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }while (retry_time--);
    if (recv)
    {
        if (recv[0] == 0)
        {
            ESP_LOGI(TAG, "delete alarm[%d] success", index);
            free(recv);
            return 0;
        }else if (recv[0] == 0xff)
        {
            ESP_LOGE(TAG, "delete alarm fail %s", (char*)&recv[1]);
            free(recv);
            return -1;
        }else{
            ESP_LOGE(TAG, "delete alarm fail other");
            free(recv);
            return -1;
        }
    }
    return ret;
}

int vb_alarm_get_info(alarm_transfer_t **pAlarm){
    static alarm_transfer_t alarm[5];
    int retry_time = 4;
    int ret = -1;
    uint8_t *recv = NULL;
    uint16_t recv_len = 0;
    do{
        ret = vb_protocol_send_block(VB_CMD_GET_ALARMS, alarm, sizeof(alarm_transfer_t), &recv, &recv_len, 100);
    }while (ret < 0 && retry_time--);

    if (recv)
    {
        if (recv[0]<5 && recv_len == 1+(recv[0]*sizeof(alarm_transfer_t)))
        {
            memcpy(alarm, &recv[1], recv[0]*sizeof(alarm_transfer_t));
            free(recv);
            *pAlarm = alarm;
            return (int)recv[1];
        }else if(recv[0] == 0xff){
            char *msg = (char*)&recv[1];
            ESP_LOGE(TAG, "get fail: %s", msg);
            free(recv);
            return -1;
        }else{
            ESP_LOGE(TAG, "get fail: other");
            free(recv);
            return -1;
        }
    }    
    return ret;
}