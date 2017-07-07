/** @file
 * Header file that includes all API & helper functions
 */

#ifndef _MSCP_MESSAGE_H_
#define _MSCP_MESSAGE_H_

#include "mscp_type.h"
#include "mico.h"


/******************************************************
 *                      Macros
 ******************************************************/

#define MSCP_MSG_PREFIX_LEN				    (7)
#define MSCP_MSG_SUFFIX_LEN				    (1)
#define MSCP_MSG_LEN(len)				    (MSCP_MSG_PREFIX_LEN + (len) + MSCP_MSG_SUFFIX_LEN)
#define MSCP_MSG_HDR_OFFSET                 (1)
#define MSCP_MSG_HDR_LENGTH                 (6)

#define MSCP_START_OF_MESSAGE			    (0x0B)

#define MSCP_VERSION						(4) //MSCP V4.0
#define MSCP_VERSION_MASK				    (0xE0)
#define MSCP_VERSION_BIT_OFFSET			    (5)
#define MSCP_VERSION_GET(val)			    ((val) >> MSCP_VERSION_BIT_OFFSET)

#define MSCP_FLAG_MASK					    (0x1F)
#define MSCP_FLAG_BIT_OFFSET				(05)
#define MSCP_FLAG_GET(val)				    ((val) & MSCP_FLAG_MASK)
#define MSCP_OP_MASK						(0xF0)
#define MSCP_OP_BIT_OFFSET				    (4)
#define MSCP_OP_GET(val)					((val) >> MSCP_OP_BIT_OFFSET)

#define MSCP_LEN_HIGH_MASK				    (0x0F)
#define MSCP_LEN_HIGH_BIT_OFFSET			(0)
#define MSCP_LEN_HIGH_GET(val)			    ((val) & MSCP_LEN_HIGH_MASK)

/******************************************************
 *            Enumerations
 ******************************************************/

typedef enum
{
	MSCP_FLAG_NONE		= 0x00,
	MSCP_FLAG_CFM		= (0x01 << 0),

	MSCP_FLAG_NUM
} mscp_flag_t;

typedef enum
{
	MSCP_OP_CFM			= 0x00,
	MSCP_OP_IND			= 0x01,
	MSCP_OP_REQ			= 0x02,
	MSCP_OP_RSP			= 0x03,

	MSCP_OP_NUM
} mscp_opcode_t;

typedef enum
{
	MSCP_CHECKSUM_NONE  	    	= 0x00,			/**< None Check sum					  */
	MSCP_CHECKSUM_HEADER       		= 0x01, 		/**< Only check sum the hearder       */
	MSCP_CHECKSUM_FULL			    = 0x02,			/**< Check the header and payload     */

	MSCP_CHECKSUM_NUM
} mscp_checksum_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct
{
	uint8_t 	som;		// Start of message
	uint8_t		flag; 		// Version and flag; version: bit5-bit7; flag: bit0-bit4
	uint8_t	    msgid; 		// Message sequence number
	uint8_t		opcode;		// Opcode and length high; opcode: bit4-bit7; length high: bit0-bit3
	uint8_t		length;     // Length low; length: <length high, length low>
	uint8_t		branch;		// Branch
	uint8_t		leaf;		// Leaf
	uint8_t		payload;	// Payload
	uint8_t		checksum;	// Check sum
} mscp_msg_t;

typedef struct
{
	uint8_t     version;
	uint8_t		flag;
	uint8_t		msgid;
	uint8_t		opcode;
	uint16_t	length;
	uint8_t		branch;
	uint8_t		leaf;
	uint8_t		checksum;
} mscp_hdr_t;

/******************************************************
 *               Function Definitions
 ******************************************************/

/*  Get local message sequence number*/
extern uint8_t mscp_msg_msgid_get(void);

/*  Parser the message header */
extern void mscp_msg_header_parser(const uint8_t *msg, mscp_hdr_t * mscp_hdr);

/*  Get the length field from MSCP message */
extern uint16_t mscp_msg_length_get(const uint8_t *msg);

/*  Check the checksum field from the MSCP message */
extern mscp_result_t mscp_msg_check_cksum(uint8_t *msg);

/*  Dump the MSCP message */
extern void mscp_msg_dump(const uint8_t *msg, uint32_t length);

/*  Set part of the message payload */
extern void mscp_msg_fill_payload(uint8_t *data, uint16_t data_len, uint16_t offset);

/*  Set header and check sum of the message payload */
extern void mscp_msg_fill_header(mscp_hdr_t * hdr);

/*  Get the checksum type */
extern mscp_checksum_t mscp_msg_checksum_get(void);

/*  Set the checksum type */
extern void mscp_msg_checksum_set(mscp_checksum_t type);

#endif
