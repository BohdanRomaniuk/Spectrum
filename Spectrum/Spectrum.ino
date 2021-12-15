#include <FastLED.h>

// Led settings
#define NUM_LEDS 60           // Number of leds in WS2812B
#define CURRENT_LIMIT 2000    // Power supply current limit 3000 mA = 3 A
#define BRIGHTNESS 30         // Leds brigthtness from range 0-255
#define LED_PIN 12            // Pin to connect Data line of led strip (GPIO23 on ESP32)  

// Sound pins for 3.5 analog connection
#define SOUND_R_PIN 35        // Pin to connect right sound channel (GPIO35 on ESP32)
#define SOUND_L_PIN 34        // Pin to connect left sound channel (GPIO34 on ESP32)

// Music settings
#define IS_MONO 1             // 1 - analyze only right channel (mono), 0 - analyze both left and right channels
#define SAMPLES 100
#define EXP 1.4
#define MAX_COEF 1.8
#define DEFAULT_MODE 0

//LED
CRGB leds[NUM_LEDS];
int MIDDLE = NUM_LEDS / 2;
//LED

//Settings
byte selectedMode = DEFAULT_MODE;
//Settings

//VOLUME ANALYZER
uint16_t LOW_PASS = 100;
float SMOOTH = 0.3;

int rightChannel, leftChannel;
float rFreqFiltered, lFreqFiltered;
float averK = 0.006;
float averageLevel = 50;
int maxLevel = 100;
//VOLUME ANALYZER

//VU meter
float idx = (float)255 / MIDDLE;
DEFINE_GRADIENT_PALETTE(vuPallette) {
  0,    0,    255,  0,
  100,  255,  255,  0,
  150,  255,  100,  0,
  200,  255,  50,   0,
  255,  255,  0,    0
};
CRGBPalette32 vuMeterPallette = vuPallette;
//VU meter

//Rainbow VU meter
int hue;
float RAINBOW_STEP = 5.00;
unsigned long rainbow_timer;
//Rainbow VU meter

void vu_meter()
{
  byte count = 0;
  for (int i = (MIDDLE - 1); i > ((MIDDLE - 1) - rightChannel); --i)
  {
    leds[i] = ColorFromPalette(vuMeterPallette, (count * idx));
    count++;
  }
  count = 0;
  for (int i = MIDDLE; i < (MIDDLE + leftChannel); ++i)
  {
    leds[i] = ColorFromPalette(vuMeterPallette, (count * idx));
    count++;
  }
}

void rainbow_vu_meter()
{
  if (millis() - rainbow_timer > 30)
  {
    rainbow_timer = millis();
    hue = floor((float)hue + RAINBOW_STEP);
  }
  byte count = 0;
  for (int i = (MIDDLE - 1); i > ((MIDDLE - 1) - rightChannel); --i)
  {
    leds[i] = ColorFromPalette(RainbowColors_p, (count * idx) / 2 - hue);
    count++;
  }
  count = 0;
  for (int i = MIDDLE; i < (MIDDLE + leftChannel); ++i )
  {

    leds[i] = ColorFromPalette(RainbowColors_p, (count * idx) / 2 - hue);
    count++;
  }
}

void render()
{
  FastLED.clear();
  switch (selectedMode)
  {
    case 0:
      vu_meter();
      break;
    case 1:
      rainbow_vu_meter();
      break;
  }
  FastLED.show();
}

void analyze_audio()
{
  float rFreq = 0;
  float lFreq = 0;

  for (int i = 0; i < SAMPLES; ++i)
  {
    int16_t rCurrentFreq = analogRead(SOUND_R_PIN);
    rFreq = rCurrentFreq > rFreq ? rCurrentFreq : rFreq;

    if (!IS_MONO)
    {
      int16_t lCurrentFreq = analogRead(SOUND_L_PIN);
      lFreq = lCurrentFreq > lFreq ? lCurrentFreq : lFreq;
    }
  }

  //Filter noise
  rFreq = map(rFreq, LOW_PASS, 1023, 0, 500);
  rFreq = constrain(rFreq, 0, 500);
  rFreq = pow(rFreq, EXP);

  if (!IS_MONO)
  {
    lFreq = map(lFreq, LOW_PASS, 1023, 0, 500);
    lFreq = constrain(lFreq, 0, 500);
    lFreq = pow(lFreq, EXP);
  }

  //Filter
  rFreqFiltered = rFreq * SMOOTH + rFreqFiltered * (1 - SMOOTH);
  lFreqFiltered = IS_MONO ? rFreqFiltered : lFreq * SMOOTH + lFreqFiltered * (1 - SMOOTH);

  if (rFreqFiltered > 15 && lFreqFiltered > 15)
  {
    averageLevel = (float)(rFreqFiltered + lFreqFiltered) / 2 * averK + averageLevel * (1 - averK);
    maxLevel = (float)averageLevel * MAX_COEF;

    rightChannel = map(rFreqFiltered, 0, maxLevel, 0, MIDDLE);
    leftChannel = map(lFreqFiltered, 0, maxLevel, 0, MIDDLE);

    rightChannel = constrain(rightChannel, 0, MIDDLE);
    leftChannel = constrain(leftChannel, 0, MIDDLE);
    render();
  }
}

void setup()
{
  Serial.begin(115200);
  //Serial.setDebugOutput(true);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  if (CURRENT_LIMIT > 0)
  {
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  }
  FastLED.setBrightness(BRIGHTNESS);
}

unsigned long main_timer;
#define MAIN_LOOP 5

void loop()
{
  int rCurrentFreq = analogRead(SOUND_R_PIN);
  Serial.println(rCurrentFreq);
delay(20);
  
//  if (millis() - main_timer > MAIN_LOOP)
//  {
//    analyze_audio();
//
//    main_timer = millis();
//  }
}
