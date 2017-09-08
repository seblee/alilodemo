#include "mico.h"
#include "eland_up_thread.h"

/**************數據上傳 http 發送線程****************************/
void eland_upstream_thread(mico_thread_arg_t args)
{
    OSStatus err = kGeneralErr;

    while (1)
    {
        mico_thread_msleep(6000);
    }
}
