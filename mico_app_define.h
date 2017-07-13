#ifndef __MICO_APP_DEFINE_H
#define __MICO_APP_DEFINE_H

#include "Common.h"
#include "Debug.h"

/*Application's configuration stores in flash*/
typedef struct
{
    uint8_t arr[50];
} application_config_t;

typedef struct _app_context_t
{
  /*Flash content*/
  application_config_t*     appConfig;

} app_context_t;



#endif

