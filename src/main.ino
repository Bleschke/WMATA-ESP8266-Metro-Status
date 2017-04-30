
/* 
 * Brian Leschke
 * April 27, 2017
 * Adafruit Huzzah WMATA ESP8266 Metro Status
 * An ESP8266 will control a neopixel ring (metro line), 7-segment LED (arrival time), and 16x4 LCD screen (station updates).
 * Version 1.0
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
 * 4/26/2017 - Added some code
 * 4/27/2017 - Added JSON parsing, modified code and libraries, INITIAL RELEASE!
 * 4/28/2017 - Changed neopixel brightness, parsing port (parsing), and added neopixel sparkle code (arrival)
 * 4/30/2017 - fixed color notifications and modified button.
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
//#include <LiquidCrystal.h>
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
//LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display


// ** Default WiFi connection Information **
const char* ap_default_ssid = "esp8266";   // Default SSID.
const char* ap_default_psk  = "esp8266";   // Default PSK.

// ** WMATA Information **
// API Calling Usage: https://api.wmata.com/StationPrediction.svc/json/GetPrediction/<STATIONCODE>?api_key=<API-KEY>

#define      WMATAServer       "api.wmata.com" // name address for WMATA (using DNS)
const String myKey           = "API_KEY";           // See: https://developer.wmata.com/ (change here with your Primary/Secondary API KEY)
const String stationCodeA    = "STATION_CODE";      // Metro station code 1
const String stationCodeB    = "STATION_CODE";      // Metro station code 2
const String stationCodeC    = "STATION_CODE";      // Metro station code 3
const String stationCodeD    = "STATION_CODE";      // Metro station code 4

const String stationA        = "STATION_NAME-CUA";      // Metro station name. ex. Shady Grove
const String stationB        = "STATION_NAME";      // Metro station name. ex. Shady Grove
const String stationC        = "STATION_NAME";      // Metro station name. ex. Shady Grove
const String stationD        = "STATION_NAME";      // Metro station name. ex. Shady Grove 
  
long metroCheckInterval              = 60000;       // DO NOT Exceed 50000 API calls a day. Time (milliseconds) until next metro train check.
unsigned long previousMetroMillis    = 0;           // Do not change.
int changeButton                     = 14;          // Do not change.
int counter                          = 0;           // Do not change.


// ** JSON Parser Information
const int buffer_size = 300;                        // length of json buffer. Do not change. 
const int buffer=300;                               // Do not change.

int passNum = 1;                                    // Do not change.

char* metroConds[]={
   "\"Car\":",
   "\"Destination\":",
   "\"DestinationCode\":",
   "\"DestinationName\":",
   "\"Group\":",
   "\"Line\":",
   "\"LocationCode\":",
   "\"LocationName\":",
   "\"Min\":",
};

int num_elements        = 9;  // number of conditions you are retrieving, count of elements in conds

unsigned long WMillis   = 0;                           // temporary millis() register


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
  delay(2000);
  matrix.begin(0x70);
  matrix.print(1234);  // 7 Segment LED
  matrix.writeDisplay();
  delay(1000); 
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WMATA Status");
  lcd.setCursor(0,1);
  lcd.print("Version 1.0");
  lcd.setCursor(0,2);
  lcd.print("Brian Leschke");  
  delay(3000);
  String station_ssid = ""; // Do Not Change
  String station_psk = "";  // Do Not Change

  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(changeButton, INPUT_PULLUP);
  
  
  pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
  pixels.show(); // This sends the updated pixel color to the hardware.
  rainbowCycle(10);  // Loading screen

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
  //Handle input
  Serial.println("button state:");
  Serial.println(digitalRead(changeButton));
  Serial.println("CHECKING FOR BUTTON PRESS");
  if(digitalRead(changeButton) == LOW)
  {
    counter = counter +1;
    delay(100);
    Serial.println("Button Pressed");
    //Reset count if over max mode number
    if (counter <=0)
    {
      counter = counter +1;
      Serial.println("counter set to 1");
      DetermineStation();
    }
    else if (counter < 2 && counter > 0)
    {
      // ** Metro Station A Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station A");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationA);
        delay(2000);
        MetroCheckA();
      }
      else {
        Serial.println("Station A: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 3 && counter > 1)
    {
      // ** Metro Station B Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station B");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationB);
        delay(2000);
        MetroCheckB();
      }
      else {
        Serial.println("Station B: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 4 && counter > 2)
    {
      // ** Metro Station C Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station C");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationC);
        delay(2000);
        MetroCheckC();
      }
      else {
        Serial.println("Station C: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 5 && counter > 3)
    {
      // ** Metro Station D Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station D");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationD);
        delay(2000);
        MetroCheckD();
      }
      else {
        Serial.println("Station D: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else
    {
      Serial.println("ERROR: Low Value is <= 0 or > 4.");
      counter = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ERROR: Button L");
    }
  }
  else
  {
    if (counter <= 0 )
    {
      counter = counter +1;
      Serial.println("counter set to 1");
      DetermineStation();
    }
    else if (counter < 2 && counter > 0)
    {
      // ** Metro Station A Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station A");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationA);
        delay(2000);
        MetroCheckA();
      }
      else {
        Serial.println("Station A: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 3 && counter > 1)
    {
      // ** Metro Station B Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station B");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationB);
        delay(2000);
        MetroCheckB();
      }
      else {
        Serial.println("Station B: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 4 && counter > 2)
    {
      // ** Metro Station C Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station C");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationC);
        delay(2000);
        MetroCheckC();
      }
      else {
        Serial.println("Station C: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else if (counter < 5 && counter > 3)
    {
      // ** Metro Station D Check **
      unsigned long currentMetroMillis = millis();
      if(currentMetroMillis - previousMetroMillis >= metroCheckInterval) {
        previousMetroMillis = currentMetroMillis; //remember the time(millis)
        Serial.println("Checking Station D");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(stationD);
        delay(2000);
        MetroCheckD();
      }
      else {
        Serial.println("Station D: Bypassing Metro Check. Less than 1 minute since last check.");        
        Serial.println("Previous Millis: ");
        Serial.println(previousMetroMillis);
        Serial.println("Current Millis: ");
        Serial.println(currentMetroMillis);
        Serial.println("Subtracted Millis: ");
        Serial.println(currentMetroMillis-previousMetroMillis);
        Serial.println();
      }
    }
    else
    {
      Serial.println("ERROR: High Value is <= 0 or > 4.");
      counter = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ERROR: Button H");
    }
  }
}

void MetroCheckA()
{
  Serial.print("Metro A: connecting to ");
  Serial.println(WMATAServer);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  const int httpPort = 80;
  
  if (!client.connect(WMATAServer, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String cmd = "GET /StationPrediction.svc/json/GetPrediction/";  cmd += stationCodeA;      // build request_string cmd
  cmd += "?api_key=";  cmd += myKey;  //
  cmd += " HTTP/1.1\r\nHost: api.wmata.com\r\n\r\n";            
  delay(500);
  client.print(cmd);                                            
  delay(500);
  unsigned int i = 0;                                           // timeout counter
  char json[buffer_size]="{";                                   // first character for json-string is begin-bracket 
  int n = 1;                                                    // character counter for json  
  
  for (int j=0;j<num_elements;j++){                             // do the loop for every element/condition
    boolean quote = false; int nn = false;                      // if quote=fals means no quotes so comma means break
    while (!client.find(metroConds[j])){}                            // find the part we are interested in.
  
    String Str1= metroConds[j];                                     // Str1 gets the name of element/condition
  
    for (int l=0; l<(Str1.length());l++)                        // for as many character one by one
        {json[n] = Str1[l];                                     // fill the json string with the name
         n++;}                                                  // character count +1
    while (i<5000) {                                            // timer/counter
      if(client.available()) {                                  // if character found in receive-buffer
        char c = client.read();                                 // read that character
           Serial.print(c);                                   // 
// ************************ construction of json string converting comma's inside quotes to dots ********************        
               if ((c=='"') && (quote==false))                  // there is a " and quote=false, so start of new element
                  {quote = true;nn=n;}                          // make quote=true and notice place in string
               if ((c==',')&&(quote==true)) {c='.';}            // if there is a comma inside quotes, comma becomes a dot.
               if ((c=='"') && (quote=true)&&(nn!=n))           // if there is a " and quote=true and on different position
                  {quote = false;}                              // quote=false meaning end of element between ""
               if((c==',')&&(quote==false)) break;              // if comma delimiter outside "" then end of this element
 // ****************************** end of construction ******************************************************
          json[n]=c;                                            // fill json string with this character
          n++;                                                  // character count + 1
          i=0;                                                  // timer/counter + 1
        }
        i++;                                                    // add 1 to timer/counter
      }                    // end while i<5000
     if (j==num_elements-1)                                     // if last element
        {json[n]='}';}                                          // add end bracket of json string
     else                                                       // else
        {json[n]=',';}                                          // add comma as element delimiter
     n++;                                                       // next place in json string
  }
  Serial.println(json);                                         // debugging json string 
  parseJSON(json);                                              // extract the conditions
  WMillis=millis();
}

void MetroCheckB()
{
  Serial.print("Metro B: connecting to ");
  Serial.println(WMATAServer);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  const int httpPort = 80;
  
  if (!client.connect(WMATAServer, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String cmd = "GET /StationPrediction.svc/json/GetPrediction/";  cmd += stationCodeB;      // build request_string cmd
  cmd += "?api_key=";  cmd += myKey;  //
  cmd += " HTTP/1.1\r\nHost: api.wmata.com\r\n\r\n";            
  delay(500);
  client.print(cmd);                                            
  delay(500);
  unsigned int i = 0;                                           // timeout counter
  char json[buffer_size]="{";                                   // first character for json-string is begin-bracket 
  int n = 1;                                                    // character counter for json  
  
  for (int j=0;j<num_elements;j++){                             // do the loop for every element/condition
    boolean quote = false; int nn = false;                      // if quote=fals means no quotes so comma means break
    while (!client.find(metroConds[j])){}                            // find the part we are interested in.
  
    String Str1 = metroConds[j];                                     // Str1 gets the name of element/condition
  
    for (int l=0; l<(Str1.length());l++)                        // for as many character one by one
        {json[n] = Str1[l];                                     // fill the json string with the name
         n++;}                                                  // character count +1
    while (i<5000) {                                            // timer/counter
      if(client.available()) {                                  // if character found in receive-buffer
        char c = client.read();                                 // read that character
           Serial.print(c);                                     // 
// ************************ construction of json string converting comma's inside quotes to dots ********************        
               if ((c=='"') && (quote==false))                  // there is a " and quote=false, so start of new element
                  {quote = true;nn=n;}                          // make quote=true and notice place in string
               if ((c==',')&&(quote==true)) {c='.';}            // if there is a comma inside quotes, comma becomes a dot.
               if ((c=='"') && (quote=true)&&(nn!=n))           // if there is a " and quote=true and on different position
                  {quote = false;}                              // quote=false meaning end of element between ""
               if((c==',')&&(quote==false)) break;              // if comma delimiter outside "" then end of this element
 // ****************************** end of construction ******************************************************
          json[n]=c;                                            // fill json string with this character
          n++;                                                  // character count + 1
          i=0;                                                  // timer/counter + 1
        }
        i++;                                                    // add 1 to timer/counter
      }                    // end while i<5000
     if (j==num_elements-1)                                     // if last element
        {json[n]='}';}                                          // add end bracket of json string
     else                                                       // else
        {json[n]=',';}                                          // add comma as element delimiter
     n++;                                                       // next place in json string
  }
  Serial.println(json);                                         // debugging json string 
  parseJSON(json);                                              // extract the conditions
}
              
void MetroCheckC()
{
  Serial.print("Metro C: connecting to ");
  Serial.println(WMATAServer);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  const int httpPort = 80;
  
  if (!client.connect(WMATAServer, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String cmd = "GET /StationPrediction.svc/json/GetPrediction/";  cmd += stationCodeC;      // build request_string cmd
  cmd += "?api_key=";  cmd += myKey;  //
  cmd += " HTTP/1.1\r\nHost: api.wmata.com\r\n\r\n";            
  delay(500);
  client.print(cmd);                                            
  delay(500);
  unsigned int i = 0;                                           // timeout counter
  char json[buffer_size]="{";                                   // first character for json-string is begin-bracket 
  int n = 1;                                                    // character counter for json  
  
  for (int j=0;j<num_elements;j++){                             // do the loop for every element/condition
    boolean quote = false; int nn = false;                      // if quote=fals means no quotes so comma means break
    while (!client.find(metroConds[j])){}                            // find the part we are interested in.
  
    String Str1 = metroConds[j];                                     // Str1 gets the name of element/condition
  
    for (int l=0; l<(Str1.length());l++)                        // for as many character one by one
        {json[n] = Str1[l];                                     // fill the json string with the name
         n++;}                                                  // character count +1
    while (i<5000) {                                            // timer/counter
      if(client.available()) {                                  // if character found in receive-buffer
        char c = client.read();                                 // read that character
           Serial.print(c);                                     // 
// ************************ construction of json string converting comma's inside quotes to dots ********************        
               if ((c=='"') && (quote==false))                  // there is a " and quote=false, so start of new element
                  {quote = true;nn=n;}                          // make quote=true and notice place in string
               if ((c==',')&&(quote==true)) {c='.';}            // if there is a comma inside quotes, comma becomes a dot.
               if ((c=='"') && (quote=true)&&(nn!=n))           // if there is a " and quote=true and on different position
                  {quote = false;}                              // quote=false meaning end of element between ""
               if((c==',')&&(quote==false)) break;              // if comma delimiter outside "" then end of this element
 // ****************************** end of construction ******************************************************
          json[n]=c;                                            // fill json string with this character
          n++;                                                  // character count + 1
          i=0;                                                  // timer/counter + 1
        }
        i++;                                                    // add 1 to timer/counter
      }                    // end while i<5000
     if (j==num_elements-1)                                     // if last element
        {json[n]='}';}                                          // add end bracket of json string
     else                                                       // else
        {json[n]=',';}                                          // add comma as element delimiter
     n++;                                                       // next place in json string
  }
  Serial.println(json);                                         // debugging json string 
  parseJSON(json);                                              // extract the conditions
}              
              
void MetroCheckD()
{
  Serial.print("Metro D: connecting to ");
  Serial.println(WMATAServer);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  const int httpPort = 80;
  
  if (!client.connect(WMATAServer, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String cmd = "GET /StationPrediction.svc/json/GetPrediction/";  cmd += stationCodeD;      // build request_string cmd
  cmd += "?api_key=";  cmd += myKey;  //
  cmd += " HTTP/1.1\r\nHost: api.wmata.com\r\n\r\n";            
  delay(500);
  client.print(cmd);                                            
  delay(500);
  unsigned int i = 0;                                           // timeout counter
  char json[buffer_size]="{";                                   // first character for json-string is begin-bracket 
  int n = 1;                                                    // character counter for json  
  
  for (int j=0;j<num_elements;j++){                             // do the loop for every element/condition
    boolean quote = false; int nn = false;                      // if quote=fals means no quotes so comma means break
    while (!client.find(metroConds[j])){}                            // find the part we are interested in.
  
    String Str1 = metroConds[j];                                     // Str1 gets the name of element/condition
  
    for (int l=0; l<(Str1.length());l++)                        // for as many character one by one
        {json[n] = Str1[l];                                     // fill the json string with the name
         n++;}                                                  // character count +1
    while (i<5000) {                                            // timer/counter
      if(client.available()) {                                  // if character found in receive-buffer
        char c = client.read();                                 // read that character
           Serial.print(c);                                     // 
// ************************ construction of json string converting comma's inside quotes to dots ********************        
               if ((c=='"') && (quote==false))                  // there is a " and quote=false, so start of new element
                  {quote = true;nn=n;}                          // make quote=true and notice place in string
               if ((c==',')&&(quote==true)) {c='.';}            // if there is a comma inside quotes, comma becomes a dot.
               if ((c=='"') && (quote=true)&&(nn!=n))           // if there is a " and quote=true and on different position
                  {quote = false;}                              // quote=false meaning end of element between ""
               if((c==',')&&(quote==false)) break;              // if comma delimiter outside "" then end of this element
 // ****************************** end of construction ******************************************************
          json[n]=c;                                            // fill json string with this character
          n++;                                                  // character count + 1
          i=0;                                                  // timer/counter + 1
        }
        i++;                                                    // add 1 to timer/counter
      }                    // end while i<5000
     if (j==num_elements-1)                                     // if last element
        {json[n]='}';}                                          // add end bracket of json string
     else                                                       // else
        {json[n]=',';}                                          // add comma as element delimiter
     n++;                                                       // next place in json string
  }
  Serial.println(json);                                         // debugging json string 
  parseJSON(json);                                              // extract the conditions
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

  for(uint8_t b=60; b <128; b++) {
     for(uint8_t i=0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, red*b/255, green*b/255, blue*b/255);
     }
     pixels.show();
     delay(wait);
  }

  for(uint8_t b=128; b > 60; b--) {
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
      pixels.setBrightness(128);
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
      pixels.setBrightness(128);
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

void Sparkle(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {
  int Pixel = random(NUMPIXELS);
  pixels.setBrightness(128);
  pixels.setPixelColor(Pixel, pixels.Color(red, green, blue)); 
  pixels.show();
  delay(wait);
  pixels.setPixelColor(Pixel,0,0,0);
}

void parseJSON(char json[300])
{
  StaticJsonBuffer<buffer> jsonBuffer;
 JsonObject& root = jsonBuffer.parseObject(json);
 
 if (!root.success())
{
  lcd.setCursor(0,3);
  lcd.print("?fparseObject() failed");
  //return;
}

 
 int Car                      = root["Car"];
 const char* Destination      = root["Destination"];
 const char* DestinationCode  = root["DestinationCode"];
 const char* DestinationName  = root["DestinationName"];
 int Group                    = root["Group"];
 String Line                  = root["Line"];
 const char* LocationCode     = root["LocationCode"];
 const char* LocationName     = root["LocationName"];
 const char* CMin             = root["Min"];
 float Min                    = root["Min"];

 Serial.println(Car);
 Serial.println(Destination);
 Serial.println(DestinationCode);
 Serial.println(DestinationName);
 Serial.println(Group);
 Serial.println(Line);
 Serial.println(LocationCode);
 Serial.println(CMin);
 Serial.println(Min);

 if (Line == "RD")
 {
  Serial.println("RED LINE");
  colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels to Red
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  { 
    matrix.print(Min);
    matrix.writeDisplay();
    
    for(int i=0; i< 25; i++) {
      FadeInOut(255 ,0, 0, waitTime);    // Fade in and out Red
      delay(500);
    }
    colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels back to Red
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();
    
    for(int i=0; i< 1500; i++) {
      Sparkle(255 ,0, 0, waitTime);      // Sparkle Red
      delay(20);
    }
    colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels back to Red
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 } 
 else if (Line == "OR")
 {
  Serial.println("ORANGE LINE");
  colorWipe(pixels.Color(255, 165, 0), 0); // set all neopixels to Orange
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 25; i++) {
      FadeInOut(255 ,165, 0, waitTime);    // Fade in and out Orange
      delay(500);
    }
    colorWipe(pixels.Color(255 ,165, 0), 0); // set all neopixels back to Orange
  }
  
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 1500; i++) {
      Sparkle(255 ,165, 0, waitTime);      // Sparkle Orange
      delay(20);
    }
    colorWipe(pixels.Color(255 ,165, 0), 0); // set all neopixels back to Orange
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 }
 else if (Line == "YL")
 {
  Serial.println("YELLOW LINE");
  colorWipe(pixels.Color(255, 255, 0), 0); // set all neopixels to Yellow
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving 
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 25; i++) {
      FadeInOut(255 ,255, 0, waitTime);    // Fade in and out Yellow
      delay(500);
    }
    colorWipe(pixels.Color(255 ,255, 0), 0); // set all neopixels back to Yellow
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 1500; i++) {
      Sparkle(255 ,255, 0, waitTime);      // Sparkle Yellow
      delay(20);
    }
    colorWipe(pixels.Color(255 ,255, 0), 0); // set all neopixels back to Yellow
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 }
 else if (Line == "GR")
 {
  Serial.println("GREEN LINE");
  colorWipe(pixels.Color(0, 128, 0), 0); // set all neopixels to Green
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 25; i++) {
      FadeInOut(0 ,128, 0, waitTime);    // Fade in and out Green
      delay(500);
    }
    colorWipe(pixels.Color(0 ,128, 0), 0); // set all neopixels back to Green
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 1500; i++) {
      Sparkle(0 ,128, 0, waitTime);      // Sparkle Green
      delay(20);
    }
    colorWipe(pixels.Color(0,128, 0), 0); // set all neopixels back to Green
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 }
 else if (Line == "BL")
 {
  Serial.println("BLUE LINE");
  colorWipe(pixels.Color(0, 0, 255), 0); // set all neopixels to Blue
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 25; i++) {
      FadeInOut(0 ,0, 255, waitTime);    // Fade in and out Blue
      delay(500);
    }
    colorWipe(pixels.Color(0 ,0, 255), 0); // set all neopixels back to Blue
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 1500; i++) {
      Sparkle(0 ,0, 255, waitTime);      // Sparkle Blue
      delay(20);
    }
    colorWipe(pixels.Color(0,0, 255), 0); // set all neopixels back to Blue
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 }
 else if (Line == "SV")
 {
  Serial.println("SILVER LINE");
  colorWipe(pixels.Color(192, 192, 192), 0); // set all neopixels to Silver
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);
    
  if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 25; i++) {
      FadeInOut(192 ,192, 192, waitTime);    // Fade in and out Silver
      delay(500);
    }
    colorWipe(pixels.Color(192 ,192, 192), 0); // set all neopixels back to Silver
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< 1500; i++) {
      Sparkle(192 ,192, 192, waitTime);      // Sparkle Silver
      delay(20);
    }
    colorWipe(pixels.Color(192,192, 192), 0); // set all neopixels back to Silver
  }
  else
  {
    matrix.print(Min);
    matrix.writeDisplay();
  }
 }
 else if (Line == "--")
 {
  Serial.println("LINE/TRAIN OUT OF SERVICE");
  colorWipe(pixels.Color(212, 0, 212), 0); // set all neopixels to Purple
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Out of Service");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(Min);

  matrix.clear();
  matrix.writeDigitRaw(0, B01011100);  // 7 Segment LED "O"
  matrix.writeDigitRaw(1, B01011100);  // 7 Segment LED "O"
  matrix.writeDigitRaw(3, B01101101);  // 7 Segment LED "S"
  matrix.writeDisplay();
 }
 else
 {
  Serial.println("LINE UNKNOWN");
  colorWipe(pixels.Color(128, 0, 128), 0); // set all neopixels to Purple
  matrix.print(Min);
  matrix.writeDisplay();
  
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Line Unknown");
	lcd.setCursor(0,1);
	lcd.print("LN  CAR  DEST  MIN");
	lcd.setCursor(0,2);
	lcd.print(Line);
	lcd.print("  ");
	lcd.print(Car);
	lcd.print("  ");
	lcd.print(Destination);
	lcd.print("  ");
	lcd.print(Min);
 }
}

// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  yield();
  

  // ---------- USER CODE GOES HERE ----------

  // ** Receive Time (NTP)
  //GetTime();

  // ** Metro Check **
  DetermineStation();

  
  // ---------- USER CODE GOES HERE ----------
}
