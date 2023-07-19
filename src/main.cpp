#include <Arduino.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>

#define projectName "ESP32-Snitch"

#define NUM_LEDS 16
#define DATA_PIN 12

// Time in seconds
const int AwakeHour = 7 * 3600; // all Green
const int SleepHour = 19 * 3600; // all Red
const int PreAwakeHour = AwakeHour - (1 * 3600); // Red to Green
const int PreSleepHour = SleepHour - (1 * 3600); // Green to Red

const int brightness = 1; // Set the brightness level

int red[3] = {brightness, 0, 0};
int green[3] = {0, brightness, 0};

CRGB leds[NUM_LEDS];

WiFiUDP ntpUDP; // Used by NTPClient to get time
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 7200, 60000);

unsigned long lastNTPUpdate = 0; // Stores the last time the NTP client was updated
const unsigned long NTPUpdateInterval = 3600000; // Update interval for NTP client in milliseconds (1 hour)

class DummyTimeClient {
  private:
    unsigned long currentTime = 0;
    const unsigned long dayNightCycleDuration = 24 * 3600; // Duration of a day-night cycle in seconds

  public:
    void begin() {
      currentTime = 0 * 3600; // Initialize time to start of day-night cycle
    }

    void update() {
      currentTime++; // Increment current time by 1 second
      if (currentTime >= dayNightCycleDuration) {
        currentTime = 0; // Reset time to start of day-night cycle
      }
      Serial.printf("Current time: %02d:%02d:%02d\n", getHours(), getMinutes(), getSeconds());
    }

    int getHours() {
      return (currentTime / 3600) % 24;
    }

    int getMinutes() {
      return (currentTime / 60) % 60;
    }

    int getSeconds() {
      return currentTime % 60;
    }
};

//DummyTimeClient timeClient;

void setLEDs(int color[3]) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].setRGB(color[0], color[1], color[2]);
  }
}

void transitionLEDs(int TimeDaySec, int startHour, int endHour, int startColor[3], int endColor[3]) {
  // Calculate the transition period and threshold as floating-point numbers to allow for fractional values.
  // This ensures a smooth transition even when the time or number of LEDs doesn't divide evenly.
  float transitionPeriod = static_cast<float>(endHour - startHour) / NUM_LEDS;
  float threshold = static_cast<float>(TimeDaySec - startHour) / transitionPeriod;

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < threshold) {
      leds[i].setRGB(endColor[0], endColor[1], endColor[2]); // Swap start and end colors
    } else {
      leds[i].setRGB(startColor[0], startColor[1], startColor[2]); // Swap start and end colors
    }
  }
}

void drawChildClock(int TimeDaySec) {
  if (TimeDaySec > SleepHour || TimeDaySec < PreAwakeHour) {
    setLEDs(red); // All Red
  }
  else if (TimeDaySec > AwakeHour && TimeDaySec <= PreSleepHour) {
    setLEDs(green); // All Green
  }
  else if (TimeDaySec > PreAwakeHour && TimeDaySec <= AwakeHour) {
    transitionLEDs(TimeDaySec, PreAwakeHour, AwakeHour, red, green); // Red to Green
  }
  else if (TimeDaySec > PreSleepHour && TimeDaySec <= SleepHour) {
    transitionLEDs(TimeDaySec, PreSleepHour, SleepHour, green, red); // Green to Red
  }
}

void connectWifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // set a custom hostname, sets sta and ap dhcp client id for esp32, and sta for esp8266
  wifiManager.setHostname(projectName);

  wifiManager.setClass("invert"); // dark theme

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  //wifiManager.resetSettings();
  //Debug is enabled by default on Serial in non-stable releases. To disable add before autoConnect/startConfigPortal
  wifiManager.setDebugOutput(false);

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wifiManager.autoConnect(projectName); // password protected ap

  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
  }
  else {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);

  // Connect to Wifi
  connectWifi();

  timeClient.begin();

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // Startup animation
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(i * (255 / NUM_LEDS), 255, 255); // sets the color
    FastLED.show(); // updates the strip to show the new color
    delay(125); // waits for a bit before moving on to the next LED
    leds[i] = CRGB::Black; // resets the LED to off after it's been lit
  }
}

void loop() {
  unsigned long currentMillis = millis(); // Get the current time in milliseconds

  // Check if it's time to update the NTP client
  if (currentMillis - lastNTPUpdate > NTPUpdateInterval) {
    timeClient.update(); // Update the NTP client
    lastNTPUpdate = currentMillis; // Save the time of the last update
  }

  int TimeDaySec = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
  drawChildClock(TimeDaySec);

  FastLED.show();
  delay(1000);
}
