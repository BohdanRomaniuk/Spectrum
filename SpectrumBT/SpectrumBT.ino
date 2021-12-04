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

// Bluetooth Settings
#define DEVICE_NAME "Spectrum"
#define MAX_COEF 1.8

BluetoothA2DPSink a2dp_sink;
QueueHandle_t queue;
CRGB leds[NUM_LEDS];
byte selectedMode = DEFAULT_MODE;

#define EXP 1.4
int Rlenght, Llenght;
int RcurrentLevel, LcurrentLevel;
float RsoundLevel, RsoundLevel_f;
float LsoundLevel, LsoundLevel_f;
uint16_t LOW_PASS = 100;
int MAX_CH = NUM_LEDS / 2;
float idx = (float)255 / MAX_CH;
float averK = 0.006;
float averageLevel = 50;
float SMOOTH = 0.3;
int maxLevel = 100;
byte count;

DEFINE_GRADIENT_PALETTE(vuPallette) {
  0,    0,    255,  0,
  100,  255,  255,  0,
  150,  255,  100,  0,
  200,  255,  50,   0,
  255,  255,  0,    0
};
CRGBPalette32 vuMeterPallette = vuPallette;

//Rainbow
int hue;
float RAINBOW_STEP = 5.00;
unsigned long rainbow_timer;
//Rainbow

static const i2s_pin_config_t pin_config =
{
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRC,
  .data_out_num = I2S_DOUT,
  .data_in_num = I2S_PIN_NO_CHANGE
};

void vu_meter()
{
  int count = 0;
  for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - Rlenght); --i)
  {
    leds[i] = ColorFromPalette(vuMeterPallette, (count * idx));
    count++;
  }
  count = 0;
  for (int i = MAX_CH; i < (MAX_CH + Llenght); ++i)
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
  int count = 0;
  for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - Rlenght); --i)
  {
    leds[i] = ColorFromPalette(RainbowColors_p, (count * idx) / 2 - hue);
    count++;
  }
  count = 0;
  for (int i = MAX_CH; i < (MAX_CH + Llenght); ++i )
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
    RsoundLevel = 0;
    LsoundLevel = 0;
    int16_t* values = (int16_t*)data;

    for (int i = 0; i < SAMPLES / 2; i += 2)
    {
      RcurrentLevel = values[i];
      RsoundLevel = RcurrentLevel > RsoundLevel ? RcurrentLevel : RsoundLevel;

      if (!IS_MONO)
      {
        LcurrentLevel = values[i + 1];
        LsoundLevel = LcurrentLevel > LsoundLevel ? LcurrentLevel : LsoundLevel;
      }
    }

    //Filter noise
    RsoundLevel = map(RsoundLevel, LOW_PASS, 1023, 0, 500);
    RsoundLevel = constrain(RsoundLevel, 0, 500);
    RsoundLevel = pow(RsoundLevel, EXP);

    if (!IS_MONO)
    {
      LsoundLevel = map(LsoundLevel, LOW_PASS, 1023, 0, 500);
      LsoundLevel = constrain(LsoundLevel, 0, 500);
      LsoundLevel = pow(LsoundLevel, EXP);
    }

    //Filter
    RsoundLevel_f = RsoundLevel * SMOOTH + RsoundLevel_f * (1 - SMOOTH);
    LsoundLevel_f = IS_MONO ? RsoundLevel_f : LsoundLevel * SMOOTH + LsoundLevel_f * (1 - SMOOTH);

    if (RsoundLevel_f > 15 && LsoundLevel_f > 15)
    {
      averageLevel = (float)(RsoundLevel_f + LsoundLevel_f) / 2 * averK + averageLevel * (1 - averK);
      maxLevel = (float)averageLevel * MAX_COEF;

      Rlenght = map(RsoundLevel_f, 0, maxLevel, 0, MAX_CH);
      Llenght = map(LsoundLevel_f, 0, maxLevel, 0, MAX_CH);

      Rlenght = constrain(Rlenght, 0, MAX_CH);
      Llenght = constrain(Llenght, 0, MAX_CH);
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
