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
 * 3/26/17 - Formed idea, sketched project on paper. 
 * 4/9/17  - Created files and started code!
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

#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal_SR.h>
#include <LiquidCrystal_SR2W.h>
#include <LiquidCrystal_SR3W.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// -------------------- CONFIGURATION --------------------

// ** mDNS and OTA Constants **
#define HOSTNAME "ESP8266-OTA-"     // Hostename. The setup function adds the Chip ID at the end.
#define PIN            0            // Pin used for Neopixel communication
#define NUMPIXELS      24           // Number of Neopixels connected to Arduino

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

bool loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open("/cl_conf.txt", "r");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt.");

    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if ther is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
    {
      pos = content.indexOf("\r");
    }
  }

  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Invalid content.");
    Serial.println(content);

    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le);

  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  Serial.println("----- file content -----");
  Serial.println(content);
  Serial.println("----- file content -----");
  Serial.println("ssid: " + *ssid);
  Serial.println("psk:  " + *pass);
#endif

  return true;
} // loadConfig

bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open("/cl_conf.txt", "w");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt for writing");

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  configFile.close();
  
  return true;
} // saveConfig


void setup()
{
  print(----); // 7 Segment LED
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WMATA Status");
  lcd.setCursor(0,1);
  lcd.print("Version 1.0");
  lcd.setCursor(0,2);
  lcd.print("Brian Leschke");  
  delay(3000);
  
  pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
  pixels.show(); // This sends the updated pixel color to the hardware.
  rainbowCycle(20);  // Loading screen
  
  String station_ssid = ""; // Do Not Change
  String station_psk = "";  // Do Not Change

  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(txPin, OUTPUT) ; // Initialize Morse Code transmission output.
  
  delay(100);

  Serial.println("\r\n");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  Serial.println("Hostname: " + hostname);
  //Serial.println(WiFi.hostname());


  // Initialize file system.
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("FAIL: Filesystem");
    return;
  }

  // Load wifi connection information.
  if (! loadConfig(&station_ssid, &station_psk))
  {
    station_ssid = "";
    station_psk = "";

    Serial.println("No WiFi connection information available.");
  }

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    Serial.println("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Print new SSID
    Serial.print("new SSID: ");
    Serial.println(WiFi.SSID());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("SSID: " + Wifi.SSID());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  Serial.println("Wait for WiFi connection.");
  lcd.setCursor(0,1);
  lcd.print("WiFi: Connecting");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    Serial.write('.');
    //Serial.print(WiFi.status());
    delay(500);
  }
  Serial.println();
  //timeClient.begin(); // Start the time client

  // Check connection
  if(WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    lcd.clear(0,1);
    lcd.setCursor(0,1);
    lcd.print("WiFi: Connected");
    lcd.serCursor(0,2);
    lcd.print(WiFi.localIP())
  }
  else
  {
    Serial.println("Failed connecting to WiFi station. Go into AP mode.");
    lcd.clear(0,1);
    lcd.setCursor(0,1);
    lcd.print("WiFi: Failed)";
              
    // Go into software AP mode.
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP(ap_default_ssid, ap_default_psk);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("AP PROGRAM MODE");
    lcd.setCursor(0,1);
    lcd.print(WiFi.softAPIP());
    lcd.setCursor(0,2);
    lcd.print("Connect to");
    lcd.setCursor(0,3);
    lcd.print("Arduino software");     
  }

  // Start OTA server.
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
}

// ---------- OTA CONFIGURATION - DO NOT MODIFY ----------

// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void MetroCheck()
{
  
 // *** MAIN CODE HERE :) *** 
  
}

void GetTime()
{
  Serial.print("connecting to ");
  Serial.println(timeHost);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if (!client.connect(timeHost, timePort)) {
    Serial.println("NIST Timeservers: connection failed");
    return;
  }
  
  // This will send the request to the server
  client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");

  delay(100);

  // Read all the lines of the reply from server and print them to Serial
  // expected line is like : Date: Thu, 01 Jan 2015 22:00:14 GMT
  char buffer[12];

  while(client.available())
  {
    String line = client.readStringUntil('\r');

    if (line.indexOf("Date") != -1)
    {
      Serial.print("=====>");
    } else
    {
      // Serial.print(line);
      // date starts at pos 10. We don't need the year.
      TimeDate = line.substring(10);
      Serial.println("UTC Time and Date:");
      Serial.println(TimeDate);
      // time starts at pos 14
      //TimeDate = line.substring(10, 15);
      //TimeDate.toCharArray(buffer, 10);
      //Serial.println("UTC Date:");    // MM-DD
      //Serial.println(buffer);
      TimeDate = line.substring(16, 24);
      TimeDate.toCharArray(buffer, 10);
      Serial.println("UTC Time:");    // HH:MM:SS UTC
      Serial.println(buffer);
    }
  }
}

void FadeInOut(byte red, byte green, byte blue){
  float r, g, b;
      
  for(int k = 0; k < 256; k=k+1) { 
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
  }
     
  for(int k = 255; k >= 128; k=k-2) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    pixels.show();
  }
}
              
void rainbowCycle(int SpeedDelay) {
  byte *c;
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixels.numPixels; i++) {
      c=Wheel(((i * 256 / pixels.numPixels) + j) & 255);
      pixels.setPixelColor(i, *c, *(c+1), *(c+2));
    }
    pixels.show();
    delay(SpeedDelay);
  }
}

byte * Wheel(byte WheelPos) {
  static byte c[3];
  
  if(WheelPos < 85) {
   c[0]=WheelPos * 3;
   c[1]=255 - WheelPos * 3;
   c[2]=0;
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   c[0]=255 - WheelPos * 3;
   c[1]=0;
   c[2]=WheelPos * 3;
  } else {
   WheelPos -= 170;
   c[0]=0;
   c[1]=WheelPos * 3;
   c[2]=255 - WheelPos * 3;
  }

  return c;
}             

// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  yield();
  

  // ---------- USER CODE GOES HERE ----------

  // ** Receive Time (NTP)
  GetTime();

  // ** Metro Check **
  unsigned long currentMetroMillis = millis();
  
  if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
    Serial.println("Checking for Metro Train arrivals");
    MetroCheck();
    previousMetroMillis = currentMetroMillis; //remember the time(millis)
  }
  else {
    Serial.println("Bypassing Metro Check. Less than 2 minutes since last check.");
    Serial.println("Previous Millis: ");
    Serial.println(previousMetroMillis);
    Serial.println("Current Millis: ");
    Serial.println(currentMetroMillis);
    Serial.println("Subtracted Millis: ");
    Serial.println(currentMetroMillis-previousMetroMillis);
    Serial.println();
  }
  
  // ---------- USER CODE GOES HERE ----------
}
