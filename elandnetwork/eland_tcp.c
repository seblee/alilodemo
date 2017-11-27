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
static void TCP_thread_main(mico_thread_arg_t arg);
/* Private functions ---------------------------------------------------------*/

TCP_Error_t TCP_Physical_is_connected(Network_t *pNetwork)
{
    LinkStatusTypeDef link_status;

    memset(&link_status, 0, sizeof(link_status));

    micoWlanGetLinkStatus(&link_status);

    elan_tcp_log("wifi link_status:%d", link_status.is_connected);

    if (link_status.is_connected == true)
    {
        return NETWORK_PHYSICAL_LAYER_CONNECTED;
    }
    else
    {
        return NETWORK_MANUALLY_DISCONNECTED;
    }
}

static bool has_timer_expired(struct timeval *timer)
{
    struct timeval now, res;
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
    OSStatus err = kGeneralErr;
    char ipstr[16] = {0};
    struct sockaddr_in addr;
    int socket_fd = -1;
    int ret = 0;
    int errno;
    int root_ca_len = 0;
    if (pNetwork == NULL)
        return NULL_VALUE_ERROR;

    if (Params != NULL)
    {
        TCP_tls_set_connect_params(pNetwork,
                                   Params->pDestinationURL,
                                   Params->DestinationPort,
                                   Params->timeout_ms);
    }

    err = usergethostbyname(pNetwork->tlsConnectParams.pDestinationURL, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        elan_tcp_log("ERROR: Unable to resolute the host address.");
        return TCP_CONNECTION_ERROR;
    }
    elan_tcp_log("host:%s, ip:%s", pNetwork->tlsConnectParams.pDestinationURL, ipstr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    ret = user_set_tcp_keepalive(socket_fd, 5000, 1000, 10, 5, 3);
    if (ret < 0)
    {
        elan_tcp_log("ERROR: Unable to resolute the tcp connect");
        return TCP_SETUP_ERROR;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(pNetwork->tlsConnectParams.DestinationPort); //

    elan_tcp_log("start connect");
    err = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (err != kNoErr)
    {
        elan_tcp_log("ERROR: Unable to resolute the tcp connect");
        return TCP_CONNECTION_ERROR;
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
            return SSL_CONNECTION_ERROR;
        }
        elan_tcp_log("ssl connected");
    }
    pNetwork->tlsDataParams.server_fd = socket_fd;
    return TCP_SUCCESS;
}
TCP_Error_t TCP_Write(Network_t *pNetwork, uint8_t *pMsg, size_t len, struct timeval *timer, size_t *written_len)
{
    size_t written_so_far;
    bool isError_flag = false;
    int frags, ret = 0;

    struct timeval current_temp, timeforward = {0, 0};
    gettimeofday(&current_temp, NULL);
    timeradd(&current_temp, timer, &timeforward);

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
TCP_Error_t TCP_Read(Network_t *pNetwork, unsigned char *pMsg, size_t len, struct timeval *timer, size_t *read_len)
{
    size_t rxlen = 0;
    int ret = 0;
    int fd = pNetwork->tlsDataParams.server_fd;
    fd_set readfds;
    struct timeval current_temp, timeforward = {0, 0};

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    if (timer != NULL)
    {
        gettimeofday(&current_temp, NULL);
        timeradd(&current_temp, timer, &timeforward);
    }

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
                ret = select(fd + 1, &readfds, NULL, NULL, timer);
                elan_tcp_log("select ret %d", ret);
                if (ret <= 0)
                {
                    break;
                }
                if (!FD_ISSET(fd, &readfds))
                {
                    elan_tcp_log("fd is set err");
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
            elan_tcp_log("socket read err");
            return NETWORK_SSL_READ_ERROR;
        }
        // Evaluate timeout after the read to make sure read is done at least once
        if (has_timer_expired(timer))
        {
            elan_tcp_log("read time out");
            break;
        }
    }
    if (len == 0)
    {
        *read_len = rxlen;
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
    return kNoErr;
}
TCP_Error_t TCP_tls_destroy(Network_t *pNetwork)
{
    return TCP_SUCCESS;
}

TCP_Error_t TCP_Client_Init(_Client_t *pClient, ServerParams_t *ServerParams)
{
    TCP_Error_t rc;
    OSStatus err;
    if (pClient == NULL)
        return NULL_VALUE_ERROR;

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
    return TCP_SUCCESS;
}
OSStatus TCP_Service_Start(void)
{
    return mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "TCP Thread", TCP_thread_main,
                                   0x3000,
                                   0);
}
static void TCP_thread_main(mico_thread_arg_t arg)
{
    TCP_Error_t rc = TCP_SUCCESS;
    _Client_t TCP_Client;
    ServerParams_t serverPara;
    serverPara.pDestinationURL = ELAND_HTTP_DOMAIN_NAME;
    serverPara.DestinationPort = ELAND_HTTP_PORT_SSL;
    serverPara.timeout_ms = 0;

    rc = TCP_Client_Init(&TCP_Client, &serverPara);
    if (TCP_SUCCESS != rc)
    {
        elan_tcp_log("Shadow Connection Error");
        goto exit;
    }

RECONN:
    elan_tcp_log("Shadow Connect...");

exit:
    mico_rtos_delete_thread(NULL);
}
