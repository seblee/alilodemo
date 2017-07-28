#include "sntp_client.h"
#include "mico.h"
#include "sntp.h"
#include <time.h>
#define sntp_demo_log(M, ...) custom_log("Stnp", M, ##__VA_ARGS__)

#define TIME_SYNC_PERIOD (60 * SECONDS)

void start_sntp_service(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "sntp_thread", sntp_thread, 0x1000, (uint32_t)NULL);
}
/* Callback function when MiCO UTC time in sync to NTP server */
static void sntp_time_synced(void)
{
    struct tm *currentTime;
    iso8601_time_t iso8601_time;
    mico_utc_time_t utc_time;
    mico_rtc_time_t rtc_time;

    mico_time_get_iso8601_time(&iso8601_time);
    sntp_demo_log("sntp_time_synced: %.26s", (char *)&iso8601_time);

    mico_time_get_utc_time(&utc_time);

    currentTime = localtime((const time_t *)&utc_time);
    rtc_time.sec = currentTime->tm_sec;
    rtc_time.min = currentTime->tm_min;
    rtc_time.hr = currentTime->tm_hour;

    rtc_time.date = currentTime->tm_mday;
    rtc_time.weekday = currentTime->tm_wday;
    rtc_time.month = currentTime->tm_mon + 1;
    rtc_time.year = (currentTime->tm_year + 1900) % 100;

    MicoRtcSetTime(&rtc_time);
}

/**/
static void read_utc_time_from_rtc(struct tm *utc_time)
{
    mico_rtc_time_t time;

    /*Read current time from RTC.*/
    if (MicoRtcGetTime(&time) == kNoErr)
    {
        utc_time->tm_sec = time.sec;
        utc_time->tm_min = time.min;
        utc_time->tm_hour = time.hr;
        utc_time->tm_mday = time.date;
        utc_time->tm_wday = time.weekday;
        utc_time->tm_mon = time.month - 1;
        utc_time->tm_year = time.year + 100;
        sntp_demo_log("Current RTC Time: %s", asctime(utc_time));
    }
    else
    {
        sntp_demo_log("RTC function unsupported");
    }
}

void sntp_thread(mico_thread_arg_t args)
{
    struct tm utc_time;
    mico_utc_time_ms_t utc_time_ms;

    /* Read UTC time from RTC hardware */
    read_utc_time_from_rtc(&utc_time);

    /* Set UTC time to MiCO system */
    utc_time_ms = (uint64_t)mktime(&utc_time) * (uint64_t)1000;
    mico_time_set_utc_time_ms(&utc_time_ms);

    /* Start auto sync with NTP server */
    sntp_start_auto_time_sync(TIME_SYNC_PERIOD, sntp_time_synced);

    mico_rtos_delete_thread(NULL);
}
