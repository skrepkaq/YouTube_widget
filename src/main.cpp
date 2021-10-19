/* Счётчик подписчиков канала YouTube
  by AlexGyver
  2017
  Реализовано: подключение к wifi с возможностью восстановить подключение, если оно было разорвано (без перезагрузки)
  Из ответа google.api получаем число подписчиков с любым количеством знаков (идёт отсечка по символу ")
  Запаздывающий фильтр обеспечивает плавную смену цвета
*/

//-----------НАСТРОЙКИ-------------
const char* ssid = "WIFISSID";  // your network SSID (name)
const char* password = "WIFIPASSWORD";  // your network key
const char* API_KEY = "APIKEY";  // your google API Token
const char* CHANNEL_ID = "ID";  // url of channel

int max_gain = 50000;   // число просмотров в неделю, при котором цвет станет красным
//-----------НАСТРОЙКИ-------------

const char* host = "WiFiWidget";
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";
const char* MYssid = "SUB_widget";
const char* MYpassword = "admin";
const char* google_host = "www.googleapis.com";

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>

WiFiClientSecure wsclient;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
String httpsFingerprint = "";
String payload, sub_string;
uint32_t delta, color, brightness;
uint32_t mid_s, max_s;
uint16_t R, G, B;
uint16_t get_brightness = 1000;
uint16_t start_point, end_point;
uint8_t tries;
float delta_f = 0.0;
float K = 0.99, k;
float temp = 0.0;
uint16_t i = 0, j = 0;
uint32_t views, new_views = 0;
uint32_t subscribers;
boolean display_mode = true, wifi_connect = false;
boolean start_flag = true;
boolean mqtt_update = false;
const char *compares[] = {"\"subscriberCount", "viewCount"};
uint32_t last_check, last_color, last_Submins, last_BL, last_mode, last_http_get, wifi_try, dif_h;
HTTPClient http;
uint32_t delta_week[7], delta_month[30], total_week_views, total_month_views;
uint32_t minute_views;
uint32_t last_minute;
uint32_t last_day;
uint8_t days = 0, days_week = 0;
uint32_t min_step = 60;    // считать прирост за указанное число минут

LiquidCrystal_I2C lcd(0x3F, 16, 2);

//---- настрйока пинов-----
#define ledR 14
#define ledG 13
#define ledB 12
#define BL_pin 16
//---- настрйока пинов-----


void setup () {
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  Serial.begin(115200);
  EEPROM.begin(40);
  pinMode(A0, INPUT);
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  pinMode(BL_pin, OUTPUT);
  Wire.begin(5, 4);
  analogWrite(BL_pin, 1023);

  lcd.init();
  lcd.display();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  for (int i = 0; i < 7; i++) {
    delta_week[i] = EEPROM.read(i) * 100; //0-59
  }
  for (int i = 0; i < 30; i++) {
    delta_month[i] = EEPROM.read(i + 7) * 100; //60-83
  }
  days_week = EEPROM.read(37);
  days = EEPROM.read(38);
  dif_h = EEPROM.read(39) * 1000000;
  total_week_views = 0;
  for (int i = 0; i < 7; i++) {
    total_week_views += delta_week[i];
  }
  total_month_views = 0;
  for (int i = 0; i < 30; i++) {
    total_month_views += delta_month[i];
  }
  delta_f = total_week_views;

  // Attempt to connect to Wifi network:
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi");
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi.");
    tries++;
  }

  if (tries > 19) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP(MYssid, MYpassword);
    IPAddress ip = WiFi.softAPIP();
    MDNS.begin(host);
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("WiFi hosted");
    lcd.setCursor(2, 1);
    lcd.print(ip);
    httpUpdater.setup(&httpServer, update_path, update_username, update_password);
    httpServer.begin();
    MDNS.addService("http", "tcp", 80);
    while (true) {
      httpServer.handleClient();
    }
  }

  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
}


boolean getSubs() {
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - last_http_get < 60000 && millis() > 60000) {
      return true;
    }

    wsclient.setInsecure();
    if (!wsclient.connect(google_host, 443)) {
        Serial.println("connection failed");
        return false;
      }

      String url = String("/youtube/v3/channels?part=statistics&key=") + API_KEY + "&id=" + CHANNEL_ID;

      wsclient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                  "Host: " + google_host + "\r\n" + 
                  "Connection: close\r\n\r\n");
      while (wsclient.connected()) {
        String line = wsclient.readStringUntil('\n');
        for (int type = 0; type < 2; type++) {
          i = 0;
          j = 0;
          while (i < line.length()) {
            if (line[i] == compares[type][j]) {
              j++;
            } else j = 0;
            if (j == strlen(compares[type])-1) {
              sub_string = "";
              last_http_get = millis();
              int start_point = i + 6;
              for (int ii = start_point; line[ii] != '"'; ii++) {
                sub_string += line[ii];
              }
              if (type == 0) {
                subscribers = sub_string.toInt();
                Serial.print("SUBS: ");
                Serial.println(subscribers);
                return true;
              } else {
                Serial.print("VIEWS: ");
                new_views = sub_string.toInt();
                Serial.println(new_views);
              }
            }
            i++;
          }
        }
      }
    }
  return false;
}


void loop() {
  ESP.wdtFeed();   // покормить пёсика
  if (!wifi_connect || WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    wifi_try = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (millis() - wifi_try > 10000) break;
    }
    if (WiFi.status() != WL_CONNECTED) {
      lcd.clear();
      lcd.print("Connect failed");
      delay(500);
      wifi_connect = false;
    } else {
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("WiFi connected");
      lcd.setCursor(2, 1);
      lcd.print(WiFi.localIP());
      delay(500);
      wifi_connect = true;
    }

  }
  httpServer.handleClient();
  if (start_flag && wifi_connect) {
    max_gain = max_gain / (60 / min_step);  // узнать пороговое число подписок за min_step
    min_step = (long)min_step * 60000;
    mid_s = max_gain * 5;
    max_s = max_gain * 10;
    while (new_views == 0) {
      Serial.println("try");
      Serial.println(new_views);
      getSubs();
    }
    views = new_views;
    minute_views = new_views;
    start_flag = false;
  }
  if (millis() - last_mode > 10000) {
    display_mode = !display_mode;
    last_mode = millis();
  }
  if (millis() - last_check > 6000 && wifi_connect) {
    if (getSubs()) {
      switch (display_mode) {
        case 0:
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("Subscribers:");
          lcd.setCursor(6, 1);
          lcd.print(subscribers / 1000);
          lcd.print("K");
          break;
        case 1:
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Last week:");
          lcd.setCursor(11, 0);
          lcd.print(total_week_views);
          lcd.setCursor(0, 1);
          lcd.print("Last month:");
          lcd.setCursor(12, 1);
          lcd.print(total_month_views / 1000);
          lcd.print("K");
          break;
      }
      last_check = millis();
    } else {
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("No internet");
      delay(1000);
    }
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Connecting WiFi");
        delay(500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Connecting WiFi.");
      }
    }
  }
  if (millis() - last_Submins > min_step && wifi_connect) {
    last_Submins = millis();
  }
  if (millis() - last_color > 30000 && wifi_connect) {
    delta = total_week_views;
    if (delta < 0) delta = 0;
    views = new_views;
    delta_f = delta_f * K + delta * (1 - K);
    color = delta_f * 10;
    if (color <= mid_s) {
      k = map(color, 0, mid_s, 0, 1023);
      B = 1023 - k;
      G = k;
      R = 0;
    }
    if (color > mid_s) {
      k = map(color, mid_s, max_s, 0, 1023);
      if (k > 1023) k = 1023;
      B = 0;
      G = 1023 - k;
      R = k;
    }
    //brightness = map(analogRead(A0), 140, 1024, 1023, 200);
    brightness = map(get_brightness, 0, 1000, 0, 1023);
    if (brightness > 1023) brightness = 1023;
    if (brightness < 200) brightness = 200;
    analogWrite(BL_pin, brightness);
    analogWrite(ledR, R * brightness / 1023);
    analogWrite(ledG, G * brightness / 1023);
    analogWrite(ledB, B * brightness / 1023);
    last_color = millis();
  }
  if ((millis() - last_minute > 60000) && wifi_connect) {
    delta_month[days] += (long)new_views - minute_views;
    if (delta_month[days] < 0) delta_month[days] = 0;
    minute_views = new_views;

    delta_week[days_week] = delta_month[days];

    total_week_views = 0;

    for (int i = 0; i < 7; i++) {
      total_week_views += delta_week[i];
    }
    total_month_views = 0;
    for (int i = 0; i < 30; i++) {
      total_month_views += delta_month[i];
    }

    for (int i = 0; i < 7; i++) {
      EEPROM.write(i, delta_week[i] / 100);
    }
    for (int i = 0; i < 30; i++) {
      EEPROM.write(i + 7, delta_month[i] / 100);
    }
    EEPROM.write(37, days_week);
    EEPROM.write(38, days);
    EEPROM.write(39, int((millis() - last_day + dif_h) / 1000000));
    EEPROM.commit();
    last_minute = millis();
  }
  if ((millis() + dif_h - last_day > 86400000) && wifi_connect) {
    days++;
    days_week++;
    if (days > 29) {
      days = 0;
    }
    delta_month[days] = 0;
    if (days_week > 6) {
      days_week = 0;
    }
    delta_week[days_week] = 0;
    dif_h = 0;
    last_day = millis();
  }
}
