# Netclock
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

