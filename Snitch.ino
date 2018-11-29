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

int neoPin = 13;

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

uint32_t off = strip.Color(255, 0, 0);
uint32_t on = strip.Color(0, 255, 0);
uint32_t other = strip.Color(0, 0, 255);

// Variables to save date and time
String formattedDate;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while(!Serial);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize a NTPClient to get time
  timeClient.begin();

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  //Serial.println(formattedDate);
  
  int hours = (formattedDate.substring(11, 13)).toInt();
  int minutes = (formattedDate.substring(14, 16)).toInt();
  
  Serial.print(hours);
  Serial.print(":");
  Serial.println(minutes);
  
  String sMinutes = String(minutes, BIN);
  String sHours = String(hours, BIN);

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

  Serial.print(sHours.length());
  Serial.print(":");
  Serial.println(sMinutes.length());
  
  Serial.print(sHours);
  Serial.println(sMinutes);

  // strip.setPixelColor(n, color);
  for (int i=0; i < sHours.length(); i++) {
    if (sHours[i] == '0') {
      strip.setPixelColor(i, off);
    }
    if (sHours[i] == '1') {
      strip.setPixelColor(i, on);
    }
    if (sHours[i] == '-') {
      strip.setPixelColor(i, other);
    }
  }

  for (int i=0; i < sMinutes.length(); i++) {
    if (sMinutes[i] == '0') {
      strip.setPixelColor(i+8, off);
    }
    if (sHours[i] == '1') {
      strip.setPixelColor(i+8, on);
    }
    if (sHours[i] == '-') {
      strip.setPixelColor(i+8, other);
    }
  }
  
  strip.show();
  
  delay(1000);
}
