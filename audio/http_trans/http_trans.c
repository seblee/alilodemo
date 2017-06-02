#include "MICO.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "http_trans.h" 
#include "url.h"
#include "media_play.h"
//#include "baichuan.h"
//#include "MICOAPPDefine.h"


#define http_trans_log(M, ...) custom_log("HTTP_TRANS", M, ##__VA_ARGS__)

#define HTTP_SEND_HEAD_SIZE 2*1024

#define BOUNDARY "----WebKitFormBoundaryHYdQAGrKrTPB3QUC"
static http_trans_url_t *http_trans_url=NULL;
static HTTPHeader_t *httpHeader = NULL;
static int http_trans_fd = 0;
static mico_ssl_t http_trans_ssl = NULL;

static OSStatus onReceivedData(struct _HTTPHeader_t * httpHeader, 
                               uint32_t pos, 
                               uint8_t *data, 
                               size_t len,
                               void * userContext);

static int http_trans_send(char *data, int datalen)
{
  if(http_trans_url->HTTP_SECURITY == HTTP_SECURITY_HTTP){
    return send(http_trans_fd, data, datalen, 0);
  }else{
    return ssl_send(http_trans_ssl, data, datalen);
  }
}

static int http_trans_connect(const struct sockaddr_in *addr, socklen_t addrlen)
{
  int ret = 0;
  int ssl_errno = 0;
  ret = connect(http_trans_fd, (struct sockaddr *)addr, addrlen);
  if(http_trans_url->HTTP_SECURITY != HTTP_SECURITY_HTTP){
    http_trans_ssl = ssl_connect( http_trans_fd, 0, NULL, &ssl_errno );
    if( http_trans_ssl == NULL ){
      ret = -1;
    }
  }
  return ret;
}

static int http_trans_read_header(HTTPHeader_t *httpHeader){
  if(http_trans_url->HTTP_SECURITY == HTTP_SECURITY_HTTP){
    return SocketReadHTTPHeader( http_trans_fd, httpHeader );
  }else{
    return 0;//SocketReadHTTPSHeader( http_trans_ssl, httpHeader );
  }
}

static int http_stran_read_body(HTTPHeader_t *httpHeader){
  if(http_trans_url->HTTP_SECURITY == HTTP_SECURITY_HTTP){
    return SocketReadHTTPBody( http_trans_fd, httpHeader );
  }else{
    return 0;//SocketReadHTTPSBody( http_trans_ssl, httpHeader );
  }
}

#define CHUNK_SIZE_DIGITS 0x10
int http_trans_send_chunk_begin(int size)
{
  
  int err;
  uint8_t buf[CHUNK_SIZE_DIGITS];
  int i;
  int digit;
  int begin = 1;
  
  for (i = CHUNK_SIZE_DIGITS - 1; i >= 0; i--) {
    digit = size & 0xf;
    if (!begin && !size) {
      i++;
      break;
    }
    buf[i] = (digit > 9) ? digit - 0xA + 'a' : digit + '0';
    size = size >> 4;
    begin = 0;
  }
  err = http_trans_send((char *)&buf[i], CHUNK_SIZE_DIGITS - i);
  if (err != kNoErr) {
    return err;
  }
  
  err = http_trans_send("\r\n", 2);
  
  return err;
}

/* Send the last chunk which is simply an ascii "0\r\n\r\n"
*/
int http_trans_send_last_chunk( void )
{
  int err;
  
  err = http_trans_send("0\r\n\r\n", strlen("0\r\n\r\n"));
  
  if (err != kNoErr) {
    http_trans_log("Send last chunk failed");
  }
  
  return err;
}

/* Send one chunk of data of a given size
*/
int http_trans_send_chunk(int conn, const char *buf, int len)
{
  int err = kNoErr;
  
  if (len) {
    /* Send chunk begin header */
    err = http_trans_send_chunk_begin(len);
    if (err != kNoErr)
      return err;
    /* Send our data */
    err = http_trans_send((char *)buf, len);
    if (err != kNoErr)
      return err;
    /* Send chunk end indicator */
    err = http_trans_send("\r\n", 2);
    if (err != kNoErr)
      return err;
  } else {
    /* Length is 0, last chunk */
    err = http_trans_send_last_chunk();
    if (err != kNoErr)
      return err;
  }
  
  return err;
}

extern struct ring_buffer *ring_buf;
static int http_trans_send_header( void )
{
  char *header = NULL;
  char *boundary_header = NULL;
  int ret = 0, j = 0;
#ifdef BC_UPLOAD
  int i = 0;
  int rlen=0;
  int content_len=0;
#endif
  header = malloc( HTTP_SEND_HEAD_SIZE );
  boundary_header = calloc( HTTP_SEND_HEAD_SIZE/2, 1 );
  
  memset(header, 0x00, HTTP_SEND_HEAD_SIZE);
  
  //upload boundary body header
  if( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ){
#ifdef BC_UPLOAD
    i += sprintf(boundary_header,"--%s\r\n",BOUNDARY);
#ifdef UPLOAD_AMR
    i += sprintf(boundary_header + i,"%s\r\n\r\n%s\r\n",CONTENT_NAME,"1.amr");
#else
    i += sprintf(boundary_header + i,"%s\r\n\r\n%s\r\n",CONTENT_NAME,"1.ogg");
#endif
    
    i += sprintf(boundary_header + i,"--%s\r\n",BOUNDARY);
    rlen=ring_buf_len(ring_buf);
    i += sprintf(boundary_header + i,"%s\r\n\r\n%d\r\n",CONTENT_SIZE,rlen);
    
    i += sprintf(boundary_header + i,"--%s\r\n",BOUNDARY);
    i += sprintf(boundary_header + i,"%s\r\n%s\r\n",CONTENT_FILENAME,CONTENT_TYPE);
    i += sprintf(boundary_header + i, "\r\n");
#endif
  }
  
  if( http_trans_url->HTTP_TYPE == HTTP_TYPE_GET ){
    j = sprintf(header, "GET ");
  }else if(http_trans_url->HTTP_TYPE == HTTP_TYPE_POST){
    j = sprintf(header, "POST ");
  }
  
  j += sprintf(header + j, "/%s HTTP/1.1\r\n", http_trans_url->url);
  
  if( http_trans_url->port == 0 ){
    j += sprintf(header + j, "Host: %s\r\n", http_trans_url->host);
  }else{
    j += sprintf(header + j, "Host: %s:%d\r\n", http_trans_url->host, http_trans_url->port);
  }
  j += sprintf(header + j, "Connection: Keep-Alive\r\n");
  
  if( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ){
    j += sprintf(header + j, "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
#ifdef BC_UPLOAD
    content_len = strlen(boundary_header) + rlen + strlen("\r\n----\r\n\r\n")+strlen(BOUNDARY);  //"\r\n--%s--\r\n\r\n"
    j += sprintf(header + j, "Content-Length: %d\r\n", content_len);
    j += sprintf(header + j, "Authorization: UPLOAD_AK_TOP %s\r\n", UPLOAD_AK_TOP);
#else
    j += sprintf(header + j, "Transfer-Encoding: chunked\r\n");
#endif
  }
  
  //Range: bytes=start-end
  if( (http_trans_url->HTTP_TYPE == HTTP_TYPE_GET) && (http_trans_url->download_begin_pos > 0) ){
    if( http_trans_url->download_end_pos > 0 ){
      j += sprintf(header + j, "Range: bytes=%d-%d\r\n", http_trans_url->download_begin_pos, http_trans_url->download_end_pos);
    }else{
      j += sprintf(header + j, "Range: bytes=%d-\r\n", http_trans_url->download_begin_pos);
    }
  }
  
  j += sprintf(header + j, "\r\n");
  
  ret = http_trans_send(header, strlen(header));
  
  if( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ){
    ret = http_trans_send(boundary_header, strlen(boundary_header));
  }
  
  http_trans_log(">>>send http header: len=%d\r\n%s", strlen(header), header);
  http_trans_log(">>>send http boundary header: len=%d\r\n%s", strlen(boundary_header), boundary_header);
  
  if( header != NULL ) free(header);
  if( boundary_header != NULL ) free(boundary_header);
  return ret;
}

static int http_trans_send_body(fd_set readfds)
{
  uint32_t len;
  char *send_data = malloc(1024);
  
  http_trans_url->is_upload = true;
  
  do
  {
    len=media_play_get_data((void *)send_data,1024);
    http_trans_send(send_data,len);
  }while(len);

  len=sprintf(send_data, "\r\n--%s--\r\n\r\n", BOUNDARY);
  http_trans_log("send: [%s]", send_data);
  http_trans_send(send_data,len);
  free(send_data);
  return 0;
}

static void http_trans_socket_close( void )
{
  if( http_trans_ssl ){
    ssl_close( http_trans_ssl );
    http_trans_ssl = NULL;
  }
  SocketClose(&http_trans_fd);
  http_trans_fd = 0;
}

static void http_trans_int(void)
{
  if( http_trans_url == NULL )
    http_trans_url = malloc(sizeof(http_trans_url_t));
}

static int http_trans_connect_server( void )
{
  int err = 0;
  struct hostent* hostent_content = NULL;
  char **pptr = NULL;
  struct in_addr in_addr;
  struct sockaddr_in server_address;
  char hostaddress[16];
  
  hostent_content = gethostbyname( http_trans_url->host );
  require_action_quiet( hostent_content != NULL, exit, http_trans_log("gethostbyname err"));
  pptr=hostent_content->h_addr_list;
  in_addr.s_addr = *(uint32_t *)(*pptr);
  strcpy( hostaddress, inet_ntoa(in_addr));

  http_trans_log("HTTP server address: host:%s, ip: %s", http_trans_url->host, hostaddress);
  
  server_address.sin_family = AF_INET;
  server_address.sin_addr = in_addr;

  if( http_trans_url->port == 0 ){
    if( http_trans_url->HTTP_SECURITY == HTTP_SECURITY_HTTP ){
      server_address.sin_port = htons(80);
    }else{
      server_address.sin_port = htons(443);
    }
  }else{
    server_address.sin_port = htons(http_trans_url->port);
  }

  err = http_trans_connect(&server_address, sizeof(server_address));
  if( err != 0 ){
    mico_thread_sleep(1);
    return -1;
  }
  
  http_trans_log("server connected!");
  return 0;
exit:
  return -1;
}

int http_trans_set_url( HTTP_TYPE_E http_type, char *url)
{
  url_field_t *url_t;
  char *pos = NULL;
  
  if( http_trans_url == NULL )
    return -1;
  
  url_t = url_parse(url);
  http_trans_url->HTTP_TYPE = http_type;
  
  if( !strcmp(url_t->schema, "http") ){
    http_trans_url->HTTP_SECURITY = HTTP_SECURITY_HTTP;
  }else if( !strcmp(url_t->schema, "https") ){
    http_trans_url->HTTP_SECURITY = HTTP_SECURITY_HTTPS;
  }else{
    http_trans_url->HTTP_SECURITY = HTTP_SECURITY_HTTP;
  }
  
  strcpy(http_trans_url->host, url_t->host);
  http_trans_url->port = atoi(url_t->port);
  pos = strstr(url, url_t->path);
  if( pos == NULL){
    strcpy(http_trans_url->url, "");
  }else{
    strcpy(http_trans_url->url, pos);
  }
  
  //url_field_print(url_t);
  
  url_free(url_t);
  return 0;
}

void http_trans_set_response_header( char *header, int header_len, int begin, int end )
{
  if( header_len != 0 ){
    memcpy(http_trans_url->header, header, header_len);
  }
  http_trans_url->download_begin_pos = begin;
  http_trans_url->download_end_pos = end;
}

bool http_trans_is_contune_get(void)
{
  return http_trans_url->is_contune;
}

void http_trans_is_contune_set(bool value)
{
  if( value == true ){
    http_trans_url->is_contune = true;
  }else{
    http_trans_url->is_contune = false;
  }
}

bool http_trans_is_end(void)
{
  return http_trans_url->is_end;
}

void http_trans_state_start(void)
{
  http_trans_url->HTTP_STATE = HTTP_STATE_START;
}

void http_trans_state_pause(void)
{
  http_trans_url->HTTP_STATE = HTTP_STATE_PAUSE;
}

void http_trans_state_continue(void)
{
  http_trans_url->HTTP_STATE = HTTP_STATE_CONTINUE;
}

void http_trans_state_stop(void)
{
  http_trans_url->HTTP_STATE = HTTP_STATE_STOP;
}

void http_trans_upload_stop(void)
{
  http_trans_url->is_upload = false;
}

HTTP_STATE_E http_trans_state_get(void)
{
  return http_trans_url->HTTP_STATE;
}

void http_get_redirect_url(char * inBuf, uint8_t * outBuf)
{
   int len;
   char * p;
   inBuf=strstr((const char *)inBuf, "Location");
   inBuf=strstr((const char *)inBuf, "http");
   p=strstr((const char *)inBuf,"\r\n\r\n");
   len=p-inBuf;
   memcpy(outBuf, inBuf, len);
}

static void http_thread( uint32_t arg )
{
  OSStatus err;
  
  fd_set readfds;
  int nodelay = 1;
  
  http_trans_int();
  http_trans_set_response_header(NULL, 0, 0, 0);
  http_trans_url->HTTP_STATE = HTTP_STATE_IDLE;
  http_trans_url->is_upload = false;
  http_trans_url->is_contune = true;
  http_trans_url->is_redirect=false;
  while(1)
  {
    if( (http_trans_url->HTTP_STATE == HTTP_STATE_IDLE) || (http_trans_url->HTTP_STATE == HTTP_STATE_PAUSE) || (http_trans_url->HTTP_STATE == HTTP_STATE_STOP) ){
       mico_thread_msleep(100);
      if(http_trans_url->is_redirect)
      {
        http_trans_url->is_redirect=false;
        http_trans_set_url( HTTP_TYPE_GET, http_trans_url->web_redirect );
        media_play_cmd_start( );
      }
      continue;
    }else if( http_trans_url->HTTP_STATE == HTTP_STATE_START ){
      /*HTTPHeaderCreateWithCallback set some callback functions */
      httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, NULL, NULL );
      require_action( httpHeader, exit, err = kNoMemoryErr );
      http_trans_url->is_end = false;
    }
    
    http_trans_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    setsockopt(http_trans_fd, IPPROTO_TCP, 1, &nodelay, 4 );
    err = http_trans_connect_server();
    require_noerr( err, exit );
    
    /* Send HTTP Request */
    http_trans_send_header();
    
    if( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ){
      err = http_trans_send_body(readfds);
      require_noerr( err, exit );
    }
    
    FD_ZERO( &readfds );
    FD_SET( http_trans_fd, &readfds );
    
    select( http_trans_fd + 1, &readfds, NULL, NULL, NULL );
    if( FD_ISSET( http_trans_fd, &readfds ) )
    {
      /*parse header*/
      err = http_trans_read_header( httpHeader );
      http_trans_log("<<<recv http header:\r\n%.*s\r\n[bodylen=%lld]", 
                      httpHeader->len, httpHeader->buf, httpHeader->contentLength);
      if( http_trans_url->HTTP_STATE == HTTP_STATE_START ){
        http_trans_url->download_len = httpHeader->contentLength;
        http_trans_url->HTTP_STATE = HTTP_STATE_CONTINUE;
      }
      switch ( err )
      {
      case kNoErr:
        PrintHTTPHeader( httpHeader );
        
        if(302==httpHeader->statusCode)  //�ض���
        {
          http_trans_url->is_redirect=true;
          memset(http_trans_url->web_redirect,0x0, sizeof(http_trans_url->web_redirect));
          http_get_redirect_url((char *)httpHeader->buf, (uint8_t *)http_trans_url->web_redirect);
          http_trans_log("website need redirect");
          goto exit;
        
        }
        err = http_stran_read_body( httpHeader );/*get body data*/
        if(( err != kNoErr )&&(false==http_trans_is_contune_get())){
          http_trans_url->HTTP_STATE = HTTP_STATE_STOP;
        }
        require_noerr( err, exit );
        /*get data and print*/
        break;
      case EWOULDBLOCK:
        http_trans_log("ERROR: EWOULDBLOCK");
      case kNoSpaceErr:
        http_trans_log("ERROR: kNoSpaceErr");
      case kConnectionErr:
        http_trans_log("ERROR: kConnectionErr");
      default:
        http_trans_log("ERROR: HTTP Header parse error: %d", err);
        break;
      }
    }
    
  exit:
    
    if( ( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ) || (http_trans_url->download_len == http_trans_url->download_begin_pos) || ( http_trans_url->HTTP_STATE == HTTP_STATE_STOP) ){
      http_trans_set_response_header(NULL, 0, 0, 0);
      HTTPHeaderDestory( &httpHeader );
      http_trans_url->is_end = true;
      http_trans_url->HTTP_STATE = HTTP_STATE_IDLE;
    }
    
    http_trans_socket_close();
    http_trans_log("Free memory: %d", mico_memory_info()->free_memory);
    
  }
  
}

/*one request may receive multi reply*/
static OSStatus onReceivedData( struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData, size_t inLen, void * inUserContext )
{
  OSStatus err = kNoErr;
  uint32_t put_len = 0;
  
  if( inLen == 0 )
    return err;
  
  http_trans_url->download_begin_pos += inLen;
  http_trans_log("http data: %d, recv: %d, total: %d", inLen, http_trans_url->download_begin_pos, http_trans_url->download_len);
  
  if( http_trans_url->HTTP_TYPE == HTTP_TYPE_POST ){
    return err;
  }
  
#ifndef MEDIA_USE_RING_BUF   
  if( http_trans_url->HTTP_STATE == HTTP_STATE_PAUSE ){
    while(1){
      mico_thread_msleep(100);
      if( (http_trans_url->HTTP_STATE == HTTP_STATE_CONTINUE) || (http_trans_url->HTTP_STATE == HTTP_STATE_STOP) )
        break;
    }
  }
#endif
  
  if( http_trans_url->HTTP_STATE == HTTP_STATE_STOP ){
    err = kConnectionErr;
  }
  
#ifdef MEDIA_USE_RING_BUF  
  do{
    put_len = media_play_put_data(inData, inLen);
    if( http_trans_url->HTTP_STATE == HTTP_STATE_STOP ){
      err = kConnectionErr;
    }
    if( put_len == 0 ){
      mico_thread_msleep(20);
      //http_trans_log("***********************************ring buf full");
    }
  }while( put_len == 0 );
#endif
  
  return err;
}

void http_trans_start(void)
{
  //context = get_app_context();
  http_trans_test();
  //ssl_version_set(TLS_V1_1_MODE);
  mico_rtos_create_thread(NULL, 5, "HTTPT", http_thread, 0x2000, 0 );
}
