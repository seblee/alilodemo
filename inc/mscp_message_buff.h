#ifndef _MSCP_MESSAGE_BUFF_H_
#define _MSCP_MESSAGE_BUFF_H_

#define MSCP_MESSAGE_TX_BUFF_LEN         (2048)
#define MSCP_MESSAGE_RX_BUFF_LEN         (2048)

extern uint8_t  mscp_tx_buff[MSCP_MESSAGE_TX_BUFF_LEN + 100];
extern uint8_t  mscp_rx_buff[MSCP_MESSAGE_RX_BUFF_LEN + 100];

extern uint32_t mscp_tx_len;
extern uint32_t mscp_rx_len;

#endif
