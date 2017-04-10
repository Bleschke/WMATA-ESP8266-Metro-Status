/* 
 * Brian Leschke
 * April 9, 2017
 * Adafruit Huzzah WMATA ESP8266 Metro Status
 * An ESP8266 will control a neopixel ring (metro line), 7-segment LED (arrival time), and 16x4 LCD screen (station updates).
 * Version 0.*
 * 
 *
 * -- Credit and Recognition: --
 * Thanks to WMATA for providing the API!
 *
 * -- Changelog: -- 
 * 
 * 4/9/17 - Create .ino, data files, beginning to code project!!
 * // - 
 * // - 
 *
 * 
*/

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPI.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// -------------------- CONFIGURATION --------------------

// ** mDNS and OTA Constants **
#define HOSTNAME "ESP8266-OTA-"     // Hostename. The setup function adds the Chip ID at the end.
#define PIN            0            // Pin used for Neopixel communication
#define NUMPIXELS      24            // Number of Neopixels connected to Arduino

// Neopixel Setup
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


// ** Default WiFi connection Information **
const char* ap_default_ssid = "esp8266";   // Default SSID.
const char* ap_default_psk  = "esp8266";   // Default PSK.

// ** WMATA Information **
// Usage: https://api.wmata.com/StationPrediction.svc/json/GetPrediction/{StationCodes}

const char   WMATAServer[]   = "https://api.wmata.com/StationPrediction.svc/json/GetPrediction/"; // name address for WMATA (using DNS)
const String myKey           = "API_KEY";           // See: https://developer.wmata.com/ (change here with your Primary/Secondary API KEY)
const String myStationCodeA  = "STATION_CODE";      // Metro station code 1
const String myStationCodeB  = "STATION_CODE";      // Metro station code 2
const String myStationCodeC  = "STATION_CODE";      // Metro station code 3
const String myStationCodeD  = "STATION_CODE";      // Metro station code 4

long metroCheckInterval              = 120000;      // DO NOT SET LOWER THAN 2 min default. Time (milliseconds) until next metro train check.
unsigned long previousMetroMillis    = 0;           // Do not change.

// ** NTP SERVER INFORMATION **
// const char* timeHost = "time-c.nist.gov";
const char* timeHost    = "129.6.15.30";
const int timePort      = 13;

int ln = 0;
String TimeDate = "";

// Uncomment the next line for verbose output over UART.
#define SERIAL_VERBOSE


// ---------- OTA CONFIGURATION - DO NOT MODIFY ----------
