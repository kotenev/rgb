/*
 * Arduino IDE 1.6.5
 * ArduinoJson 5.2.0
 */
#include <ESP8266WiFi.h>        //Содержится в пакете
#include <ESP8266WebServer.h>   //Содержится в пакете
#include <ESP8266SSDP.h>        //Содержится в пакете
#include <FS.h>                 //Содержится в пакете
#include <time.h>               //Содержится в пакете
#include <Ticker.h>             //Содержится в пакете
#include <WiFiUdp.h>            //Содержится в пакете
#include <ESP8266HTTPUpdateServer.h> //Содержится в пакете
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
int DDNSPort=8080; // порт для обращение к устройству с wan
// Web интерфейс для устройства
ESP8266WebServer HTTP(80);
ESP8266WebServer HTTPWAN(DDNSPort);
ESP8266HTTPUpdateServer httpUpdater;
// Для файловой системы
File fsUploadFile;
// Для сервопривода
//Servo myled;
// Для тикера
Ticker tickerSetLow;
Ticker tickerAlert;
Ticker tickerBizz;

// Кнопка управления
#define Tach0 0

// 2811 на ноге в количестве
#define led_pin 2
#define led_num 15 //Количество лед огней
#define buzer_pin 3
Adafruit_NeoPixel strip = Adafruit_NeoPixel(led_num, led_pin, NEO_GRB + NEO_KHZ800);
// Определяем переменные

String _ssid     = "WiFi"; // Для хранения SSID
String _password = "Pass"; // Для хранения пароля сети
String _ssidAP = "RGB05";   // SSID AP точки доступа
String _passwordAP = ""; // пароль точки доступа
String XML;              // формирование XML
String _setAP ="1";           // AP включен
String SSDP_Name = "jalousie";      // SSDP
String times1 = "00:00:00";      // Таймер 1
String times2 = "00:00:00";    // Таймер 2
String Devices = "";    // IP адреса устройств в сети
String DDNS ="";      // url страницы тестирования WanIP
String DDNSName ="";  // адрес сайта DDNS
String Language ="ru";  // язык web интерфейса
int timezone = 3;        // часовой пояс GTM
int TimeLed = 60;  // Время работы будильника
String kolibrTime = "03:00:00"; // Время колибровки часов
volatile int chaingtime = LOW;
volatile int chaing = LOW;
volatile int chaing1 = LOW;
int volume = 512;// max =1023
int state0 = 0;
int r=255;
int g=138;
int b=10;
int t=300;
int s=5;

int step=0;
int ledon=0;
int ledState = LOW;
long previousTime = 0;
long interval = 100;

unsigned int localPort = 2390;
unsigned int ssdpPort = 1900;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

void setup() {
 Serial.begin(115200);
 pinMode(Tach0, INPUT);
 pinMode(led_pin, OUTPUT);
 pinMode(buzer_pin, OUTPUT);
 digitalWrite(buzer_pin,1);
 // Serial.println("");
 // Включаем работу с файловой системой
 FS_init();
 // Загружаем настройки из файла
 loadConfig();
 // Подключаем RGB
 initRGB();
 // Кнопка будет работать по прерыванию
 attachInterrupt(Tach0, Tach_0, FALLING);
 //Запускаем WIFI
 WIFIAP_Client();
 // Закускаем UDP
 udp.begin(localPort);
 //udp.beginMulticast(WiFi.localIP(), ssdpAdress1, ssdpPort);
 //Serial.print("Local port: ");
 //Serial.println(udp.localPort());
 //настраиваем HTTP интерфейс
 HTTP_init();
 //Serial.println("HTTP Ready!");
 //запускаем SSDP сервис
 SSDP_init();
 //Serial.println("SSDP Ready!");
 // Включаем время из сети
 Time_init(timezone);
 // Будет выполняться каждую секунду проверяя будильники
 tickerAlert.attach(1, alert);
 ip_wan();
}

void loop() {
 dnsServer.processNextRequest();
 HTTP.handleClient();
 delay(1);
 HTTPWAN.handleClient();
  delay(1);
 handleUDP();
 if (chaing && !chaing1) {
  noInterrupts();
  switch (state0) {
   case 0:
    ledon=1;
    //LedON(r, g, b);
    chaing=!chaing;
    state0=1;
    break;
   case 1:
    ledon=1;
    // LedON(0, 0, 0);
    chaing=!chaing;
    state0=0;
    break;
   case 3:
    analogWrite(buzer_pin, 0);
    digitalWrite(buzer_pin,1);
    state0=0;
    break;
   case 4:
   ip_wan();
   state0=0;
    break;
  }
  interrupts();
 }
 if (chaingtime) {
  Time_init(timezone);
  chaingtime=0;
 }

 if (ledon) {
  unsigned long currentTime = millis();
  if(currentTime - previousTime > interval) {
   previousTime = currentTime;
   if (ledState == LOW) ledState = HIGH; else ledState = LOW;
   if (state0) {
    strip.setPixelColor(step, strip.Color(r, g, b));
   } else {
    strip.setPixelColor(step, strip.Color(0, 0, 0));
   }
   strip.show();
   step++;
   if (strip.numPixels() == step) { ledon=0; step=0; }
  }
 }

}

// Вызывается каждую секунду в обход основного цикла.
void alert() {
 String Time=XmlTime();
 if (times1.compareTo(Time) == 0 && times1 != "00:00:00") {
  alarm_clock();
 }
 if (times2.compareTo(Time) == 0 && times2 != "00:00:00") {
  alarm_clock();
 }
  Time = Time.substring(3, 8); // Выделяем из строки минуты секунды
  // Каждые 15 минут делаем запрос на сервер DDNS
 if (Time == "00:00" || Time == "15:00" || Time == "30:00"|| Time == "45:00") {
  chaing=1;
  state0=4;
 }
 if (kolibrTime.compareTo(Time) == 0) {
  chaingtime=1;
 }
}


