#include "mico.h"
#include "command_console/mico_cli.h"
#include "media_play.h"

#define media_test_log(M, ...) custom_log("MEDIA_TEST", M, ##__VA_ARGS__)

static void media_test_set(char *pcWriteBuffer, int xWriteBufferLen,int argc, char **argv)
{
  int err = -1;
  if( argc != 3 ){
    media_test_log("para err");
    return;
  }
  
  if( !strcmp(argv[1], "net") ){
    err = media_play_set_source_decoder_type(MEDIA_SOURCE_NET, argv[2]);
  }else if( !strcmp(argv[1], "tf") ){
    err = media_play_set_source_decoder_type(MEDIA_SOURCE_TF, argv[2]);
  }else if( !strcmp(argv[1], "mem") ){
    err = media_play_set_source_decoder_type(MEDIA_SOURCE_MEM, argv[2]);
  }
  
  if( err == 0){
    media_test_log("set ok");
  }else{
    media_test_log("set err");
  }
}

static void media_test_option(char *pcWriteBuffer, int xWriteBufferLen,int argc, char **argv)
{
  if( !strcmp(argv[1], "start") ){
    media_play_cmd_start();
  }else if( !strcmp(argv[1], "pause") ){
    media_play_cmd_pause_resum();
  }else if( !strcmp(argv[1], "mute") ){
    media_play_cmd_mute_unmute();
  }else if( !strcmp(argv[1], "vol") ){
    media_play_cmd_vol_set( atoi(argv[2]) );
  }else if( !strcmp(argv[1], "stop") ){
    media_play_cmd_stop();
  }else if( !strcmp(argv[1], "+") ){
    media_play_cmd_vol_add();
  }else if( !strcmp(argv[1], "-") ){
    media_play_cmd_vol_sub();
  }else if( !strcmp(argv[1], "call") ){
    media_recode_type_call();
    media_play_cmd_encode();
  }else if( !strcmp(argv[1], "en") ){
    media_recode_type_recoder();
    media_play_cmd_encode();
  }else if( !strcmp(argv[1], "de") ){
    media_play_cmd_decode();
  }else{
    media_test_log("media option err\r\n");
    return;
  }
  media_test_log("media option ok\r\n");
}

static const struct cli_command media_test_clis[] = {
  {"medias", "set media source and file name", media_test_set},
  {"media", "media cmd", media_test_option},
};

void media_play_test( void )
{
#ifdef MICO_CLI_ENABLE
  cli_register_commands(media_test_clis, sizeof(media_test_clis)/sizeof(struct cli_command));
#endif
}

