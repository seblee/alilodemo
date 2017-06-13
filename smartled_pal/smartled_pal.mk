############################################################################### 
#
#  The MIT License
#  Copyright (c) 2016 MXCHIP Inc.
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy 
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights 
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is furnished
#  to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
#  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
############################################################################### 
NAME := App_Smaartled_emb

$(NAME)_SOURCES := alink_main.c mico_main.c alink_device_raw.c alink_device_json.c alink_device_asr.c
				   
$(NAME)_COMPONENTS := lib_alink/alink_emb/alink_emb_pal \
					  lib_alink/alink_common \
				      protocols/libwebsocket


$(NAME)_COMPONENTS += lib_audio_service

$(NAME)_COMPONENTS += lib_audio_interface

$(NAME)_COMPONENTS += lib_robot_event_notification

$(NAME)_COMPONENTS += lib_http_file_download

$(NAME)_COMPONENTS += lib_robot_key_led
