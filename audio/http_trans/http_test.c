#include "mico.h"
#include "command_console/mico_cli.h"
#include "http_trans.h"

#define http_test_log(M, ...) custom_log("HTTP_TEST", M, ##__VA_ARGS__)

static void http_test_option(char *pcWriteBuffer, int xWriteBufferLen,int argc, char **argv)
{
  if( argc != 2){
    http_test_log("http option err");
    return;
  }

  if( !strcmp(argv[1], "start") ){
    http_trans_state_start();
  }else if( !strcmp(argv[1], "pause") ){
    http_trans_state_pause();
  }else if( !strcmp(argv[1], "continue") ){
    http_trans_state_continue();
  }else if( !strcmp(argv[1], "stop") ){
    http_trans_state_stop();
  }else if( !strcmp(argv[1], "usp") ){
    http_trans_upload_stop();
  }else{
    http_test_log("http option err");
    return;
  }
  http_test_log("http option ok");
}

static void http_test_set_url(char *pcWriteBuffer, int xWriteBufferLen,int argc, char **argv)
{
  int err = kNoErr;
  if( argc != 3 ){
    http_test_log("para err");
    return;
  }
  
  if( !strcmp(argv[1], "get") ){
     err = http_trans_set_url( HTTP_TYPE_GET, argv[2]);
  }else if( !strcmp(argv[1], "post") ){
     err = http_trans_set_url( HTTP_TYPE_POST, argv[2]);
  }
  
  if( err != kNoErr ){
    http_test_log("url set err");
  }else{
    http_test_log("url set ok");
  }
  
}

static const struct cli_command http_test_clis[] = {
  {"httpurl", "set http url", http_test_set_url},
  {"httpop", "http option", http_test_option},
};

void http_trans_test( void )
{
#ifdef MICO_CLI_ENABLE
  cli_register_commands(http_test_clis, sizeof(http_test_clis)/sizeof(struct cli_command));
#endif
}
