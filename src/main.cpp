#include <SPIFFS.h>               //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define projectName "ESP32-Snitch"

#define NUM_LEDS 16
#define DATA_PIN 12
CRGB leds[NUM_LEDS];

// Timer for when to update NTP
unsigned long timePreviousMillis = 0;               // will store last time
const unsigned long timeInterval = 10 * 60 * 1000;  // interval at which to run (milliseconds)

// Timer for when to update temperature
unsigned long tempPreviousMillis = 0;               // will store last time
const unsigned long tempInterval = 5* 60 * 1000;  // interval at which to run (milliseconds)

// Timer for when to switch between time and temperature
unsigned long displayPreviousMillis = 0;               // will store last time
const unsigned long displayInterval = 60 * 1000;  // interval at which to run (milliseconds)

int CityID = 2673730;  //Stockholm, SE
char APIKEY[32] = "PLACEHOLDER_FOR_APIKEY";

bool shouldSaveConfig = false;

WiFiClient client; // Used to get temperature
const char* servername = "api.openweathermap.org"; // remote server we will connect to

WiFiUDP ntpUDP; // Used by NTPClient to get time

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 7200, 60000);

bool doTime = true;
int temperature = -273;

void saveConfigCallback() {
  //callback notifying us of the need to save config
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

bool readFS() {
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
          Serial.println("\nparsed json");
          CityID = json["CityID"].as<int>();
          strcpy(APIKEY, json["APIKEY"]);
          return true;
        } 
        else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  }
  else {
    Serial.println("failed to mount FS");
  }
  return false;
}

void writeFS() {
    DynamicJsonDocument json(1024);
    json["cityid"] = CityID;
    json["apikey"] = APIKEY;

    Serial.println("Attempting to SPIFFS.open /config.json");
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    //end save
}

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
    String url = "/data/2.5/weather?id=" + String(CityID) + "&units=metric&APPID=" + APIKEY;

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
  if (temperature >= NUM_LEDS + 1) {
    // Up and red
    for (int i = 0; i < NUM_LEDS; i++) {
      if (temperature - 16 > i) {
        leds[i].setRGB(1, 0, 0);
      }
      else {
        leds[i] = CRGB::Black;
      }
    }
  }
  else if (temperature >= 1 && temperature <= NUM_LEDS) {
    // Up and green
    for (int i = 0; i < NUM_LEDS; i++) {
      if (temperature > i) {
        leds[i].setRGB(0, 1, 0);
      }
      else {
        leds[i] = CRGB::Black;
      }
    }
  }
  else if (temperature == 0) {
    // All off
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
  }
  else if (temperature <= -1 && temperature >= -16) {
    // Down and blue
    for (int i = NUM_LEDS - 1; i >= 0; i--) {
      if (temperature < 0) {
        leds[i].setRGB(0, 0, 1);
        temperature++;
      }
      else {
        leds[i] = CRGB::Black;
      }
    }
  }
  else if (temperature == -273) {
    for (int i = 0; i < 16; i = i + 2) {
      leds[i].setRGB(0, 0, 1);
      leds[i+1] = CRGB::Black;
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
      leds[i].setRGB(1, 0, 0);
    }
    if (sHours[i] == '1') {
      leds[i].setRGB(0, 1, 0);
    }
    if (sHours[i] == '-') {
      leds[i].setRGB(0, 0, 1);
    }
  }

  for (int i = 0; i < sMinutes.length(); i++) {
    if (sMinutes[i] == '0') {
      leds[i+8].setRGB(1, 0, 0);
    }
    if (sMinutes[i] == '1') {
      leds[i+8].setRGB(0, 1, 0);
    }
    if (sMinutes[i] == '-') {
      leds[i+8].setRGB(0, 0, 1);
    }
  }
}

void connectWifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  // Need to convert numerical input to string to display the default value.
  char convertedValue[7];
  sprintf(convertedValue, "%d", CityID); 

  WiFiManagerParameter custom_cityid("cityid", "CityID", convertedValue, 7);
  WiFiManagerParameter custom_apikey("apikey", "APIKEY", APIKEY, 32);

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // set a custom hostname, sets sta and ap dhcp client id for esp32, and sta for esp8266
  wifiManager.setHostname(projectName);

  wifiManager.setClass("invert"); // dark theme

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_cityid);
  wifiManager.addParameter(&custom_apikey);

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  wifiManager.resetSettings();
  //Debug is enabled by default on Serial in non-stable releases. To disable add before autoConnect/startConfigPortal
  //wifiManager.setDebugOutput(false);

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  if (!wifiManager.autoConnect(projectName)) {
    Serial.println("Failed to connect");
    //ESP.restart();
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  CityID = atoi(custom_cityid.getValue());
  strcpy(APIKEY, custom_apikey.getValue());
  Serial.println("The values in the file are: ");
  Serial.println("\tCityID : " + String(CityID));
  Serial.println("\tAPIKEY : " + String(APIKEY));

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    writeFS();
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);

  // Connect to Wifi
  connectWifi();

  // Initialize a NTPClient and get time
  timeClient.begin();
  timeClient.update();

  temperature = getTemperature();

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
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
  Serial.print(timeClient.getHours());
  Serial.print(" ");
  Serial.print(timeClient.getMinutes());
  Serial.print(" ");
  Serial.println(timeClient.getSeconds());
  */

  FastLED.show();
  delay(1000);
}
