# WMATA-ESP8266-Metro-Status

Brian Leschke 2017
 
 ** Update 4/30/17: Fully Working release!

## **Overview**

 An ESP8266 will control a neopixel ring (metro line), 7-segment LED (arrival time), and 20x4 LCD screen (station updates).

Project Breakdown:
* Neopixel Ring
    * Displays Metro Line
        * RD - RED
        * OR - ORANGE
        * YL - YELLOW
        * GR - GREEN
        * BL - BLUE
        * SV - SILVER
    * Fades in-out upon arrival (1-3 min)
    * Sparkle flash upon boarding (0-1 min)
* 7-Segment LED
    * Displays arrival time (minutes)
* 20x4 LCD
    * Displays station updates
        * LN   - Line
        * CAR  - Number of cars on train
        * DEST - Destination station
        * MIN  - Minutes until arrival

### **Prerequisities**

You will need:

1. Adafruit Huzzah ESP8266 (http://adafru.it/2471)
2. Neopixel Ring (http://adafru.it/1586)
3. 7-Segment Alphanumeric LED (http://adafru.it/878)
4. Serial 20x4 LCD (ebay)
5. Arduino programming software
6. Python installed on your operating system (used for OTA updates).
7. FTDI cable (https://www.sparkfun.com/products/9717)
8. WMATA API (http://developer.wmata.com)


### **Usage**

In order to use successfully implement this project and its code, you will need to install the Arduino Programming Software.
    
### **Libraries**

The following libraries are required for this project:
    
  * [Arduino Framework (core library)](https://github.com/esp8266/Arduino)
  * ArduinoOTA
  * ArduinoJson
  * LiquidCrystal_V1.2.1 (or any supporting version for your lcd)
  * Adafruit_Neopixel
  * ESP8266WiFi
  * NTPClient
  * [Adafuit LED Backpack](https://github.com/adafruit/Adafruit-LED-Backpack-Library)
  * [Adafuit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
        
## **Uploading**

The code can be uploaded to the ESP8266 in two ways, either by OTA (over-the-air) updates or by a serial (FTDI cable) connection.
Users will need to initially upload the code using an FTDI cable.

### **FTDI Upload**

This method must be used in the initial flashing of the ESP8266. An FTDI or UART cable has 6 pins.
Every time the you want to flash the ESP8266, you will need to do the following to enter the bootloader mode:

  1. Press and hold the RESET button.
  2. Press and hold the GPIO0 button.
  3. Release the RESET button and continue to hold the GPIO0 button.
  4. Release the GPIO0 button (you should see a faint red glow from the GPIO0 LED).
  

### **OTA Upload**

Over-the-air updates is a method to update the ESP8266 without physically being with the device. To update the device you will need to
know the IP address and be connected to the same network.

If for some reason, the network ESP8266 network configuration is incorrect, the device will restart.

        
### **Recognition and Credit**
Thanks to WMATA for providing the API!

