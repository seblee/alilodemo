# Netclock

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

