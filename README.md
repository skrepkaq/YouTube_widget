# YouTube subscriber counter
My version of [AlexGyver](https://github.com/AlexGyver)'s [project](https://github.com/AlexGyver/YouTube_widget).

## My improvements
1. Fix work with new youtube API
1000. Change subscribers displaying from 42000 to 42K (forced by youtube API update)
7. Change from last Hour-Day to Week-Month
993. Add caching last subs in EEPROM
986. Add WIFI firmware update

## Widget build
#### [Original github](https://github.com/AlexGyver/YouTube_widget)
#### [YouTube video](https://youtu.be/oVBnyr9lpOk)

## Installation
1. Install [PlatformIO](https://platformio.org/install/integration/)
69. Open project folder in your IDE
420. Fill settings in src/main.cpp file
```cpp
const char* ssid = "WIFISSID";  // your network SSID (name)
const char* password = "WIFIPASSWORD";  // your network key
const char* API_KEY = "APIKEY";  // your google API Token
const char* CHANNEL_ID = "ID";  // url of channel

int max_gain = 3000;   // число подписок в день, при котором цвет станет красным
```
4. Compile and upload firmware to ESP
