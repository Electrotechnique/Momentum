/*
  https://forum.pjrc.com/threads/64746-Yet-another-highly-optimzed-ILI9341-library-for-T4-4-1-with-diff-updates-and-vsync-!/page2?highlight=ili9341
*/

#ifndef MOMENTUM_ILI9341_DISPLAY_H
#define MOMENTUM_ILI9341_DISPLAY_H

// These are Teensy numbering on https://learn.sparkfun.com/tutorials/micromod-teensy-processor-hookup-guide/all
#define sclk 13  // SCK
#define mosi 11  // MOSI
#define cs 10    // CS - NOT USED
#define dc 5     // D1 - Should use pin 10?
#define rst 4    // D0
#define miso 255 // Unused

// #define SPI_SPEED 100'000'000 Too fast for most displays
#define SPI_SPEED 70'000'000
#define SPICLOCK_READ 80'000'000

#define DISPLAYTIMEOUT 700
#define DISPLAY_FRAME_MS 30    // Minimum time between screen refreshes (~33fps max)
#define DISPLAY_THREAD_TICKS 1 // Display thread time slice in 1ms ticks. Main loop (MIDI) waits at most this long

#include "Voice.h"
#include <utils.h>
#include "Momentum20pt7b.h"
#include "Yeysk12pt7b.h"
#include "Yeysk16pt7b.h"
#include <ili9341_t3n_font_Arial.h>
#include <ili9341_t3n_font_ArialBold.h>

#include <Fonts/FreeMono9pt7b.h>

#define PULSE 1
#define VAR_TRI 2
#define FILTER_ENV 3
#define AMP_ENV 4

const uint8_t patternSpacerArray[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
const uint8_t barSpacerArray[64] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15};
const uint8_t noteArray[12 * 4] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

// https://trolsoft.ru/en/articles/rgb565-color-picker
#define ILI9341_LIGHTBLUE 0x853E
#define ILI9341_DARKRED 0x6000
#define ILI9341_MIDGREY 0xB576
#define ILI9341_DARKERGREY 0x31A6
#define ILI9341_DARKYELLOW 0xA540

ILI9341_t3n tft = ILI9341_t3n(cs, dc, rst, mosi, sclk, miso);

String currentParameter = "";
String currentValue = "";
float currentFloatValue = 0.0;
String currentPgmNum = "";
String currentPatchName = ""; // TODO move to parameters
String newPatchName = "";
uint32_t paramType = 0;

uint32_t peakCount = 0;
uint16_t prevLen = 0;

size_t max_rows = 3;
size_t x_step = 10;
size_t y_step = 10;
size_t x_end = 175;
size_t x_start = x_end - (x_step * ceil(global.maxVoices() / float(max_rows))) + x_step;
size_t y_start = 57;
size_t y_end = 77;
size_t idx = 0;

int16_t xt = 0;
int16_t yt = 0;
uint16_t wt = 0;
uint16_t ht = 0;

const uint32_t colourPriority[5] = {ILI9341_BLACK, ILI9341_BLUE, ILI9341_YELLOW, ILI9341_ORANGE, ILI9341_MAROON};
const uint32_t encTriColour[4] = {ILI9341_LIGHTBLUE, ILI9341_YELLOW, ILI9341_WHITE, ILI9341_ORANGE};

unsigned long timer = 0;
boolean updateDisplay = true;

void startTimer()
{
  timer = millis();
}

void renderKeyboard(uint8_t octave)
{
  uint8_t offset[5] = {9, 8, 11, 9, 8}; //+ is up
  int offcount = 0;
  uint8_t oct_count = 0;
  int ypos = 217;

  // uint8_t patternspacer = 0;
  // uint8_t barspacer = 0;
  uint8_t CHAR_height = 12;
  tft.setTextColor(ILI9341_DARKYELLOW);
  tft.setFont(Arial_11_Bold);
  // draw piano keys
  for (uint8_t y = 0; y < 14; y++)
  {
    tft.fillRect(0, ypos - CHAR_height - (y * 14), 30, 13, ILI9341_WHITE); // pianoroll white key
    if (y == 0 || y == 7 || y == 14)
    {
      tft.setCursor(10, ypos - 11 - (y * 14));
      tft.print("C");
      tft.print(octave - 1 + oct_count);
      oct_count++;
    }
  }
  for (uint8_t y = 0; y < 23; y++)
  {
    if (noteArray[y] == 1)
    {
      tft.fillRect(0, ypos - (y * 8) - offset[offcount], 12, 8, ILI9341_BLACK); // BLACK key
      offcount++;
      if (offcount == 5)
        offcount = 0;
    }
  }
  // draw grid
  for (uint8_t y = 0; y < 24; y++)
  {
    for (uint8_t x = 0; x < 64; x++)
    {
      if (x < currentSequence.length)
      {
        uint16_t colour = ILI9341_DARKGREY;
        if (noteArray[y] == 0)
        {
          colour = ILI9341_MIDGREY;
        }
        if (currentSequence.Notes[x] == (octave - 1 + oct_count) * 12 + y)
        {
          colour = ILI9341_WHITE;
        }
        if (currentSeqNote == (octave - 1 + oct_count) * 12 + y && currentSeqPosition == x)
        {
          colour = ILI9341_YELLOW;
        }
        tft.fillRect(37 + patternSpacerArray[x] + barSpacerArray[x] + x * 4, ypos + 3 - CHAR_height - (y * 8), 3, 6, colour); // GRID white key
      }
      else
      {
        tft.fillRect(37 + patternSpacerArray[x] + barSpacerArray[x] + x * 4, ypos + 3 - CHAR_height - (y * 8), 3, 6, ILI9341_DARKERGREY); // Unused keys
      }
    }
  }
  if (currentSequence.running)
  {
    tft.drawFastVLine(37 + patternSpacerArray[currentSequence.step] + barSpacerArray[currentSequence.step] +
                          (currentSequence.step * 4),
                      21, 197, ILI9341_ORANGE);
  }
}

FLASHMEM void renderBootUpPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.drawRect(10, 40, 134, 26, ILI9341_WHITE);
  tft.fillRect(144, 40, 155, 26, ILI9341_WHITE);
  tft.setCursor(13, 42);
  tft.setFont(&Yeysk12pt7b);
  tft.setTextColor(ILI9341_WHITE);
  tft.println(F("ELECTRO"));
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(145, 48);
  tft.println(F("TECHNIQUE"));
  tft.setCursor(90, 82);
  tft.setFont(&Yeysk16pt7b);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("MOMENTUM"));
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_16);
  tft.setCursor(140, 120);
  tft.println(VERSION);
}

FLASHMEM void renderMIDI()
{
  tft.setFont(Arial_12);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.drawString(F("MIDI In: "), 40, 145);
  tft.setTextColor(ILI9341_GREENYELLOW);
  if (midiChannel == MIDI_CHANNEL_OMNI)
  {
    tft.print(F("All"));
  }
  else
  {
    tft.print(midiChannel);
  }

  tft.setTextColor(ILI9341_GREENYELLOW);
  if (midiOutCh == 0)
  {
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print(F("  MIDI Out: "));
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.print(F("Off"));
    return;
  }

  tft.setTextColor(ILI9341_LIGHTGREY);
  switch (MIDIThru)
  {
  case midi::Thru::SameChannel:
    tft.print(F("  MIDI Thru: "));
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.print(F("Same Ch "));
    tft.print(midiChannel);
    break;
  case midi::Thru::DifferentChannel:
    tft.print(F("  MIDI Thru: "));
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.print(F("Dif. Ch "));
    tft.print(midiOutCh);
    break;
  case midi::Thru::Full:
    tft.print(F("  MIDI Thru: "));
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.print(F("All"));
    break;
  default:
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print(F("  MIDI Out: "));
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.print(midiOutCh);
    break;
  }
}

FLASHMEM void renderMidiClk()
{
  if (MIDIClkSignal)
  {
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setFont(Arial_10_Bold);
    tft.fillRect(94, 57, 30, 15, ILI9341_LIGHTGREY);
    tft.drawString(F("CLK"), 109, 59);
  }
}

FLASHMEM void renderKeyboardIndicator()
{
  if (keyboardActive)
  {
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setFont(Arial_10_Bold);
    tft.fillRect(204, 57, 30, 15, ILI9341_WHITE);
    tft.drawString(F("KEY"), 219, 59);
  }
}

FLASHMEM void renderArpIndicator()
{
  if (arpRunning)
  {
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setFont(Arial_10_Bold);
    tft.fillRect(94, 73, 30, 15, ILI9341_DARKGREEN);
    tft.drawString(F("ARP"), 109, 76);
  }
}

FLASHMEM void renderSeqIndicator()
{
  if (currentSequence.running)
  {
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setFont(Arial_10_Bold);
    tft.fillRect(94, 73, 30, 15, ILI9341_DARKYELLOW);
    tft.drawString(F("SEQ"), 109, 76);
  }
}

FLASHMEM void renderPerfIndicator()
{
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setFont(Arial_10_Bold);
  tft.fillRect(204, 73, 30, 15, ILI9341_PINK);
  tft.drawString(F("PER"), 219, 76);
}

FLASHMEM void renderPageIndicator(uint8_t pages, uint8_t current)
{
  if (pages < 2)
    return;
  for (uint8_t i = 0; i < pages; i++)
  {
    if (i + 1 == current)
    {
      tft.fillRect(130 + (i * 23) - (pages * pages), 175, 20, 7, ILI9341_BLUE);
    }
    else
    {
      tft.drawRect(130 + (i * 23) - (pages * pages), 175, 20, 7, ILI9341_BLUE);
    }
  }
}

// Prints sysmbols for waveforms from custom font
FLASHMEM void renderWaveformSymbol(uint8_t waveform)
{

  tft.setFont(&Momentum20pt7b);
  tft.setTextSize(1);
  tft.setCursor(6, 165);
  switch (waveform)
  {
  case WAVEFORM_TRIANGLE_VARIABLE:
    tft.println("A");
    break;
  case WAVEFORM_BANDLIMIT_PULSE:
    tft.println("B");
    break;
  case WAVEFORM_BANDLIMIT_SAWTOOTH:
  case WAVEFORM_SAWTOOTH:
    tft.println("J");
    break;
  case WAVEFORM_BANDLIMIT_SQUARE:
  case WAVEFORM_SQUARE:
    tft.println("D");
    break;
  case WAVEFORM_TRIANGLE:
    tft.println("E");
    break;
  case WAVEFORM_SAMPLE_HOLD:
    tft.println("F");
    break;
  case WAVEFORM_PARABOLIC:
    tft.println("G");
    break;
  case WAVEFORM_HARMONIC:
    tft.println("H");
    break;
  case WAVEFORM_SINE:
    tft.println("I");
    break;
  case WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE:
  case WAVEFORM_SAWTOOTH_REVERSE:
    tft.println("C");
    break;
  }
}

FLASHMEM void renderCorners()
{
  // ENC_TL
  if (encMap[ENC_TL].active)
  {
    tft.fillTriangle(0, 0, 0, 14, 14, 0, encTriColour[ENC_TL]);
    // if (encMap[ENC_TL].Push)
    // {
    //   tft.fillTriangle(2, 2, 2, 10, 10, 2, ILI9341_BLACK);
    // }
    tft.setTextColor(encTriColour[ENC_TL]);
    tft.setTextDatum(TL_DATUM);
    tft.setFont(Arial_13);
    tft.drawString(encMap[ENC_TL].ParameterStr, 14, 4);
    if (encMap[ENC_TL].ShowValue)
    {
      tft.setFont(Arial_16);
      tft.drawString(encMap[ENC_TL].ValueStr, 4, 27);
    }
  }
  // ENC_TR
  if (encMap[ENC_TR].active)
  {
    tft.fillTriangle(305, 0, 319, 0, 319, 14, encTriColour[ENC_TR]);
    // if (encMap[ENC_TR].Push)
    // {
    //   tft.fillTriangle(309, 2, 317, 2, 317, 10, ILI9341_BLACK);
    // }
    tft.setTextColor(encTriColour[ENC_TR]);
    tft.setTextDatum(TR_DATUM);
    tft.setFont(Arial_13);
    tft.drawString(encMap[ENC_TR].ParameterStr, 303, 4);
    if (encMap[ENC_TR].ShowValue)
    {
      tft.setFont(Arial_16);
      tft.drawString(encMap[ENC_TR].ValueStr, 310, 27);
    }
  }
  // ENC_BR
  if (encMap[ENC_BR].active)
  {
    tft.fillTriangle(305, 239, 319, 239, 319, 225, encTriColour[ENC_BR]);
    // if (encMap[ENC_BR].Push)
    // {
    //   tft.fillTriangle(309, 237, 317, 237, 317, 229, ILI9341_BLACK);
    // }
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(encTriColour[ENC_BR]);
    tft.setFont(Arial_13);
    tft.drawString(encMap[ENC_BR].ParameterStr, 303, 225);
    if (encMap[ENC_BR].ShowValue)
    {
      tft.setFont(Arial_16);
      tft.drawString(encMap[ENC_BR].ValueStr, 310, 196);
    }
  }

  // ENC_BL
  if (encMap[ENC_BL].active)
  {
    tft.fillTriangle(0, 225, 0, 239, 14, 239, encTriColour[ENC_BL]);
    // if (encMap[ENC_BL].Push)
    // {
    //   tft.fillTriangle(2, 229, 2, 237, 10, 237, ILI9341_BLACK);
    // }
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(encTriColour[ENC_BL]);
    tft.setFont(Arial_13);
    tft.drawString(encMap[ENC_BL].ParameterStr, 15, 225);
    if (encMap[ENC_BL].ShowValue)
    {
      tft.setFont(Arial_16);
      tft.drawString(encMap[ENC_BL].ValueStr, 4, 196);
      if (encMap[ENC_BL].Parameter == CCoscwaveformA || encMap[ENC_BL].Parameter == CCoscwaveformB)
      {
        renderWaveformSymbol(encMap[ENC_BL].Parameter == CCoscwaveformA ? getWaveformA(encMap[ENC_BL].Value) : getWaveformB(encMap[ENC_BL].Value));
      }
      else if (encMap[ENC_BL].Parameter == CCoscLfoWaveform || encMap[ENC_BL].Parameter == CCfilterlfowaveform)
      {
        renderWaveformSymbol(getLFOWaveform(encMap[ENC_BL].Value));
      }
    }
  }
}

FLASHMEM void renderPeak()
{
  // vuMeter = true;
  if (vuMeter && global.peak.available())
  {
    uint16_t len = 0;
    if (peakCount > 1)
    {
      len = (int)(global.peak.read() * 200.0f);
      prevLen = len;
      peakCount = 0;
    }
    else
    {
      len = prevLen;
      peakCount++;
    }
    tft.drawFastVLine(314, 200 - len, len, len > 180 ? ILI9341_RED : ILI9341_WHITE);
    tft.drawFastVLine(315, 200 - len, len, len > 180 ? ILI9341_RED : ILI9341_WHITE);
    tft.drawFastVLine(316, 200 - len, len, len > 180 ? ILI9341_RED : ILI9341_WHITE);
  }
}

FLASHMEM void renderVoiceGrid()
{
  size_t fillColour[global.maxVoices()] = {};
  size_t borderColour[global.maxVoices()] = {};
  // Select colours based on voice state.
  size_t i = 0;
  for (size_t group = 0; group < groupvec.size(); group++)
  {
    for (size_t voice = 0; voice < groupvec[group]->size(); voice++)
    {
      borderColour[i] = group + 1;
      if ((*groupvec[group])[voice]->on())
        fillColour[i] = (*groupvec[group])[voice]->noteId() + 1;
      else
        fillColour[i] = 0;
      i++;
    }
  }

  // Draw rectangles to represent each voice.
  idx = 0;
  for (size_t y = y_start; y <= y_end; y += y_step)
  {
    for (size_t x = x_start; x <= x_end; x += x_step)
    {
      // Always draw border to indicate timbre.
      tft.fillRect(x, y, 8, 8, colourPriority[fillColour[idx]]);
      tft.drawRect(x, y, 8, 8, colourPriority[borderColour[idx]]);
      idx++;
      if (idx >= global.maxVoices())
      {
        break;
      }
    }
  }
}

FLASHMEM void setPatchNoAndNameForDisp(String number, String patchName)
{
  currentPgmNum = number;
  currentPatchName = patchName;
}

FLASHMEM void renderPulseWidth(float value)
{
  const int16_t a = 216;
  const int16_t b = 148;
  const int16_t c = 246;
  const int16_t d = 26;
  const int16_t e = 40;
  const int16_t f = 188;
  const int16_t g = 32;
  const int16_t h = 42;
  const int16_t i = 276;
  const int16_t j = 30;

  tft.drawFastHLine(a, b, j + (value * d), ILI9341_CYAN);
  tft.drawFastVLine(c + (value * d), b, e, ILI9341_CYAN);
  tft.drawFastHLine(c + (value * d), f, g - (value * d), ILI9341_CYAN);
  if (value < 0)
  {
    tft.drawFastVLine(a, b, h, ILI9341_CYAN);
  }
  else
  {
    tft.drawFastVLine(i, b, h, ILI9341_CYAN);
  }
  tft.setOrigin(0, 0);
}

FLASHMEM void renderVarTriangle(float value)
{
  const int16_t a = 220; // x1
  const int16_t b = 192; // y1
  const int16_t c = 196; // x2
  const int16_t d = 24;  // factor
  const int16_t e = 172; // x2
  const int16_t f = 150; // y2

  tft.drawLine(a, b, c + (value * d), f, ILI9341_CYAN);
  tft.drawLine(c + (value * d), f, e, b, ILI9341_CYAN);
  tft.setOrigin(0, 0);
}

FLASHMEM void renderEnv(float att, float dec, float sus, float rel)
{
  const int16_t a = 200;
  const int16_t b = 188;
  const int16_t c = 30;
  const int16_t d = 278;
  const int16_t e = 40;
  const int16_t f = 148;
  const int16_t g = 80;
  const int16_t h = 26;

  tft.drawLine(a, b, a + (att * c), f, ILI9341_CYAN);                                           // attack
  tft.drawLine(a + (att * c), f, a + ((att + dec) * c), b - (sus * e), ILI9341_CYAN);           // decay
  tft.drawFastHLine(a + ((att + dec) * c), b - (sus * e), g - ((att + dec) * c), ILI9341_CYAN); // sustain
  tft.drawLine(d, b - (sus * e), d + (rel * h), b, ILI9341_CYAN);                               // release
  tft.setOrigin(0, 0);
}

void renderCharacterChooser()
{
  tft.setTextSize(2);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(ILI9341_MIDGREY);
  int8_t row = 0;
  int8_t start = 0;
  for (int8_t i = 0; i < TOTALCHARS; i++)
  {
    i % 20 != 0 ? row : row++;
    i % 20 != 0 ? start++ : start = 0;
    if (charCursor == i)
    {
      tft.setTextColor(ILI9341_BLACK);
      tft.getTextBounds('W', 19 + start * 15, 90 + row * 22, &xt, &yt, &wt, &ht);
      tft.fillRect(xt - 2, yt - 2, wt - 8, ht - 3, ILI9341_WHITE);
    }
    else
    {
      tft.setTextColor(ILI9341_MIDGREY);
    }

    if (i == 26 || i == 53)
    {
      tft.drawString('_', 19 + start * 15, 90 + row * 22);
    }
    else
    {
      tft.drawString(CHARACTERS[i], 19 + start * 15, 90 + row * 22);
    }
  }
}

FLASHMEM void renderBankList()
{
  // Bank list - 8 banks
  tft.setFont(Arial_12_Bold);
  tft.setTextColor(ILI9341_RED);
  for (size_t i = 0; i < 8; i++)
  {
    String bankStr;
    if (bankNames[i].length() > 9)
    {
      bankStr = bankNames[i].substring(0, 8) + String("..");
    }
    else
    {
      bankStr = bankNames[i];
    }
    if (tempBankIndex == i)
    {
      tft.setTextColor(ILI9341_BLACK);
      tft.getTextBounds(bankStr, 2, 57 + (20 * i), &xt, &yt, &wt, &ht);
      tft.fillRect(0, yt - 1, wt + 2, ht + 2, ILI9341_ORANGE);
    }
    else
    {
      tft.setTextColor(ILI9341_RED);
    }
    tft.drawString(bankStr, 2, 57 + (20 * i));
  }
}

FLASHMEM void renderCurrentParameterOverlay()
{
  tft.fillRect(40, 30, 230, 150, ILI9341_DARKRED);
  tft.drawRect(40, 30, 230, 150, ILI9341_RED);

  tft.setFont(Arial_12_Bold);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(currentParameter, 60, 70);

  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_WHITE);
  tft.drawString(currentValue, 60, 120);
  switch (paramType)
  {
  case PULSE:
    tft.setOrigin(-40, -30);
    renderPulseWidth(currentFloatValue);
    break;
  case VAR_TRI:
    tft.setOrigin(-10, -30);
    renderVarTriangle(currentFloatValue);
    break;
  case FILTER_ENV:
    tft.setOrigin(-40, -30);
    renderEnv(groupvec[activeGroupIndex]->getFilterAttack() * 0.0001f, groupvec[activeGroupIndex]->getFilterDecay() * 0.0001f, groupvec[activeGroupIndex]->getFilterSustain(), groupvec[activeGroupIndex]->getFilterRelease() * 0.0001f);
    break;
  case AMP_ENV:
    tft.setOrigin(-40, -30);
    renderEnv(groupvec[activeGroupIndex]->getAmpAttack() * 0.0001f, groupvec[activeGroupIndex]->getAmpDecay() * 0.0001f, groupvec[activeGroupIndex]->getAmpSustain(), groupvec[activeGroupIndex]->getAmpRelease() * 0.0001f);
    break;
  }
}

FLASHMEM void renderCharactersInd(uint8_t currentlength, uint8_t maxlength)
{
  tft.setTextDatum(BL_DATUM);
  tft.setFont(Arial_13);
  tft.setTextColor(encTriColour[ENC_BL]);
  tft.drawString(String(currentlength) + "/" + String(maxlength), 110, 225);
}

FLASHMEM void renderPatchNameAndBank()
{
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TL_DATUM);
  switch (currentPatchIndex)
  {
  case 0 ... 8:
    tft.drawString("00" + String(currentPatchIndex + 1), 45, 94);
    break;
  case 9 ... 98:
    tft.drawString("0" + String(currentPatchIndex + 1), 45, 94);
    break;
  default:
    tft.drawString(String(currentPatchIndex + 1), 45, 94);
  }
  currentPatchName.length() > 12
      ? tft.setFont(Arial_12)
      : tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(currentPatchName, 45, 120);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setTextDatum(TL_DATUM);
  tft.setFont(Arial_16);
  if (cardStatus)
  {
    tft.drawString(bankNames[currentBankIndex], 90, 94);
  }
  else
  {
    tft.drawString(F("NO SD CARD"), 90, 94);
  }
}

FLASHMEM void renderSeqTempo()
{
  tft.setFont(Arial_13);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(encMap[ENC_TL].ValueStr, 80, 5); // Tempo is TL
}

FLASHMEM void renderSeqLength()
{
  tft.setFont(Arial_13);
  tft.setTextColor(ILI9341_LIGHTBLUE);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(SEQPOSSTR[currentSequence.length - 1] + "-" + String(currentSequence.length), 242, 5);
}

FLASHMEM void renderSeqNote()
{
  tft.setFont(Arial_13);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(NOTENAME[currentSeqNote], 250, 225);
}

FLASHMEM void renderSeqPosition()
{
  tft.setFont(Arial_13);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setTextDatum(BL_DATUM);
  tft.drawString(SEQPOSSTR[currentSeqPosition] + "-" + String(currentSeqPosition + 1), 85, 225);
}

FLASHMEM void renderDeletePatchPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16_Bold);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Delete Patch"), 160, 26);
  tft.drawFastHLine(10, 48, tft.width() - 20, ILI9341_RED);

  renderBankList();

  // Patches -  up to 128 in each bank
  tft.setTextDatum(TL_DATUM);
  tft.setFont(Arial_12_Bold);
  tft.setTextColor(ILI9341_LIGHTGREY);
  size_t offset = ceil(currentPatchIndex / 8);

  for (size_t i = 0; i < min(patches.size(), 8); i++)
  {
    if (i + 1 + (8 * offset) <= patches.size())
    {
      String patchStr = String(i + 1 + (8 * offset)) + " " + patches[i + (8 * offset)].patchName;
      if (currentPatchIndex == i + (8 * offset))
      {
        tft.setTextColor(ILI9341_BLACK);
        tft.getTextBounds(patchStr, 110, 57 + (20 * i), &xt, &yt, &wt, &ht);
        tft.fillRect(xt - 2, yt - 1, wt + 4, ht + 2, ILI9341_YELLOW);
      }
      else
      {
        tft.setTextColor(ILI9341_DARKYELLOW);
      }
      tft.drawString(patchStr, 110, 57 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderDeleteMessagePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(40, 30, 230, 150, ILI9341_DARKRED);
  tft.drawRect(40, 30, 230, 150, ILI9341_RED);
  tft.setFont(Arial_16);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(F("Deleting Patch"), 160, 100);
  tft.drawString(currentPatch.PatchName, 160, 125);
}

FLASHMEM void renderDeleteBankMessagePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(40, 30, 230, 150, ILI9341_DARKRED);
  tft.drawRect(40, 30, 230, 150, ILI9341_RED);
  tft.setFont(Arial_16);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(F("Deleting Bank"), 160, 100);
  tft.drawString(bankNames[tempBankIndex], 160, 125);
}

FLASHMEM void renderSavePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(40, 30, 230, 150, ILI9341_DARKRED);
  tft.drawRect(40, 30, 230, 150, ILI9341_RED);
  tft.setFont(Arial_16);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  if (patches[currentPatchIndex].patchUID == 0)
    tft.drawString(F("Saving Patch"), 160, 100);
  else
    tft.drawString(F("Overwriting Patch"), 160, 100);
  tft.setTextColor(ILI9341_WHITE);
  if (strlen(currentPatch.PatchName) > 12)
    tft.drawString(String(currentPatch.PatchName).substring(0, 11) + String(".."), 160, 125);
  else
    tft.drawString(currentPatch.PatchName, 160, 125);
}

FLASHMEM void renderReinitialisePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.fillRect(40, 30, 230, 150, ILI9341_DARKRED);
  tft.drawRect(40, 30, 230, 150, ILI9341_RED);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Initialise"), 160, 80);
  tft.drawString(F("Patch Settings"), 160, 105);
}

FLASHMEM void renderPatchSavingPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16_Bold);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Save Patch"), 160, 26);
  tft.drawFastHLine(10, 48, tft.width() - 20, ILI9341_RED);

  renderBankList();

  // Patches -  up to 128 in each bank
  tft.setTextDatum(TL_DATUM);
  tft.setFont(Arial_12_Bold);
  tft.setTextColor(ILI9341_LIGHTGREY);
  size_t offset = ceil(currentPatchIndex / 8);
  for (size_t i = 0; i < min(patches.size(), 8); i++)
  {
    if (i + 1 + (8 * offset) <= patches.size())
    {
      String patchStr = String(i + 1 + (8 * offset)) + " " + patches[i + (8 * offset)].patchName;
      if (currentPatchIndex == i + (8 * offset))
      {
        tft.setTextColor(ILI9341_BLACK);
        tft.getTextBounds(patchStr, 110, 57 + (20 * i), &xt, &yt, &wt, &ht);
        tft.fillRect(xt - 2, yt - 1, wt + 4, ht + 2, ILI9341_YELLOW);
      }
      else
      {
        tft.setTextColor(ILI9341_DARKYELLOW);
      }
      tft.drawString(patchStr, 110, 57 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderPatchNamingPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Patch Name"), 160, 19);
  tft.drawFastHLine(10, 42, tft.width() - 20, ILI9341_RED);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(19, 70);
  if (currentPatchName.length() > 0)
  {
    for (uint8_t i = 0; i < currentPatchName.length(); i++)
    {
      if (i == nameCursor)
      {
        tft.setTextColor(ILI9341_ORANGE);
        currentPatchName.charAt(i) == 32 ? tft.drawFontChar(95) : tft.drawFontChar(currentPatchName.charAt(i)); // Show selected space as underscore
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
        tft.drawFontChar(currentPatchName.charAt(i));
      }
    }
  }
  else
  {
    tft.drawFontChar(95); // Underscore to show start of empty patchname
  }

  renderCharacterChooser();
  renderCorners();
  renderCharactersInd(currentPatchName.length(), PATCHNAMEMAXLEN);
}

FLASHMEM void renderBankNamingPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Bank Name"), 160, 19);
  tft.drawFastHLine(10, 42, tft.width() - 20, ILI9341_RED);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(19, 70);

  if (bankNames[tempBankIndex].length() > 0)
  {
    for (uint8_t i = 0; i < bankNames[tempBankIndex].length(); i++)
    {
      if (i == nameCursor)
      {
        tft.setTextColor(ILI9341_ORANGE);
        bankNames[tempBankIndex].charAt(i) == 32 ? tft.drawFontChar(95) : tft.drawFontChar(bankNames[tempBankIndex].charAt(i)); // Show selected space as underscore
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
        tft.drawFontChar(bankNames[tempBankIndex].charAt(i));
      }
    }
  }
  else
  {
    tft.drawFontChar(95); // Underscore to show start of empty bank name
  }

  tft.setTextSize(2);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(ILI9341_MIDGREY);
  int row = 0;
  int start = 0;
  for (int i = 0; i < TOTALCHARS; i++)
  {
    i % 20 != 0 ? row : row++;
    i % 20 != 0 ? start++ : start = 0;
    charCursor == i ? tft.setTextColor(ILI9341_WHITE) : tft.setTextColor(ILI9341_MIDGREY);
    if (i == 26 || i == 53)
    {
      tft.drawString('_', 19 + start * 15, 90 + row * 22);
    }
    else
    {
      tft.drawString(CHARACTERS[i], 19 + start * 15, 90 + row * 22);
    }
  }
  renderCorners();
  renderCharactersInd(bankNames[tempBankIndex].length(), BANKNAMEMAXLEN);
}

FLASHMEM void renderSequenceNamingPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Sequence Name"), 160, 26);
  tft.drawFastHLine(10, 49, tft.width() - 20, ILI9341_RED);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(19, 70);
  if (currentSequence.SequenceName.length() > 0)
  {
    for (uint8_t i = 0; i < currentSequence.SequenceName.length(); i++)
    {
      if (i == nameCursor)
      {
        tft.setTextColor(ILI9341_ORANGE);
        currentSequence.SequenceName.charAt(i) == 32 ? tft.drawFontChar(95) : tft.drawFontChar(currentSequence.SequenceName.charAt(i)); // Show selected space as underscore
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
        tft.drawFontChar(currentSequence.SequenceName.charAt(i));
      }
    }
  }
  else
  {
    tft.drawFontChar(95); // Underscore to show start of empty performance name
  }
  renderCharacterChooser();
  renderCorners();
  renderCharactersInd(currentSequence.SequenceName.length(), SEQUENCENAMEMAXLEN);
}

FLASHMEM void renderPerformanceNamingPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(F("Performance Name"), 160, 26);
  tft.drawFastHLine(10, 49, tft.width() - 20, ILI9341_RED);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(19, 70);
  if (currentPerformance.performanceName.length() > 0)
  {
    for (uint8_t i = 0; i < currentPerformance.performanceName.length(); i++)
    {
      if (i == nameCursor)
      {
        tft.setTextColor(ILI9341_ORANGE);
        currentPerformance.performanceName.charAt(i) == 32 ? tft.drawFontChar(95) : tft.drawFontChar(currentPerformance.performanceName.charAt(i)); // Show selected space as underscore
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
        tft.drawFontChar(currentPerformance.performanceName.charAt(i));
      }
    }
  }
  else
  {
    tft.drawFontChar(95); // Underscore to show start of empty performance name
  }

  renderCharacterChooser();
  renderCorners();
  renderCharactersInd(currentPerformance.performanceName.length(), PERFORMANCENAMEMAXLEN);
}

FLASHMEM void renderPatchRecallPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Recall Patch", 160, 26);
  tft.drawFastHLine(10, 48, tft.width() - 20, ILI9341_RED);

  renderBankList();

  // Patches -  up to 128 in each bank
  tft.setFont(Arial_12_Bold);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  size_t offset = ceil(currentPatchIndex / 8);
  for (size_t i = 0; i < min(patches.size(), 8); i++)
  {
    if (i + 1 + (8 * offset) <= patches.size())
    {
      String patchStr = String(i + 1 + (8 * offset)) + " " + patches[i + (8 * offset)].patchName;
      if (currentPatchIndex == i + (8 * offset))
      {
        tft.setTextColor(ILI9341_BLACK);
        tft.getTextBounds(patchStr, 110, 57 + (20 * i), &xt, &yt, &wt, &ht);
        tft.fillRect(xt - 2, yt - 1, wt + 4, ht + 2, ILI9341_YELLOW);
      }
      else
      {
        tft.setTextColor(ILI9341_DARKYELLOW);
      }
      tft.drawString(patchStr, 110, 57 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderOscPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  switch (no)
  {
  case 1:
  case 2:
    tft.drawString("Oscillator " + String(no), 160, 150);
    break;
  case 3:
    tft.drawString("Noise & Unison", 160, 150);
    break;
  case 4:
    tft.drawString("Oscillator Effects", 160, 150);
    break;
  }
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderPageIndicator(4, no);
  renderCorners();
}

FLASHMEM void renderOscModPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  switch (no)
  {
  case 1:
    tft.drawString("Osc 1 PWM", 160, 150);
    renderWaveformSymbol(groupvec[activeGroupIndex]->getWaveformA());
    break;
  case 2:
    tft.drawString("Osc 2 PWM", 160, 150);
    renderWaveformSymbol(groupvec[activeGroupIndex]->getWaveformB());
    break;
  case 3:
    tft.drawString("Osc Pitch", 160, 150);
    renderWaveformSymbol(groupvec[activeGroupIndex]->getPitchLfoWaveform());
    break;
  case 4:
    tft.drawString("Osc Pitch", 160, 150);
    break;
  }
  renderPageIndicator(4, no);
  if (no == 1 && currentPatch.PWMSourceA == PWMSOURCEMANUAL)
  {
    if (groupvec[activeGroupIndex]->getWaveformA() == WAVEFORM_BANDLIMIT_PULSE)
    {
      tft.setOrigin(-90, -50);
      renderPulseWidth(groupvec[activeGroupIndex]->getPwA());
    }
    else
    {
      tft.setOrigin(-30, -55);
      renderVarTriangle(groupvec[activeGroupIndex]->getPwA());
    }
  }
  else if (no == 2 && currentPatch.PWMSourceB == PWMSOURCEMANUAL)
  {
    if (groupvec[activeGroupIndex]->getWaveformB() == WAVEFORM_BANDLIMIT_PULSE)
    {
      tft.setOrigin(-90, -50);
      renderPulseWidth(groupvec[activeGroupIndex]->getPwB());
    }
    else
    {
      tft.setOrigin(-30, -55);
      renderVarTriangle(groupvec[activeGroupIndex]->getPwB());
    }
  }
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderCorners();
}

FLASHMEM void renderFilterPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  renderPageIndicator(2, no);
  if (no == 2)
  {
    tft.drawString("Filter Envelope", 160, 150);
    tft.setOrigin(-80, -55);
    renderEnv(groupvec[activeGroupIndex]->getFilterAttack() * 0.0001f,
              groupvec[activeGroupIndex]->getFilterDecay() * 0.0001f,
              groupvec[activeGroupIndex]->getFilterSustain(),
              groupvec[activeGroupIndex]->getFilterRelease() * 0.0001f);
  }
  else
  {
    tft.drawString("Filter", 160, 150);
  }
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderCorners();
}

FLASHMEM void renderFilterModPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Filter Modulation", 160, 150);
  renderPageIndicator(3, no);
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderCorners();
}

FLASHMEM void renderAmpPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Amplifier", 160, 150);
  renderPageIndicator(3, no);
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderCorners();
  if (no == 1)
  {
    tft.setOrigin(-80, -55);
    renderEnv(groupvec[activeGroupIndex]->getAmpAttack() * 0.0001f,
              groupvec[activeGroupIndex]->getAmpDecay() * 0.0001f,
              groupvec[activeGroupIndex]->getAmpSustain(),
              groupvec[activeGroupIndex]->getAmpRelease() * 0.0001f);
  }
}

FLASHMEM void renderFXPage(size_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  switch (no)
  {
  case 1:
    tft.drawString("FX - Stereo Ensemble", 160, 150);
    break;
  case 2:
    tft.drawString("FX - Reverb", 160, 150);
    break;
  }
  renderPageIndicator(2, no);
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderCorners();
}

FLASHMEM void renderSeqRecIndicator()
{
  if (currentSequence.recording)
  {
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setFont(Arial_10_Bold);
    tft.fillRect(142, 3, 30, 15, ILI9341_RED);
    tft.drawString(F("REC"), 157, 5);
  }
}

FLASHMEM void renderSequencerEditPage()
{
  tft.fillScreen(ILI9341_BLACK);
  renderCorners();
  renderKeyboard(seqCurrentOctPos);
  renderSeqTempo();
  renderSeqLength();
  renderSeqRecIndicator();
  renderSeqNote();
  renderSeqPosition();
}

FLASHMEM void renderSequencerPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(currentSequence.SequenceName, 160, 19);
  renderPatchNameAndBank();
  renderMidiClk();
  renderSeqIndicator();
  renderKeyboardIndicator();
  renderPeak();
  renderCorners();
  renderMIDI();
  renderVoiceGrid();
}

FLASHMEM void renderSequenceRecallPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Recall Sequence", 160, 26);
  tft.drawFastHLine(10, 48, tft.width() - 20, ILI9341_RED);

  tft.setFont(Arial_12);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_RED);
  String sequenceStr;

  size_t offset = ceil(currentSequenceIndex / 8);

  for (size_t i = 0; i < min(SEQUENCES_LIMIT, 8); i++)
  {
    if (i + 1 + (8 * offset) <= SEQUENCES_LIMIT)
    {
      switch (i + 1 + (8 * offset))
      {
      case 0 ... 9:
        sequenceStr = "00" + String(i + 1 + (8 * offset)) + " " + sequences[i + (8 * offset)];
        break;
      case 10 ... 99:
        sequenceStr = "0" + String(i + 1 + (8 * offset)) + " " + sequences[i + (8 * offset)];
        break;
      default:
        sequenceStr = String(i + 1 + (8 * offset)) + " " + sequences[i + (8 * offset)];
      }
      if (currentSequenceIndex == i + (8 * offset))
      {

        tft.setTextColor(ILI9341_BLACK);
        tft.getTextBounds(sequenceStr, 2, 56 + (20 * i), &xt, &yt, &wt, &ht);
        tft.fillRect(0, yt - 1, wt + 4, ht + 4, ILI9341_ORANGE);
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
      }

      tft.drawString(sequenceStr, 2, 56 + (20 * i));
    }
  }

  renderCorners();
}

FLASHMEM void renderMIDIPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("MIDI", 160, 150);
  renderCorners();
}

FLASHMEM void renderArpPage(uint8_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  renderPatchNameAndBank();
  renderMidiClk();
  renderArpIndicator();
  renderKeyboardIndicator();
  renderPeak();
  renderCorners();
  renderMIDI();
  renderVoiceGrid();
  renderPageIndicator(2, no);
}

FLASHMEM void renderMainPage()
{
  tft.fillScreen(ILI9341_BLACK);
  renderPatchNameAndBank();
  renderMidiClk();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderKeyboardIndicator();
  renderPeak();
  renderCorners();
  renderMIDI();
  renderVoiceGrid();
}

FLASHMEM void renderPerformanceName()
{
  tft.setFont(Arial_12);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ILI9341_RED);
  tft.drawString(currentPerformance.performanceName, 160, 110);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_DARKYELLOW);
  // THIS ASSUMES JUST ONE PATCH - MULTITIMBRALITY NOT YET SUPPORTED
  tft.drawString(bankNames[currentPerformance.patches[0].bankIndex], 80, 130);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(currentPerformance.patches[0].patchName, 80, 150);
}

FLASHMEM void renderPerformancePage()
{
  tft.fillScreen(ILI9341_BLACK);
  renderMidiClk();
  renderArpIndicator();
  renderSeqIndicator();
  renderKeyboardIndicator();
  renderPerfIndicator();
  renderPeak();
  renderMIDI();
  renderVoiceGrid();
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setTextDatum(TL_DATUM);
  switch (currentPerformanceIndex)
  {
  case 0 ... 8:
    tft.drawString("00" + String(currentPerformanceIndex + 1), 45, 94);
    break;
  case 9 ... 98:
    tft.drawString("0" + String(currentPerformanceIndex + 1), 45, 94);
    break;
  default:
    tft.drawString(String(currentPerformanceIndex + 1), 45, 94);
  }
  tft.drawString(currentPerformance.performanceName, 100, 94);
  currentPerformance.performanceName.length() > 12 ? tft.setFont(Arial_12) : tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(currentPerformance.patches[0].patchName, 45, 120);
  renderCorners();
}

FLASHMEM void renderPerformanceMidiEditPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Edit Performance", 160, 59);
  tft.drawString("Choose MIDI", 160, 79);
  tft.drawFastHLine(10, 102, tft.width() - 20, ILI9341_RED);
  renderPerformanceName();
  renderCorners();
}

FLASHMEM void renderPerformanceEncEditPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Edit Performance", 160, 59);
  tft.drawString("Choose Encoders", 160, 79);
  tft.drawFastHLine(10, 102, tft.width() - 20, ILI9341_RED);

  renderPerformanceName();

  // ENC_TL
  tft.fillTriangle(0, 0, 0, 14, 14, 0, encTriColour[ENC_TL]);
  tft.fillTriangle(2, 2, 2, 10, 10, 2, ILI9341_BLACK);
  tft.setTextColor(encTriColour[ENC_TL]);
  tft.setTextDatum(TL_DATUM);
  tft.setFont(Arial_13);
  tft.drawString(encMap[ENC_TL].ValueStr, 14, 27);

  // ENC_TR
  tft.fillTriangle(305, 0, 319, 0, 319, 14, encTriColour[ENC_TR]);
  tft.fillTriangle(309, 2, 317, 2, 317, 10, ILI9341_BLACK);
  tft.setTextColor(encTriColour[ENC_TR]);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(encMap[ENC_TR].ValueStr, 303, 7);

  // ENC_BR
  tft.fillTriangle(305, 239, 319, 239, 319, 225, encTriColour[ENC_BR]);
  tft.fillTriangle(309, 237, 317, 237, 317, 229, ILI9341_BLACK);
  tft.setTextColor(encTriColour[ENC_BR]);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(encMap[ENC_BR].ValueStr, 303, 196);

  // ENC_BL
  tft.fillTriangle(0, 225, 0, 239, 14, 239, encTriColour[ENC_BL]);
  tft.fillTriangle(2, 229, 2, 237, 10, 237, ILI9341_BLACK);
  tft.setTextColor(encTriColour[ENC_BL]);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(encMap[ENC_BL].ValueStr, 14, 220);
}

FLASHMEM void renderPerformancePatchEditPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Edit Performance", 160, 0);
  tft.drawString("Choose Patch", 160, 26);
  tft.drawFastHLine(10, 49, tft.width() - 20, ILI9341_RED);

  renderBankList();

  // Patches -  up to 128 in each bank
  tft.setFont(Arial_12_Bold);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  size_t offset = ceil(currentPatchIndex / 8);
  for (size_t i = 0; i < min(patches.size(), 8); i++)
  {
    currentPatchIndex == i + (8 * offset) ? tft.setTextColor(ILI9341_YELLOW)
                                          : tft.setTextColor(ILI9341_DARKYELLOW);
    if (i + 1 + (8 * offset) <= patches.size())
    {
      tft.drawString(String(i + 1 + (8 * offset)) + " " + patches[i + (8 * offset)].patchName, 110, 57 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderPerformanceRecallPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Recall Performance", 160, 26);
  tft.drawFastHLine(10, 48, tft.width() - 20, ILI9341_RED);

  tft.setFont(Arial_12);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_RED);
  String performanceStr;
  size_t offset = ceil(currentPerformanceIndex / 8);
  for (size_t i = 0; i < min(PERFORMANCES_LIMIT, 8); i++)
  {
    if (i + 1 + (8 * offset) <= PERFORMANCES_LIMIT)
    {
      switch (i + 1 + (8 * offset))
      {
      case 0 ... 9:
        performanceStr = "00" + String(i + 1 + (8 * offset)) + " " + performances[i + (8 * offset)];
        break;
      case 10 ... 99:
        performanceStr = "0" + String(i + 1 + (8 * offset)) + " " + performances[i + (8 * offset)];
        break;
      default:
        performanceStr = String(i + 1 + (8 * offset)) + " " + performances[i + (8 * offset)];
      }
      if (currentPerformanceIndex == i + (8 * offset))
      {

        tft.setTextColor(ILI9341_BLACK);
        tft.getTextBounds(performanceStr, 2, 56 + (20 * i), &xt, &yt, &wt, &ht);
        tft.fillRect(0, yt - 1, wt + 4, ht + 4, ILI9341_ORANGE);
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
      }

      tft.drawString(performanceStr, 2, 56 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderEditBankPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Edit Banks", 160, 19);
  tft.drawFastHLine(10, 42, tft.width() - 20, ILI9341_RED);

  renderBankList();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  if (patches[0].patchUID == 0)
  {
    tft.drawString("0 Patches", 120, 51);
  }
  else
  {
    tft.drawString(String(patches.size()) + " Patches", 120, 51);
  }
  for (size_t i = 0; i < min(patches.size(), 6); i++)
  {
    tft.setTextColor(ILI9341_DARKYELLOW);
    if (i + 1 <= patches.size())
    {
      tft.drawString(String(i + 1) + " " + patches[i].patchName, 110, 76 + (20 * i));
    }
  }
  renderCorners();
}

FLASHMEM void renderDeleteBankPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Delete Bank", 160, 19);
  tft.drawFastHLine(10, 42, tft.width() - 20, ILI9341_RED);

  tft.setFont(Arial_12_Bold);
  tft.setTextColor(ILI9341_RED);
  tft.drawString(bankNames[currentBankIndex], 160, 120);
  tft.drawString("This will delete all", 160, 145);
  tft.drawString("patches in this bank", 160, 165);

  renderCorners();
}

FLASHMEM void renderUpDown(uint16_t x, uint16_t y, uint16_t colour)
{
  // Produces up/down indicator glyph at x,y
  tft.setCursor(x, y);
  tft.fillTriangle(x, y, x + 8, y - 8, x + 16, y, colour);
  tft.fillTriangle(x, y + 4, x + 8, y + 12, x + 16, y + 4, colour);
}

FLASHMEM void renderSettingsPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Global Settings", 160, 150);
  renderCorners();
}

FLASHMEM void renderFirmwareUpdatePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Updating Firmware", 160, 150);
}

FLASHMEM void renderFirmwareDeletePage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Firmware Update Complete", 160, 110);
  tft.setTextColor(ILI9341_ORANGE);
  tft.drawString("Delete Firmware", 160, 130);
  tft.drawString("File from SD Card", 160, 150);
}

FLASHMEM void renderFirmwareUpdateAbortPage()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_RED);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Aborting Firmware Update", 160, 110);
}

FLASHMEM void showCurrentParameterOverlay2(const char *param, float val, int pType)
{
  if (!updateDisplay)
    return;
  currentParameter = param;
  currentValue = String(val);
  currentFloatValue = val;
  paramType = pType;
  startTimer();
}

FLASHMEM void showCurrentParameterOverlay(String param, String val, int pType)
{
  // Prevents all the changing parameters showing when a patch is loaded
  if (!updateDisplay)
    return;
  currentParameter = param;
  currentValue = val;
  paramType = pType;
  startTimer();
}

FLASHMEM void showCurrentParameterOverlay(String param, String val)
{
  showCurrentParameterOverlay(param, val, 0);
}

FLASHMEM void enableScope(boolean enable)
{
  enable ? global.scope.ScreenSetup(&tft) : global.scope.ScreenSetup(NULL);
}

// Send the frame buffer to the display using DMA so the CPU is free during the
// SPI transfer (~18ms for a full 320x240 frame). Falls back to a blocking update.
void updateScreenDMA()
{
  global.scope.Display(); // Draws only if the scope is enabled
  if (tft.updateScreenAsync(false))
  {
    while (tft.asyncUpdateActive())
      threads.yield();
  }
  else
  {
    tft.updateScreen();
  }
}

void displayThread()
{
  threads.delay(1500); // Give bootup page chance to display
  elapsedMillis frameTime;
  while (1)
  {
    frameTime = 0;
    if ((millis() - timer) > DISPLAYTIMEOUT)
    {
      switch (state)
      {
      case State::MAIN:
        renderMainPage();
        break;
      case State::PATCHLIST:
        renderPatchRecallPage();
        break;
      case State::SAVE:
        renderSavePage();
        updateScreenDMA(); // update before delay
        threads.delay(1000);
        break;
      case State::REINITIALISE:
        renderReinitialisePage();
        updateScreenDMA(); // update before delay
        threads.delay(1000);
        break;
      case State::PATCHSAVING:
        renderPatchSavingPage();
        break;
      case State::RENAMEPATCH:
      case State::CHOOSECHARPATCH:
      case State::DELETECHARPATCH:
        renderPatchNamingPage();
        break;
      case State::DELETEPATCH:
        renderDeletePatchPage();
        break;
      case State::DELETEMSG:
        renderDeleteMessagePage();
        updateScreenDMA(); // update before delay
        threads.delay(1000);
        break;
      case State::DELETEBANKMSG:
        renderDeleteBankMessagePage();
        updateScreenDMA(); // update before delay
        threads.delay(1000);
        break;
      case State::SETTINGS:
        renderSettingsPage();
        break;
      case State::OSCPAGE1:
        renderOscPage(1);
        break;
      case State::OSCPAGE2:
        renderOscPage(2);
        break;
      case State::OSCPAGE3:
        renderOscPage(3);
        break;
      case State::OSCPAGE4:
        renderOscPage(4);
        break;
      case State::OSCMODPAGE1:
        renderOscModPage(1);
        break;
      case State::OSCMODPAGE2:
        renderOscModPage(2);
        break;
      case State::OSCMODPAGE3:
        renderOscModPage(3);
        break;
      case State::OSCMODPAGE4:
        renderOscModPage(4);
        break;
      case State::FILTERPAGE1:
        renderFilterPage(1);
        break;
      case State::FILTERPAGE2:
        renderFilterPage(2);
        break;
      case State::FILTERMODPAGE1:
        renderFilterModPage(1);
        break;
      case State::FILTERMODPAGE2:
        renderFilterModPage(2);
        break;
      case State::FILTERMODPAGE3:
        renderFilterModPage(3);
        break;
      case State::AMPPAGE1:
        renderAmpPage(1);
        break;
      case State::AMPPAGE2:
        renderAmpPage(2);
        break;
      case State::AMPPAGE3:
        renderAmpPage(3);
        break;
      case State::FXPAGE1:
        renderFXPage(1);
        break;
      case State::FXPAGE2:
        renderFXPage(2);
        break;
      case State::SEQUENCEPAGE:
        renderSequencerPage();
        break;
      case SEQUENCERECALL:
        renderSequenceRecallPage();
        break;
      case SEQUENCEEDIT:
        renderSequencerEditPage();
        break;
      case State::ARPPAGE1:
        renderArpPage(1);
        break;
      case State::ARPPAGE2:
        renderArpPage(2);
        break;
      case State::MIDIPAGE:
        renderMIDIPage();
        break;
      case State::PERFORMANCEPAGE:
        renderPerformancePage();
        break;
      case State::PERFORMANCERECALL:
        renderPerformanceRecallPage();
        break;
      case PERFORMANCEPATCHEDIT:
        renderPerformancePatchEditPage();
        break;
      case PERFORMANCEENCEDIT:
        renderPerformanceEncEditPage();
        break;
      case PERFORMANCEMIDIEDIT:
        renderPerformanceMidiEditPage();
        break;
      case RENAMEPERFORMANCE:
      case State::CHOOSECHARPERFORMANCE:
      case State::DELETECHARPERFORMANCE:
        renderPerformanceNamingPage();
        break;
      case State::EDITBANK:
        renderEditBankPage();
        break;
      case State::RENAMEBANK:
      case State::CHOOSECHARBANK:
      case State::DELETECHARBANK:
        renderBankNamingPage();
        break;
      case State::RENAMESEQUENCE:
      case State::CHOOSECHARSEQUENCE:
      case State::DELETECHARSEQUENCE:
        renderSequenceNamingPage();
        break;
      case State::FIRMWAREUPDATE:
        renderFirmwareUpdatePage();
        break;
      case State::FIRMWAREUPDATE_ABORT:
        renderFirmwareUpdateAbortPage();
        break;
      case State::FIRMWAREDELETE:
        renderFirmwareDeletePage();
      default:
        break;
      }
    }
    else
    {
      renderCurrentParameterOverlay();
    }
    updateScreenDMA();
    // Yield the CPU back to the main loop (MIDI handling) for the rest of the frame
    if (frameTime < DISPLAY_FRAME_MS)
      threads.delay(DISPLAY_FRAME_MS - frameTime);
  }
}

void setupDisplay()
{
  tft.begin(SPI_SPEED, SPICLOCK_READ);
  tft.useFrameBuffer(true);
  tft.setRotation(1);
  // tft.invertDisplay(true);
  tft.setTextWrap(false);
  renderBootUpPage();
  tft.updateScreen();
  int displayThreadId = threads.addThread(displayThread);
  // Default slice is 10ms, which could hold off MIDI note handling by up to 10ms
  if (displayThreadId > 0)
    threads.setTimeSlice(displayThreadId, DISPLAY_THREAD_TICKS);
}

#endif