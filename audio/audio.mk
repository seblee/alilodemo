#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Audio

GLOBAL_INCLUDES := http_trans \
				   media_play \
				   util

$(NAME)_SOURCES := http_trans/http_test.c \
				   http_trans/http_trans.c \
				   http_trans/url.c \
				   media_play/media_button.c \
				   media_play/media_play.c \
				   media_play/media_test.c \
				   util/check_pin.c \
				   util/ring_buf.c \
				   util/ring_test.c \
				   util/stream_crc.c
				  
