# LT10_eland

## v01.30
2018年8月8日11:40:00

* set version v01.30
* change flash districts capacity
* fix sid ofid bug when sid and bbbbbbbbb

## v01.29
2018年8月3日15:59:50

* fix flash write error
* merge change_flash branch
* reserve capacity 2 sector
* change flash struct

## v01.28
2018年8月1日14:30:37

* set version v01.28
* add download reback result
* alarm waitting when downloading
* add vid ccccccccccc
* add vid bbbbbbbbb
* change file download try again and again until alarm start,file include sid,dddddddd,cccccccccccccc,00000000,ffffffff,eeeeeeee,and other vid
* issue alarm over ,before ofid volume problem
* change ofid timeout 2s more
* vid:ccccccccc/ddddddd download at120s alarm start
* distinguish aaaaa bbbbb sound download
* add aaaaa bbbbbb download when alarm operating
* issue alarm ofid do not play error
* 


## v01.27
2018年7月27日15:45:33

* set version v01.27
* change alarm_sound_download function
* issue download sequence 
* add AlarmlibMutex in weather
* issue alarm compare bug 
* http download timeout increase 2s

## v01.26
2018年7月20日14:32:01

* set version v01.26
* issue simple clock alarm history
* add TCP_Reconnect_sem issue tcp not reconnect bug
* change volume set style
* issue weather download push bug
* alarm info add utc check
* issue wait for sudio stoped bug
*


## v01.25
2018年7月13日13:47:59

* issue mcu time reset when restart by software
* change alarm_off Key_Restain to 4s
* change DOWNLOAD_SCAN push as 1 second later
* if alarm second < now utc +110 then do not download sound
* tcp_ip donot reconnect when read timeout or write timeout
* change error_bin default_bin to C
* set version v01.25
* above 50 alarms, cover the latest one
* change alarm play error_bin when donot download success
* issue tcp_ip bug at RECONN
* if al01 contains operating alarm,the alarm continue
* issue file download distinguish whether it is a continued file
* weather download always time
* issue flash capacity insufficient bug when download file
* chear alarm list when update alarm info
* consummate bad file remove operation
* 

## v01.24
2018年7月6日13:45:12

* issue miss alarm_skip alarm history
* change default stack size as test 0x1000
* add netclock_des_g->download_flag
* set version v01.24
* issue check_default_sound error logic bug


## v01.23
2018年7月3日16:03:53
 
* set version v01.23
* add check_RDID function
* check flash on time in test mode 
* move alarm file check to voice_play function
* add alarm_repeat = -1
* change alarm history time get as GET_current_second()
* change firmware read interval from 2ms to 10 ms

## v01.22
2018年6月29日14:03:45
 
* set version v02.22
* issue chunked data memory bug
* issue alarm latest history stop error
* issue eland_alarm_is_same alarm_off_dates error
* alarm al01 add reserved para
* add TCP_RX_MAX_LEN
* http setting add default data
* skip alarm playing when oid is on
* rain file as one byte
* alarm sound download split
* change flash file write in style
* issue block_erase bug

## v01.21
2018年6月29日14:08:03
 
* set version v01.21
* add Restart status juge
* add __no_init when IWDG
* set Brightness * Brightness / 10
* add time out when usart send
* change alarm on/off days calculet style
* add goto little_cycle_loop when tcp receive success
* issue alarm data push bug

## v01.20
2018年6月15日13:48:49
 
* add function RGBLED_FlashRainBow_Color
* add test operation
* add single signal check in lcd check
## 
* set version v01.20
* change history to history queue
* add eland_test functory
* add flash check flag
* eland_test wifi play voice
* change default volume 50
* issue sound bug
* when alarm off pressed in snoozing paly ofid
* ALARM_STOP push history
* change AP start functory
* Opration_02H with operation function
* change uart task size

## v01.19
2018年6月7日13:49:35
 
* add over 24h led do not display
* add dddddd sound download like weather
* save ssid and key with out checkout
* add 1s time read
* issue alarm play volume err
* add audio play stop stop_play
* issue default err_sound volume max
* issue display time set year problem
* issue when up down pressed ,ap can in

## v01.18
2018年5月31日14:40:04

* change scan alarm sound as 3
* change alarm operation
* change alarm skip
* issue snooze_key notice not play err_sound
* add time_Mutex
* change some error not goto exit in tcp
## v01.17
2018年5月25日10:20:23

* set version01.17
* AP time out change tu 600 seconds
* issue default alarm
* add oid_error_sound
* issue al01 do not refresh alarm,when alarms did not changed
* alarm data duplicate add err = kGeneralErr
* issue eland_alarm_is_same bug
* add alarm same check
* sound download retry time change to 3
* change sound sort rule
* add sound file checkout function
* add ssid_scan time out response json
* issue other mode alarm dead
* sound file add file_end_string

## v01.16
2018年5月18日16:14:14

###  5V LCD
* set v01.16
* issue alarm stop double times then do not play ofid 
* change HTTP_W_R_struct.mutex place,make sure ,alarm and file download only one display
* eland_communication_info_get err wait 3s before again
* TCP_send receive goto exit when != eland_NC eland_NA
* issue two https request mutex not useful err
* change stream_id to only one
* oid do net play in other mode but nc
* issue weather scan when ppattern ==2 ==3 do not download ofid
* issue alarm play 61 seconde
* add sound file remove operation
* issue sound file arrange delete other file
* change flash erase when writing
* issue is_sound_file_usable pattern ==3 sid return false bug
* add file download err remove file
* add oid eeeeee 204 error bug
* change alarm ofid play bug
* change domain name
* issue week alarm bug
* issue next alarm not display befor one day more
* add Alarm clock schedule get when alarm refreshed
* issue odi just weather bug
* change AP mode function to function file_download
* issue des_recovery IsAlreadySet set 0 bug
* issue simple clock mode oid bug
* AP mode change to simple clock mode reboot mcu
* online offline mode do not restart system
* send eland mode to mcu when set_eland_mode


## v01.15
2018年4月28日14:56:18

###  3.3V LCD 

## v01.14
2018年4月28日14:19:34

* 1 alarm led display
* 2 issue ALARM_SKIP mode alarm not operation
* 3 simple clock operation
* 4 add area code in eland data
* 5 issue alarm_number =0 clear alarm display
* 6 add led bright in eland data
* 7 alarm skip according to elsv
* 8 add oid sound control
* 9 add download fail count out
* 10 issue alarm skip push failed
* 11 add oid sound volume
* 12 add data get_notification_volume
* 13 add alarm_nearest temporary memory
* 14 change flash sector erase in SOUND_FILE_CLEAR
* 15 add Alarm clock schedule request
* 16 display alarm clock schedule operation NA mode
* 17 add timezone area code save in flash inside
* 18 add weather file download
* 19 add eland_error
* 20 wait weather queue operation
* 21 issue file download err exit
* 22 tcp health check err --> reconnect
* 23 change muc iwdg 1.717s


## v01.12
2018年4月12日14:31:13

* change lcd 5V（LCD_All_angle_view）
## v01.11
2018年4月12日13:44:18 

* 1 fix alarm repeat 3 bug
* 2 alarm off with app record time & stop reason 2
* 3 press ALARM_OFF in snoozing,stop alarm snooze
* 4 new alarm data play priority
* 5 change alarm_mcu_data struct
* 6 add primary_dns
* 7 change ssid scan value 0-100
* 8 add ELAND_H0E_Send to issue eland delet data operation
* 9 change _alarm_mcu_data_t struct
* 10 add alarm skip tcp send
* 11 alarm skip operation
* 12 flash capacity arrange

## v01.10 
2018年4月3日11:25:30

* `fix alarm off history data empty bug`
* `fix AP mode eland_data.eland_name empty bug`
* `change simple clock:`
    * snooze:3 
    * interval:10 minutes
    * continue:15 minutes
* `change time set(simple clock) year 00~37`
* `fix snooze for alarm paly snooze + 1`
* `interval time begain at alarm stop `
* `fix power on week display error`
* `fix flash erase bug`

# add branch for communication
time:2017年9月15日09:41:22

 * 1.eland device info login
 * 2.eland device info get
 * 3.alarm start notice
 * 4.alarm OFF notice
 * 5.OTA start notice
# branch:netclockPerformance 
2017年8月7日 帶去日本的表演程序

