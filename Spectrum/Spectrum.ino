//#include <FastLED.h>

// Led settings
#define NUM_LEDS 60           // Number of leds in WS2812B
#define CURRENT_LIMIT 3000    // Power supply current limit 3000 mA = 3 A
#define BRIGHTNESS 30         // Leds brigthtness from range 0-255
#define LED_PIN 23            // Pin to connect Data line of led strip (GPIO23 on ESP32)  

// Sound pins for 3.5 analog connection
#define SOUND_R_PIN 34        // Pin to connect right sound channel (GPIO34 on ESP32)
#define SOUND_L_PIN 35        // Pin to connect left sound channel (GPIO35 on ESP32)

// Music settings
#define IS_MONO 1             // 1 - analyze only right channel (mono), 0 - analyze both left and right channels

void setup()
{
  Serial.begin(115200);
  delay(1000);
}

void loop()
{
  int value = analogRead(34);
  Serial.println(value);
  delay(500);
}
