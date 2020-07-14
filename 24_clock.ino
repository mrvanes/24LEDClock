/*
   Based on https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon
*/
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE
extern "C" {
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <EEPROM.h>
#include "Field.h"

/*######################## MAIN CONFIG ########################*/
#define LED_TYPE            WS2812                      // You might also use a WS2811 or any other strip that is Fastled compatible
#define DATA_PIN            D1                          // Be aware: the pin mapping might be different on boards like the NodeMCU
//#define CLK_PIN           D5                          // Only required when using 4-pin SPI-based LEDs
#define CORRECTION          UncorrectedColor            // If colors are weird use TypicalLEDStrip
#define COLOR_ORDER         GRB                         // Change this if colors are swapped (in my case, red was swapped with green)
#define MILLI_AMPS          1500                        // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define VOLTS               5                           // Voltage of the Power Supply

#define HOSTNAME "LedClock"                 // Name that appears in your network, don't use whitespaces, use "-" instead

//---------------------------------------------------------------------------------------------------------//
// Device Configuration:
//---------------------------------------------------------------------------------------------------------//
#define NUM_LEDS 24
#define FRAMES_PER_SECOND  120  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.

//---------------------------------------------------------------------------------------------------------//
// Feature Configuration: Enabled by removing the "//" in front of the define statements
//---------------------------------------------------------------------------------------------------------//
    //#define ACCESS_POINT_MODE                 // the esp8266 will create a wifi-access point instead of connecting to one, credentials must be in Secrets.h
    #define ENABLE_MULTICAST_DNS              // allows to access the UI via "http://<HOSTNAME>.local/", implemented by GitHub/WarDrake

/*#########################################################################################################//
-----------------------------------------------------------------------------------------------------------//
  _____ ____   _  __ ____ ____ _____    ____ _  __ ___
 / ___// __ \ / |/ // __//  _// ___/   / __// |/ // _ \
/ /__ / /_/ //    // _/ _/ / / (_ /   / _/ /    // // /
\___/ \____//_/|_//_/  /___/ \___/   /___//_/|_//____/
-----------------------------------------------------------------------------------------------------------//
###########################################################################################################*/

#include <WiFiUdp.h>

#define NTP_REFRESH_INTERVAL_SECONDS 600            // 10 minutes
const char* ntpServerName = "nl.pool.ntp.org";      // Dutch ntp-timeserver
int t_offset = 2;                                   // offset added to the time from the ntp server
const int NTP_PACKET_SIZE = 48;

IPAddress timeServerIP;
WiFiUDP udpTime;
byte packetBuffer[NTP_PACKET_SIZE];
int hours = 0; int mins = 0; int secs = 0;
unsigned int localPortTime = 2390;
unsigned long update_timestamp = 0;
unsigned long last_diff = 0;
unsigned long ntp_timestamp = 0;

ESP8266WebServer webServer(80);

#include "FSBrowser.h"
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "Secrets.h" // this file is intentionally not included in the sketch, so nobody accidentally commits their secret information.
// create a Secrets.h file with the following:
// AP mode password
// const char WiFiAPPSK[] = "your-password";

// Wi-Fi network to connect to (if not in AP mode)
// char* ssid = "your-ssid";
// char* password = "your-password";

#include <ESP8266mDNS.h>

CRGB leds[NUM_LEDS + 1];
uint8_t hourColor = 40;
uint8_t minuteColor = 130;
uint8_t secondColor = 210;

#include "Fields.h"

String getRebootString()
{
    return "<html><head><meta http-equiv=\"refresh\" content=\"4; url=/\"/></head><body><font face='arial'><b><h2>Rebooting... returning in 4 seconds</h2></b></font></body></html>";
}
void handleReboot()
{
    webServer.send(200, "text/html", getRebootString());
    delay(500);
    ESP.restart();
}

void addRebootPage(int webServerNr = 0)
{
    if (webServerNr < 2)
    {
        webServer.on("/reboot", handleReboot);
    }
}

void setup() {
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    Serial.begin(115200);

    delay(100);
    //Serial.setDebugOutput(true);

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS + 1);         // WS2812 (Neopixel)

    FastLED.setDither(true);
    FastLED.setCorrection(CORRECTION);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MILLI_AMPS);
    fill_solid(leds, NUM_LEDS + 1, CRGB::Black);
    FastLED.show();

    EEPROM.begin(4095);
    loadSettings();

    FastLED.setBrightness(brightness);

    Serial.println();
    Serial.print(F("Heap: ")); Serial.println(system_get_free_heap_size());
    Serial.print(F("Boot Vers: ")); Serial.println(system_get_boot_version());
    Serial.print(F("CPU: ")); Serial.println(system_get_cpu_freq());
    Serial.print(F("SDK: ")); Serial.println(system_get_sdk_version());
    Serial.print(F("Chip ID: ")); Serial.println(system_get_chip_id());
    Serial.print(F("Flash ID: ")); Serial.println(spi_flash_get_id());
    Serial.print(F("Flash Size: ")); Serial.println(ESP.getFlashChipRealSize());
    Serial.print(F("Vcc: ")); Serial.println(ESP.getVcc());
    Serial.println();

    SPIFFS.begin();
    {
        Serial.println("SPIFFS contents:");

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
        }
        Serial.printf("\n");
    }

#ifdef ACCESS_POINT_MODE
    WiFi.mode(WIFI_AP);
    //WiFi.mode(WIFI_AP_STA);
    //WiFi.persistent(true);
    // Do a little work to get a unique-ish name. Append the
    // last two bytes of the MAC (HEX'd) to "Thing-":
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
        String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "DeskLamp-" + macID;
    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);
    for (int i = 0; i < AP_NameString.length(); i++)
        AP_NameChar[i] = AP_NameString.charAt(i);
    //WiFi.softAP(AP_NameChar, WiFiAPPSK);
    WiFi.softAP(AP_NameChar);
    Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
    Serial.println("and open http://192.168.4.1 in your browser");
#else
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
        WiFi.hostname(HOSTNAME);
        WiFi.begin(ssid, password);
    }
#endif

    udpTime.begin(localPortTime);

    webServer.on("/all", HTTP_GET, []() {
        String json = getFieldsJson(fields, fieldCount);
        json += ",{\"name\":\"hostname\",\"label\":\"Name of the device\",\"type\":\"String\",\"value\":\"";
        json += HOSTNAME;
        json += "\"}";
        json += "]";
        webServer.send(200, "text/json", json);
        });

    webServer.on("/fieldValue", HTTP_GET, []() {
        String name = webServer.arg("name");
        String value = getFieldValue(name, fields, fieldCount);
        webServer.send(200, "text/json", value);
        });

    webServer.on("/fieldValue", HTTP_POST, []() {
        String name = webServer.arg("name");
        String value = webServer.arg("value");
        String newValue = setFieldValue(name, value, fields, fieldCount);
        webServer.send(200, "text/json", newValue);
        });

    webServer.on("/power", HTTP_POST, []() {
        String value = webServer.arg("value");
        setPower(value.toInt());
        });

    webServer.on("/brightness", HTTP_POST, []() {
        String value = webServer.arg("value");
        setBrightness(value.toInt());
        });

    webServer.on("/hour", HTTP_POST, []() {
        String value = webServer.arg("value");
        setHourColor(value.toInt());
        });

    webServer.on("/minute", HTTP_POST, []() {
        String value = webServer.arg("value");
        setMinuteColor(value.toInt());
        });

    webServer.on("/second", HTTP_POST, []() {
        String value = webServer.arg("value");
        setSecondColor(value.toInt());
        });

    //list directory
    webServer.on("/list", HTTP_GET, handleFileList);
    webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");
    webServer.begin();
    Serial.println("HTTP web server started");

    // Test RGB colors (in that order)
    fill_solid(leds, NUM_LEDS + 1, CRGB::Red);
    //FastLED.show();
    FastLED.delay(200);
    fill_solid(leds, NUM_LEDS + 1, CRGB::Green);
    //FastLED.show();
    FastLED.delay(200);
    fill_solid(leds, NUM_LEDS + 1, CRGB::Blue);
    //FastLED.show();
    FastLED.delay(200);
    fill_solid(leds, NUM_LEDS + 1, CRGB::Black);

    // Test full cirle
    for (int i=0; i<=NUM_LEDS; i++)
    {
      leds[i] = CHSV(80, 255, 255);
      FastLED.delay(200);
      fill_solid(leds, NUM_LEDS + 1, CRGB::Black);
    }

}
void sendInt(uint8_t value)
{
    sendString(String(value));
}

void sendString(String value)
{
    webServer.send(200, "text/plain", value);
}


void loop() {

    webServer.handleClient();


    EVERY_N_SECONDS(1) {
      updateTime();
      printTime();
      fill_solid(leds, NUM_LEDS + 1, CRGB::Black);
      showTime();
    }

    if (power == 0) {
        fill_solid(leds, NUM_LEDS + 1, CRGB::Black);
        FastLED.show();
        FastLED.delay(50);
        return;
    }

    //fill_solid(leds, NUM_LEDS + 1, CRGB::Black);
    //showTime();
    //FastLED.show();


#ifdef ENABLE_MULTICAST_DNS
    MDNS.update();
#endif // ENABLE_MULTICAST_DNS

    static bool hasConnected = false;

    EVERY_N_SECONDS(1) {
        if (WiFi.status() != WL_CONNECTED) {
            //      Serial.printf("Connecting to %s\n", ssid);
            hasConnected = false;
        }
        else if (!hasConnected) {
            hasConnected = true;
            Serial.print("Connected! Open http://");
            Serial.print(WiFi.localIP());
            Serial.println(" in your browser");
#ifdef ENABLE_MULTICAST_DNS
            if (!MDNS.begin(HOSTNAME)) {
                Serial.println("\nError setting up MDNS responder! \n");
            }
            else {
                Serial.println("\nmDNS responder started \n");
                MDNS.addService("http", "tcp", 80);
            }
#endif
        }
    }

    //m = (1000 - (millis() % 1000));
    // insert a delay to keep the framerate modest
    // FastLED.delay(1000 / FRAMES_PER_SECOND);
    FastLED.delay(50);
}

void loadSettings()
{
    power = EEPROM.read(0);
    brightness = EEPROM.read(1);
    hourColor = EEPROM.read(2);
    minuteColor = EEPROM.read(3);
    secondColor = EEPROM.read(4);
}

void setPower(uint8_t value)
{
    power = value == 0 ? 0 : 1;

    EEPROM.write(0, power);
    EEPROM.commit();
}

void setBrightness(uint8_t value)
{
    if (value > 255)
        value = 255;
    else if (value < 0) value = 0;

    brightness = value;

    FastLED.setBrightness(brightness);

    EEPROM.write(1, brightness);
    EEPROM.commit();
}

void setHourColor(uint8_t h)
{
    hourColor = h;
    EEPROM.write(2, h);
    EEPROM.commit();
}

void setMinuteColor(uint8_t m)
{
    minuteColor = m;
    EEPROM.write(3, m);
    EEPROM.commit();
}

void setSecondColor(uint8_t s)
{
    secondColor = s;
    EEPROM.write(4, s);
    EEPROM.commit();
}


// #################### Clock
void printTime()
{
    //hours
    //mins
    //secs
    Serial.println("h:" + String(hours) + ", m:" + String(mins) + ", s:" + String(secs));
}

float curve(float f) {
  // Two 0.5 brightness LED's aren't as bright as one on 1.0
  // So they need a little push on the low-end: y=1-(x-1)^2
  return 1-(f-1)*(f-1);
}

bool isBlack(CRGB& led) {
  return led.r == 0 && led.g == 0 && led.b == 0;
}

void showTime() {
    //hours
    //mins
    //secs
    //millis()
    float mil;
    mil = (millis() % 1000) / 1000.0;

    float s = secs + mil;
    float m = mins + s / 60.0;
    float h = (hours % 12) + m / 60.0;

    // hours
    float hour_led_frac = NUM_LEDS / 12.0 * h;
    uint8_t hour_led_floor = int(floor(hour_led_frac));
    uint8_t hour_led_ceil =  int(ceil(hour_led_frac));
    float hour_frac_floor = curve(hour_led_ceil - hour_led_frac);
    float hour_frac_ceil =  curve(hour_led_frac - hour_led_floor);
    // Don't let a coinciding 0 bright LED spoil the fun
    // for both
    if (hour_led_floor == hour_led_ceil) {
      hour_frac_floor = hour_frac_ceil = 1.0;
    }
    uint8_t hour_fint_floor = floor(hour_frac_floor * 255);
    uint8_t hour_fint_ceil = floor(hour_frac_ceil * 255);
    float hour_div = min(hour_led_floor, hour_led_ceil);

    // minutes
    float min_led_frac = NUM_LEDS / 60.0 * m;
    uint8_t min_led_floor = int(floor(min_led_frac));
    uint8_t min_led_ceil =  int(ceil(min_led_frac));
    float min_frac_floor = curve(min_led_ceil - min_led_frac);
    float min_frac_ceil =  curve(min_led_frac - min_led_floor);
    // Don't let a coinciding 0 bright LED spoil the fun
    // for both
    if (min_led_floor == min_led_ceil) {
      min_frac_floor = min_frac_ceil = 1.0;
    }
    uint8_t min_fint_floor = floor(min_frac_floor * 255);
    uint8_t min_fint_ceil = floor(min_frac_ceil * 255);
    float min_div = max(min_frac_floor, min_frac_ceil);


    // seconds
    float sec_led_frac = NUM_LEDS / 60.0 * s;
    uint8_t sec_led_floor = int(floor(sec_led_frac));
    uint8_t sec_led_ceil =  int(ceil(sec_led_frac));
    float sec_frac_floor = curve(sec_led_ceil - sec_led_frac);
    float sec_frac_ceil =  curve(sec_led_frac - sec_led_floor);
    // Don't let a coinciding 0 bright LED spoil the fun
    // for both
    if (sec_led_floor == sec_led_ceil) {
      sec_frac_floor = sec_frac_ceil = 1.0;
    }
    uint8_t sec_fint_floor = floor(sec_frac_floor * 255);
    uint8_t sec_fint_ceil = floor(sec_frac_ceil * 255);
    float sec_div = max(sec_frac_floor, sec_frac_ceil);


    leds[NUM_LEDS - (hour_led_floor % NUM_LEDS)] = CHSV(hourColor, 255, hour_fint_floor);
    leds[NUM_LEDS - (hour_led_ceil % NUM_LEDS)] = CHSV(hourColor, 255, hour_fint_ceil);

    if (isBlack(leds[NUM_LEDS - (min_led_floor % NUM_LEDS)])) {
      leds[NUM_LEDS - (min_led_floor % NUM_LEDS)] = CHSV(minuteColor, 255, min_fint_floor);
    } else {
      nblend(leds[NUM_LEDS - (min_led_floor % NUM_LEDS)], CHSV(minuteColor, 255, min_fint_floor), 128);
    }
    if (isBlack(leds[NUM_LEDS - (min_led_ceil % NUM_LEDS)])) {
      leds[NUM_LEDS - (min_led_ceil % NUM_LEDS)] = CHSV(minuteColor, 255, min_fint_ceil);
    } else {
      nblend(leds[NUM_LEDS - (min_led_ceil % NUM_LEDS)],  CHSV(minuteColor, 255, min_fint_ceil),  128);
    }

    if (isBlack(leds[NUM_LEDS - (sec_led_floor % NUM_LEDS)])) {
      leds[NUM_LEDS - (sec_led_floor % NUM_LEDS)] = CHSV(secondColor, 255, sec_fint_floor);
    } else {
      nblend(leds[NUM_LEDS - (sec_led_floor % NUM_LEDS)], CHSV(secondColor, 255, sec_fint_floor), 128);
    }
    if  (isBlack(leds[NUM_LEDS - (sec_led_ceil % NUM_LEDS)])) {
      leds[NUM_LEDS - (sec_led_ceil % NUM_LEDS)] =  CHSV(secondColor, 255, sec_fint_ceil);
    } else {
      nblend(leds[NUM_LEDS - (sec_led_ceil % NUM_LEDS)],  CHSV(secondColor, 255, sec_fint_ceil),  128);
    }
}

// Update LEDs to show time, may be fractions for smooth second hand
void updateTime()
{
    incrementTime();
    if (shouldUpdateNTP())
    {
      GetTime();
    }
}

bool shouldUpdateNTP()
{
    if ((millis() - ntp_timestamp) > (NTP_REFRESH_INTERVAL_SECONDS * 1000) || ntp_timestamp == 0) return true;
    return false;
}

bool incrementTime()
{
    secs++;
    if (secs >= 60)
    {
        secs = 0;
        mins++;
    }
    if (mins >= 60)
    {
        mins = 0;
        hours++;
    }
    if (hours >= 24) hours = 0;
}

void initUdp(int port)
{
    udpTime.begin(localPortTime);
}

unsigned long sendNTPpacket(IPAddress& address)
{
    //Serial.println("sending NTP packet...");
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udpTime.beginPacket(address, 123); //NTP requests are to port 123
    udpTime.write(packetBuffer, NTP_PACKET_SIZE);
    udpTime.endPacket();
}

bool GetTime()
{
    static bool requested = false;

    if (!requested) {
      WiFi.hostByName(ntpServerName, timeServerIP);
      sendNTPpacket(timeServerIP);
      Serial.println("request sent");
      requested = true;
    } else {
      requested = false;
      int cb = udpTime.parsePacket();
      if (!cb) {
          Serial.println("No packet received?");
      }
      else {
          Serial.print("packet received, length=");
          Serial.println(cb);
          udpTime.read(packetBuffer, NTP_PACKET_SIZE);
          ntp_timestamp = millis();

          unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
          unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
          unsigned long secsSince1900 = highWord << 16 | lowWord;
          const unsigned long seventyYears = 2208988800UL;
          unsigned long epoch = secsSince1900 - seventyYears;

          hours = (epoch % 86400L) / 3600;
          hours += t_offset;

          if (hours >= 24)hours -= 24;
          if (hours < 0)    hours += 24;

          mins = (epoch % 3600) / 60;
          secs = (epoch % 60);

          if (hours < 10)Serial.print("0");
          Serial.print(hours);
          Serial.print(':');
          if (mins < 10)Serial.print("0");
          Serial.print(mins);
          Serial.print(':');
          if (secs < 10)Serial.print("0");
          Serial.println(secs);
      }
    }
}
