/*********
  Based on ESP32_NTPClient by Rui Santos
  https://github.com/RuiSantosdotme/Random-Nerd-Tutorials/blob/master/Projects/ESP32/ESP32_NTPClient.ino
*********/

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>

// Replace with your network credentials
const char* ssid     = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

int neoPin = 12;
int neoBrightness = 16;

unsigned long previousMillis = 0;               // will store last time
const unsigned long interval = 10 * 60 * 1000;  // interval at which to run (milliseconds)

// Define NTP Client to get time
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 3600, 60000);

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, neoPin, NEO_GRB + NEO_KHZ800);

uint32_t ledOff = strip.Color(255, 0, 0);
uint32_t ledOn = strip.Color(0, 255, 0);
uint32_t ledOther = strip.Color(0, 0, 255);

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  //WiFi.hostname("ESP32-Snitch");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize a NTPClient and get time
  timeClient.begin();
  timeClient.update();

  strip.begin();
  strip.setBrightness(neoBrightness);
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  // Don't update from internet every second. Instead update every
  // "interval" (default 10 mins).
  if (millis() - previousMillis >= interval) {
    while (!timeClient.update()) {
      timeClient.forceUpdate();
    }
    previousMillis = millis();
  }


  // The getHours and getMinutes comes with the following format:
  // 16 and 00
  // We need to use the String function to make it binary.
  String sHours = String(timeClient.getHours(), BIN);
  String sMinutes = String(timeClient.getMinutes(), BIN);

  while (5 - sHours.length() > 0) {
    sHours = "0" + sHours;
  }
  while (8 - sHours.length() > 0) {
    sHours = sHours + "-";
  }

  while (6 - sMinutes.length() > 0) {
    sMinutes = "0" + sMinutes;
  }
  while (8 - sMinutes.length() > 0) {
    sMinutes = "-" + sMinutes;
  }

  for (int i = 0; i < sHours.length(); i++) {
    if (sHours[i] == '0') {
      strip.setPixelColor(i, ledOff);
    }
    if (sHours[i] == '1') {
      strip.setPixelColor(i, ledOn);
    }
    if (sHours[i] == '-') {
      strip.setPixelColor(i, ledOther);
    }
  }

  for (int i = 0; i < sMinutes.length(); i++) {
    if (sMinutes[i] == '0') {
      strip.setPixelColor(i + 8, ledOff);
    }
    if (sMinutes[i] == '1') {
      strip.setPixelColor(i + 8, ledOn);
    }
    if (sMinutes[i] == '-') {
      strip.setPixelColor(i + 8, ledOther);
    }
  }

  Serial.print(sHours);
  Serial.print(" ");
  Serial.print(sMinutes);
  Serial.print(" ");
  Serial.println(timeClient.getSeconds());

  strip.show();

  delay(1000);
}
