#ifndef __HTTP_TRANS_H
#define __HTTP_TRANS_H

typedef enum _HTTP_TYPE_E{
  HTTP_TYPE_GET,
  HTTP_TYPE_POST
} HTTP_TYPE_E;

typedef enum _HTTP_SECURITY_E{
  HTTP_SECURITY_HTTP,
  HTTP_SECURITY_HTTPS
} HTTP_SECURITY_E;

typedef enum _HHTTP_STATE_E{
  HTTP_STATE_IDLE,
  HTTP_STATE_START,
  HTTP_STATE_START_ERR,
  HTTP_STATE_PAUSE,
  HTTP_STATE_CONTINUE,
  HTTP_STATE_STOP,
} HTTP_STATE_E;

typedef struct _http_trans_url_t{
  //char              url[100];
  char              url[800];
  //char              header[512];
  char              header[800];
  HTTP_TYPE_E       HTTP_TYPE;
  HTTP_SECURITY_E   HTTP_SECURITY;
  char              host[30];
  int               port;
  int               download_len;
  int               download_begin_pos;
  int               download_end_pos;
  HTTP_STATE_E      HTTP_STATE;
  bool              is_upload;
  bool              is_end;
  bool              is_contune;
  bool              is_redirect;
  char              web_redirect[200];
} http_trans_url_t;

void http_trans_test( void );
int http_trans_set_url( HTTP_TYPE_E http_type, char *url);
void http_trans_set_response_header( char *header, int header_len, int begin, int end );
void http_trans_start(void);

HTTP_STATE_E http_trans_state_get(void);
void http_trans_state_start(void);
void http_trans_state_pause(void);
void http_trans_state_continue(void);
void http_trans_state_stop(void);
void http_trans_upload_stop(void);
bool http_trans_is_end(void);
void http_trans_is_contune_set(bool value);

void http_trans_data_to_device(uint8_t *data, uint32_t datalen);
#endif