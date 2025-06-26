/* 
International Space Station Tracker for ESP32 CYD (Cheap Yellow Display)

Simple application that displays the current location of the ISS
using data from Where the ISS At API:
https://api.wheretheiss.at/v1/satellites/25544

The Mercator projection worldmap was generated using the 
Python Basemap (an extension of matplotlib), and scaled 
for a 320x240 display.

As of the time of writing, the end of life for the ISS is slated for 2030.
This code could be repurposed for other orbiting space objects.

This project was a great way to learn programming for ESP32/Arduino/CYD,
and includes topics such as graphics using LVGL, wifi connection, getting
information from a restful API, parsing json data, and touchscreen interaction.
I've added lots of comments throughout the code to explain what everything does.

Author: K. Bloemer
Date: 6-25-2025

Acknowledgements:
https://randomnerdtutorials.com/  (CYD related tutorials)
https://matplotlib.org/basemap/stable/ (for worldmap)
https://science.nasa.gov/multimedia/spacecraft-icons/ (ISS icon, which I scaled down)
https://github.com/Surrey-Homeware/Aura (Weather App used for example esp32 Wifi configuration)
*/

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <esp32-hal-ledc.h>


/////////////////////////////////////////
/// User Preferences, edit as desired ///
/////////////////////////////////////////
#define POSITION_UPDATE_TIME 5000UL    // Every 5 seconds attempt to update the ISS position from the API
#define FACT_FADE_UPDATE_TIME 58UL     // Every 58ms fade fact text by 1 from 255 to 0, about 15 seconds
#define BRIGHTNESS_UPDATE_TIME 10000UL // Decrease screen brightness a little every 10 seconds
#define MAX_TRACK_DOTS 200             // The maximum number of trailing track dots, set as desired.
#define SCREEN_MINIMUM_BRIGHTNESS 30   // Edit this from 0 to 255 to set the minimum screen brightness (0 = off, 255=always on full)
/////////////////////////////////////////


// Define touchscreen pins, specific to this particular CYD hardware.
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS
#define LCD_BACKLIGHT_PIN 21

// Define constants for screen info and update times.
// The app attempts to update the ISS position every 5 seconds.
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define DEFAULT_CAPTIVE_SSID "SpaceStationTracker"
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Screen brightness parameters
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
int brightness = 255; // Initial brightness

uint32_t draw_buf[DRAW_BUF_SIZE / 4];
int x, y, z;

// Global variables used by various function in the app
float latitude = 0.0;
float longitude = 0.0;
float map_width = 320;
float map_height = 240;
int unix_time = 0;
int x_pixel = 0;
int y_pixel = 0;
int track_counter = 5; // Create a track dot every 5 position updates

LV_IMAGE_DECLARE(iss_icon_trans60x60);
lv_obj_t * img1;

// Create our text labels
lv_obj_t * lat_label;
lv_obj_t * lon_label;
lv_obj_t * time_label;
lv_obj_t * fact_label;
lv_opa_t fact_opacity = 0; // 0 to 255, 0 is completely transparent

int factIndex = 0;
int last_fact_index = 0;

lv_obj_t * track_dots[MAX_TRACK_DOTS];
int track_dot_counter = 0;

// Array of ISS Facts to display when the ISS icon is clicked on the screen.
// Edit these to display your own facts or anything else.
const char* iss_facts[] = {
  "The ISS orbits Earth about every\n90 minutes.",
  "The average altitude of the ISS\nis 250 miles.",
  "The speed of the ISS is 17,000 mph!",
  "The ISS orbits Earth over 15\ntimes per day.",
  "The ISS has two bathrooms.",
  "The ISS has six sleeping areas.",
  "Astronauts conduct science experiments\nwhile aboard the ISS.",
  "The ISS is 356 feet long.",
  "The ISS has a 55 foot long robotic\narm built by Canada.",
  "The ISS weighs 925,335 pounds.",
  "The ISS is a joint project involving\ncountries all over the world.",
  "The ISS has been continuously occupied\nby astronauts since November 2000.",
  "The first piece of the ISS was\nlaunched in November 1998.",
  "Building ISS required more than\n40 assembly launches.",
  "The ISS was assembled with help\nfrom NASA's Space Shuttle.",
  "The ISS astronauts exercise every\nday to keep healthy.",
  "Eight spaceships can be connected\nto the space station at once.",
  "The ISS includes contributions\nfrom 15 different nations.",
  "Astronauts on the ISS see 16\nsunrises and sunsets per day.",

};

// Lat Lon to Pixel Calculations /////////////
//
// Since the worldmap is formatted to fit on the CYD display,
// the Latitude and Longitude has to be converted to pixel x/y
// coordinates to accurately show where the ISS is located.
// Mercator projection is a cylindrical projection used for 
// the 2D map, which means the area around the poles are very distorted,
// while areas near the equator are more accurate. Furthermore, 
// the map has been skewed to fit within a 320x240 screen, and this
// has been taken into account in the calculations below. 

float degrees_to_radians(float degrees) {
  return degrees * PI / 180;
}

float calculate_mercator_x(float longitude_degrees, float map_width) {
  return (longitude_degrees + 180) * (map_width / 360);
}

float calculate_mercator_y(float latitude_degrees, float map_height, float map_width) {
  float lat_rad = degrees_to_radians(latitude_degrees);

  // Mercator N value
  float merc_n = log(tan((PI / 4) + (lat_rad / 2)));

  // Adjust for map height and aspect ratio
  float y = (map_height / 2) - (map_width * merc_n / (2 * PI));
  return y;
}

void lat_lon_to_pixel() {
  x_pixel = static_cast<int>(calculate_mercator_x(longitude, map_width));
  y_pixel = static_cast<int>(calculate_mercator_y(latitude, map_height, map_width));
  return;
}
//////////////////////////////////////////////

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Function to draw the worldmap
void draw_map(void) {

  LV_IMAGE_DECLARE(bluemarble_320x240);
  lv_obj_t * img0 = lv_image_create(lv_screen_active());
  lv_image_set_src(img0, &bluemarble_320x240);
  lv_obj_align(img0, LV_ALIGN_CENTER, 0, 0);
}

void init_track_dots() {
  // Initialize the array in a loop
  for (int i = 0; i < MAX_TRACK_DOTS; ++i) {

    track_dots[i] = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(track_dots[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(track_dots[i], 4, 4);

    lv_obj_set_style_radius(track_dots[i], LV_RADIUS_CIRCLE, 4);
    lv_obj_set_style_bg_color(track_dots[i], lv_color_hex(0xFF0000), 0); // Red background
    lv_obj_set_style_border_width(track_dots[i], 4, 0);
    lv_obj_set_style_border_color(track_dots[i], lv_color_hex(0xFF0000), 0); // Red border

  }
}

// This updates the ISS icon location based on the current Lat/Lon obtained from the API.
void update_iss_position() {       
  lat_lon_to_pixel();
  lv_obj_set_pos(img1, x_pixel - 30, y_pixel - 24); // -30 and -24 makes icon appear centered
}


// Print lat, lon, and UTC time text to the display
// The ISS is tracked by Coordinated Universal Time (UTC)
// and is returned by the API as a Unix timestamp.
// The code below uses RTClib to convert this to a
// human readable date-time format.
void printLatLongToDisplay() {

  lv_label_set_text_fmt(lat_label, "Lat: %f", latitude);
  lv_label_set_text_fmt(lon_label, "Lon: %f", longitude);

  DateTime dt(unix_time);             // TODO Update so we dont make a new datetime each loop
  String formattedDateTime = "UTC: " + String(dt.year(), DEC) + "-" +
                             String(dt.month(), DEC) + "-" +
                             String(dt.day(), DEC) + " " +
                             String(dt.hour(), DEC) + ":" +
                             String(dt.minute(), DEC) + ":" +
                             String(dt.second(), DEC);

  char charDateTime[40];

  formattedDateTime.toCharArray(charDateTime, 40);
  lv_label_set_text(time_label, charDateTime);
}

// Add track dot to the screen
void add_track_point() {

  // Every 15 position updates, update the track dots on the screen
  if (track_counter > 14) {

    // Set the position of the dot and make it visible
    lv_obj_set_pos(track_dots[track_dot_counter], x_pixel, y_pixel);
    lv_obj_clear_flag(track_dots[track_dot_counter], LV_OBJ_FLAG_HIDDEN);

    // Reset the counter
    track_counter = 0;

    // Keep track of the available dots, reusing dots when the maxiumum is reached
    track_dot_counter += 1;
    if (track_dot_counter > MAX_TRACK_DOTS - 1) {
      track_dot_counter = 0;
    }
  }

  track_counter += 1;
}

// Get the ISS location data from the API over WiFi,
// read the data using json and update the current
// position and timestamp.
void get_iss_current_position() {

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    //http.begin("http://api.open-notify.org/iss-now.json"); // Open Notify API endpoint
    http.begin("https://api.wheretheiss.at/v1/satellites/25544"); // Where the ISS at API endpoint

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();

      // Parse JSON
      DynamicJsonDocument doc(2048); // Adjust size as needed
      deserializeJson(doc, payload);

      // WheretheISS format
      latitude = doc["latitude"];
      longitude = doc["longitude"];
      unix_time = doc["timestamp"];

      // Open notify json format
      //latitude = doc["iss_position"]["latitude"];
      //longitude = doc["iss_position"]["longitude"];
      //unix_time = doc["timestamp"];

      Serial.print("Latitude: ");
      Serial.println(latitude);
      Serial.print("Longitude: ");
      Serial.println(longitude);
      Serial.print("Unix Timestamp: ");
      Serial.println(unix_time);

      update_iss_position();
      printLatLongToDisplay();

    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }

}

void flush_wifi_splashscreen(uint32_t ms = 200) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

// Initial splashscreen telling the user how to 
// connect the CYD to their local WiFi.
void wifi_splash_screen() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x4c8cb9), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0xa6cdec), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *lbl = lv_label_create(scr);
  lv_label_set_text(lbl,
                    "ISS Tracker Wi-Fi Configuration:\n\n"
                    "Please connect your phone or laptop\n"
                    "to the temporary Wi-Fi access point\n "
                    DEFAULT_CAPTIVE_SSID
                    "\n"
                    "to configure.\n\n"
                    "If you don't see a configuration screen \n"
                    "after connecting, visit http://192.168.4.1\n"
                    "in your web browser.");
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
  lv_scr_load(scr);
}

// Callback used by the WiFiManager during initial setup.
void apModeCallback(WiFiManager *mgr) {
  wifi_splash_screen();
  flush_wifi_splashscreen();
}

// Function to read info from the touchscreen when 
// the user interacts with it.
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;

    // Reset screen brightness to max when screen is clicked
    brightness = 255;
    ledcWrite(LCD_BACKLIGHT_PIN, brightness);

  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }

}


// Callback for when the ISS icon is clicked.
void iss_icon_clicked_cb(lv_event_t * event)
{
  
  // Get a random ISS fact but don't repeat the previous one
  while (factIndex == last_fact_index) {
    factIndex = random(sizeof(iss_facts) / sizeof(iss_facts[0]));
  }

  const char* randomFact = iss_facts[factIndex];
  lv_label_set_text(fact_label, randomFact);
  fact_opacity = 255; // Set the fact opacity to solid.

  last_fact_index = factIndex;

}

// The setup function (specific to Arduino sketches) that runs one time
// when the app starts up.
void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);


  TFT_eSPI tft = TFT_eSPI();
  tft.init();

  // NOTE: TFT Library was updated to replace ledcSetup() and ledcAttachPin() with ledcAttach(), and ledChannel input is not needed.
  ledcAttach(LCD_BACKLIGHT_PIN, freq, resolution);
  //ledcSetup(ledChannel, freq, resolution);
  //ledcAttachPin(LCD_BACKLIGHT_PIN, ledChannel);

  ledcWrite(LCD_BACKLIGHT_PIN, brightness); // Set initial brightness
  delay(10);

  // Start LVGL
  lv_init();

  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Init touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(90);  // Make sure the rotation is correct

  // Initialize the TFT display using the TFT_eSPI library
  lv_display_t * disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

  // This handles logging the esp32 into wifi from a remote device
  // and displays the initial splash screen on the CYD.
  WiFiManager wifiManager;
  wifiManager.setAPCallback(apModeCallback);
  wifiManager.autoConnect(DEFAULT_CAPTIVE_SSID);

  // Once we are logged into WiFi, clear the screen and start the app.
  lv_obj_clean(lv_scr_act());
    
  // Draw background worldmap
  draw_map();

  // Prepare track dots
  init_track_dots();

  // Set the ISS icon
  img1 = lv_image_create(lv_screen_active());
  lv_image_set_src(img1, &iss_icon_trans60x60);

  // Make ISS icon clickable
  lv_obj_add_flag(img1, LV_OBJ_FLAG_CLICKABLE);

  // Add an event callback for click
  lv_obj_add_event_cb(img1, iss_icon_clicked_cb, LV_EVENT_CLICKED, NULL);

  // Create Lat/Lon/UTC Time labels
  lat_label = lv_label_create(lv_screen_active());
  lon_label = lv_label_create(lv_screen_active());
  time_label = lv_label_create(lv_screen_active());
  fact_label = lv_label_create(lv_screen_active());
  lv_label_set_text(lat_label, " ");
  lv_label_set_text(lon_label, " ");
  lv_label_set_text(time_label, " ");
  lv_label_set_text(fact_label, " ");
  lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xFF0000), LV_PART_MAIN); // green text
  lv_obj_align(lat_label, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_align(lon_label, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_align(fact_label, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_t * screen = lv_scr_act(); // Get the active screen object
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE); // Make the screen non-scrollable

  // Get the initial ISS position and start tracking
  get_iss_current_position();
  add_track_point();

}

// The loop that runs (speicific to Arduino sketches) throughout 
// the lifetime of the app, after the setup() function runs.
void loop() {

  lv_timer_handler();
  static uint32_t last = millis();
  static uint32_t last_fact = millis();
  static uint32_t last_brightness = millis();

  if (millis() - last >= POSITION_UPDATE_TIME) {
    get_iss_current_position();
    add_track_point();
    last = millis();
  }

  // Random fact fade
  // When the ISS icon is clicked via the touch screen, a random fact 
  //  about the ISS will appear over Antarctica. Fade out gradually over 
  //  15 seconds, or until the ISS icon is clicked again.
  if (millis() - last_fact >= FACT_FADE_UPDATE_TIME) {
    if (fact_opacity > 0) {
      fact_opacity -= 1;
      lv_obj_set_style_opa(fact_label, fact_opacity, 0);
    }
    last_fact = millis();
  }
  
  // Decrease brightness gradually over time
  if (millis() - last_brightness >= BRIGHTNESS_UPDATE_TIME) {
    brightness -= 10;
    if (brightness < SCREEN_MINIMUM_BRIGHTNESS) { 
     brightness = SCREEN_MINIMUM_BRIGHTNESS;
    }
    ledcWrite(LCD_BACKLIGHT_PIN, brightness);
    Serial.print("Brightness: ");
    Serial.println(brightness);
    last_brightness = millis();
  }
  
  lv_tick_inc(5);
  delay(5);

}