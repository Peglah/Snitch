/*
 * Snitch
 * Simple Night Watch. An Arduino sketch for the Huzzah32 telling you the time using binary LEDs.
 */
 
 #include <Adafruit_NeoPixel.h>
 
const int PIN = 6;
  
 void setup() {
    // Parameter 1 = number of pixels in strip
    // Parameter 2 = pin number (most are valid)
    // Parameter 3 = pixel type flags, add together as needed:
    //   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
    //   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
    //   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
    //   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
    Adafruit_NeoPixel strip = Adafruit_NeoPixel(8+8, PIN, NEO_GRB + NEO_KHZ800);
    
    
 }
 
 void loop() {
    
 }
