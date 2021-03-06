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
NAME := App_Audio_7100

$(NAME)_SOURCES := mico_main.c \
				   audio_test.c \
				   hal_alilo_rabbit.c \
				   netclock/netclock.c \
				   netclock/netclock_httpd.c \
				   netclock/netclock_wifi.c \
				   netclock/netclock_uart.c \
				   netclockota/netclock_ota.c \
				   flash_kh25/eland_spi.c \
				   flash_kh25/flash_kh25.c \
				   elandnetwork/eland_http_client.c \
				   elandnetwork/eland_tcp.c \
				   elandsound/eland_sound.c \
				   elandsound/default_bin.c \
				   elandsound/error_bin.c \
				   eland_mcu_ota/eland_mcu_ota.c \
				   eland_alarm/eland_alarm.c \
				   eland_alarm/eland_control.c 
				   
$(NAME)_COMPONENTS := utilities/url \
					  utilities \
					  daemons/http_server 
				 	  
$(NAME)_COMPONENTS += daemons/ota_server

$(NAME)_PREBUILT_LIBRARY := Lib_7100.Cortex-M4F.GCC.release.a
					  
GLOBAL_INCLUDES := .	

$(NAME)_INCLUDES := inc \
					../mico-os/include \
					netclock \
					netclockota \
					flash_kh25 \
					elandnetwork \
					elandsound \
					eland_mcu_ota \
					eland_alarm
