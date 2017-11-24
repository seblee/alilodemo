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
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define elan_tcp_log(M, ...) custom_log("elan_tcp", M, ##__VA_ARGS__)
//#define elan_tcp(M, ...)
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

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

static OSStatus Eland_TCP_IP_Connect(Eland_Network_t *pNetwork, TLSConnectParams_t *Params)
{
    OSStatus err = kGeneralErr;
    char ipstr[16] = {0};
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

    user_set_tcp_keepalive(int socket, int send_timeout, int recv_timeout, int idle, int interval, int count);

    if (pNetwork->tlsConnectParams.isUseSSL == true)
    {
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
        pNetwork->tlsDataParams.ssl = ssl_connect(socket_fd,
                                                  root_ca_len,
                                                  pNetwork->tlsDataParams.cacert, &errno);
        elan_tcp_log("fd: %d, err:  %d", socket_fd, errno);
        if (pNetwork->tlsDataParams.ssl == NULL)
        {
            elan_tcp_log("ssl connect err");
            close(socket_fd);
            pNetwork->tlsDataParams.server_fd = -1;
            goto exit;
            // return SSL_CONNECTION_ERROR;
        }
        elan_tcp_log("ssl connected");
    }
    pNetwork->tlsDataParams.server_fd = socket_fd;
exit:
    return err;
}
