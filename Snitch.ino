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

// Define NTP Client to get time
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "se.pool.ntp.org", 3600, 60000);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
int hours;
int minutes;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  //while(!Serial);
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
}

void loop() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.print("DATE: ");
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.print("HOUR: ");
  Serial.println(timeStamp);
  // Split hours and minutes
  hours = (timeStamp.substring(0, 2)).toInt();
  minutes = (timeStamp.substring(2, 4)).toInt();
  delay(1000);
  
  
}
