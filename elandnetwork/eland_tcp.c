/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-24 14:52:13
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-16 15:06:54
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "eland_tcp.h"
#include "eland_http_client.h"
#include "netclock.h"
#include "netclock_wifi.h"
#include "netclock_uart.h"
#include <time.h>

//#include "timer_platform.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define CONFIG_TCP_DEBUG
#ifdef CONFIG_TCP_DEBUG
#define elan_tcp_log(M, ...) custom_log("TCP", M, ##__VA_ARGS__)
#else
#define elan_tcp_log(...)
#endif /* ! CONFIG_TCP_DEBUG */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
const char CommandTable[TCPCMD_MAX][COMMAND_LEN] = {
    {'C', 'N', '0', '0'}, //Connection Request
    {'C', 'N', '0', '1'}, //Connection Response
    {'H', 'C', '0', '0'}, //health check request
    {'H', 'C', '0', '1'}, //health check response
    {'D', 'V', '0', '0'}, //eland info request
    {'D', 'V', '0', '1'}, //eland info response
    {'D', 'V', '0', '2'}, //eland info change Notification
    {'D', 'V', '0', '3'}, //eland info remove Notification
    {'A', 'L', '0', '0'}, //alarm info request
    {'A', 'L', '0', '1'}, //alarm info response
    {'A', 'L', '0', '2'}, //alarm info add Notification
    {'A', 'L', '0', '3'}, //alarm info change Notification
    {'A', 'L', '0', '4'}, //alarm info delete notification
    {'H', 'D', '0', '0'}, //holiday data request
    {'H', 'D', '0', '1'}, //holiday data response
    {'H', 'D', '0', '2'}, //holiday data change notice
    {'H', 'T', '0', '0'}, //alarm on notification
    {'H', 'T', '0', '1'}, //alarm off notification
    {'H', 'T', '0', '2'}, //alarm jump over notification
    {'F', 'W', '0', '0'}, //firmware update start request
    {'F', 'W', '0', '1'}, //firmwart update start response
    {'S', 'D', '0', '0'}, //firmwart update start response
    {'S', 'D', '0', '1'}, //firmwart update start response
};

mico_utc_time_t eland_send_time;
mico_utc_time_t elsv_receive_time;
mico_utc_time_t elsv_send_time;
mico_utc_time_t eland_receive_time;

mico_queue_t TCP_queue = NULL;

/* Private function prototypes -----------------------------------------------*/
static void TCP_thread_main(mico_thread_arg_t arg);
static TCP_Error_t eland_tcp_connect(_Client_t *pClient, ServerParams_t *ServerParams);
static TCP_Error_t TCP_connection_request(_Client_t *pClient); /*need back function*/
static TCP_Error_t TCP_update_elandinfo(_Client_t *pClient);   /*need back function*/
static TCP_Error_t TCP_update_alarm(_Client_t *pClient);       /*need back function*/
static TCP_Error_t TCP_update_holiday(_Client_t *pClient);     /*need back function*/
static TCP_Error_t TCP_health_check(_Client_t *pClient);       /*need back function*/
static TCP_Error_t TCP_request_response(_Client_t *pClient, _TCP_CMD_t request, _TCP_CMD_t response);
static TCP_Error_t TCP_upload(_Client_t *pClient, _TCP_CMD_t tcp_cmd);
static void HandleRequeseCallbacks(uint8_t *pMsg, _TCP_CMD_t cmd_type);
static TCP_Error_t TCP_send_packet(_Client_t *pClient, _TCP_CMD_t cmd_type, _time_t *timer);
static TCP_Error_t TCP_receive_packet(_Client_t *pClient, _time_t *timer);
static TCP_Error_t eland_set_client_state(_Client_t *pClient, ClientState_t expectedCurrentState, ClientState_t newState);
static TCP_Error_t TCP_Operate(const char *buff);
static TCP_Error_t TCP_Operate_HC01(char *buf);
static TCP_Error_t TCP_Operate_DV01(char *buf);
static TCP_Error_t TCP_Operate_ALXX(char *buf, _TCP_CMD_t telegram_cmd);
static TCP_Error_t TCP_Operate_HD0X(char *buf, _elsv_holiday_t *list);
static TCP_Error_t TCP_Operate_FW00(char *buf);
static TCP_Error_t TCP_Operate_SD01(char *buf);
static void time_record(TIME_RECORD_T_t type, mico_utc_time_t *value);
static void eland_set_time(void);
/* Private functions ---------------------------------------------------------*/

TCP_Error_t TCP_Physical_is_connected(Network_t *pNetwork)
{
    LinkStatusTypeDef link_status;
    memset(&link_status, 0, sizeof(link_status));
    micoWlanGetLinkStatus(&link_status);
    elan_tcp_log("wifi link_status:%d", link_status.is_connected);
    if (link_status.is_connected == true)
        return NETWORK_PHYSICAL_LAYER_CONNECTED;
    else
        return NETWORK_MANUALLY_DISCONNECTED;
}

static bool has_timer_expired(_time_t *timer)
{
    _time_t now, res;
    gettimeofday(&now, NULL);
    timersub(timer, &now, &res);
    return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}

static void TCP_tls_set_connect_params(Network_t *pNetwork,
                                       char *pDestinationURL,
                                       uint16_t destinationPort,
                                       uint32_t timeout_ms)
{
    pNetwork->tlsConnectParams.pRootCALocation = capem;
    pNetwork->tlsConnectParams.pDeviceCertLocation = certificate;
    pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = private_key;
    pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
    pNetwork->tlsConnectParams.DestinationPort = destinationPort;
    pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
    pNetwork->tlsConnectParams.ServerVerificationFlag = true;
    pNetwork->tlsConnectParams.isUseSSL = true;

    if (pNetwork->tlsConnectParams.isUseSSL == true)
    {
        ssl_set_client_version(TLS_V1_2_MODE);
    }

    pNetwork->connect = TCP_Connect;
    pNetwork->write = TCP_Write;
    pNetwork->read = TCP_Read;
    pNetwork->disconnect = TCP_disconnect;
    pNetwork->isConnected = TCP_Physical_is_connected;
    pNetwork->destroy = TCP_tls_destroy;

    pNetwork->tlsDataParams.server_fd = -1;
    pNetwork->tlsDataParams.ssl = NULL;
}

TCP_Error_t TCP_Connect(Network_t *pNetwork, ServerParams_t *Params)
{
    TCP_Error_t rc = TCP_SUCCESS;
    OSStatus err = kGeneralErr;
    char ipstr[16] = {0};
    struct sockaddr_in addr;
    int socket_fd = -1;
    int ret = 0;
    int errno;
    int root_ca_len = 0;
    if (pNetwork == NULL)
    {
        rc = NULL_VALUE_ERROR;
        goto exit;
    }
    if (Params)
    {
        TCP_tls_set_connect_params(pNetwork,
                                   Params->pDestinationURL,
                                   Params->DestinationPort,
                                   Params->timeout_ms);
    }
    elan_tcp_log("TCP_Connect");
    err = usergethostbyname(pNetwork->tlsConnectParams.pDestinationURL, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        elan_tcp_log("ERROR: Unable to resolute the host address.");
        rc = TCP_CONNECTION_ERROR;
        goto exit;
    }
    elan_tcp_log("host:%s, ip:%s, port:%d", pNetwork->tlsConnectParams.pDestinationURL, ipstr, pNetwork->tlsConnectParams.DestinationPort);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(pNetwork->tlsConnectParams.DestinationPort); //

    ret = user_set_tcp_keepalive(socket_fd, 3000, 3000, 6, 3, 3);
    if (ret < 0)
    {
        elan_tcp_log("ERROR: Unable to resolute the tcp connect");
        rc = TCP_SETUP_ERROR;
        goto exit;
    }

    elan_tcp_log("start connect");
    err = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (err != kNoErr)
    {
        elan_tcp_log("#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
        elan_tcp_log("ERROR: Unable to resolute the tcp connect err:%d", err);
        rc = TCP_CONNECTION_ERROR;
        goto exit;
    }

    if (pNetwork->tlsConnectParams.isUseSSL == true)
    {
        ssl_set_client_version(TLS_V1_2_MODE);
        if ((pNetwork->tlsConnectParams.pDeviceCertLocation != NULL) && (pNetwork->tlsConnectParams.pDevicePrivateKeyLocation != NULL))
        {
            pNetwork->tlsDataParams.clicert = pNetwork->tlsConnectParams.pDeviceCertLocation;
            pNetwork->tlsDataParams.pkey = pNetwork->tlsConnectParams.pDevicePrivateKeyLocation;
            ssl_set_client_cert(pNetwork->tlsDataParams.clicert, pNetwork->tlsDataParams.pkey);
            elan_tcp_log("use client ca");
        }

        if ((pNetwork->tlsConnectParams.ServerVerificationFlag == true))
        {
            pNetwork->tlsDataParams.cacert = pNetwork->tlsConnectParams.pRootCALocation;
            root_ca_len = strlen(pNetwork->tlsDataParams.cacert);
            elan_tcp_log("use server ca");
        }
        else
        {
            pNetwork->tlsDataParams.cacert = NULL;
            root_ca_len = 0;
        }
        elan_tcp_log("start ssl_connect");
        pNetwork->tlsDataParams.ssl = ssl_connect(socket_fd,
                                                  root_ca_len,
                                                  pNetwork->tlsDataParams.cacert, &errno);
        elan_tcp_log("fd: %d, err:  %d", socket_fd, errno);
        if (pNetwork->tlsDataParams.ssl == NULL)
        {
            elan_tcp_log("ssl connect err");
            close(socket_fd);
            pNetwork->tlsDataParams.server_fd = -1;
            err = kGeneralErr;
            rc = SSL_CONNECTION_ERROR;
            goto exit;
        }
        elan_tcp_log("ssl connected");
    }
    pNetwork->tlsDataParams.server_fd = socket_fd;
    return TCP_SUCCESS;
exit:
    close(socket_fd);
    return rc;
}
TCP_Error_t TCP_Write(Network_t *pNetwork,
                      uint8_t *pMsg,
                      _time_t *timer,
                      size_t *written_len)
{
    _TELEGRAM_t telegram;
    size_t written_so_far;
    size_t len;
    bool isError_flag = false;
    int frags, ret = 0;
    _time_t current_temp, timeforward = {0, 0};

    memcpy(&telegram, pMsg, sizeof(_TELEGRAM_t));
    len = telegram.lenth + sizeof(_TELEGRAM_t);

    gettimeofday(&current_temp, NULL);
    timeradd(&current_temp, timer, &timeforward);
    time_record(SET_ELAND_SEND_TIME, NULL);
    for (written_so_far = 0, frags = 0; written_so_far < len && !has_timer_expired(&timeforward); written_so_far += ret, frags++)
    {
        while ((!has_timer_expired(&timeforward)) && ((ret = ssl_send(pNetwork->tlsDataParams.ssl, pMsg + written_so_far, len - written_so_far)) <= 0))
        {
            if (ret < 0)
            {
                elan_tcp_log(" failed");
                /* All other negative return values indicate connection needs to be reset.
                 * Will be caught in ping request so ignored here */
                isError_flag = true;
                break;
            }
        }
        if (isError_flag)
        {
            break;
        }
    }
    *written_len = written_so_far;
    elan_tcp_log("socket write done");
    if (isError_flag)
    {
        return NETWORK_SSL_WRITE_ERROR;
    }
    else if (has_timer_expired(timer) && written_so_far != len)
    {
        return NETWORK_SSL_WRITE_TIMEOUT_ERROR;
    }
    return TCP_SUCCESS;
}
TCP_Error_t TCP_Read(Network_t *pNetwork,
                     uint8_t **pMsg,
                     _time_t *timer,
                     size_t *read_len)
{
    _TELEGRAM_t telegram;
    size_t rxlen = 0;
    int ret = 0;
    int fd = pNetwork->tlsDataParams.server_fd;
    fd_set readfds;
    _time_t time, current_temp, timeforward = {0, 0};
    size_t len = sizeof(_TELEGRAM_t);
    uint8_t *pMsg_p = NULL;

    pMsg_p = calloc(1, len + 1);
    *pMsg = pMsg_p;
startread:
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    if (timer != NULL)
    {
        memcpy(&time, timer, sizeof(_time_t));
        gettimeofday(&current_temp, NULL);
        timeradd(&current_temp, timer, &timeforward);
    }

    while (len > 0)
    {
        if (pNetwork->tlsConnectParams.isUseSSL == true)
        {
            if (ssl_pending(pNetwork->tlsDataParams.ssl))
            {
                ret = ssl_recv(pNetwork->tlsDataParams.ssl, pMsg_p, len);
            }
            else
            {
                ret = select(fd + 1, &readfds, NULL, NULL, &time);
                elan_tcp_log("select ret %d fd %d", ret, fd);
                if (ret <= 0)
                {
                    break;
                }
                if (!FD_ISSET(fd, &readfds))
                {
                    elan_tcp_log("fd is set err");
                    break;
                }
                ret = ssl_recv(pNetwork->tlsDataParams.ssl, pMsg_p, len);
            }
        }
        if (ret >= 0)
        {
            if (ret == 0)
                goto startread;
            if (rxlen == 0) //set len at first time
            {
                memcpy(&telegram, *pMsg, sizeof(_TELEGRAM_t));
                if (telegram.lenth > 0)
                {
                    len = telegram.lenth + sizeof(_TELEGRAM_t);
                    pMsg_p = realloc(*pMsg, len + 1);
                    if (*pMsg == NULL)
                    {
                        return BACK_DATA_MEMERY_ERROR;
                    }
                    *pMsg = pMsg_p;
                }
            }
            rxlen += ret;
            pMsg_p += ret;
            len -= ret;
        }
        else if (ret < 0)
        {
            elan_tcp_log("socket read err");
            return NETWORK_SSL_READ_ERROR;
        }
        // Evaluate timeout after the read to make sure read is done at least once
        if (has_timer_expired(&timeforward))
        {
            elan_tcp_log("read time out");
            break;
        }
    }
    if (len == 0)
    {
        *read_len = rxlen;
        time_record(SET_ELAND_RECE_TIME, NULL);
        return TCP_SUCCESS;
    }
    if (rxlen == 0)
    {
        return NETWORK_SSL_NOTHING_TO_READ;
    }
    else
    {
        return NETWORK_SSL_READ_TIMEOUT_ERROR;
    }
}
TCP_Error_t TCP_disconnect(Network_t *pNetwork)
{

    /* All other negative return values indicate connection needs to be reset.
     * No further action required since this is disconnect call */
    if (pNetwork->tlsDataParams.ssl != NULL)
    {
        ssl_close(pNetwork->tlsDataParams.ssl);
        pNetwork->tlsDataParams.ssl = NULL;
    }
    if (pNetwork->tlsDataParams.server_fd != -1)
    {
        close(pNetwork->tlsDataParams.server_fd);
        pNetwork->tlsDataParams.server_fd = -1;
    }
    return TCP_SUCCESS;
}
TCP_Error_t TCP_tls_destroy(Network_t *pNetwork)
{
    return TCP_SUCCESS;
}

TCP_Error_t TCP_Client_Init(_Client_t *pClient, ServerParams_t *ServerParams)
{
    OSStatus err;
    if (pClient == NULL)
        return NULL_VALUE_ERROR;
    pClient->clientStatus.clientState = CLIENT_STATE_INVALID;
    memset(pClient, 0, sizeof(_Client_t));

    if (ServerParams != NULL)
        TCP_tls_set_connect_params(&(pClient->networkStack),
                                   ServerParams->pDestinationURL,
                                   ServerParams->DestinationPort,
                                   ServerParams->timeout_ms);

    pClient->clientData.packetTimeoutMs = 20000;
    pClient->clientData.commandTimeoutMs = 20000;

    if ((pClient->networkStack.tlsConnectParams.pRootCALocation == NULL) ||
        (pClient->networkStack.tlsConnectParams.pDeviceCertLocation == NULL) ||
        (pClient->networkStack.tlsConnectParams.pDevicePrivateKeyLocation == NULL) ||
        (pClient->networkStack.tlsConnectParams.pDestinationURL == NULL) ||
        (pClient->networkStack.tlsConnectParams.DestinationPort == 0))
        return NULL_VALUE_ERROR;

    pClient->clientData.isBlockOnThreadLockEnabled = true;
    pClient->clientData.readBuf = NULL;

    err = mico_rtos_init_mutex(&(pClient->clientData.state_change_mutex));
    if (err != kNoErr)
        return MUTEX_INIT_ERROR;
    err = mico_rtos_init_mutex(&(pClient->clientData.tls_read_mutex));
    if (err != kNoErr)
        return MUTEX_INIT_ERROR;
    err = mico_rtos_init_mutex(&(pClient->clientData.tls_write_mutex));
    if (err != kNoErr)
        return MUTEX_INIT_ERROR;

    pClient->clientStatus.clientState = CLIENT_STATE_INITIALIZED;
    pClient->clientStatus.isAutoReconnectEnabled = true; // auto reconnect

    return TCP_SUCCESS;
}

OSStatus TCP_Service_Start(void)
{
    OSStatus err = kGeneralErr;
    require_string(get_wifi_status() == true, exit, "wifi is not connect");
    /*初始化互斥信号量*/

    //TCP_queue connect message
    err = mico_rtos_init_queue(&TCP_queue, "TCP_queue", sizeof(_tcp_cmd_sem_t), 5);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "TCP_Thread", TCP_thread_main,
                                  0x3000, (mico_thread_arg_t)NULL);
    require_noerr(err, exit);
exit:
    elan_tcp_log("TCP_Service_Start err = %d", err);
    return err;
}
static void TCP_thread_main(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    TCP_Error_t rc = TCP_SUCCESS;
    _Client_t Eland_Client;
    bool tcp_HC_flag = true;
    uint8_t HC00_moment_sec = 61, HC00_moment_min = 61;
    ServerParams_t serverPara;
    _time_t timer;
    mico_rtc_time_t cur_time = {0};
    _tcp_cmd_sem_t tcp_message;

    HC00_moment_sec = (netclock_des_g->eland_id) % 100 % 60;
GET_CONNECT_INFO:
    /*pop queue*/
    err = mico_rtos_pop_from_queue(&TCP_queue, &tcp_message, 0);
    if ((err == kNoErr) && (tcp_message = TCP_Stop_Sem))
        mico_rtos_delete_thread(NULL);

    if (TCP_Physical_is_connected(&(Eland_Client.networkStack)) == NETWORK_MANUALLY_DISCONNECTED)
    {
        elan_tcp_log("PHYSICAL_LAYER_DISCONNECTED WAIT");
        mico_rtos_thread_sleep(1);
        goto GET_CONNECT_INFO;
    }
    err = eland_communication_info_get();
    require_noerr(err, GET_CONNECT_INFO);

    if ((netclock_des_g != NULL) && (strlen(netclock_des_g->tcpIP_host) != 0))
    {
        elan_tcp_log("tcpIP_host");
        serverPara.pDestinationURL = netclock_des_g->tcpIP_host;
        serverPara.DestinationPort = netclock_des_g->tcpIP_port;
    }
    else
    {
        elan_tcp_log("default");
        serverPara.pDestinationURL = ELAND_HTTP_DOMAIN_NAME;
        serverPara.DestinationPort = 6380;
    }
    rc = TCP_Client_Init(&Eland_Client, &serverPara);
    if (TCP_SUCCESS != rc)
    {
        elan_tcp_log("Shadow Connection Error");
        goto exit;
    }

RECONN:
    err = mico_rtos_pop_from_queue(&TCP_queue, &tcp_message, 0);
    if ((err == kNoErr) && (tcp_message = TCP_Stop_Sem))
    {
        Eland_Client.networkStack.disconnect(&Eland_Client.networkStack);
        Eland_Client.networkStack.destroy(&Eland_Client.networkStack);
        mico_rtos_delete_thread(NULL);
    }

    elan_tcp_log("Shadow Connect...");
    rc = eland_tcp_connect(&Eland_Client, NULL);
    elan_tcp_log("Connect.over..");
    if (TCP_SUCCESS != rc)
    {
        elan_tcp_log("Server Connection Error,rc = %d", rc);
        if (rc == NETWORK_MANUALLY_DISCONNECTED)
            mico_thread_sleep(5);
        else
            mico_thread_sleep(3);
        goto RECONN;
    }
cycle_loop:

    elan_tcp_log("TCP_connection_request");
    /**Connection Request**/
    rc = TCP_connection_request(&Eland_Client);
    if (TCP_SUCCESS != rc)
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit;
        else if (rc == NETWORK_SSL_NOTHING_TO_READ)
            goto cycle_loop;
    }
    /**eland info**/
    rc = TCP_update_elandinfo(&Eland_Client);
    if (TCP_SUCCESS != rc)
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit;
    }
    /**health check**/
    rc = TCP_health_check(&Eland_Client);
    if (TCP_SUCCESS != rc)
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
    }
    /**holiday data **/
    rc = TCP_update_holiday(&Eland_Client);
    if (TCP_SUCCESS != rc)
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit;
    }
    /**alarm info **/
    rc = TCP_update_alarm(&Eland_Client);
    if (TCP_SUCCESS != rc)
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit;
    }
    /**Alarm clock schedule**/
    rc = TCP_request_response(&Eland_Client, SD00, SD01);
    {
        mico_thread_sleep(1);
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit;
    }
little_cycle_loop:
    timer.tv_sec = 1;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(&Eland_Client, &timer);
    if (TCP_SUCCESS != rc)
    {
        elan_tcp_log("Connection Error rc = %d", rc);
        if (rc == NETWORK_SSL_READ_ERROR)
            goto exit; //reconnect
    }
    MicoRtcGetTime(&cur_time);
    elan_tcp_log("cur_time.sec:%d", cur_time.sec);
    if ((cur_time.sec == HC00_moment_sec) && (cur_time.min != HC00_moment_min))
    {
        tcp_HC_flag = true;
        HC00_moment_min = cur_time.min;
    }
    if (tcp_HC_flag)
    {
        rc = TCP_health_check(&Eland_Client);
        if (TCP_SUCCESS != rc)
        {
            mico_thread_sleep(1);
            elan_tcp_log("Connection Error rc = %d", rc);
        }
        else
            tcp_HC_flag = false;
    }
pop_queue:
    /*pop queue*/
    err = mico_rtos_pop_from_queue(&TCP_queue, &tcp_message, 0);
    if (err == kNoErr)
    {
        /*******upload alarm on notification********/
        if (tcp_message == TCP_HT00_Sem)
            rc = TCP_upload(&Eland_Client, HT00);
        /*******upload alarm history********/
        else if (tcp_message == TCP_HT01_Sem)
            rc = TCP_upload(&Eland_Client, HT01);
        /*******upload alarm skip notice********/
        else if (tcp_message == TCP_HT02_Sem)
            rc = TCP_upload(&Eland_Client, HT02);
        /*******delete tcp thread***********/
        else if (tcp_message == TCP_Stop_Sem)
        {
            elan_tcp_log("TCP_Stop_Sem");
            Eland_Client.networkStack.disconnect(&Eland_Client.networkStack);
            Eland_Client.networkStack.destroy(&Eland_Client.networkStack);
            if (Eland_Client.clientData.readBuf != NULL)
            {
                free(Eland_Client.clientData.readBuf);
                Eland_Client.clientData.readBuf = NULL;
            }
            mico_rtos_delete_thread(NULL);
        }
        else if (tcp_message == TCP_HC00_Sem)
        {
            tcp_HC_flag = true;
        }
        else if (tcp_message == TCP_FW01_Sem)
        {
            rc = TCP_upload(&Eland_Client, FW01);
            if (TCP_SUCCESS == rc)
            {
                set_eland_mode(ELAND_OTA);
                eland_push_uart_send_queue(SEND_LINK_STATE_08);
                eland_push_http_queue(DOWNLOAD_OTA);
            }
        }

        if (TCP_SUCCESS != rc)
        {
            mico_thread_sleep(1);
            elan_tcp_log("Connection Error rc = %d", rc);
        }
        else
            goto pop_queue;
    }
    goto little_cycle_loop;
exit:
    Eland_Client.networkStack.disconnect(&Eland_Client.networkStack);
    Eland_Client.networkStack.destroy(&Eland_Client.networkStack);
    SendElandStateQueue(WifyConnected);

    goto RECONN;
    if (Eland_Client.clientData.readBuf != NULL)
    {
        free(Eland_Client.clientData.readBuf);
        Eland_Client.clientData.readBuf = NULL;
    }
    mico_rtos_delete_thread(NULL);
}
static TCP_Error_t eland_tcp_connect(_Client_t *pClient, ServerParams_t *ServerParams)
{
    TCP_Error_t rc = TCP_SUCCESS;
    ClientState_t clientState;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    rc = TCP_Physical_is_connected(&(pClient->networkStack));
    if (rc != NETWORK_PHYSICAL_LAYER_CONNECTED)
    {
        elan_tcp_log("PHYSICAL_LAYER_DISCONNECTED WAIT");
        return rc;
    }

    clientState = eland_get_client_state(pClient);
    eland_set_client_state(pClient, clientState, CLIENT_STATE_CONNECTING);

    rc = pClient->networkStack.connect(&(pClient->networkStack), ServerParams);
    if (TCP_SUCCESS != rc)
    {
        pClient->networkStack.disconnect(&(pClient->networkStack));
        pClient->networkStack.destroy(&(pClient->networkStack));
        eland_set_client_state(pClient, CLIENT_STATE_CONNECTING, CLIENT_STATE_DISCONNECTED_ERROR);
    }
    else
        eland_set_client_state(pClient, CLIENT_STATE_CONNECTING, CLIENT_STATE_CONNECTED_IDLE);
    return rc;
}

static TCP_Error_t TCP_connection_request(_Client_t *pClient)
{
    TCP_Error_t rc = TCP_SUCCESS;
    ClientState_t clientState;
    _TELEGRAM_t *telegram;
    _time_t timer;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, CN00, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);
    if (rc != TCP_SUCCESS)
        return rc;
    elan_tcp_log("connection_request:OK");
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[CN01], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;
    if (rc == TCP_SUCCESS)
    {
        SendElandStateQueue(TCP_CN00);
        clientState = eland_get_client_state(pClient);
        eland_set_client_state(pClient, clientState, CLIENT_STATE_CONNECTING);
    }

    return rc;
}
static TCP_Error_t TCP_update_elandinfo(_Client_t *pClient)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    _TELEGRAM_t *telegram;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, DV00, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);

    if (rc != TCP_SUCCESS)
        return rc;
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[DV01], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;

    return rc;
}

static TCP_Error_t TCP_update_alarm(_Client_t *pClient)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    _TELEGRAM_t *telegram;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;
    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, AL00, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);

    if (rc != TCP_SUCCESS)
        return rc;
    elan_tcp_log("update_alarm:OK");
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[AL01], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;

    return rc;
}

static TCP_Error_t TCP_update_holiday(_Client_t *pClient)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    _TELEGRAM_t *telegram;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, HD00, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);

    if (rc != TCP_SUCCESS)
        return rc;
    elan_tcp_log("update_holiday:OK");
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[HD01], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;
    if (rc == TCP_SUCCESS)
        SendElandStateQueue(TCP_HD00);

    return rc;
}
static TCP_Error_t TCP_health_check(_Client_t *pClient)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    _TELEGRAM_t *telegram;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 5;
    timer.tv_usec = 0;

    rc = TCP_send_packet(pClient, HC00, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);

    if (rc != TCP_SUCCESS)
        return rc;
    eland_set_time();
    elan_tcp_log("health_check:OK");
    // elan_tcp_log("health_check:OK json:%s", pClient->clientData.readBuf + sizeof(_TELEGRAM_t));
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[HC01], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;
    if (rc == TCP_SUCCESS)
        SendElandStateQueue(TCP_HC00);

    return rc;
}
static TCP_Error_t TCP_request_response(_Client_t *pClient, _TCP_CMD_t request, _TCP_CMD_t response)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    _TELEGRAM_t *telegram;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 2;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, request, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    timer.tv_sec = 2;
    timer.tv_usec = 0;
    rc = TCP_receive_packet(pClient, &timer);

    if (rc != TCP_SUCCESS)
        return rc;
    elan_tcp_log("eland_IF:OK");
    telegram = (_TELEGRAM_t *)pClient->clientData.readBuf;
    if (strncmp(telegram->command, CommandTable[response], COMMAND_LEN) != 0)
        rc = CMD_BACK_ERROR;

    return rc;
}
static TCP_Error_t TCP_upload(_Client_t *pClient, _TCP_CMD_t tcp_cmd)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _time_t timer;
    if (NULL == pClient)
        return NULL_VALUE_ERROR;

    timer.tv_sec = 5;
    timer.tv_usec = 0;
    rc = TCP_send_packet(pClient, tcp_cmd, &timer);
    if (rc != TCP_SUCCESS)
        return rc;

    elan_tcp_log("upload %c%c%c%c:OK",
                 CommandTable[tcp_cmd][0],
                 CommandTable[tcp_cmd][1],
                 CommandTable[tcp_cmd][2],
                 CommandTable[tcp_cmd][3]);
    return rc;
}

static TCP_Error_t TCP_send_packet(_Client_t *pClient, _TCP_CMD_t cmd_type, _time_t *timer)
{
    size_t wrtied_len = 0;
    TCP_Error_t rc = TCP_SUCCESS;
    if (NULL == pClient)
    {
        return NULL_VALUE_ERROR;
    }
    HandleRequeseCallbacks(pClient->clientData.writeBuf, cmd_type);

    rc = pClient->networkStack.write(&(pClient->networkStack),
                                     pClient->clientData.writeBuf,
                                     timer,
                                     &wrtied_len);
    return rc;
}

static void HandleRequeseCallbacks(uint8_t *pMsg, _TCP_CMD_t cmd_type)
{
    _TELEGRAM_t *telegram = (_TELEGRAM_t *)pMsg;
    char *telegram_data = NULL;
    AlarmOffHistoryData_t *history_data_p = NULL;
    static uint16_t telegram_squence_num = 0;

    memset(pMsg + sizeof(_TELEGRAM_t), 0, MQTT_TX_BUF_LEN - sizeof(_TELEGRAM_t));

    memcpy(telegram->head, TELEGRAMHEADER, strlen(TELEGRAMHEADER));
    telegram->squence_num = telegram_squence_num++;
    memcpy(telegram->command, CommandTable[cmd_type], COMMAND_LEN);
    telegram->reserved = 0;
    telegram->lenth = 0;
    telegram_data = (char *)(pMsg + sizeof(_TELEGRAM_t));
    switch (cmd_type)
    {
    case CN00:
        /*build eland json data*/
        InitUpLoadData(telegram_data);
        telegram->lenth = strlen(telegram_data);
        elan_tcp_log("CN00 len:%ld,json:%s", telegram->lenth, telegram_data);
        break;
    case HT00:
        /*build alarm json data*/
        Alarm_build_JSON(telegram_data);
        telegram->lenth = strlen(telegram_data);
        elan_tcp_log("HT00 len:%ld,json:%s", telegram->lenth, telegram_data);
        break;
    case HT01:
        history_data_p = get_alarm_history_data();
        if (history_data_p)
            alarm_off_history_json_data_build(history_data_p, (char *)telegram_data);
        telegram->lenth = strlen(telegram_data);
        elan_tcp_log("HT01 len:%ld,json:%s", telegram->lenth, telegram_data);
        set_alarm_history_data_state(DONE_UPLOAD);
        break;
    case HT02:
        /*build alarm json data*/
        Alarm_build_JSON(telegram_data);
        telegram->lenth = strlen(telegram_data);
        elan_tcp_log("HT02 len:%ld,json:%s", telegram->lenth, telegram_data);
        break;
    case FW01:
        break;
    case SD00:
        break;
    default:
        break;
    }
}
static TCP_Error_t TCP_receive_packet(_Client_t *pClient, _time_t *timer)
{
    TCP_Error_t rc = TCP_SUCCESS;
    size_t readed_len = 0;
    if (NULL == pClient)
    {
        return NULL_VALUE_ERROR;
    }

    if (pClient->clientData.readBuf != NULL)
    {
        free(pClient->clientData.readBuf);
        pClient->clientData.readBuf = NULL;
    }
    rc = pClient->networkStack.read(&(pClient->networkStack),
                                    (uint8_t **)(&(pClient->clientData.readBuf)),
                                    timer,
                                    &readed_len);
    if (rc == TCP_SUCCESS)
        TCP_Operate((const char *)pClient->clientData.readBuf);

    return rc;
}

ClientState_t eland_get_client_state(_Client_t *pClient)
{
    if (NULL == pClient)
    {
        return CLIENT_STATE_INVALID;
    }

    return pClient->clientStatus.clientState;
}

static TCP_Error_t eland_set_client_state(_Client_t *pClient, ClientState_t expectedCurrentState, ClientState_t newState)
{
    TCP_Error_t rc = TCP_SUCCESS;
    OSStatus err = kNoErr;
    if (NULL == pClient)
    {
        return NULL_VALUE_ERROR;
    }
    err = mico_rtos_lock_mutex(&(pClient->clientData.state_change_mutex));
    if (err != kNoErr)
        return MUTEX_LOCK_ERROR;

    if (expectedCurrentState == eland_get_client_state(pClient))
    {
        pClient->clientStatus.clientState = newState;
        rc = TCP_SUCCESS;
    }
    else
    {
        rc = TCP_UNEXPECTED_CLIENT_STATE_ERROR;
    }
    err = mico_rtos_unlock_mutex(&(pClient->clientData.state_change_mutex));

    if ((err != kNoErr) && (rc == TCP_SUCCESS))
        return MUTEX_UNLOCK_ERROR;

    return rc;
}
static TCP_Error_t TCP_Operate(const char *buff)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _TELEGRAM_t *telegram;
    _TCP_CMD_t tep_cmd = TCPCMD_NONE;
    uint8_t i;
    if (NULL == buff)
    {
        return NULL_VALUE_ERROR;
    }
    telegram = (_TELEGRAM_t *)buff;

    for (i = CN00; i < TCPCMD_MAX; i++)
    {
        if (strncmp(telegram->command, CommandTable[i], COMMAND_LEN) == 0)
            break;
    }
    tep_cmd = (_TCP_CMD_t)i;
    elan_tcp_log("command:%s,telegram:%s", telegram->command, (char *)(buff + sizeof(_TELEGRAM_t)));
    switch (tep_cmd)
    {
    case CN00: //00 Connection Request
        break;
    case CN01: //01 Connection Response
        break;
    case HC00: //02 health check request
        break;
    case HC01: //03 health check response
        rc = TCP_Operate_HC01((char *)(buff + sizeof(_TELEGRAM_t)));
        break;
    case DV00: //04 eland info request
        break;
    case DV01: //05 eland info response
    case DV02: //06 eland info change Notification
        SendElandStateQueue(TCP_DV00);
        //   elan_tcp_log("command:%s,telegram:%s", telegram->command, (char *)(buff + sizeof(_TELEGRAM_t)));
        rc = TCP_Operate_DV01((char *)(buff + sizeof(_TELEGRAM_t)));
        break;
    case DV03: //07 eland info remove Notification
        reset_eland_flash_para(ELAND_DELETE_0E);
        break;
    case AL00: //08 alarm info request
        break;
    case AL01: //09 alarm info response
    case AL02: //10 alarm info add Notification
    case AL03: //11 alarm info change Notification
    case AL04: //12 alarm info delete notification
        rc = TCP_Operate_ALXX((char *)(buff + sizeof(_TELEGRAM_t)), tep_cmd);
        SendElandStateQueue(TCP_AL00);
        break;
    case HD00: //13 holiday data request
        break;
    case HD01: //14 holiday data response
    case HD02: //15 holiday data change notice
        rc = TCP_Operate_HD0X((char *)(buff + sizeof(_TELEGRAM_t)), &holiday_list);
        elan_tcp_log("##### memory debug:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
        break;
    case HT00: //16 alarm on notification
        break;
    case HT01: //17 alarm off notification
        break;
    case HT02: //18 alarm jump over notification
        break;
    case FW00: //19 firmware update start request
        rc = TCP_Operate_FW00((char *)(buff + sizeof(_TELEGRAM_t)));
        break;
    case FW01: //20 firmwart update start response
        break;
    case SD00: //21 Alarm clock schedule request
        break;
    case SD01: //22 Alarm clock schedule response
        rc = TCP_Operate_SD01((char *)(buff + sizeof(_TELEGRAM_t)));
        break;
    case TCPCMD_MAX:
        break;
    default:
        break;
    }
    return rc;
}

static TCP_Error_t TCP_Operate_HC01(char *buf)
{
    json_object *ReceivedJsonCache = NULL;
    char time_str_cache[30];
    DATE_TIME_t datetimeTemp;
    mico_utc_time_t iMsecond;

    memset(time_str_cache, 0, 30);
    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        return JSON_PARSE_ERROR;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        return JSON_PARSE_ERROR;
    }
    json_object_object_foreach(ReceivedJsonCache, key, val)
    {
        if (!strcmp(key, "received_at"))
        {
            sprintf(time_str_cache, "%s", json_object_get_string(val));
            sscanf(time_str_cache, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%03hd", &datetimeTemp.iYear, &datetimeTemp.iMon, &datetimeTemp.iDay, &datetimeTemp.iHour, &datetimeTemp.iMin, &datetimeTemp.iSec, &datetimeTemp.iMsec);
            iMsecond = (mico_utc_time_t)GetSecondTime(&datetimeTemp);
            time_record(SET_ELSV_RECE_TIME, &iMsecond);
        }
        else if (!strcmp(key, "send_at"))
        {
            sprintf(time_str_cache, "%s", json_object_get_string(val));
            sscanf(time_str_cache, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%03hd", &datetimeTemp.iYear, &datetimeTemp.iMon, &datetimeTemp.iDay, &datetimeTemp.iHour, &datetimeTemp.iMin, &datetimeTemp.iSec, &datetimeTemp.iMsec);
            iMsecond = (mico_utc_time_t)GetSecondTime(&datetimeTemp);
            time_record(SET_ELSV_SEND_TIME, &iMsecond);
        }
    }
    free_json_obj(&ReceivedJsonCache);
    return TCP_SUCCESS;
}
static TCP_Error_t TCP_Operate_DV01(char *buf)
{
    TCP_Error_t rc = TCP_SUCCESS;
    __msg_function_t eland_cmd = ELAND_DATA_0C;
    bool des_data_chang_flag = false;

    json_object *ReceivedJsonCache = NULL, *device_JSON = NULL;
    ELAND_DES_S des_data_cache;
    memset(&des_data_cache, 0, sizeof(ELAND_DES_S));
    memcpy(&des_data_cache, netclock_des_g, sizeof(ELAND_DES_S));

    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    device_JSON = json_object_object_get(ReceivedJsonCache, "device");
    if ((device_JSON == NULL) || ((json_object_get_object(device_JSON)->head) == NULL))
    {
        elan_tcp_log("get device_JSON error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }

    json_object_object_foreach(device_JSON, key, val)
    {
        if (!strcmp(key, "eland_id"))
        {
            if (des_data_cache.eland_id != json_object_get_int(val))
            {
                elan_tcp_log("get eland_id error");
                rc = JSON_PARSE_ERROR;
                goto exit;
            }
        }
        else if (!strcmp(key, "user_id"))
        {
            memset(des_data_cache.user_id, 0, sizeof(des_data_cache.user_id));
            sprintf(des_data_cache.user_id, "%s", json_object_get_string(val));
            if (!strncmp(des_data_cache.user_id, "\0", 1))
            {
                elan_tcp_log("user_id  = %s", des_data_cache.user_id);
                rc = JSON_PARSE_ERROR;
                goto exit;
            }
        }
        else if (!strcmp(key, "eland_name"))
        {
            memset(des_data_cache.eland_name, 0, sizeof(des_data_cache.eland_name));
            sprintf(des_data_cache.eland_name, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "timezone_offset_sec"))
        {
            des_data_cache.timezone_offset_sec = json_object_get_int(val);
        }
        else if (!strcmp(key, "area_code"))
        {
            des_data_cache.area_code = json_object_get_int(val);
        }
        else if (!strcmp(key, "serial_number"))
        {
            if (strcmp(des_data_cache.serial_number, json_object_get_string(val)))
            {
                elan_tcp_log("serial des:%s", des_data_cache.serial_number);
                elan_tcp_log("serial elsv:%s", json_object_get_string(val));
                rc = JSON_PARSE_ERROR;
                goto exit;
            }
        }
        else if (!strcmp(key, "dhcp_enabled"))
        {
            des_data_cache.dhcp_enabled = json_object_get_int(val);
        }
        else if (!strcmp(key, "time_display_format"))
        {
            des_data_cache.time_display_format = json_object_get_int(val);
        }
        else if (!strcmp(key, "brightness_normal"))
        {
            des_data_cache.brightness_normal = json_object_get_int(val);
        }
        else if (!strcmp(key, "brightness_night"))
        {
            des_data_cache.brightness_night = json_object_get_int(val);
        }
        else if (!strcmp(key, "led_normal"))
        {
            des_data_cache.led_normal = json_object_get_int(val);
        }
        else if (!strcmp(key, "led_night"))
        {
            des_data_cache.led_night = json_object_get_int(val);
        }
        else if (!strcmp(key, "notification_volume_normal"))
        {
            des_data_cache.notification_volume_normal = json_object_get_int(val);
        }
        else if (!strcmp(key, "notification_volume_night"))
        {
            des_data_cache.notification_volume_night = json_object_get_int(val);
        }
        else if (!strcmp(key, "night_mode_enabled"))
        {
            des_data_cache.night_mode_enabled = json_object_get_int(val);
        }
        else if (!strcmp(key, "night_mode_begin_time"))
        {
            sprintf(des_data_cache.night_mode_begin_time, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "night_mode_end_time"))
        {
            sprintf(des_data_cache.night_mode_end_time, "%s", json_object_get_string(val));
        }
    }
    mico_rtos_lock_mutex(&netclock_des_g->des_mutex);
    if (des_data_cache.timezone_offset_sec !=
        netclock_des_g->timezone_offset_sec)
        des_data_chang_flag = true;
    memcpy(netclock_des_g, &des_data_cache, sizeof(ELAND_DES_S));
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 0);
    /**refresh flash inside**/
    eland_update_flash();
    /**stop tcp communication**/
    if (des_data_chang_flag)
        TCP_Push_MSG_queue(TCP_HC00_Sem);
exit:
    free_json_obj(&ReceivedJsonCache);
    return rc;
}

static TCP_Error_t TCP_Operate_ALXX(char *buf, _TCP_CMD_t telegram_cmd)
{
    json_object *ReceivedJsonCache = NULL, *alarm_array = NULL, *alarm = NULL;
    TCP_Error_t rc = TCP_SUCCESS;
    uint8_t list_len = 0, i;
    __elsv_alarm_data_t alarm_data_cache;
    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);

    if (telegram_cmd == AL01)
    {
        alarm_array = json_object_object_get(ReceivedJsonCache, "alarms");
        if ((alarm_array == NULL) || ((json_object_get_object(alarm_array)->head) == NULL))
        {
            elan_tcp_log("get alarm_array error");
            rc = JSON_PARSE_ERROR;
            goto exit;
        }
        list_len = json_object_array_length(alarm_array);
        elan_tcp_log("alarm_array list_len:%d", list_len);
        for (i = 0; i < list_len; i++)
        {
            alarm = json_object_array_get_idx(alarm_array, i);
            TCP_Operate_AL_JSON(alarm, &alarm_data_cache);
            elan_tcp_log("moment_second:%ld", alarm_data_cache.alarm_data_for_eland.moment_second);
            elsv_alarm_data_sort_out(&alarm_data_cache);
            alarm_list_add(&alarm_list, &alarm_data_cache);
        }
    }
    else if ((telegram_cmd == AL02) ||
             (telegram_cmd == AL03) ||
             (telegram_cmd == AL04))
    {
        alarm = json_object_object_get(ReceivedJsonCache, "alarm");
        if ((alarm == NULL) || ((json_object_get_object(alarm)->head) == NULL))
        {
            elan_tcp_log("get alarm error");
            rc = JSON_PARSE_ERROR;
            goto exit;
        }
        TCP_Operate_AL_JSON(alarm, &alarm_data_cache);
        elsv_alarm_data_sort_out(&alarm_data_cache);
        if (telegram_cmd == AL04)
            alarm_list_minus(&alarm_list, &alarm_data_cache);
        else
            alarm_list_add(&alarm_list, &alarm_data_cache);
        elan_tcp_log("alarm_number:%d", alarm_list.alarm_number);
    }
exit:
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    free_json_obj(&ReceivedJsonCache);
    return rc;
}
void TCP_Operate_AL_JSON(json_object *alarm, __elsv_alarm_data_t *alarm_data)
{
    char alarm_on_dates_buffer[ALARM_OFF_DATES_LEN + 1]; //鬧鐘日期排列
    json_object *alarm_on_dates = NULL;
    uint8_t list_len = 0, i, j;
    uint16_t Cache;
    int year, month, day;
    memset(alarm_data, 0, sizeof(__elsv_alarm_data_t));
    json_object_object_foreach(alarm, key, val)
    {
        if (!strcmp(key, "alarm_id"))
        {
            sprintf(alarm_data->alarm_id, "%s", json_object_get_string(val));
            if (!strncmp(alarm_data->alarm_id, "\0", 1))
            {
                elan_tcp_log("alarm_id:%s", alarm_data->alarm_id);
                continue;
            }
        }
        if (!strcmp(key, "alarm_color"))
        {
            alarm_data->alarm_color = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_time"))
        {
            sprintf(alarm_data->alarm_time, "%s", json_object_get_string(val));
            if (!strncmp(alarm_data->alarm_time, "\0", 1))
            {
                elan_tcp_log("alarm_time = %s", alarm_data->alarm_time);
                continue;
            }
        }
        else if (!strcmp(key, "alarm_off_dates"))
        {
            memcpy(alarm_data->alarm_off_dates, json_object_get_string(val), sizeof(alarm_data->alarm_off_dates));
        }
        else if (!strcmp(key, "snooze_enabled"))
        {
            alarm_data->snooze_enabled = json_object_get_int(val);
        }
        else if (!strcmp(key, "snooze_count"))
        {
            alarm_data->snooze_count = json_object_get_int(val);
        }
        else if (!strcmp(key, "snooze_interval_min"))
        {
            alarm_data->snooze_interval_min = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_pattern"))
        {
            alarm_data->alarm_pattern = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_sound_id"))
        {
            alarm_data->alarm_sound_id = json_object_get_int(val);
        }
        else if (!strcmp(key, "voice_alarm_id"))
        {
            sprintf(alarm_data->voice_alarm_id, "%s", json_object_get_string(val));
            if (!strncmp(alarm_data->voice_alarm_id, "\0", 1))
            {
                elan_tcp_log("voice_alarm_id = %s", alarm_data->voice_alarm_id);
            }
        }
        else if (!strcmp(key, "alarm_off_voice_alarm_id"))
        {
            sprintf(alarm_data->alarm_off_voice_alarm_id, "%s", json_object_get_string(val));
            if (!strncmp(alarm_data->alarm_off_voice_alarm_id, "\0", 1))
            {
                elan_tcp_log("alarm_off_voice_alarm_id = %s", alarm_data->alarm_off_voice_alarm_id);
            }
        }
        else if (!strcmp(key, "alarm_volume"))
        {
            alarm_data->alarm_volume = json_object_get_int(val);
        }
        else if (!strcmp(key, "volume_stepup_enabled"))
        {
            alarm_data->volume_stepup_enabled = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_continue_min"))
        {
            alarm_data->alarm_continue_min = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_repeat"))
        {
            alarm_data->alarm_repeat = json_object_get_int(val);
        }
        else if (!strcmp(key, "alarm_on_days_of_week"))
        {
            sprintf(alarm_data->alarm_on_days_of_week, "%s", json_object_get_string(val));
            if (!strncmp(alarm_data->alarm_on_days_of_week, "\0", 1))
            {
                elan_tcp_log("alarm_on_days_of_week = %s", alarm_data->alarm_on_days_of_week);
            }
        }
        else if (!strcmp(key, "skip_flag"))
        {
            alarm_data->skip_flag = json_object_get_int(val);
        }
    }
    if (alarm_data->alarm_repeat == 5)
    {
        alarm_on_dates = json_object_object_get(alarm, "alarm_on_dates");
        list_len = json_object_array_length(alarm_on_dates);
        elan_tcp_log("list_len:%d ", list_len);
        memset(alarm_data->alarm_on_dates, 0, ALARM_ON_DATES_COUNT);

        for (i = 0; i < list_len; i++)
        {
            if (i < ALARM_ON_DATES_COUNT)
            {
                memset(alarm_on_dates_buffer, 0, ALARM_OFF_DATES_LEN + 1);
                sprintf(alarm_on_dates_buffer, "%s",
                        json_object_get_string(json_object_array_get_idx(alarm_on_dates, i)));
                sscanf((const char *)alarm_on_dates_buffer, "%04d-%02d-%02d", &year, &month, &day);
                alarm_data->alarm_on_dates[i] = (year % 100) * SIMULATE_DAYS_OF_YEAR +
                                                month * SIMULATE_DAYS_OF_MONTH + day;
                elan_tcp_log("alarm_on_dates%d:%d", i, alarm_data->alarm_on_dates[i]);
            }
            else
                break;
        }
        /*****Sequence*****/
        for (i = 0; i < list_len - 1; i++)
        {
            for (j = 0; j < list_len - i - 1; j++)
            {
                if (alarm_data->alarm_on_dates[j] > alarm_data->alarm_on_dates[j + 1])
                {
                    Cache = alarm_data->alarm_on_dates[j + 1];
                    alarm_data->alarm_on_dates[j + 1] = alarm_data->alarm_on_dates[j];
                    alarm_data->alarm_on_dates[j] = Cache;
                }
            }
        }
        free_json_obj(&alarm_on_dates);
    }
}
static TCP_Error_t TCP_Operate_HD0X(char *buf, _elsv_holiday_t *list)
{
    json_object *ReceivedJsonCache = NULL, *holiday_array = NULL;
    char holiday_dates_buffer[ALARM_OFF_DATES_LEN + 1]; //holiday buf
    int year, month, day;
    TCP_Error_t rc = TCP_SUCCESS;
    uint8_t list_len = 0, i;
    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    holiday_array = json_object_object_get(ReceivedJsonCache, "holidays");
    if ((holiday_array == NULL) || ((json_object_get_object(holiday_array)->head) == NULL))
    {
        elan_tcp_log("get holiday_array error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    mico_rtos_lock_mutex(&list->holidaymutex);
    if (list->number > 0)
    {
        list->number = 0;
        free(list->list);
        list->list = NULL;
    }
    list_len = json_object_array_length(holiday_array);
    list->list = calloc(list_len, sizeof(uint16_t));
    elan_tcp_log("holiday_array list_len:%d", list_len);
    for (i = 0; i < list_len; i++)
    {
        memset(holiday_dates_buffer, 0, ALARM_OFF_DATES_LEN + 1);
        sprintf(holiday_dates_buffer, "%s",
                json_object_get_string(json_object_array_get_idx(holiday_array, i)));
        sscanf((const char *)holiday_dates_buffer, "%04d-%02d-%02d", &year, &month, &day);
        *(list->list + i) = (year % 100) * SIMULATE_DAYS_OF_YEAR +
                            month * SIMULATE_DAYS_OF_MONTH + day;
        //  elan_tcp_log("holiday%d:%04d-%02d-%02d", i, year, month, day);
    }
    list->number = list_len;
    mico_rtos_unlock_mutex(&list->holidaymutex);
exit:
    free_json_obj(&ReceivedJsonCache);
    return rc;
}
static TCP_Error_t TCP_Operate_FW00(char *buf)
{
    json_object *ReceivedJsonCache = NULL, *firmware_json = NULL;
    TCP_Error_t rc = TCP_SUCCESS;
    char Cache_version[firmware_version_len];
    char Cache_url[URL_Len];
    char Cache_hash[hash_Len];
    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    firmware_json = json_object_object_get(ReceivedJsonCache, "FirmwareUpdateData");
    if ((firmware_json == NULL) || ((json_object_get_object(firmware_json)->head) == NULL))
    {
        elan_tcp_log("get firmware_json error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    json_object_object_foreach(firmware_json, key, val)
    {
        if (!strcmp(key, "version"))
        {
            memset(Cache_version, 0, firmware_version_len);
            sprintf(Cache_version, "%s", json_object_get_string(val));
            elan_tcp_log("version:%s", Cache_version);
            if (!strncmp(Cache_version, "\0", 1))
            {
                elan_tcp_log("version not Available");
                goto exit;
            }
        }
        else if (!strcmp(key, "download_url"))
        {
            memset(Cache_url, 0, URL_Len);
            sprintf(Cache_url, "%s", json_object_get_string(val));
            elan_tcp_log("download_url:%s", Cache_url);
            if (!strncmp(Cache_url, "\0", 1))
            {
                elan_tcp_log("download_url not Available");
                goto exit;
            }
        }
        else if (!strcmp(key, "hash"))
        {
            memset(Cache_hash, 0, hash_Len);
            sprintf(Cache_hash, "%s", json_object_get_string(val));
            elan_tcp_log("hash:%s", Cache_hash);
            if (!strncmp(Cache_hash, "\0", 1))
            {
                elan_tcp_log("hash not Available");
                goto exit;
            }
        }
    }
    mico_rtos_lock_mutex(&netclock_des_g->des_mutex);
    memcpy(netclock_des_g->OTA_version, Cache_version, firmware_version_len);
    memcpy(netclock_des_g->OTA_url, Cache_url, URL_Len);
    memcpy(netclock_des_g->OTA_hash, Cache_hash, hash_Len);
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    TCP_Push_MSG_queue(TCP_FW01_Sem);
exit:
    free_json_obj(&ReceivedJsonCache);
    return rc;
}
static TCP_Error_t TCP_Operate_SD01(char *buf)
{
    json_object *ReceivedJsonCache = NULL, *schedule_array = NULL, *schedule_json = NULL;
    uint8_t list_len, i;
    TCP_Error_t rc = TCP_SUCCESS;

    if (*buf != '{')
    {
        elan_tcp_log("error:received err json format data");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(buf));
    if (ReceivedJsonCache == NULL)
    {
        elan_tcp_log("json_tokener_parse error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }

    schedule_array = json_object_object_get(ReceivedJsonCache, "schedules");

    if ((schedule_array == NULL) || ((json_object_get_object(schedule_array)->head) == NULL))
    {
        elan_tcp_log("get schedule_array error");
        rc = JSON_PARSE_ERROR;
        goto exit;
    }
    list_len = json_object_array_length(schedule_array);
    elan_tcp_log("schedule_array list_len:%d", list_len);
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    memset(&alarm_list.schedules, 0, sizeof(alarm_list.schedules));
    alarm_list.schedules_num = list_len;
    for (i = 0; i < list_len; i++)
    {
        schedule_json = json_object_array_get_idx(schedule_array, i);
        TCP_Operate_Schedule_json(schedule_json, alarm_list.schedules + i);
    }

exit:
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    free_json_obj(&ReceivedJsonCache);
    return rc;
}

mico_utc_time_ms_t GetSecondTime(DATE_TIME_t *date_time)
{
    int16_t iYear, iMon, iDay, iHour, iMin, iSec; //, iMsec;
    uint8_t DayOfMon[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int16_t i, Cyear = 0;
    mico_utc_time_ms_t count_MS;
    unsigned long CountDay = 0;

    iYear = date_time->iYear;
    iMon = date_time->iMon;
    iDay = date_time->iDay;
    iHour = date_time->iHour;
    iMin = date_time->iMin;
    iSec = date_time->iSec;
    // iMsec = date_time->iMsec;

    for (i = 1970; i < iYear; i++) /* 1970-20xx å¹´çš„é–�å¹´*/
    {
        if (((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0))
            Cyear++;
    }
    CountDay = Cyear * 366 + (iYear - 1970 - Cyear) * 365;
    for (i = 1; i < iMon; i++)
    {
        if ((i == 2) && (((iYear % 4 == 0) && (iYear % 100 != 0)) || (iYear % 400 == 0)))
            CountDay += 29;
        else
            CountDay += DayOfMon[i - 1];
    }
    CountDay += (iDay - 1);

    CountDay = CountDay * SECOND_ONE_DAY + (unsigned long)iHour * SECOND_ONE_HOUR + (unsigned long)iMin * SECOND_ONE_MINUTE + iSec;
    count_MS = CountDay; //*1000 + iMsec;
    return count_MS;
}
static void time_record(TIME_RECORD_T_t type, mico_utc_time_t *value)
{
    switch (type)
    {
    case SET_ELAND_SEND_TIME:
        mico_time_get_utc_time(&eland_send_time);
        break;
    case SET_ELAND_RECE_TIME:
        mico_time_get_utc_time(&eland_receive_time);
        break;
    case SET_ELSV_SEND_TIME:
        elsv_send_time = *value;
        break;
    case SET_ELSV_RECE_TIME:
        elsv_receive_time = *value;
        break;
    default:
        break;
    }
}

static void eland_set_time(void)
{
    mico_utc_time_ms_t utc_time_ms;
    __msg_function_t eland_cmd = TIME_SET_03;
    struct tm *currentTime;
    iso8601_time_t iso8601_time;
    mico_utc_time_t utc_time;
    mico_rtc_time_t rtc_time;
    int32_t offset_time, offset_time1, offset_time2;

    offset_time1 = elsv_receive_time - eland_send_time;
    offset_time2 = elsv_send_time - eland_receive_time;
    offset_time = (offset_time1 + offset_time2) / 2;

    offset_time1 = offset_time - (Timezone_offset_elsv);
    offset_time = offset_time1 + (netclock_des_g->timezone_offset_sec);

    mico_time_get_utc_time(&utc_time);
    utc_time += offset_time;
    elan_tcp_log("utc_time:%ld", utc_time);
    utc_time_ms = (mico_utc_time_ms_t)((mico_utc_time_ms_t)utc_time * 1000);
    mico_time_set_utc_time_ms(&utc_time_ms);

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
    mico_time_get_iso8601_time(&iso8601_time);
    elan_tcp_log("sntp_time_synced: %.26s", (char *)&iso8601_time);
    /*send time to lcd*/
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
    if ((offset_time > 86400) || (offset_time < -86400))
        mico_rtos_set_semaphore(&alarm_update);
}
OSStatus TCP_Push_MSG_queue(_tcp_cmd_sem_t message)
{
    _tcp_cmd_sem_t tcp_message;
    tcp_message = message;
    return mico_rtos_push_to_queue(&TCP_queue, &tcp_message, 10);
}
void TCP_Operate_Schedule_json(json_object *json, _alarm_schedules_t *schedule)
{
    uint8_t i;
    char alarm_id[ALARM_ID_LEN + 1];
    char date_buffer[ALARM_OFF_DATES_LEN + 1];
    char time_buffer[ALARM_TIME_LEN + 1];
    int year, month, day;
    int ho, mi, se;
    DATE_TIME_t date_time;
    json_object_object_foreach(json, key, val)
    {
        if (!strcmp(key, "alarm_id"))
        {
            memset(alarm_id, 0, sizeof(alarm_id));
            sprintf(alarm_id, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "date"))
        {
            memset(date_buffer, 0, ALARM_OFF_DATES_LEN + 1);
            sprintf(date_buffer, "%s", json_object_get_string(val));
            sscanf((const char *)date_buffer, "%04d-%02d-%02d", &year, &month, &day);
            date_time.iYear = year;
            date_time.iMon = month;
            date_time.iDay = day;
        }
        else if (!strcmp(key, "time"))
        {
            memset(time_buffer, 0, ALARM_TIME_LEN + 1);
            sprintf(time_buffer, "%s", json_object_get_string(val));
            sscanf((const char *)time_buffer, "%02d:%02d:%02d", &ho, &mi, &se);
            date_time.iHour = (int16_t)ho;
            date_time.iMin = (int16_t)mi;
            date_time.iSec = (int16_t)se;
            date_time.iMsec = 0;
        }
    }
    schedule->utc_second = GetSecondTime(&date_time);
    for (i = 0; i < alarm_list.alarm_number; i++)
    {
        if (memcmp(alarm_id, (alarm_list.alarm_lib + i)->alarm_id, ALARM_ID_LEN) == 0)
        {
            schedule->alarm_color = (alarm_list.alarm_lib + i)->alarm_color;
            schedule->snooze_enabled = (alarm_list.alarm_lib + i)->snooze_enabled;
            break;
        }
    }
    elan_tcp_log("%04d-%02d-%02d %02d:%02d:%02d---utc:%ld", date_time.iYear, date_time.iMon, date_time.iDay, date_time.iHour, date_time.iMin, date_time.iSec, schedule->utc_second);
}
