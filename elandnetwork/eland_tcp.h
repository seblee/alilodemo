/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-24 14:52:18
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-19 14:51:21
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_TCP_H_
#define __ELAND_TCP_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "eland_alarm.h"
/* Private typedef -----------------------------------------------------------*/
typedef struct _Eland_Network Network_t;
/*! \public
 * @brief IoT Error enum
 *
 * Enumeration of return values from the IoT_* functions within the SDK.
 * Values less than -1 are specific error codes
 * Value of -1 is a generic failure response
 * Value of 0 is a generic success response
 * Values greater than 0 are specific non-error return codes
 */
typedef enum
{
    /** Returned when the Network physical layer is connected */
    NETWORK_PHYSICAL_LAYER_CONNECTED = 6,
    /** Returned when the Network is manually disconnected */
    NETWORK_MANUALLY_DISCONNECTED = 5,
    /** Returned when the Network is disconnected and the reconnect attempt is in progress */
    NETWORK_ATTEMPTING_RECONNECT = 4,
    /** Return value of yield function to indicate auto-reconnect was successful */
    NETWORK_RECONNECTED = 3,
    /** Returned when a read attempt is made on the TLS buffer and it is empty */
    TCP_NOTHING_TO_READ = 2,
    /** Returned when a connection request is successful and packet response is connection accepted */
    TCP_CONNACK_CONNECTION_ACCEPTED = 1,
    /** Success return value - no error occurred */
    TCP_SUCCESS = 0, //
    /** A generic error. Not enough information for a specific error code */
    TCP_FAILURE = -1,
    /** A required parameter was passed as null */
    NULL_VALUE_ERROR = -2, //
    /** The TCP socket could not be established */
    TCP_CONNECTION_ERROR = -3,
    /** The TLS handshake failed */
    SSL_CONNECTION_ERROR = -4,
    /** Error associated with setting up the parameters of a Socket */
    TCP_SETUP_ERROR = -5,
    /** A timeout occurred while waiting for the TLS handshake to complete. */
    NETWORK_SSL_CONNECT_TIMEOUT_ERROR = -6,
    /** A Generic write error based on the platform used */
    NETWORK_SSL_WRITE_ERROR = -7,
    /** SSL initialization error at the TLS layer */
    NETWORK_SSL_INIT_ERROR = -8,
    /** An error occurred when loading the certificates.  The certificates could not be located or are incorrectly formatted. */
    NETWORK_SSL_CERT_ERROR = -9,
    /** SSL Write times out */
    NETWORK_SSL_WRITE_TIMEOUT_ERROR = -10,
    /** SSL Read times out */
    NETWORK_SSL_READ_TIMEOUT_ERROR = -11,
    /** A Generic error based on the platform used */
    NETWORK_SSL_READ_ERROR = -12,
    /** Returned when the Network is disconnected and reconnect is either disabled or physical layer is disconnected */
    NETWORK_DISCONNECTED_ERROR = -13,
    /** Returned when the Network is disconnected and the reconnect attempt has timed out */
    NETWORK_RECONNECT_TIMED_OUT_ERROR = -14,
    /** Returned when the Network is already connected and a connection attempt is made */
    NETWORK_ALREADY_CONNECTED_ERROR = -15,
    /** Network layer Error Codes */
    /** Network layer Random number generator seeding failed */
    NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED = -16,
    /** A generic error code for Network layer errors */
    NETWORK_SSL_UNKNOWN_ERROR = -17,
    /** Returned when the physical layer is disconnected */
    NETWORK_PHYSICAL_LAYER_DISCONNECTED = -18,
    /** Returned when the root certificate is invalid */
    NETWORK_X509_ROOT_CRT_PARSE_ERROR = -19,
    /** Returned when the device certificate is invalid */
    NETWORK_X509_DEVICE_CRT_PARSE_ERROR = -20,
    /** Returned when the private key failed to parse */
    NETWORK_PK_PRIVATE_KEY_PARSE_ERROR = -21,
    /** Returned when the network layer failed to open a socket */
    NETWORK_ERR_NET_SOCKET_FAILED = -22,
    /** Returned when the server is unknown */
    NETWORK_ERR_NET_UNKNOWN_HOST = -23,
    /** Returned when connect request failed */
    NETWORK_ERR_NET_CONNECT_FAILED = -24,
    /** Returned when there is nothing to read in the TLS read buffer */
    NETWORK_SSL_NOTHING_TO_READ = -25,
    /** A connection could not be established. */
    TLS_CONNECTION_ERROR = -26,
    /** A timeout occurred while waiting for the TLS handshake to complete */
    TCP_CONNECT_TIMEOUT_ERROR = -27,
    /** A timeout occurred while waiting for the TLS request complete */
    TCP_REQUEST_TIMEOUT_ERROR = -28,
    /** The current client state does not match the expected value */
    TCP_UNEXPECTED_CLIENT_STATE_ERROR = -29,
    /** The client state is not idle when request is being made */
    TCP_CLIENT_NOT_IDLE_ERROR = -30,
    /** The MQTT RX buffer received corrupt or unexpected message  */
    TCP_RX_MESSAGE_PACKET_TYPE_INVALID_ERROR = -31,
    /** The MQTT RX buffer received a bigger message. The message will be dropped  */
    TCP_RX_BUFFER_TOO_SHORT_ERROR = -32,
    /** The MQTT TX buffer is too short for the outgoing message. Request will fail  */
    TCP_TX_BUFFER_TOO_SHORT_ERROR = -33,
    /** The client is subscribed to the maximum possible number of subscriptions  */
    TCP_MAX_SUBSCRIPTIONS_REACHED_ERROR = -34,
    /** Failed to decode the remaining packet length on incoming packet */
    TCP_DECODE_REMAINING_LENGTH_ERROR = -35,
    /** Connect request failed with the server returning an unknown error */
    TCP_CONNACK_UNKNOWN_ERROR = -36,
    /** Connect request failed with the server returning an unacceptable protocol version error */
    TCP_CONNACK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR = -37,
    /** Connect request failed with the server returning an identifier rejected error */
    TCP_CONNACK_IDENTIFIER_REJECTED_ERROR = -38,
    /** Connect request failed with the server returning an unavailable error */
    TCP_CONNACK_SERVER_UNAVAILABLE_ERROR = -39,
    /** Connect request failed with the server returning a bad userdata error */
    TCP_CONNACK_BAD_USERDATA_ERROR = -40,
    /** Connect request failed with the server failing to authenticate the request */
    TCP_CONNACK_NOT_AUTHORIZED_ERROR = -41,
    /** An error occurred while parsing the JSON string.  Usually malformed JSON. */
    JSON_PARSE_ERROR = -42,
    /** Shadow: The response Ack table is currently full waiting for previously published updates */
    SHADOW_WAIT_FOR_PUBLISH = -43,
    /** Any time an snprintf writes more than size value, this error will be returned */
    SHADOW_JSON_BUFFER_TRUNCATED = -44,
    /** Any time an snprintf encounters an encoding error or not enough space in the given buffer */
    SHADOW_JSON_ERROR = -45,
    /** Mutex initialization failed */
    MUTEX_INIT_ERROR = -46,
    /** Mutex lock request failed */
    MUTEX_LOCK_ERROR = -47,
    /** Mutex unlock request failed */
    MUTEX_UNLOCK_ERROR = -48,
    /** Mutex destroy failed */
    MUTEX_DESTROY_ERROR = -49,
    /** Response err cmd */
    CMD_BACK_ERROR = -50,
    /** Response memery err */
    BACK_DATA_MEMERY_ERROR = -51,
} TCP_Error_t;

typedef enum _TCP_CMD
{
    CN00 = 0,   //00 Connection Request
    CN01,       //01 Connection Response
    HC00,       //02 health check request
    HC01,       //03 health check response
    DV00,       //04 eland info request
    DV01,       //05 eland info response
    DV02,       //06 eland info change Notification
    DV03,       //07 eland info remove Notification
    AL00,       //08 alarm info request
    AL01,       //09 alarm info response
    AL02,       //10 alarm info add Notification
    AL03,       //11 alarm info change Notification
    AL04,       //12 alarm info delete notification
    HD00,       //13 holiday data request
    HD01,       //14 holiday data response
    HD02,       //15 holiday data change notice
    HT00,       //16 alarm on notification
    HT01,       //17 alarm off notification
    HT02,       //18 alarm jump over notification
    FW00,       //19 firmware update start request
    FW01,       //20 firmwart update start response
    SD00,       //21 Alarm clock schedule request
    SD01,       //22 Alarm clock schedule response
    TCPCMD_MAX, //23
    TCPCMD_NONE,
} _TCP_CMD_t;

/**
 * @brief MQTT Client State Type
 *
 * Defining a type for MQTT Client State
 *
 */
typedef enum _ClientState
{
    CLIENT_STATE_INVALID = 0,
    CLIENT_STATE_INITIALIZED = 1,
    CLIENT_STATE_CONNECTING = 2,
    CLIENT_STATE_CONNECTED_IDLE = 3,
    CLIENT_STATE_CONNECTED_YIELD_IN_PROGRESS = 4,
    CLIENT_STATE_CONNECTED_PUBLISH_IN_PROGRESS = 5,
    CLIENT_STATE_CONNECTED_SUBSCRIBE_IN_PROGRESS = 6,
    CLIENT_STATE_CONNECTED_UNSUBSCRIBE_IN_PROGRESS = 7,
    CLIENT_STATE_CONNECTED_RESUBSCRIBE_IN_PROGRESS = 8,
    CLIENT_STATE_CONNECTED_WAIT_FOR_CB_RETURN = 9,
    CLIENT_STATE_DISCONNECTING = 10,
    CLIENT_STATE_DISCONNECTED_ERROR = 11,
    CLIENT_STATE_DISCONNECTED_MANUALLY = 12,
    CLIENT_STATE_PENDING_RECONNECT = 13
} ClientState_t;
typedef struct _ClientStatus
{
    ClientState_t clientState;
    bool isPingOutstanding;
    bool isAutoReconnectEnabled;
} ClientStatus_t;
/**
 * @brief TLS Connection Parameters
 *
 * Defines a type containing TLS specific parameters to be passed down to the
 * TLS networking layer to create a TLS secured socket.
 */
typedef struct
{
    char *pDestinationURL;    //< Pointer to string containing the endpoint of the MQTT service.
    uint16_t DestinationPort; //< Integer defining the connection port of the MQTT service.
    uint32_t timeout_ms;      //< Unsigned integer defining the TLS handshake timeout value in milliseconds.
} ServerParams_t;

/**
 * @brief TLS Connection Parameters
 *
 * Defines a type containing TLS specific parameters to be passed down to the
 * TLS networking layer to create a TLS secured socket.
 */
typedef struct
{
    char *pRootCALocation;           //< Pointer to string containing the filename (including path) of the root CA file.
    char *pDeviceCertLocation;       //< Pointer to string containing the filename (including path) of the device certificate.
    char *pDevicePrivateKeyLocation; //< Pointer to string containing the filename (including path) of the device private key file.
    char *pDestinationURL;           //< Pointer to string containing the endpoint of the MQTT service.
    uint16_t DestinationPort;        //< Integer defining the connection port of the MQTT service.
    uint32_t timeout_ms;             //< Unsigned integer defining the TLS handshake timeout value in milliseconds.
    bool ServerVerificationFlag;     //< Boolean.  True = perform server certificate hostname validation.  False = skip validation \b NOT recommended.
    bool isUseSSL;                   //< is used ssl connect
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

struct _Eland_Network
{
    TCP_Error_t (*connect)(Network_t *, ServerParams_t *);

    TCP_Error_t (*read)(Network_t *, uint8_t **, struct timeval *, size_t *); //< Function pointer pointing to the network function to read from the network
    TCP_Error_t (*write)(Network_t *, uint8_t *, struct timeval *, size_t *); //< Function pointer pointing to the network function to write to the network
    TCP_Error_t (*disconnect)(Network_t *);                                   //< Function pointer pointing to the network function to disconnect from the network
    TCP_Error_t (*isConnected)(Network_t *);                                  //< Function pointer pointing to the network function to check if TLS is connected
    TCP_Error_t (*destroy)(Network_t *);                                      //< Function pointer pointing to the network function to destroy the network object

    TLSConnectParams_t tlsConnectParams; //< TLSConnect params structure containing the common connection parameters
    TLSDataParams_t tlsDataParams;       //< TLSData params structure containing the connection data parameters that are specific to the library being used
};
/**
 * @brief MQTT Client Data
 *
 * Defining a type for MQTT Client Data
 * Contains data used by the MQTT Client
 *
 */
// MQTT pub and sub buff len
#define MQTT_TX_BUF_LEN (2048 + 200) //
#define MQTT_RX_BUF_LEN (2048 + 200) //

#define TCP_RX_MAX_LEN 4096

typedef struct _ClientData
{
    uint16_t nextPacketId;

    uint32_t packetTimeoutMs;
    uint32_t commandTimeoutMs;
    uint16_t keepAliveInterval;
    uint32_t currentReconnectWaitInterval;
    uint32_t counterNetworkDisconnected;

    /* The below values are initialized with the
	 * lengths of the TX/RX buffers and never modified
	 * afterwards */
    size_t writeBufSize;
    size_t readBufSize;

    unsigned char writeBuf[MQTT_TX_BUF_LEN];
    unsigned char *readBuf;

    bool isBlockOnThreadLockEnabled;
    mico_mutex_t state_change_mutex;
    mico_mutex_t tls_read_mutex;
    mico_mutex_t tls_write_mutex;

    //MessageHandlers messageHandlers[MQTT_NUM_SUBSCRIBE_HANDLERS];
    //iot_disconnect_handler disconnectHandler;

    void *disconnectHandlerData;
} ClientData_t;
/**
 * @brief MQTT Client
 *
 * Defining a type for MQTT Client
 *
 */
typedef struct _Client
{
    //Timer pingTimer;
    //Timer reconnectDelayTimer;

    ClientStatus_t clientStatus;
    ClientData_t clientData;
    Network_t networkStack;
} _Client_t;

typedef struct _TELEGRAM
{
    char head[2];
    int16_t squence_num;
    char command[4];
    int32_t lenth;
    int32_t reserved;
} _TELEGRAM_t;

typedef struct timeval _time_t;
typedef struct date_time
{
    int16_t iYear;
    int16_t iMon;
    int16_t iDay;

    int16_t iHour;
    int16_t iMin;
    int16_t iSec;
    int16_t iMsec;
} DATE_TIME_t;

typedef enum TIME_RECORD_T
{
    SET_ELAND_SEND_TIME,
    SET_ELAND_RECE_TIME,
    SET_ELSV_SEND_TIME,
    SET_ELSV_RECE_TIME,
} TIME_RECORD_T_t;

typedef enum TCP_CMD_SEM
{
    TCP_CMD_NONE,
    TCP_Stop_Sem, //stop tcp thread
    TCP_HT00_Sem, //alarm on notice
    TCP_HT02_Sem, //ALARM skip notice
    TCP_HC00_Sem, //helth check
    TCP_FW01_Sem, //OTA start
    TCP_SD00_Sem, //OTA start
} _tcp_cmd_sem_t;
/* Private define ------------------------------------------------------------*/
#define TELEGRAMHEADER "EL"
#define COMMAND_LEN 4
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_queue_t TCP_queue;
extern mico_semaphore_t TCP_Reconnect_sem;
/* Private function prototypes -----------------------------------------------*/
TCP_Error_t TCP_Connect(Network_t *pNetwork, ServerParams_t *Params);
TCP_Error_t TCP_Write(Network_t *pNetwork, uint8_t *pMsg, struct timeval *timer, size_t *written_len);
TCP_Error_t TCP_Read(Network_t *pNetwork, uint8_t **pMsg, struct timeval *timer, size_t *read_len);
TCP_Error_t TCP_disconnect(Network_t *pNetwork);
TCP_Error_t TCP_tls_destroy(Network_t *pNetwork);
TCP_Error_t TCP_Physical_is_connected(Network_t *pNetwork);
OSStatus TCP_Service_Start(void);
ClientState_t eland_get_client_state(_Client_t *pClient);
void TCP_Operate_AL_JSON(json_object *alarm, __elsv_alarm_data_t *alarm_data);
void TCP_Operate_Schedule_json(json_object *json, _alarm_schedules_t *schedule);
mico_utc_time_ms_t GetSecondTime(DATE_TIME_t *date_time);
OSStatus TCP_Push_MSG_queue(_tcp_cmd_sem_t message);
/* Private functions ---------------------------------------------------------*/

#endif
