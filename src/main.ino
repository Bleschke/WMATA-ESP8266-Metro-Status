/* 
 * Brian Leschke
 * April 26, 2017
 * Adafruit Huzzah WMATA ESP8266 Metro Status
 * An ESP8266 will control a neopixel ring (metro line), 7-segment LED (arrival time), and 20x4 LCD screen (station updates).
 * Version 0.*
 * 
 *
 * -- Credit and Recognition: --
 * Thanks to WMATA for providing the API!
 *
 * -- Changelog: -- 
 * 
 * 3/26/2017 - Formed idea, sketched project on paper. 
 * 4/9/2017  - Created files and coded Neopixel ring and button to change stations.
 * 4/10/2017 - Fixed errors and made modifications 
 * 4/11/2017 - Fixed errors and refined code
 * 4/13/2017 - Added metro colors
 * 4/23/2017 - Update LCD Library include
 * 4/26/2017 - Modified 7 segment code for BRD and ARR and fixed LCD library
 * 4/27/2017 - A.M. Added some JSON parsing code (not everything yet)
 *
 *
*/

// includes
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPI.h>

//#include <I2CIO.h>
//#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include "Adafruit_LEDBackpack.h"


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
int waitTime = 10;

uint32_t metroRed = pixels.Color(209 ,18, 66);        // convert color value to name "metroRed"
uint32_t metroBlue = pixels.Color(0, 150, 214);       // convert color value to name "metroBlue"
uint32_t metroOrange = pixels.Color(248, 151, 29);    // convert color value to name "metroOrange"
uint32_t metroYellow = pixels.Color(255, 221, 0);     // convert color value to name "metroYellow"
uint32_t metroGreen = pixels.Color(0, 183, 96);       // convert color value to name "metroGreen"
uint32_t metroSilver = pixels.Color(167, 169, 172);   // convert color value to name "metroSilver"

// 7-segment LED setup
Adafruit_7segment matrix = Adafruit_7segment();

// LCD setup
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


// ** Default WiFi connection Information **
const char* ap_default_ssid = "esp8266";   // Default SSID.
const char* ap_default_psk  = "esp8266";   // Default PSK.

// ** WMATA Information **
// Usage: https://api.wmata.com/StationPrediction.svc/json/GetPrediction/{StationCodes}
// API Calling Usage: https://api.wmata.com/StationPrediction.svc/json/GetPrediction/<STATION>?api_key=<API-KEY>

const char   WMATAServer[]   = "https://api.wmata.com/StationPrediction.svc/json/GetPrediction/"; // name address for WMATA (using DNS)
const String myKey           = "API_KEY";           // See: https://developer.wmata.com/ (change here with your Primary/Secondary API KEY)
const String stationLocA    = "STATION_LOC";      // Metro station code "Your station stop"
const String stationLocB    = "STATION_LOC";      // Metro station code "Your station stop"
const String stationLocC    = "STATION_LOC";      // Metro station code "Your station stop"
const String stationLocD    = "STATION_LOC";      // Metro station code "Your station stop"

const String stationDestA        = "STATION_DEST";      // Metro station name. usually end of line
const String stationDestB        = "STATION_DEST";      // Metro station name. usually end of line
const String stationDestC        = "STATION_DEST";      // Metro station name. usually end of line
const String stationDestD        = "STATION_DEST";      // Metro station name. usually end of line
  
  
long metroCheckInterval              = 60000;       // DO NOT Exceed 50000 API calls a day. Time (milliseconds) until next metro train check.
unsigned long previousMetroMillis    = 0;           // Do not change.

int changeButton             = 13;

// ** JSON Parsing Information **
// Data that we want to extract from WMATA
struct UserData {
  char Trains[32];
};


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

  // Check if there is a second line available.
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
  matrix.begin(0x70);
  matrix.print(0000);  // 7 Segment LED
  matrix.writeDisplay();
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WMATA Metro Status");
  lcd.setCursor(0,1);
  lcd.print("Version 1.0");
  lcd.setCursor(0,2);
  lcd.print("Brian Leschke"); 
  lcd.setCursor(0,3);
  lcd.print("April 26, 2017");
  delay(3000);
  
  String station_ssid = ""; // Do Not Change
  String station_psk = "";  // Do Not Change

  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(changeButton, INPUT);
  
  pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
  pixels.show(); // This sends the updated pixel color to the hardware.
  rainbowCycle(20);  // Loading screen
  
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
    lcd.print("SSID: " + WiFi.SSID());

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
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("WiFi: Connected");
    lcd.setCursor(0,2);
    lcd.print(WiFi.localIP());
  }
  else
  {
    Serial.println("Failed connecting to WiFi station. Go into AP mode.");
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("WiFi: Failed to connect to AP");
              
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
              
void DetermineStation()
{
  int counter;
  //Handle input
  digitalRead(changeButton);
  if(changeButton = HIGH)
  {
    counter = counter + 1;
    //Reset count if over max mode number
    if(counter == 1 )
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationA);
      delay(2000);      
      MetroCheckA();
    }
    else if (counter == 2)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationB);
      delay(2000);     
      MetroCheckB();
    }
    else if (counter == 3)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationC);
      delay(2000);     
      MetroCheckC();
    }
    else if (counter == 4)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationD);
      delay(2000);     
      MetroCheckD();
    }
    else
    {
      Serial.println("ERROR: Value is < 0 or > 4.");
      counter = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ERROR: Button H");
    }
  }
  else
  {
    if(counter == 1 )
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationLocA);
      delay(2000);  
      MetroCheckA();
    }
    else if (counter == 2)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationLocB);
      delay(2000);  
      MetroCheckB();
    }
    else if (counter == 3)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationLocC);
      delay(2000);  
      MetroCheckC();
    }
    else if (counter == 4)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(stationLocD);
      delay(2000);  
      MetroCheckD();
    }
    else
    {
      Serial.println("ERROR: Value is < 0 or > 4.");
      counter = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ERROR: Button L");
    }
  }
}

void MetroCheckA(const struct UserData* userData)
{
  colorWipe(pixels.Color(209 ,18, 66), 0); // set all neopixels to Red
  
  // *** MAIN CODE HERE :) *** 
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("LN  CAR  DEST  MIN");
  
  // If train is arriving 
  FadeInOut(209 ,18, 66, waitTime); // Fade in and out Red
  matrix.writeDigitRaw(1, B11101110);  // 7 Segment LED "A"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B00101000);  // 7 Segment LED "R"
  matrix.writeDisplay();
  
  // If train is boarding
  FadeInOut(209 ,18, 66, waitTime); // Fade in and out Red
  matrix.writeDigitRaw(1, B00111110);  // 7 Segment LED "B"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B01111010);  // 7 Segment LED "D"  
  matrix.writeDisplay();
  
}

void MetroCheckB(const struct UserData* userData)
{
  colorWipe(pixels.Color(0, 183, 96), 0); // Set all neopixels to Green
  
  // *** MAIN CODE HERE :) *** 
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("LN  CAR  DEST  MIN");
  
  // If train is arriving 
  FadeInOut(0, 183, 96, waitTime); // Fade in and out Green
  matrix.writeDigitRaw(1, B11101110);  // 7 Segment LED "A"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B00101000);  // 7 Segment LED "R"
  matrix.writeDisplay();
 
  // If train is boarding
  FadeInOut(0, 183, 96, waitTime); // Fade in and out Green
  matrix.writeDigitRaw(1, B00111110);  // 7 Segment LED "B"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B01111010);  // 7 Segment LED "D"  
  matrix.writeDisplay();
  
}
              
void MetroCheckC(const struct UserData* userData)
{
  colorWipe(pixels.Color(0, 150, 214), 0); // Set all neopixels to Blue
  
  // *** MAIN CODE HERE :) *** 
  
  lcd.clear(const struct UserData* userData);
  lcd.setCursor(0,0);
  lcd.print("LN  CAR  DEST  MIN");
  
  // If train is arriving 
  FadeInOut(0, 150, 214, waitTime); // Fade in and out Blue
  matrix.writeDigitRaw(1, B11101110);  // 7 Segment LED "A"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B00101000);  // 7 Segment LED "R"
  matrix.writeDisplay();
  
  // If train is boarding
  FadeInOut(0, 150, 214, waitTime); // Fade in and out Blue
  matrix.writeDigitRaw(1, B00111110);  // 7 Segment LED "B"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B01111010);  // 7 Segment LED "D"  
  matrix.writeDisplay();
  
}              
              
void MetroCheckD(const struct UserData* userData)
{
  colorWipe(pixels.Color(167, 169, 172), 0); // Set all neopixels to Silver
  
  // *** MAIN CODE HERE :) *** 
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("LN  CAR  DEST  MIN");
  
  // If train is arriving 
  FadeInOut(167, 169, 172, waitTime); // Fade in and out Silver
  matrix.writeDigitRaw(1, B11101110);  // 7 Segment LED "A"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B00101000);  // 7 Segment LED "R"
  matrix.writeDisplay();
  
  // If train is boarding
  FadeInOut(167, 169, 172, waitTime); // Fade in and out Silver
  matrix.writeDigitRaw(1, B00111110);  // 7 Segment LED "B"
  matrix.writeDigitRaw(3, B00101000);  // 7 Segment LED "R"
  matrix.writeDigitRaw(4, B01111010);  // 7 Segment LED "D"   
  matrix.writeDisplay(); 

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

void FadeInOut(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {

  for(uint8_t b=128; b <255; b++) {
     for(uint8_t i=0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, red*b/255, green*b/255, blue*b/255);
     }
     pixels.show();
     delay(wait);
  }

  for(uint8_t b=255; b > 128; b--) {
     for(uint8_t i=0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, red*b/255, green*b/255, blue*b/255);
     }
     pixels.show();
     delay(wait);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, c);
      pixels.show();
      delay(wait);
  }
}
              
// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
 
  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
    }
    pixels.show();
    delay(wait);
  }
}
 
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/*
Parse the JSON from the input string and extract the interesting values
Here is the JSON we need to parse
{
  "Trains": [
    {
      "Car": "6",
      "Destination": "Shady Gr",
      "DestinationCode": "A15",
      "DestinationName": "Shady Grove",
      "Group": "2",
      "Line": "RD",
      "LocationCode": "B03",
      "LocationName": "Union Station",
      "Min": "BRD"
    },
    {
      "Car": "8",
      "Destination": "NoMa",
      "DestinationCode": "B35",
      "DestinationName": "NoMa-Gallaudet",
      "Group": "1",
      "Line": "RD",
      "LocationCode": "B03",
      "LocationName": "Union Station",
      "Min": "6"
    },
    {
      "Car": "-",
      "Destination": "Train",
      "DestinationCode": null,
      "DestinationName": "Train",
      "Group": "1",
      "Line": "--",
      "LocationCode": "B03",
      "LocationName": "Union Station",
      "Min": "12"
    },
    {
      "Car": "6",
      "Destination": "Frgut N.",
      "DestinationCode": "A02",
      "DestinationName": "Farragut North",
      "Group": "2",
      "Line": "RD",
      "LocationCode": "B03",
      "LocationName": "Union Station",
      "Min": ""
    },
   ]
}
*/
bool readReponseContent(struct UserData* userData) {
  // Compute optimal size of the JSON buffer according to what we need to parse.
  // This is only required if you use StaticJsonBuffer.
  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(1)    // the root object has 1 element
      + JSON_OBJECT_SIZE(9)  // the "Trains" object has 9 elements
      + MAX_CONTENT_SIZE;    // additional space for strings

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

  JsonObject& root = jsonBuffer.parseObject(client);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    lcd.setCursor(0,3);
    lcd.print("JSON parsing failed");
    
    return false;
  }

  // Here we copy the strings we're interested in
  strcpy(userData->Trains, root["Trains"]["Car"]);              // number of cars per train
  strcpy(userData->Trains, root["Trains"]["Destination"]);      // usually the end of the line (abbreviated)
  strcpy(userData->Trains, root["Trains"]["DestinationName"]);  // usually the end of the line (non-abbreviated)
  strcpy(userData->Trains, root["Trains"]["Line"]);             // line color
  strcpy(userData->Trains, root["Trains"]["LocationName"]);     // usually your station
  strcpy(userData->Trains, root["Trains"]["Min"]);              // minutes until train arrives at 'LocationName'

  return true;
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
    DetermineStation();
    previousMetroMillis = currentMetroMillis; //remember the time(millis)
  }
  else {
    Serial.println("Bypassing Metro Check. Less than 1 minute since last check.");
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
