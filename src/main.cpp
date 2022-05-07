#include <Arduino.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <NeoPixelBus.h>
#include <ArduinoJson.h>

int neoCount = 16;
int neoPin = 12;
int colorSaturation = 1;

// Timer for when to update NTP
unsigned long timePreviousMillis = 0;               // will store last time
const unsigned long timeInterval = 10 * 60 * 1000;  // interval at which to run (milliseconds)

// Timer for when to update temperature
unsigned long tempPreviousMillis = 0;               // will store last time
const unsigned long tempInterval = 5* 60 * 1000;  // interval at which to run (milliseconds)

// Timer for when to switch between time and temperature
unsigned long displayPreviousMillis = 0;               // will store last time
const unsigned long displayInterval = 60 * 1000;  // interval at which to run (milliseconds)

// Timer for when to check for wifi connection
unsigned long wifiPreviousMillis = 0;               // will store last time
const unsigned long wifiInterval = 10 * 1000;       // interval at which to run (milliseconds) (default 60 sec)

String CityID = "REPLACE_WITH_YOUR_CITY";
String APIKEY = "REPLACE_WITH_YOUR_API_KEY";

WiFiManager wifiManager;  // Initialize library

WiFiClient client; // Used to get temperature
const char* servername = "api.openweathermap.org"; // remote server we will connect to

WiFiUDP ntpUDP; // Used by NTPClient to get time

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 7200, 60000);

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(neoCount, neoPin);
//NeoPixelBus<NeoRgbFeature, Neo400KbpsMethod> strip(PixelCount, PixelPin);

RgbColor ledOff(colorSaturation, 0, 0);
RgbColor ledOn(0, colorSaturation, 0);
RgbColor ledOther(0, 0, colorSaturation);

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

bool doTime = true;
int temperature = -273;

float computeHeatIndex(float temperature, int percentHumidity) {
  temperature = temperature * 1.8 + 32;
  float hi = 0.5 * (temperature + 61.0 + ((temperature - 68.0) * 1.2) + (percentHumidity * 0.094));

  if (hi > 79) {
    hi = -42.379 +
         2.04901523 * temperature +
         10.14333127 * percentHumidity +
         -0.22475541 * temperature * percentHumidity +
         -0.00683783 * pow(temperature, 2) +
         -0.05481717 * pow(percentHumidity, 2) +
         0.00122874 * pow(temperature, 2) * percentHumidity +
         0.00085282 * temperature * pow(percentHumidity, 2) +
         -0.00000199 * pow(temperature, 2) * pow(percentHumidity, 2);

    if ((percentHumidity < 13) && (temperature >= 80.0) && (temperature <= 112.0))
      hi -= ((13.0 - percentHumidity) * 0.25) * sqrt((17.0 - abs(temperature - 95.0)) * 0.05882);

    else if ((percentHumidity > 85.0) && (temperature >= 80.0) && (temperature <= 87.0))
      hi += ((percentHumidity - 85.0) * 0.1) * ((87.0 - temperature) * 0.2);
  }
  return (hi - 32) * 0.55555;
}

int getTemperature() {
  String json;
  int tempInt = -273;
  float tempFloat = -273;
  const int httpPort = 80;

  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 270;
  DynamicJsonDocument doc(capacity);

  if (client.connect(servername, httpPort)) {
    // We now create a URI for the request
    String url = "/data/2.5/weather?id=" + CityID + "&units=metric&APPID=" + APIKEY;

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + servername + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return tempInt;
      }
    }

    // Read all the lines of the reply from server
    while (client.available()) {
      json = client.readStringUntil('\r');
    }

    // Parse the json and extract the temperature
    deserializeJson(doc, json);

    JsonObject main = doc["main"];
    float main_temp = main["temp"];
    int main_humidity = main["humidity"];

    Serial.print("Main temp: ");
    Serial.println(main_temp);
    Serial.print("Main humidity: ");
    Serial.println(main_humidity);

    tempFloat = computeHeatIndex(main_temp, main_humidity);

    Serial.print("Heat index: ");
    Serial.println(tempFloat);

    // Round and store with out decimals.
    tempInt = round(tempFloat);

    Serial.print("Rounded heat index: ");
    Serial.println(tempInt);

    return tempInt;
  }

  else {
    Serial.println("Unable to connect");
    return tempInt;
  }
}

void drawTemperature(int temperature) {
  if (temperature >= neoCount + 1) {
    // Up and red
    for (int i = 0; i < neoCount; i++) {
      if (temperature - 16 > i) {
        strip.SetPixelColor(i, red);
      }
      else {
        strip.SetPixelColor(i, black);
      }
    }
  }
  else if (temperature >= 1 && temperature <= neoCount) {
    // Up and green
    for (int i = 0; i < neoCount; i++) {
      if (temperature > i) {
        strip.SetPixelColor(i, green);
      }
      else {
        strip.SetPixelColor(i, black);
      }
    }
  }
  else if (temperature == 0) {
    // All off
    for (int i = 0; i < neoCount; i++) {
      strip.SetPixelColor(i, black);
    }
  }
  else if (temperature <= -1 && temperature >= -16) {
    // Down and blue
    for (int i = neoCount - 1; i >= 0; i--) {
      if (temperature < 0) {
        strip.SetPixelColor(i, blue);
        temperature++;
      }
      else {
        strip.SetPixelColor(i, black);
      }
    }
  }
  else if (temperature == -273) {
    for (int i = 0; i < 16; i = i + 2) {
      strip.SetPixelColor(i, blue);
      strip.SetPixelColor(i + 1, black);
    }
  }
}

void drawTime(String sHours, String sMinutes) {
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
      strip.SetPixelColor(i, ledOff);
    }
    if (sHours[i] == '1') {
      strip.SetPixelColor(i, ledOn);
    }
    if (sHours[i] == '-') {
      strip.SetPixelColor(i, ledOther);
    }
  }

  for (int i = 0; i < sMinutes.length(); i++) {
    if (sMinutes[i] == '0') {
      strip.SetPixelColor(i + 8, ledOff);
    }
    if (sMinutes[i] == '1') {
      strip.SetPixelColor(i + 8, ledOn);
    }
    if (sMinutes[i] == '-') {
      strip.SetPixelColor(i + 8, ledOther);
    }
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);

  // Connect to Wifi
  wifiManager.autoConnect();

  // Initialize a NTPClient and get time
  timeClient.begin();
  timeClient.update();

  temperature = getTemperature();

  strip.Begin();
  strip.Show(); // Initialize all pixels to 'off'
}

void loop() {
  // Don't update from internet every second. Instead update every
  // "interval" (default 10 mins).
  if (millis() - timePreviousMillis >= timeInterval) {
    while (!timeClient.update()) {
      timeClient.forceUpdate();
    }
    timePreviousMillis = millis();
  }

  if (millis() - tempPreviousMillis >= tempInterval) {
    temperature = getTemperature();
    Serial.print(temperature);
    Serial.println(" degrees celsius.");
    tempPreviousMillis = millis();
  }

  if (doTime) { // Do the time thing
    // The getHours and getMinutes comes with the following format:
    // 16 and 00
    // We need to use the String function to make it binary.
    String sHours = String(timeClient.getHours(), BIN);
    String sMinutes = String(timeClient.getMinutes(), BIN);
    
    drawTime(sHours, sMinutes);
  }
  else { // Do the temp thing
    drawTemperature(temperature);
  }
  
  if (millis() - displayPreviousMillis >= displayInterval) {
    doTime = !doTime;
    displayPreviousMillis = millis();
  }

  /* TIME DEBUG
  Serial.print(sHours);
  Serial.print(" ");
  Serial.print(sMinutes);
  Serial.print(" ");
  Serial.println(timeClient.getSeconds());
  */

  strip.Show();
  delay(1000);
}
