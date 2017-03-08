/**The MIT License (MIT)

Copyright (c) 2016 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

/* Customizations by Neptune (NeptuneEng on Twitter, Neptune2 on Github)
 *  
 *  Added Wifi Splash screen and credit to Squix78
 *  Modified progress bar to a thicker and symmetrical shape
 *  Replaced TimeClient with built-in lwip sntp client (no need for external ntp client library)
 *  Added Daylight Saving Time Auto adjuster with DST rules using simpleDSTadjust library
 *  https://github.com/neptune2/simpleDSTadjust
 *  Added Setting examples for Boston, Zurich and Sydney
  *  Selectable NTP servers for each locale
  *  DST rules and timezone settings customizable for each locale
   *  See https://www.timeanddate.com/time/change/ for DST rules
  *  Added AM/PM or 24-hour option for each locale
 *  Changed to 7-segment Clock font from http://www.keshikan.net/fonts-e.html
 *  Added Forecast screen for days 4-6
  *   >>> Need to change WundergroundClient.h in ESP8266_Weather_Station library
  *    #define MAX_FORECAST_PERIODS 12  // Was 7
 *  Added support for DHT22 Indoor Temperature and Humidity
 *  Fixed bug preventing display.flipScreenVertically() from working
 *  Slight adjustment to overlay
 */

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include "SSD1306.h"
//#include "SSD1306Wire.h"
// #include <SPI.h> // Only needed for Arduino 1.6.5 and earlier
//#include "SSD1306Spi.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "DSEG7Classic-BoldFont.h"
#include <time.h>
#include "DHT.h"
#include "ThingspeakClient.h"
#include <simpleDSTadjust.h>
#include <PubSubClient.h>

/***************************
 * Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions

#define HOSTNAME "ESP8266-OTA-WEATHER-"

// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
// Pin definitions for SPI OLED
//#define OLED_CS     D8  // Chip select
//#define OLED_DC     D2  // Data/Command
//#define OLED_RESET  D0  // RESET
// Pin definitions for i2c
const int I2C_DISPLAY_ADDRESS = 0x3c;
//const int SDA_PIN = D3; // NodeMCU
//const int SDC_PIN = D4; // NodeMCU
const int SDA_PIN = D2; // Wemos D1 Mini
const int SDC_PIN = D3; // Wemos D1 Mini



// DHT Settings
// #define DHTPIN D2 // NodeMCU
#define DHTPIN D4 // Wemos D1R2 Mini
#define DHTTYPE 22   // DHT22  (AM2302), AM2321
char FormattedTemperature[10];
char FormattedHumidity[10];

//MQTT / HomeKit
// Update these with values suitable for your network.
IPAddress MQTTserver(192, 168, 1, 1);
WiFiClient wclient;
PubSubClient client(wclient, MQTTserver);

// -----------------------------------
// Locales (uncomment only 1)
#define Genova
//#define Boston
// #define Sydney
//------------------------------------

#ifdef Genova
//DST rules for Central European Time Zone
#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour

// Uncomment for 24 Hour style clock
#define STYLE_24HR

#define NTP_SERVERS "0.ch.pool.ntp.org", "1.ch.pool.ntp.org", "2.ch.pool.ntp.org"

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "___";
const String WUNDERGRROUND_LANGUAGE = "IT";
const String WUNDERGROUND_COUNTRY = "IT";
const String WUNDERGROUND_CITY = "Genova";
const String WUNDERGROUND_PWS = "IGENOVA170";
#endif

/*#ifdef Boston
//DST rules for US Eastern Time Zone (New York, Boston)
#define UTC_OFFSET -5
struct dstRule StartRule = {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours
struct dstRule EndRule = {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour

// Uncomment for 24 Hour style clock
#define STYLE_24HR

#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"

// Wunderground Settings
const boolean IS_METRIC = false;
const String WUNDERGRROUND_API_KEY = "<WUNDERGROUND KEY HERE>";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "MA";
const String WUNDERGROUND_CITY = "Boston";
#endif

#ifdef Sydney
//DST Rules for Australia Eastern Time Zone (Sydney)
#define UTC_OFFSET +10
struct dstRule StartRule = {"AEDT", First, Sun, Oct, 2, 3600}; // Australia Eastern Daylight time = UTC/GMT +11 hours
struct dstRule EndRule = {"AEST", First, Sun, Apr, 2, 0};      // Australia Eastern Standard time = UTC/GMT +10 hour

// Uncomment for 24 Hour style clock
//#define STYLE_24HR

#define NTP_SERVERS "0.au.pool.ntp.org", "1.au.pool.ntp.org", "2.au.pool.ntp.org"

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "<WUNDERGROUND KEY HERE>";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "AU";
const String WUNDERGROUND_CITY = "Sydney";
#endif*/

#define italiano

#ifdef english
String str_mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
String str_wday[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
#endif

#ifdef italiano
String str_mon[] = {"Gen", "Feb", "Marzo", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};
String str_wday[] = {"Dom", "lun", "Mar", "Mer", "Gio", "Ven", "Sab"};  // 3 letras
// String str_wday[] = {"lu", "ma", "mi", "ju", "vi", "sa", "do"}; // 2 letras
#endif



void HomeKit(){
    if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect("ESPNurseryTemperatureSensor")) {
        client.publish("HomeKit","Nursery Temperature Sensor Online!");
      }
    }

    if (client.connected()){
      Serial.println("publishing");
        client.publish("NurseryTemperature",String(FormattedTemperature));   
        client.loop();
    }
      
  }
}

//Thingspeak Settings
const String THINGSPEAK_CHANNEL_ID = "67284";
const String THINGSPEAK_API_READ_KEY = "L2VIW20QVNZJBLAK";

// Initialize the I2 oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

// SPI OLED
//SSD1306Spi display(OLED_RESET, OLED_DC, OLED_CS);

OLEDDisplayUi   ui( &display );

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

/***************************
 * End Settings
 **************************/

// TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

// Initialize the temperature/ humidity sensor
DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;
float temperature = 0.0;

ThingspeakClient thingspeak;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;
// flag changed in the ticker function every 1 minute
bool readyForDHTUpdate = false;

String lastUpdate = "--";

Ticker ticker;
Ticker weathertick;

//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawOtaProgress(unsigned int, unsigned int);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();
int8_t getWifiQuality();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawIndoor, drawForecast  };
//FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawIndoor, drawThingspeak, drawForecast, drawForecast2  };
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;



void setup() {
    // Turn On VCC
  // pinMode(D4, OUTPUT);
  // digitalWrite(D4, HIGH);
  Serial.begin(115200);

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  display.flipScreenVertically();  // Comment out to flip display 180deg
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  // Credit where credit is due
  display.drawXbm(-6, 5, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.drawString(88, 18, "Weather Station\nBy Simone");
  display.display();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  // wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
 // wifiManager.autoConnect();

  //Manual Wifi
  const char* SSID = "___"; 
const char* PASSWORD = "___";
   WiFi.begin(SSID, PASSWORD);
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);


  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connessione alla WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    display.display();

    counter++;
  }

  ui.setTargetFPS(30);
  ui.setTimePerFrame(10*1000); // Setup frame display time to 10 sec
  
  //Hack until disableIndicator works:
  //Set an empty symbol
  ui.setActiveSymbol(emptySymbol);
  ui.setInactiveSymbol(emptySymbol);

  ui.disableIndicator();

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  // ui.init();  // not necessary - already done above, breaks flipScreenVertically()

  // Setup OTA
  Serial.println("Hostname: " + hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onProgress(drawOtaProgress);
  ArduinoOTA.begin();

  updateData(&display);

  //ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
  ticker.attach(60, setReadyForDHTUpdate);
  weathertick.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  if (readyForDHTUpdate && ui.getUiState()->frameState == FIXED)
    updateDHT();
    
  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    ArduinoOTA.handle();
    delay(remainingTimeBudget);
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Wifi Manager");
  display.drawString(64, 20, "Please connect to AP");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "To setup Wifi Configuration");
  display.display();
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 12, percentage);
  display->display();
}

void drawOtaProgress(unsigned int progress, unsigned int total) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "OTA Update");
  display.drawProgressBar(2, 28, 124, 12, progress / (total / 100));
  display.display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Aggiornamento data...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  drawProgress(display, 30, "Aggiornamento meteo...");
  //wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  wunderground.updateConditionsPws(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_PWS);
  drawProgress(display, 50, "Aggiornamento previsioni...");
  //wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_PWS);
  drawProgress(display, 70, "Aggiornamento Hum DHT...");
  humidity = dht.readHumidity();
  drawProgress(display, 80, "Aggiornamento Temp DHT...");
  temperature = dht.readTemperature(!IS_METRIC);
  delay(500);
  
  drawProgress(display, 90, "Fine aggiornamento...");
  thingspeak.getLastChannelItem(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_READ_KEY);
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Fatto...");
  delay(1000);
}

// Called every 1 minute
void updateDHT() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature(!IS_METRIC);
  readyForDHTUpdate = false;
 // MQTT
  HomeKit();
}


void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char *dstAbbrev;
  char time_str[11];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = ctime(&now);
  date = str_wday[timeinfo->tm_wday] + " " + String(timeinfo->tm_mday) + " " + str_mon[timeinfo->tm_mon] + " " + String(1900+timeinfo->tm_year);
  //date = date.substring(0,4) + String(timeinfo->tm_mday) + date.substring(3,8) + String(1900+timeinfo->tm_year);
  //date = date.substring(0,11) + String(1900+timeinfo->tm_year);
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(DSEG7_Classic_Bold_21);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  
#ifdef STYLE_24HR
  sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  display->drawString(108 + x, 19 + y, time_str);
#else
  int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  sprintf(time_str, "%2d:%02d:%02d\n",hour, timeinfo->tm_min, timeinfo->tm_sec);
  display->drawString(101 + x, 19 + y, time_str);
#endif

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
#ifdef STYLE_24HR
  sprintf(time_str, "%s", dstAbbrev);
  display->drawString(108 + x, 27 + y, time_str);  // Known bug: Cuts off 4th character of timezone abbreviation
#else
  sprintf(time_str, "%s\n%s", dstAbbrev, timeinfo->tm_hour>=12?"pm":"am");
  display->drawString(102 + x, 18 + y, time_str);
#endif

}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(50 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + (IS_METRIC ? "°C": "°F");
  
  display->drawString(50 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(28 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 6);
  drawForecastDetails(display, x + 44, y, 8);
  drawForecastDetails(display, x + 88, y, 10);
}

void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0, "Sensore interno");
  display->setFont(ArialMT_Plain_16);
  dtostrf(temperature,4, 1, FormattedTemperature);
  display->drawString(64+x, 12, "Temp: " + String(FormattedTemperature) + (IS_METRIC ? "°C": "°F"));
  dtostrf(humidity,4, 1, FormattedHumidity);
  display->drawString(64+x, 30, "Hum: " + String(FormattedHumidity) + "%");

}

void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "Thingspeak Sensor");
  display->setFont(ArialMT_Plain_16);
  display->drawString(64 + x, 12 + y, thingspeak.getFieldValue(0) + "°C");
  // display->drawString(64 + x, 12 + y, thingspeak.getFieldValue(0) + (IS_METRIC ? "°C": "°F"));  // Needs code to convert Thingspeak temperature string
  display->drawString(64 + x, 30 + y, thingspeak.getFieldValue(1) + "%");
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + " - " + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  char time_str[11];
  time_t now = dstAdjusted.time(nullptr);
  struct tm * timeinfo = localtime (&now);

  display->setFont(ArialMT_Plain_10);

#ifdef STYLE_24HR
  sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
#else
  int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  sprintf(time_str, "%2d:%02d:%02d%s\n",hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_hour>=12?"pm":"am");
#endif

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(5, 52, time_str);

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  String temp = wunderground.getCurrentTemp() + (IS_METRIC ? "°C": "°F");
  display->drawString(101, 52, temp);

  int8_t quality = getWifiQuality();
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        display->setPixel(120 + 2 * i, 61 - j);
      }
    }
  }


  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Meteocons_Plain_10);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  // display->drawString(64, 55, weatherIcon);
  display->drawString(77, 53, weatherIcon);

  display->drawHorizontalLine(0, 51, 128);

}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void setReadyForDHTUpdate() {
  Serial.println("Setting readyForDHTUpdate to true");
  readyForDHTUpdate = true;
}

