#include "eland_down_thread.h"
#include "mico.h"
#define eland_down_log(M, ...) custom_log("eland_http_clent", M, ##__VA_ARGS__)

//用户接收数据线程
void eland_downstream_thread(mico_thread_arg_t arg)
{
    OSStatus err = kUnknownErr;
    char *eland_recv_buff = NULL;

    eland_recv_buff = malloc(ELAND_RECV_BUFF_LEN);
    require(eland_recv_buff != NULL, exit);

    eland_down_log("------------downstream_thread start------------");

    while (1)
    {
        memset(eland_recv_buff, 0, ELAND_RECV_BUFF_LEN);
        err = eland_device_recv_command(eland_recv_buff, ELAND_RECV_BUFF_LEN, MICO_NEVER_TIMEOUT);
        if (err == kNoErr)
        {
            eland_down_log("recv:%s", eland_recv_buff);
            process_recv_data(eland_recv_buff, strlen(eland_recv_buff)); //解析数据
        }
    }

exit:
    if (eland_recv_buff != NULL)
    {
        free(eland_recv_buff);
        eland_recv_buff = NULL;
    }

    if (kNoErr != err)
    {
        eland_down_log("ERROR: user_downstream_thread exit with err=%d", err);
    }
    mico_rtos_delete_thread(NULL); // delete current thread

    return;
}

//功能：从云端接收数据
//参数： payload - 接收数据缓冲区地址
//参数： payload_len - 接收数据缓冲区地址的长度
//参数： timeout - 接收数据等待时间
//返回值：kNoErr为成功 其他值为失败
OSStatus eland_device_recv_command(char *payload, uint32_t payload_len, uint32_t timeout)
{
    OSStatus err = kUnknownErr;
    p_mqtt_recv_msg_t p_recv_msg = NULL;

    require_action(mqtt_context != NULL, exit, err = kGeneralErr);
    require_action(payload != NULL, exit, err = kGeneralErr);

    memset(payload, 0, payload_len);

    // get msg from recv queue
    err = mico_rtos_pop_from_queue(&(mqtt_context->mqtt_msg_recv_queue), &p_recv_msg, timeout);
    require_noerr(err, exit);

    if (p_recv_msg)
    {
        //app_log("user get data success! from_topic=[%s], msg=[%d][%s].", p_recv_msg->topic, p_recv_msg->datalen, p_recv_msg->data);

        require_action(payload_len > p_recv_msg->datalen, exit, err = kGeneralErr);

        memset(payload, 0, payload_len);
        memcpy(payload, p_recv_msg->data, p_recv_msg->datalen);

        //app_log("[RECV] command: %s", payload);

        // release msg mem resource
        free(p_recv_msg);
        p_recv_msg = NULL;
    }
    else
    {
        eland_down_log("[RECV] ERROR!");
    }

exit:
    return err;
}
