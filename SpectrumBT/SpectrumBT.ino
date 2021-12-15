#include "src/ESP32-A2DP/src/BluetoothA2DPSink.h"
#include <FastLED.h>

// Audio Settings
#define I2S_DOUT      25
#define I2S_BCLK      26
#define I2S_LRC       22

//Led Settings
#define NUM_LEDS 60
#define DATA_PIN 23
#define CURRENT_LIMIT 3000
#define BRIGHTNESS 30

// FFT Settings
#define IS_MONO 1
#define SAMPLES 512
#define SAMPLING_FREQUENCY 44100

// Music settings
#define DEFAULT_MODE 0
#define EXP 1.4

// Bluetooth Settings
#define DEVICE_NAME "Spectrum"
#define MAX_COEF 1.8

BluetoothA2DPSink a2dp_sink;
QueueHandle_t queue;
CRGB leds[NUM_LEDS];
int MIDDLE = NUM_LEDS / 2;
byte selectedMode = DEFAULT_MODE;

// Volume analyzer
int rightChannel, leftChannel;
float rFreqFiltered, lFreqFiltered;

uint16_t LOW_PASS = 100;
float SMOOTH = 0.3;
float averK = 0.006;
float averageLevel = 50;

int maxLevel = 100;
// Volume analyzer

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

static const i2s_pin_config_t pin_config =
{
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRC,
  .data_out_num = I2S_DOUT,
  .data_in_num = I2S_PIN_NO_CHANGE
};

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

void render(void * parameter)
{
  int item = 0;
  for (;;)
  {
    if (uxQueueMessagesWaiting(queue) > 0)
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
      xQueueReceive(queue, &item, 0);
    }
  }
}

void audio_data_callback(const uint8_t *data, uint32_t len) {
  int item = 0;
  if (uxQueueMessagesWaiting(queue) == 0)
  { 
    float rFreq = 0;
    float lFreq = 0;
    int16_t* values = (int16_t*)data;

    for (int i = 0; i < SAMPLES / 2; i += 2)
    {
      int16_t rCurrentFreq = values[i];
      rFreq = rCurrentFreq > rFreq ? rCurrentFreq : rFreq;

      if (!IS_MONO)
      {
        int16_t lCurrentFreq = values[i + 1];
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
    }

    xQueueSend(queue, &item, portMAX_DELAY);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  if (CURRENT_LIMIT > 0)
  {
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  }
  FastLED.setBrightness(BRIGHTNESS);
  queue = xQueueCreate(1, sizeof(int));
  if (queue == NULL)
  {
    Serial.println("Error creating the queue");
  }

  xTaskCreatePinnedToCore(render, "renderer", 10000, NULL, 1, NULL, 1);
  a2dp_sink.set_pin_config(pin_config);
  a2dp_sink.start((char*)DEVICE_NAME);
  esp_a2d_sink_register_data_callback(audio_data_callback);
}

void loop()
{
  esp_a2d_audio_state_t state = a2dp_sink.get_audio_state();
  switch (state)
  {
    case ESP_A2D_AUDIO_STATE_STOPPED:
      FastLED.clear();
      break;
  }
}
