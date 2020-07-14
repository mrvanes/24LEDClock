# 24LEDClock
LED Clock using 24 neopixel ring

An Arduino project

The ring only consists of 24LEDs so I created a clock that makes use of fractional brightness to display a 12hr clock using three free to choose colors.

[Thingiverse 24 LED Light Clock](https://www.thingiverse.com/thing:4541805)

# Requirements
The ESP8266 boards will need to be added to the Arduino IDE which is achieved as follows. Click File > Preferences and copy and paste the URL "http://arduino.esp8266.com/stable/package_esp8266com_index.json" into the Additional Boards Manager URLs field. Click OK. Click Tools > Boards: ... > Boards Manager. Find and click on ESP8266 (using the Search function may expedite this). Click on Install. After installation, click on Close and then select your ESP8266 board from the Tools > Board: ... menu.

The app depends on the following libraries. They must either be downloaded from GitHub and placed in the Arduino 'libraries' folder, or installed as described here by using the Arduino library manager.

- [FastLED](https://github.com/FastLED/FastLED)
- [Sketch Data Upload Tool](https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.2.0/ESP8266FS-0.2.0.zip)

