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

// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
char APIKEY[32] = "PLACEHOLDER_FOR_APIKEY";
int CityID = 2673730;  //Stockholm, SE

// Define WiFiManager Object
WiFiManager wifiManager;

WiFiClient client; // Used to get temperature
const char* servername = "api.openweathermap.org"; // remote server we will connect to

WiFiUDP ntpUDP; // Used by NTPClient to get time

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 7200, 60000);

bool doTime = true;
int temperature = -273;

// Save Config in JSON format
void saveConfigFile() {
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["APIKEY"] = APIKEY;
  json["CityID"] = CityID;

  // Open config file
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    // Error, file did not open
    Serial.println("failed to open config file for writing");
  }

  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0) {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}

// Load existing configuration file
bool loadConfigFile() {
  // Uncomment if we need to format filesystem
  // SPIFFS.format();

  // Read configuration from FS json
  Serial.println("Mounting File System...");

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error) {
          Serial.println("Parsing JSON");

          strcpy(APIKEY, json["APIKEY"]);
          CityID = json["CityID"].as<int>();

          return true;
        }
        else {
          // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else {
    // Error mounting file system
    Serial.println("Failed to mount FS");
  }

  return false;
}

// Callback notifying us of the need to save configuration
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Called when config mode launched
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered Configuration Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
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

// Change to true when testing to force configuration every time we run
void runWiFiM() {
  bool forceConfig = false;

  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup) {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }

  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);

  // Setup Serial monitor
  Serial.begin(115200);
  delay(10);

  // Reset settings (only for development)
  wifiManager.resetSettings();

  // Set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // Custom elements

  // Text box (String) - 50 characters maximum
  WiFiManagerParameter custom_text_box("key_text", "Enter your string here", APIKEY, 50);
  
  // Need to convert numerical input to string to display the default value.
  char convertedValue[6];
  sprintf(convertedValue, "%d", CityID); 
  
  // Text box (Number) - 7 characters maximum
  WiFiManagerParameter custom_text_box_num("key_num", "Enter your number here", convertedValue, 7); 

  // Add all defined parameters
  wifiManager.addParameter(&custom_text_box);
  wifiManager.addParameter(&custom_text_box_num);

  if (forceConfig) {
    // Run if we need a configuration
    if (!wifiManager.startConfigPortal(projectName)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      //ESP.restart();
      delay(5000);
    }
  }
  else {
    if (!wifiManager.autoConnect(projectName)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      //ESP.restart();
      delay(5000);
    }
  }

  // If we get here, we are connected to the WiFi

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Lets deal with the user config values

  // Copy the string value
  strncpy(APIKEY, custom_text_box.getValue(), sizeof(APIKEY));
  Serial.print("APIKEY: ");
  Serial.println(APIKEY);

  //Convert the number value
  CityID = atoi(custom_text_box_num.getValue());
  Serial.print("CityID: ");
  Serial.println(CityID);


  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfigFile();
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);

  // Connect to Wifi
  runWiFiM();

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
