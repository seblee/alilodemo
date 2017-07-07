#ifndef _MSCP_TYPE_H__
#define _MSCP_TYPE_H__

typedef enum
{
    MSCP_RST_SUCCESS                      = 0,    /**< Success */
    MSCP_RST_PENDING                      = 1,    /**< Pending */
    MSCP_RST_TIMEOUT                      = 2,    /**< Timeout */
    MSCP_RST_PARTIAL_RESULTS              = 3,    /**< Partial results */
    MSCP_RST_ERROR                        = 4,   /**< Generic Error */
    MSCP_RST_BADARG                       = 5,   /**< Bad Argument */
    MSCP_RST_BADOPTION                    = 6,   /**< Bad option */
    MSCP_RST_UNSUPPORTED                  = 7,   /**< Unsupported */
    MSCP_RST_CHECKSUM_ERROR               = 8,   /**< Check sum error */
    MSCP_RST_PENDING_LONG                 = 9,   /**< Pending Long*/
    MSCP_RST_NUM
} mscp_result_t;

#if (BYTE_ORDER == LITTLE_ENDIAN)
#define MSCP_HTONS(val)      ((uint16_t)(((uint16_t)(val) >> 8) |	\
                                        ((uint16_t)(val) << 8)))

#define MSCP_NTOHS(val)      (MSCP_HTONS(val))

#define MSCP_HTONL(val)      ((uint32_t)(((uint32_t)MSCP_HTONS((uint32_t)(val) >> 16)) |	\
                                        ((uint32_t)MSCP_HTONS((uint32_t)(val)) << 16)))

#define MSCP_NTOHL(val)      (MSCP_HTONL(val))
#else

#define MSCP_HTONS(val)      (val)
#define MSCP_NTOHS(val)      (val)
#define MSCP_HTONL(val)      (val)
#define MSCP_NTOHL(val)      (val)
#endif

#endif //__MSCP_TYPE_H__

