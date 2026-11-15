#include "ButtonTool/ButtonTool.h"
using namespace ButtonTool;
#include "EncoderTool/EncoderTool.h"
using namespace EncoderTool;

#include <avr/io.h>
#include <avr/interrupt.h>

// Encoder Shift Register numbering
#define ENC_TL 2
#define ENC_BL 3
#define ENC_TR 0
#define ENC_BR 1

// Because of the way the four encoders and ten buttons are arranged on three 74HC595 shift registers in parallel, these counts are not the same
constexpr unsigned encoderCount = 8; // There are four arranged over three Shift Registers on the last four positions, hence all eight positions must be shifted to reach them.
constexpr unsigned buttonCount = 12; // There are ten buttons, but arranged over three Shift Registers on the first four positions. The last four are skipped.

// These are Teensy numbering on https://learn.sparkfun.com/tutorials/micromod-teensy-processor-hookup-guide/all
constexpr unsigned srA = 40;   // output pin QH of shift register Enc pin A
constexpr unsigned srB = 41;   // output pin QH of shift register Enc pin B
constexpr unsigned srC = 42;   // output pin QH of shift register Button
constexpr unsigned pinLD = 45; // load pin for all shift registers
constexpr unsigned pinCLK = 6; // clock pin for all shift registers

constexpr unsigned latchPin = 26; // LEDs SR2
constexpr unsigned clockPin = 9;  // LEDs CLK2
constexpr unsigned dataPinG = 44; // Green LEDs data
constexpr unsigned dataPinR = 43; // Red LEDs data

constexpr unsigned pinCD = 27; // Card detect

uint8_t currentRLEDs = 0;
uint8_t currentGLEDs = 0;

EncPlex74165 encoders(encoderCount, pinLD, pinCLK, srB, srA, srC);
ButtonPlex74165 buttons(buttonCount, pinLD, pinCLK, srB, srA, srC);

void ledAnimation(long millis);

constexpr unsigned BACKLIGHT = 24; // A10/I2C_SCL1

typedef enum ledColour
{
  RED,
  GREEN,
  OFF,
} ledColour;

// typedef enum controlParameter
// {
//   control_noiseLevel,
//   control_pitchLfoRate,
//   control_pitchLfoWaveform,
//   control_pitchLfoAmount,
//   control_detune,
//   control_oscMix,
//   control_filterAttack,
//   control_filterDecay,
//   control_pwmAmountA,
//   control_waveformA,
//   control_pitchA,
//   control_pwmAmountB,
//   control_waveformB,
//   control_pitchB,
//   control_pwmRate,
//   control_pitchEnv,
//   control_attack,
//   control_decay,
//   control_sustain,
//   control_release,
//   control_filterLFOAmount,
//   control_FXMix,
//   control_FXAmount,
//   control_glide,
//   control_filterEnv,
//   control_filterRelease,
//   control_filterSustain,
//   control_filterType,
//   control_resonance,
//   control_cutoff,
//   control_filterLFORate,
//   control_filterLFOWaveform,
//   control_volume,
//   control_unison,
//   control_oscFX,
//   control_filterLFORetrig,
//   control_filterLFOMidiClkSync,
// } controlParameter;

const int16_t LED_TO_BIN[9] = {0, 1, 2, 4, 8, 16, 32, 64, 128}; // First value is dummy, LEDs are indexed 1-8

// Shift both red and green LEDs - FOR 595s IN PARALLEL
/*
Red: 2.1V 9mA
Green: 2.8V 1.5mA
*/
FLASHMEM void shiftOutParallel(uint8_t dataPinR, uint8_t dataPinG, uint8_t clockPin, uint8_t bitOrder, uint8_t valR, uint8_t valG)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (bitOrder == LSBFIRST)
    {
      digitalWrite(dataPinR, valR & 1);
      valR >>= 1;
      digitalWrite(dataPinG, valG & 1);
      valG >>= 1;
    }
    else
    {
      digitalWrite(dataPinR, (valR & 128) != 0);
      valR <<= 1;
      digitalWrite(dataPinG, (valG & 128) != 0);
      valG <<= 1;
    }

    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }
}

// ledNo starts at 1
FLASHMEM void singleLED(ledColour color, int8_t ledNo)
{
  digitalWrite(latchPin, LOW);
  switch (color)
  {
  case RED:
    shiftOutParallel(dataPinR, dataPinG, clockPin, MSBFIRST, LED_TO_BIN[ledNo], 0);
    currentRLEDs = ledNo;
    currentGLEDs = 0;
    break;
  case GREEN:
    shiftOutParallel(dataPinR, dataPinG, clockPin, MSBFIRST, 0, LED_TO_BIN[ledNo]);
    currentGLEDs = ledNo;
    currentRLEDs = 0;
    break;
  case OFF:
    shiftOutParallel(dataPinR, dataPinG, clockPin, MSBFIRST, 0, 0);
    currentRLEDs = 0;
    currentGLEDs = 0;
    break;
  }
  digitalWrite(latchPin, HIGH);
}

FLASHMEM void lightRGLEDs(int8_t ledRNos, int8_t ledGNos)
{
  digitalWrite(latchPin, LOW);
  shiftOutParallel(dataPinR, dataPinG, clockPin, MSBFIRST, ledRNos, ledGNos);
  digitalWrite(latchPin, HIGH);
}

FLASHMEM void ledsOff()
{
  lightRGLEDs(0, 0);
  currentRLEDs = 0;
  currentGLEDs = 0;
}

// ledNo starts at 1
FLASHMEM void seqLED(ledColour color, int8_t ledNo)
{
  if (color == RED)
    lightRGLEDs(LED_TO_BIN[ledNo] + LED_TO_BIN[currentRLEDs], LED_TO_BIN[currentGLEDs]);
  else
    lightRGLEDs(LED_TO_BIN[currentRLEDs], LED_TO_BIN[ledNo] + LED_TO_BIN[currentGLEDs]);
}

// Lights an LED for duration ms on top of the current ones. Doesn't wait: serviceLEDFlash()
// (called from loop()) restores the LEDs. It used to delay(), stopping the main loop - and
// MIDI - for 250ms after Panic.
bool ledFlashActive = false;
elapsedMillis ledFlashTime;
uint32_t ledFlashDuration = 0;

FLASHMEM void flashLED(ledColour color, int8_t ledNo, int duration)
{
  if (color == RED)
    lightRGLEDs(LED_TO_BIN[ledNo] + LED_TO_BIN[currentRLEDs], LED_TO_BIN[currentGLEDs]);
  else
    lightRGLEDs(LED_TO_BIN[currentRLEDs], LED_TO_BIN[ledNo] + LED_TO_BIN[currentGLEDs]);
  ledFlashActive = true;
  ledFlashTime = 0;
  ledFlashDuration = duration;
}

FLASHMEM void serviceLEDFlash()
{
  if (ledFlashActive && ledFlashTime >= ledFlashDuration)
  {
    ledFlashActive = false;
    lightRGLEDs(LED_TO_BIN[currentRLEDs], LED_TO_BIN[currentGLEDs]);
  }
}

FLASHMEM void ledAnimation(long millis)
{
  lightRGLEDs(24, 0);
  delay(millis);
  lightRGLEDs(36, 0);
  delay(millis);
  lightRGLEDs(66, 0);
  delay(millis);
  lightRGLEDs(129, 0);
  delay(millis);
  lightRGLEDs(0, 0);
  delay(millis);
  lightRGLEDs(0, 129);
  delay(millis);
  lightRGLEDs(0, 66);
  delay(millis);
  lightRGLEDs(0, 36);
  delay(millis);
  lightRGLEDs(0, 24);
  delay(millis);
  lightRGLEDs(0, 0);
}

FLASHMEM void sdCardInteruptRoutine()
{
  detachInterrupt(pinCD);
  sdCardInterrupt = true;
}

FLASHMEM void setupHardware(EncoderTool::allCallback_t ec, EncoderTool::allBtnCallback_t ebc, ButtonTool::allBtnCallback_t bc)
{
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPinG, OUTPUT);
  pinMode(dataPinR, OUTPUT);

  encoders.begin(CountMode::full);
  encoders.attachCallback(ec);
  encoders.attachBtnCallback(ebc);

  buttons.begin(bc);

  ledAnimation(50);

  pinMode(pinCD, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinCD), sdCardInteruptRoutine, CHANGE);

  // Display backlight - Can be used to turn off or dim using PWM
  // Not used but you never know
  // pinMode(BACKLIGHT, OUTPUT);
}