#ifndef NETCLOCK_SNTP_CLIENT_H_
#define NETCLOCK_SNTP_CLIENT_H_
#include "mico.h"
void start_sntp_service(void);
void sntp_thread(mico_thread_arg_t args);

#endif
