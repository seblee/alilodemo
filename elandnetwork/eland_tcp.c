/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-24 14:52:13
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-11-24 17:21:09
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "eland_tcp.h"
#include "eland_http_client.h"
//#include "timer_platform.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define elan_tcp_log(M, ...) custom_log("elan_tcp", M, ##__VA_ARGS__)
//#define elan_tcp(M, ...)
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static bool has_timer_expired(Timer *timer)
{
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&timer->end_time, &now, &res);
    return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}
void Eland_TCP_IP_Service_Start(mico_thread_arg_t arg)
{
}
static void _iot_tls_set_connect_params(Eland_Network_t *pNetwork, char *pRootCALocation,
                                        char *pDeviceCertLocation,
                                        char *pDevicePrivateKeyLocation,
                                        char *pDestinationURL,
                                        uint16_t destinationPort,
                                        uint32_t timeout_ms,
                                        bool ServerVerificationFlag,
                                        bool isUseSSLFlag)
{
    pNetwork->tlsConnectParams.DestinationPort = destinationPort;
    pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
    pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
    pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = pDevicePrivateKeyLocation;
    pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
    pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
    pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;
    pNetwork->tlsConnectParams.isUseSSL = isUseSSLFlag;
}

OSStatus Eland_TCP_IP_Connect(Eland_Network_t *pNetwork, TLSConnectParams_t *Params)
{
    OSStatus err = kGeneralErr;
    char ipstr[16] = {0};
    struct sockaddr_in addr;
    int socket_fd = -1;
    int ret = 0;
    int errno;
    int root_ca_len = 0;
    if (Params != NULL)
    {
        _iot_tls_set_connect_params(pNetwork, Params->pRootCALocation, Params->pDeviceCertLocation,
                                    Params->pDevicePrivateKeyLocation,
                                    Params->pDestinationURL,
                                    Params->DestinationPort,
                                    Params->timeout_ms,
                                    Params->ServerVerificationFlag,
                                    Params->isUseSSL);
    }
    err = usergethostbyname(pNetwork->tlsConnectParams.pDestinationURL, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        elan_tcp_log("ERROR: Unable to resolute the host address.");
        goto exit;
        //   return TCP_CONNECTION_ERROR;
    }
    elan_tcp_log("host:%s, ip:%s", pNetwork->tlsConnectParams.pDestinationURL, ipstr);

    socket_fd = = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    ret = user_set_tcp_keepalive(&socket_fd, 5000, 1000, 10, 5, 3);
    if (ret < 0)
    {
        client_log("user_set_tcp_keepalive() error");
        err = kGeneralErr;
        goto exit;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(pNetwork->tlsConnectParams.DestinationPort); //

    client_log("start connect");
    err = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");

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
        client_log("start ssl_connect");
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
            goto exit;
            // return SSL_CONNECTION_ERROR;
        }
        elan_tcp_log("ssl connected");
    }
    pNetwork->tlsDataParams.server_fd = socket_fd;
exit:
    return err;
}
OSStatus Eland_TCP_IP_Write(Eland_Network_t *pNetwork, uint8_t *pMsg, , uint32_t len, struct timeval *timer, uint32_t *written_len)
{
    uint32_t written_so_far;
    bool isError_flag = false;
    int frags, ret = 0;
    OSStatus err = kGeneralErr;
    struct timeval current_temp, timeforward = {0, 0};
    gettimeofday(&current_temp, NULL);
    timeradd(&current_temp, &timer, &timeforward);

    for (written_so_far = 0, frags = 0; written_so_far < len && !has_timer_expired(timeforward); written_so_far += ret, frags++)
    {
        while ((!has_timer_expired(timeforward)) && ((ret = ssl_send(pNetwork->tlsDataParams.ssl, pMsg + written_so_far, len - written_so_far)) <= 0))
        {
            if (ret < 0)
            {
                elan_tcp_log(" failed");
                /* All other negative return values indicate connection needs to be reset.
                 * Will be caught in ping request so ignored here */
                isErrorFlag = true;
                break;
            }
        }
        if (isErrorFlag)
        {
            break;
        }
    }
    *written_len = written_so_far;
    elan_tcp_log("socket write done", written_so_far);
    if (isErrorFlag)
    {
        elan_tcp_log("socket write err");
        err = kGeneralErr;
        goto exit;
    }
    else if (has_timer_expired(timer) && written_so_far != len)
    {
        elan_tcp_log("socket write time out");
        err = kGeneralErr;
        goto exit;
    }
    err = kNoErr;
exit:
    return err;
}
OSStatus Eland_TCP_IP_Read(Network *pNetwork, unsigned char *pMsg, size_t len, struct timeval *timer, size_t *read_len)
{
    OSStatus err = kGeneralErr;
    fd_set readfds;
    int ret = 0;
    uint32_t rxlen = 0;
    int fd = pNetwork->tlsDataParams.server_fd;
    struct timeval current_temp, timeforward = {0, 0};

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    struct timeval current_temp, timeforward = {0, 0};
    gettimeofday(&current_temp, NULL);
    timeradd(&current_temp, &timer, &timeforward);
    while (len > 0)
    {
        if (pNetwork->tlsConnectParams.isUseSSL == true)
        {
            if (ssl_pending(pNetwork->tlsDataParams.ssl))
            {
                ret = ssl_recv(pNetwork->tlsDataParams.ssl, pMsg, len);
            }
            else
            {
                ret = select(fd + 1, &readfds, NULL, NULL, &t);
                aws_platform_log("select ret %d", ret);
                if (ret <= 0)
                {
                    break;
                }
                if (!FD_ISSET(fd, &readfds))
                {
                    aws_platform_log("fd is set err");
                    break;
                }
                ret = ssl_recv(pNetwork->tlsDataParams.ssl, pMsg, len);
            }
        }

        if (ret >= 0)
        {
            rxlen += ret;
            pMsg += ret;
            len -= ret;
        }
        else if (ret < 0)
        {
            aws_platform_log("socket read err");
            err = kGeneralErr;
            goto exit;
        }
        // Evaluate timeout after the read to make sure read is done at least once
        if (has_timer_expired(timer))
        {
            aws_platform_log("read time out");
            err = kGeneralErr;
            goto exit;
        }
    }
    if (len == 0)
    {
        *read_len = rxLen;
    }
    if (rxlen == 0)
    {
    }
    err = kNoErr;
exit:
    return err;
}
OSStatus iot_tls_disconnect(Network *pNetwork)
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
    return kNoErr;
}
