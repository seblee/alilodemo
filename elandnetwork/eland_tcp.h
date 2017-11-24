/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-24 14:52:18
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-11-24 15:56:30
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_TCP_H_
#define __ELAND_TCP_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"
/* Private typedef -----------------------------------------------------------*/
/**
 * @brief TLS Connection Parameters
 *
 * Defines a type containing TLS specific parameters to be passed down to the
 * TLS networking layer to create a TLS secured socket.
 */
typedef struct
{
    char *pRootCALocation;           ///< Pointer to string containing the filename (including path) of the root CA file.
    char *pDeviceCertLocation;       ///< Pointer to string containing the filename (including path) of the device certificate.
    char *pDevicePrivateKeyLocation; ///< Pointer to string containing the filename (including path) of the device private key file.
    char *pDestinationURL;           ///< Pointer to string containing the endpoint of the MQTT service.
    uint16_t DestinationPort;        ///< Integer defining the connection port of the MQTT service.
    uint32_t timeout_ms;             ///< Unsigned integer defining the TLS handshake timeout value in milliseconds.
    bool ServerVerificationFlag;     ///< Boolean.  True = perform server certificate hostname validation.  False = skip validation \b NOT recommended.
    bool isUseSSL;                   ///< is used ssl connect
} TLSConnectParams_t;
/**
 * @brief TLS Connection Parameters
 *
 * Defines a type containing TLS specific parameters to be passed down to the
 * TLS networking layer to create a TLS secured socket.
 */
typedef struct _TLSDataParams
{
    mico_ssl_t ssl;
    int server_fd;
    char *cacert;
    const char *clicert;
    const char *pkey;
} TLSDataParams_t;

/**
 * @brief Network Structure
 *
 * Structure for defining a network connection.
 */
typedef struct _Eland_Network
{
    OSStatus (*connect)(Network *, TLSConnectParams *);

    OSStatus (*read)(Network *, unsigned char *, size_t, Timer *, size_t *);  ///< Function pointer pointing to the network function to read from the network
    OSStatus (*write)(Network *, unsigned char *, size_t, Timer *, size_t *); ///< Function pointer pointing to the network function to write to the network
    OSStatus (*disconnect)(Network *);                                        ///< Function pointer pointing to the network function to disconnect from the network
    OSStatus (*isConnected)(Network *);                                       ///< Function pointer pointing to the network function to check if TLS is connected
    OSStatus (*destroy)(Network *);                                           ///< Function pointer pointing to the network function to destroy the network object

    TLSConnectParams_t tlsConnectParams; //< TLSConnect params structure containing the common connection parameters
    TLSDataParams_t tlsDataParams;       //< TLSData params structure containing the connection data parameters that are specific to the library being used
} Eland_Network_t;
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

#endif
