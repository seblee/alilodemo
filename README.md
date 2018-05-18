# Netclock

# v01.16
2018年5月18日16:14:14
##  5V LCD

##
    set v01.16
    issue alarm stop double times then do not play ofid 
    change HTTP_W_R_struct.mutex place,make sure ,alarm and file download only one display
    eland_communication_info_get err wait 3s before again
    TCP_send receive goto exit when != eland_NC eland_NA
    issue two https request mutex not useful err
    change stream_id to only one
    oid do net play in other mode but nc
    issue weather scan when ppattern ==2 ==3 do not download ofid
    issue alarm play 61 seconde; add sound file remove operation
    issue sound file arrange delete other file
    change flash erase when writing
    issue is_sound_file_usable pattern ==3 sid return false bug
    add file download err remove file
    add oid eeeeee 204 error bug
    change alarm ofid play bug
    change domain name
    issue week alarm bug
    issue next alarm not display befor one day more; add Alarm clock schedule get when alarm refreshed
    issue odi just weather bug
    change AP mode function to function file_download
    issue des_recovery IsAlreadySet set 0 bug
    issue simple clock mode oid bug
    AP mode change to simple clock mode reboot mcu
    online offline mode do not restart system
    send eland mode to mcu when set_eland_mode    


# v01.15
2018年4月28日14:56:18
##  3.3V LCD 

# v01.14
2018年4月28日14:19:34
## 
    1 alarm led display;
    2 issue ALARM_SKIP mode alarm not operation;
    3 simple clock operation;
    4 add area code in eland data;
    5 issue alarm_number =0 clear alarm display;
    6 add led bright in eland data;
    7 alarm skip according to elsv
    8 add oid sound control;
    9 add download fail count out;
    10 issue alarm skip push failed;
    11 add oid sound volume;
    12 add data get_notification_volume;    
    13 add alarm_nearest temporary memory;
    14 change flash sector erase in SOUND_FILE_CLEAR;
    15 add Alarm clock schedule request;
    16 display alarm clock schedule operation NA mode
    17 add timezone area code save in flash inside;
    18 add weather file download;
    19 add eland_error;
    20 wait weather queue operation
    21 issue file download err exit;
    22 tcp health check err --> reconnect;

    23 change muc iwdg 1.717s


# v01.12
2018年4月12日14:31:13
* change lcd 5V（LCD_All_angle_view）
# v01.11
2018年4月12日13:44:18 
## 
    1 fix alarm repeat 3 bug;
    2 alarm off with app record time & stop reason 2
    3 press ALARM_OFF in snoozing,stop alarm snooze
    4 new alarm data play priority
    5 change alarm_mcu_data struct
    6 add primary_dns
    7 change ssid scan value 0-100
    8 add ELAND_H0E_Send to issue eland delet data operation;
    9 change _alarm_mcu_data_t struct; 
    10 add alarm skip tcp send; 
    11 alarm skip operation;

    12 flash capacity arrange

# v01.10 
2018年4月3日11:25:30
## fix alarm off history data empty bug
## fix AP mode eland_data.eland_name empty bug
## change simple clock:
	snooze:3 
	interval:10 minutes
	continue:15 minutes
## change time set(simple clock) year 00~37
## fix snooze for alarm paly snooze + 1 
## interval time begain at alarm stop 
## fix power on week display error
## fix flash erase bug

# add branch for communication
 time:2017年9月15日09:41:22
1. eland device info login
2. eland device info get
3. alarm start notice
4. alarm OFF notice
5. OTA start notice
# branch:netclockPerformance 
2017年8月7日 帶去日本的表演程序

