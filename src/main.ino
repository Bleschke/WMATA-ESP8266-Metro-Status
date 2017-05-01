
/* 
 * Brian Leschke
 * May 1, 2017
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
 * 5/1/2017  - added "No passenger", "brd", "arr" and simplified the wifi and OTA code.
 * 
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

#include <LCD.h>
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

// ** Neopixel Setup **
#define PIN            0            // Pin used for Neopixel communication
#define NUMPIXELS      24           // Number of Neopixels connected to Arduino
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
int waitTime     = 10;    // Do not change.
int sparkleCount = 700;   // Number of pixel sparkles.
int fadeCount    = 11;    // Number of fade in/out

// ** 7-segment LED setup **
Adafruit_7segment matrix = Adafruit_7segment();

// ** LCD setup **
#define BACKLIGHT_PIN (3)
#define LED_ADDR (0x27)      // might need to be 0x3F, if 0x27 doesn't work
LiquidCrystal_I2C lcd(LED_ADDR, 2, 1, 0, 4, 5, 6, 7, BACKLIGHT_PIN, POSITIVE);  // Set the LCD I2C address

// ** WiFi connection and OTA Information **
const char*     ssid = "WIFI_SSID";   // Default SSID.
const char* password = "WIFI_PASSWORD";   // Default PSK.

// ** WMATA Information **
// API Calling Usage: https://api.wmata.com/StationPrediction.svc/json/GetPrediction/<STATIONCODE>?api_key=<API-KEY>

#define      WMATAServer       "api.wmata.com"      // name address for WMATA (using DNS)
const String myKey           = "API_KEY";           // See: https://developer.wmata.com/ (change here with your Primary/Secondary API KEY)
const String stationCodeA    = "STATION_CODE";      // Metro station code 1
const String stationCodeB    = "STATION_CODE";      // Metro station code 2
const String stationCodeC    = "STATION_CODE";      // Metro station code 3
const String stationCodeD    = "STATION_CODE";      // Metro station code 4

const String stationA        = "STATION_NAME";      // Metro station name. ex. Shady Grove
const String stationB        = "STATION_NAME";      // Metro station name. ex. Shady Grove
const String stationC        = "STATION_NAME";      // Metro station name. ex. Shady Grove
const String stationD        = "STATION_NAME";      // Metro station name. ex. Shady Grove 
  
long metroCheckInterval              = 30000;       // DO NOT Exceed 50000 API calls a day. Time (milliseconds) until next metro train check.
unsigned long previousMetroMillis    = 0;           // Do not change.
int changeButton                     = 2;           // Do not change. Pin 2 is InputPullUp on Huzzah
int counter                          = 0;           // Do not change.

// ** JSON Parser Information
const int buffer_size = 300;                        // Do not change. Length of json buffer
const int buffer=300;                               // Do not change.
int passNum = 1;                                    // Do not change.

char* metroConds[]={                                // Do not change.
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

unsigned long WMillis   = 0;  // temporary millis() register


// ** NTP SERVER INFORMATION **
// const char* timeHost = "time-c.nist.gov";
const char* timeHost    = "129.6.15.30";
const int timePort      = 13;

int ln = 0;
String TimeDate = "";

// Uncomment the next line for verbose output over UART.
#define SERIAL_VERBOSE


// ---------- OTA CONFIGURATION - DO NOT MODIFY ----------
void setup()
{
  delay(2000);
  matrix.begin(0x70);
  matrix.writeDigitRaw(1, B01011110);  // 7 Segment LED "d"
  matrix.writeDigitRaw(3, B01011000);  // 7 Segment LED "c" 
  matrix.writeDisplay();
  delay(1000); 
  lcd.begin(20,4);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WMATA Metro Status");
  lcd.setCursor(0,1);
  lcd.print("Version 1.0");
  lcd.setCursor(0,2);
  lcd.print("Brian Leschke");  
  delay(3000);

  Serial.begin(115200);
  Serial.println("Booting");
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(changeButton, INPUT_PULLUP);
  
  colorWipe(pixels.Color(0 ,0, 0), 0); // set all neopixels to OFF
  pixels.show(); // This sends the updated pixel color to the hardware.
  pixels.setBrightness(128);
  rainbowCycle(10);  // Loading screen

  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Wifi Fail: Rebooting");
    
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)1234);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("OTA Update Initiated");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("End");
    lcd.setCursor(0,1);
    lcd.print("                    ");
    lcd.setCursor(0,1);
    lcd.print("OTA Update Complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    lcd.setCursor(0,2);
    int totalProgress  = (progress / (total/100));
    lcd.print("Progress: ");
    lcd.print(totalProgress);
    lcd.print("%");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    lcd.clear();
    lcd.setCursor(0,3);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
      lcd.print("OTA Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
      lcd.print("OTA Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
      lcd.print("OTA Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
      lcd.print("OTA Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
      lcd.print("OTA End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  lcd.setCursor(0,1);
  lcd.print("WiFi: Connected");
  lcd.setCursor(0,2);
  lcd.print(WiFi.localIP());
  delay(2000);
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
        MetroCheckA();
      }
      else {
        Serial.println("Station A: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckB();
      }
      else {
        Serial.println("Station B: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckC();
      }
      else {
        Serial.println("Station C: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckD();
      }
      else {
        Serial.println("Station D: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckA();
      }
      else {
        Serial.println("Station A: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckB();
      }
      else {
        Serial.println("Station B: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckC();
      }
      else {
        Serial.println("Station C: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
        MetroCheckD();
      }
      else {
        Serial.println("Station D: Bypassing Metro Check. Less than 30 seconds since last check.");        
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
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(stationA);
  lcd.setCursor(0,1);
  lcd.print("Checking for updates");
  delay(2000);
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
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(stationB);
  lcd.setCursor(0,1);
  lcd.print("Checking for updates");
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
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(stationC);
  lcd.setCursor(0,1);
  lcd.print("Checking for updates");
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
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(stationD);
  lcd.setCursor(0,1);
  lcd.print("Checking for updates");
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
 //const char* CMin           = root["Min"];
 String CMin                  = root["Min"];
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);
  
  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,0, 0, waitTime);      // Sparkle Red
      delay(20);
    }
    colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels back to Red
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,0, 0, waitTime);      // Sparkle Red
      delay(20);
    }
    colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels back to Red
  }
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  { 
    matrix.print(Min);
    matrix.writeDisplay();
    
    for(int i=0; i< fadeCount; i++) {
      FadeInOut(255 ,0, 0, waitTime);    // Fade in and out Red
      delay(500);
    }
    colorWipe(pixels.Color(255 ,0, 0), 0); // set all neopixels back to Red
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);

  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,165, 0, waitTime);      // Sparkle Orange
      delay(20);
    }
    colorWipe(pixels.Color(255 ,165, 0), 0); // set all neopixels back to Orange
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,165, 0, waitTime);      // Sparkle Orange
      delay(20);
    }
    colorWipe(pixels.Color(255 ,165, 0), 0); // set all neopixels back to Orange
  }    
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< fadeCount; i++) {
      FadeInOut(255 ,165, 0, waitTime);    // Fade in and out Orange
      delay(500);
    }
    colorWipe(pixels.Color(255 ,165, 0), 0); // set all neopixels back to Orange
  }
  
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< sparkleCount; i++) {
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);
    
  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,225, 0, waitTime);      // Sparkle Yellow
      delay(20);
    }
    colorWipe(pixels.Color(255 ,225, 0), 0); // set all neopixels back to Yelloe
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,225, 0, waitTime);      // Sparkle Yellow
      delay(20);
    }
    colorWipe(pixels.Color(255 ,225, 0), 0); // set all neopixels back to Yellow
  }    
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving 
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< fadeCount; i++) {
      FadeInOut(255 ,255, 0, waitTime);    // Fade in and out Yellow
      delay(500);
    }
    colorWipe(pixels.Color(255 ,255, 0), 0); // set all neopixels back to Yellow
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< sparkleCount; i++) {
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);
    
  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(0 ,128, 0, waitTime);      // Sparkle Green
      delay(20);
    }
    colorWipe(pixels.Color(0 ,128, 0), 0); // set all neopixels back to Green
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(0 ,128, 0, waitTime);      // Sparkle Green
      delay(20);
    }
    colorWipe(pixels.Color(0 ,128, 0), 0); // set all neopixels back to Green
  }    
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< fadeCount; i++) {
      FadeInOut(0 ,128, 0, waitTime);    // Fade in and out Green
      delay(500);
    }
    colorWipe(pixels.Color(0 ,128, 0), 0); // set all neopixels back to Green
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< sparkleCount; i++) {
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);
    
  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(0 ,0, 255, waitTime);      // Sparkle Blue
      delay(20);
    }
    colorWipe(pixels.Color(0 ,0, 255), 0); // set all neopixels back to Blue
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(0 ,0, 255, waitTime);      // Sparkle Blue
      delay(20);
    }
    colorWipe(pixels.Color(0 ,0, 255), 0); // set all neopixels back to Blue
  } 
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< fadeCount; i++) {
      FadeInOut(0 ,0, 255, waitTime);    // Fade in and out Blue
      delay(500);
    }
    colorWipe(pixels.Color(0 ,0, 255), 0); // set all neopixels back to Blue
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< sparkleCount; i++) {
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
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);

  if (CMin == "ARR")   // If train is arriving
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01110111); // 7 Segment LED "a"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01010000); // 7 Segment LED "r"
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(255 ,0, 0, waitTime);      // Sparkle Red
      delay(20);
    }
    colorWipe(pixels.Color(192, 192, 192), 0); // set all neopixels back to Silver
  }
  else if (CMin == "BRD")   // If train is boarding
  {
    matrix.clear();
    matrix.writeDigitRaw(0, B01111100); // 7 Segment LED "b"
    matrix.writeDigitRaw(1, B01010000); // 7 Segment LED "r"
    matrix.writeDigitRaw(3, B01011110); // 7 Segment LED "d" 
    matrix.writeDisplay();
    
    for(int i=0; i< sparkleCount; i++) {
      Sparkle(192, 192, 192, waitTime);      // Sparkle Silver
      delay(20);
    }
    colorWipe(pixels.Color(192, 192, 192), 0); // set all neopixels back to Silver
  }  
  else if (Min > 1.00 && Min <= 3.00)  // If train is arriving
  {  
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< fadeCount; i++) {
      FadeInOut(192 ,192, 192, waitTime);    // Fade in and out Silver
      delay(500);
    }
    colorWipe(pixels.Color(192 ,192, 192), 0); // set all neopixels back to Silver
  }
  else if (Min >= 0 && Min <= 1.00)   // If train is boarding
  {
    matrix.print(Min);
    matrix.writeDisplay();

    for(int i=0; i< sparkleCount; i++) {
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
  colorWipe(pixels.Color(255, 0, 255), 0); // set all neopixels to Magenta
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Out of Service");
  lcd.setCursor(0,1);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);

  matrix.clear();
  matrix.writeDigitRaw(0, B01011100);  // 7 Segment LED "O"
  matrix.writeDigitRaw(1, B01011100);  // 7 Segment LED "O"
  matrix.writeDigitRaw(3, B01101101);  // 7 Segment LED "S"
  matrix.writeDisplay();
 }
 else if (Line == "No")
 {
  Serial.println("No Passenger");
  colorWipe(pixels.Color(255, 0, 255), 0); // set all neopixels to Magenta
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("No Passenger");
  lcd.setCursor(0,1);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);

  matrix.clear();
  matrix.writeDigitRaw(1, B01010100);  // 7 Segment LED "n"
  matrix.writeDigitRaw(3, B01011100);  // 7 Segment LED "o"
  matrix.writeDisplay();
 }
 else
 {
  Serial.println("LINE UNKNOWN");
  colorWipe(pixels.Color(255, 0, 255), 0); // set all neopixels to Magenta
  matrix.print(Min);
  matrix.writeDisplay();
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Line Unknown");
  lcd.setCursor(0,1);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("LN  CAR  DEST  MIN");
  lcd.setCursor(0,2);
  lcd.print(Line);
  lcd.print("  ");
  lcd.print(Car);
  lcd.print("  ");
  lcd.print(Destination);
  lcd.print("  ");
  lcd.print(CMin);
 }
}

// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();

  // ---------- USER CODE GOES HERE ----------

  // ** Receive Time (NTP)
  //GetTime();

  // ** Metro Check **
  DetermineStation();
  yield();

  // ---------- USER CODE GOES HERE ----------
}
