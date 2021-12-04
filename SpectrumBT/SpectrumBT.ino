#include "BluetoothA2DPSink.h"
#include <FastLED.h>

// Audio Settings
#define I2S_DOUT      25
#define I2S_BCLK      26
#define I2S_LRC       22

//Led Settings
#define NUM_LEDS 60
#define DATA_PIN 23
#define BRIGHTNESS 30

// FFT Settings
#define MONO 1
#define SAMPLES 512
#define SAMPLING_FREQUENCY 44100

// Bluetooth Settings
#define DEVICE_NAME "Spectrum"
#define MAX_COEF 1.8 

BluetoothA2DPSink a2dp_sink;
QueueHandle_t queue;
CRGB leds[NUM_LEDS];

int Rlenght, Llenght;
int RcurrentLevel, LcurrentLevel;
float RsoundLevel, RsoundLevel_f;
float LsoundLevel, LsoundLevel_f;
int MAX_CH = NUM_LEDS / 2;
float ind = (float)255 / MAX_CH;
float averK = 0.006;
float averageLevel = 50;
float SMOOTH = 0.3;
int maxLevel = 100;
byte count;

DEFINE_GRADIENT_PALETTE(soundlevel_gp) {
  0,    0,    255,  0,  // green
  100,  255,  255,  0,  // yellow
  150,  255,  100,  0,  // orange
  200,  255,  50,   0,  // red
  255,  255,  0,    0   // red
};
CRGBPalette32 myPal = soundlevel_gp;

double vReal[SAMPLES];

int16_t sample_l_int;
int16_t sample_r_int;

static const i2s_pin_config_t pin_config = 
{
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRC,
  .data_out_num = I2S_DOUT,
  .data_in_num = I2S_PIN_NO_CHANGE
};

void render(void * parameter)
{
  int item = 0;
  for (;;)
  {
    if (uxQueueMessagesWaiting(queue) > 0)
    {
      FastLED.clear();
      count = 0;
      for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - Rlenght); i--) {
        leds[i] = ColorFromPalette(myPal, (count * ind));
        count++;
      }
      count = 0;
      for (int i = (MAX_CH); i < (MAX_CH + Llenght); i++ ) {
        leds[i] = ColorFromPalette(myPal, (count * ind));
        count++;
      }
      FastLED.show();
      // Release handle
      xQueueReceive(queue, &item, 0);
    }
  }
}

void audio_data_callback(const uint8_t *data, uint32_t len) {
  int item = 0;
  if (uxQueueMessagesWaiting(queue) == 0)
  {
    int byteOffset = 0;

    RsoundLevel = 0;
    LsoundLevel = 0;
    
    for (int i = 0; i < SAMPLES; i++)
    {
      LcurrentLevel = (int16_t)(((*(data + byteOffset + 1) << 8) | *(data + byteOffset)));
      RcurrentLevel = (int16_t)(((*(data + byteOffset + 3) << 8) | *(data + byteOffset + 2)));
      if (RsoundLevel < RcurrentLevel)
      {
        RsoundLevel = RcurrentLevel;
      }
      byteOffset = byteOffset + 4;
    }

    RsoundLevel_f = RsoundLevel * SMOOTH + RsoundLevel_f * (1 - SMOOTH);
    LsoundLevel_f = RsoundLevel_f;

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
  
  FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
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
  
}
