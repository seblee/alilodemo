#include "mico.h"
#include "eland_up_thread.h"

/**************數據上傳 http 發送線程****************************/
void eland_upstream_thread(mico_thread_arg_t args)
{
    OSStatus err = kGeneralErr;
    ELAND_HTTP_RESPONSE_SETTING_S user_response;
    memset(&user_response, sizeof(user_response));
    mico_rtos_thread_sleep(3);
    eland_push_http_req_mutex(HTTP_GET,                                    //POST 或者 GET
                              "/api/download.php?vid=maki_emo_16_024kbps", //uri
                              ELAND_HTTP_DOMAIN_NAME,                      //host
                              "",                                          //BODY
                              &user_response);                             //response 指針

    mico_rtos_delete_thread(NULL);
}
