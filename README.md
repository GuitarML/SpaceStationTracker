# International Space Station Tracker

SpaceStationTracker is an application that displays the current position of the International Space Station on a 2D world map. 
It runs on a ESP32-2432S028R ILI9341 device with a 2.8" screen, commonly known as the CYD "Cheap Yellow Display", 
which costs $20 or less. The code was written using Arduino IDE / ESP32 development environment in c++.

![app](https://github.com/GuitarML/SpaceStationTracker/blob/main/resources/space_cyd.jpg)

I used this project to learn programming for ESP32/Arduino/CYD.
It includes topics such as graphics using LVGL, wifi connection, getting
information from a restful API, parsing json data, and touchscreen interaction.
This would make a great space related STEM classroom project.

As of the time of writing, the end of life for the ISS is slated for 2030.
This code could be repurposed for other orbiting space objects.

### Using the SpaceStationTracker

The SpaceStationTracker requires a local Wifi connection to retrieve location data
for the International Space Station. The first time you load and run the app on the CYD, it will
display a splash screen for how to connect. The ESP32 board will create a hotspot
that you can then connect to from a phone or laptop. Choose your Wifi network and
enter your credentials to connect the CYD to your Wifi. This is a safer method than
hard coding Wifi info in the application, as with many esp32 DIY projects out there.

Once the app is connected to Wifi, it will continue to display and track the
International Space Station. The esp32 will remember the last Wifi connection
when cycling power, and if it changes you will be prompted to re connect using the above method.

![app](https://github.com/GuitarML/SpaceStationTracker/blob/main/resources/splashscreen.jpg)
The splashscreen will display on the CYD until it can connect to WiFi.

![app](https://github.com/GuitarML/SpaceStationTracker/blob/main/resources/wifi1.jpg)
On a phone or computer, connect to the temporary WiFi access point called "SpaceStationTracker".
It should show the screen above. Click "Configure WiFi".
 
![app](https://github.com/GuitarML/SpaceStationTracker/blob/main/resources/wifi2.jpg)
Enter the WiFi network you wish to connect to (or select from the list) and enter your password. 
This is used for the ESP32 board to connect to your local WiFi. After a successful connection,
the SpaceStationTracker app will load and begin tracking the International Space Station.

The Latitude, Longitude, and UTC date time is displayed in real time, updating from 
the Where The ISS At API.
You can use the touchscreen and click on the ISS icon to display a random
fact about the ISS.

The screen brightness will dim automatically over a few minutes to save power, 
and will go to full brightness again when the user interacts with the touchscreen.

### Hardware Info

I bought my CYD from this link on [Amazon](https://www.amazon.com/dp/B0CG2WQGP9) for less than $20, but many places sell them.
Keep in mind there are several variations that can be called a CYD. Make sure it is the resistive touch (as opposed to 
capacitive touch), and 2.8 inch screen, otherwise this code will not work. The designator for this model is "ESP32-2432S028R"
Some versions have a micro USB, and some have both a micro USB and USB C. Either USB should work, but keep in mind 
when getting a cable. I liked the above package from that particular Amazon seller, because it came with a 
USB micro cable and case for the board.

This is a great article on [Random Nerd Tutorials](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/) to get you started with the CYD.

There is not a specific case for this project, but there are many 3D printed cases available for the CYD.
Choose one that suits your needs, here are some [examples](https://www.printables.com/tag/cyd). 
Be aware of the differences in the printable models if you want to use the micro USB vs. the USB C cable.

### How to compile:

1. Configure Arduino IDE 
    a. For "esp32" board with a device type of "ESP32 Dev Module"
    b. Set "Tools -> Partition Scheme" to "Huge App (3MB No OTA/1MB SPIFFS)"
2. Install the libraries below in Arduino IDE
3. You can copy the entire SpaceStationTracker repository into ~/Documents/Arduino/
    a. User_Setup.h file for TFT_eSPI needs to be placed (overwrite to) the TFT_eSPI folder in ~/Documents/Arduino/libraries
    b. lv_conf.h file for LVGL needs to be placed in ~/Documents/Arduino/libraries/lvgl/src
4. Plug in your CYD over USB to your computer, connect in the Arduino IDE, and compile and upload the SpaceStationTracker code using the Arduino IDE. 
   To reset the CYD to get ready for install, you may need to hold the BOOT button, then press and release the RESET button, 
   and then release the BOOT button. These are the small white buttons located on the back of the CYD board. They are labeled "RST" and BOOT".

### Required Libraries:

- ArduinoJson 7.4.1
- HttpClient 2.2.0
- lvgl 9.2.2
- RTClib 2.1.4
- TFT_eSPI 2.5.43_
- WifiManager 2.0.17
- XPT2046_Touchscreen 1.4

### License

The SpaceStationTracker.ino code is under the terms of the GPL 3.0 license.


### Acknowledgements

- Thanks to [WhereTheISS.at](https://wheretheiss.at/w/developer) for the cool API! 
- ISS icon was resized from the ISS icon found on the [NASA Website](https://science.nasa.gov/multimedia/spacecraft-icons/)
- Background worldmap was generated using [Basemap](https://matplotlib.org/basemap/stable/) (NASA Bluemarble, Mercator Projection)
- Much info was gathered from [Random Nerd Tutorials for CYD](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/)
- Another really cool CYD app was used as a reference for Wifi login and splashscreen: [Aura](https://github.com/Surrey-Homeware/Aura)
- The background worldmap and ISS icon were converted to c code using the [LVGL Image Converter](https://lvgl.io/tools/imageconverter)
