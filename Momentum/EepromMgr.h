#include <EEPROM.h>
#define FIRSTRUN 111
#define EEPROM_MIDI_CH 0
#define EEPROM_MIDI_OUT_CH 1
#define EEPROM_MIDI_THRU 2
#define EEPROM_SCOPE_ENABLE 3
#define EEPROM_VU_ENABLE 4
#define EEPROM_VOLUME 5
#define EEPROM_LAST_BANK 6
#define EEPROM_LAST_PATCH 7
#define EEPROM_TUNING 8
#define EEPROM_FIRST_RUN 9
#define EEPROM_ARP_TEMPO 10
#define EEPROM_ARP_STYLE 11
#define EEPROM_ARP_RANGE 12
#define EEPROM_ARP_DIVISION 13
#define EEPROM_ARP_CYCLES 14
#define EEPROM_ARP_BASIS 15
#define EEPROM_KEYBOARD_SCALE 16
#define EEPROM_KEYBOARD_BASIS 17
#define EEPROM_SEND_CC 18
#define EEPROM_LAST_SEQUENCE 19
#define EEPROM_SYNC_TO_MIDI_CLK 20
#define EEPROM_SAME_VOICE_RETRIG 21

FLASHMEM int8_t getFirstRun()
{
  return EEPROM.read(EEPROM_FIRST_RUN) == FIRSTRUN; // Assuming it's never this from the factory
}

FLASHMEM void storeFirstRun()
{
  EEPROM.update(EEPROM_FIRST_RUN, FIRSTRUN);
}

FLASHMEM int8_t getSendCC()
{
  return EEPROM.read(EEPROM_SEND_CC);
}

FLASHMEM int8_t getSyncToMIDIClk()
{
  return EEPROM.read(EEPROM_SYNC_TO_MIDI_CLK);
}

FLASHMEM void storeSendCCToEEPROM(byte cc)
{
  EEPROM.update(EEPROM_SEND_CC, cc);
}

FLASHMEM void storeSyncToMIDIClkToEEPROM(byte cc)
{
  EEPROM.update(EEPROM_SYNC_TO_MIDI_CLK, cc);
}

// Note retrigger: 0 = Rotate (a repeated note takes the next voice), 1 = Same Voice.
// Unset EEPROM (0xFF) reads as Rotate, the previous behaviour.
FLASHMEM bool getSameVoiceRetrig()
{
  return EEPROM.read(EEPROM_SAME_VOICE_RETRIG) == 1;
}

FLASHMEM void storeSameVoiceRetrigToEEPROM(byte same)
{
  EEPROM.update(EEPROM_SAME_VOICE_RETRIG, same);
}


FLASHMEM int8_t getLastBank()
{
  byte b = EEPROM.read(EEPROM_LAST_BANK);
  if (b < 0 || b > 7)
    return 0;
  return b;
}

FLASHMEM int8_t getLastSequence()
{
  byte b = EEPROM.read(EEPROM_LAST_SEQUENCE);
  if (b < 0 || b > 127)
    return 0;
  return b;
}

FLASHMEM void storeLastBankToEEPROM(byte type)
{
  EEPROM.update(EEPROM_LAST_BANK, type);
}

FLASHMEM int8_t getLastPatch()
{
  byte p = EEPROM.read(EEPROM_LAST_PATCH);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeLastPatchToEEPROM(byte type)
{
  EEPROM.update(EEPROM_LAST_PATCH, type);
}

FLASHMEM void storeLastSequenceToEEPROM(byte type)
{
  EEPROM.update(EEPROM_LAST_SEQUENCE, type);
}

FLASHMEM void loadLastPatchUsed()
{
  currentBankIndex = getLastBank();
  currentPatchIndex = getLastPatch();
}

FLASHMEM int8_t getVolume()
{
  byte v = EEPROM.read(EEPROM_VOLUME);
  if (v < 0 || v > 127)
    return currentVolume;
  return v;
}

FLASHMEM void storeVolumeToEEPROM(byte type)
{
  EEPROM.update(EEPROM_VOLUME, type);
}

FLASHMEM int getMIDIChannel()
{
  byte midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  if (midiChannel < 0 || midiChannel > 16)
    midiChannel = MIDI_CHANNEL_OMNI; // If EEPROM has no MIDI channel stored
  return midiChannel;
}

FLASHMEM void storeMidiChannel(byte channel)
{
  EEPROM.update(EEPROM_MIDI_CH, channel);
}

FLASHMEM int getMIDIOutCh()
{
  byte mc = EEPROM.read(EEPROM_MIDI_OUT_CH);
  if (mc < 0 || midiOutCh > 16)
    mc = 0; // If EEPROM has no MIDI channel stored
  return mc;
}

FLASHMEM void storeMidiOutCh(byte channel)
{
  EEPROM.update(EEPROM_MIDI_OUT_CH, channel);
}

FLASHMEM midi::Thru::Mode getMidiThru()
{
  byte midiThruMode = EEPROM.read(EEPROM_MIDI_THRU);
  if (midiThruMode < midi::Thru::Off || midiThruMode > midi::Thru::DifferentChannel)
    return midi::Thru::Full; // If EEPROM has no MIDI Thru stored
  return (midi::Thru::Mode)midiThruMode;
}

FLASHMEM void storeMidiThru(byte thru)
{
  EEPROM.update(EEPROM_MIDI_THRU, thru);
}

FLASHMEM boolean getScopeEnable()
{
  byte sc = EEPROM.read(EEPROM_SCOPE_ENABLE);
  if (sc < 0 || sc > 1)
    return false; // If EEPROM has no scope enable stored
  return sc == 1 ? true : false;
}

FLASHMEM void storeScopeEnable(byte ScopeEnable)
{
  EEPROM.update(EEPROM_SCOPE_ENABLE, ScopeEnable);
}

FLASHMEM boolean getVUEnable()
{
  byte vu = EEPROM.read(EEPROM_VU_ENABLE);
  if (vu < 0 || vu > 1)
    return false; // If EEPROM has no VU enable stored
  return vu == 1 ? true : false;
}

FLASHMEM void storeVUEnable(byte VUEnable)
{
  EEPROM.update(EEPROM_VU_ENABLE, VUEnable);
}

FLASHMEM int8_t getTuningCents()
{
  byte t = EEPROM.read(EEPROM_TUNING);
  if (t < 0 || t > 100)
    return 0;    // If EEPROM has no value stored
  return t - 50; // zero is stored as 50, +/-50cents
}

FLASHMEM void storeTuning(byte tuning)
{
  EEPROM.update(EEPROM_TUNING, tuning + 50); // zero is stored as 50, +/-50cents
}

FLASHMEM int8_t getArpStyle()
{
  byte p = EEPROM.read(EEPROM_ARP_STYLE);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeArpRangeToEEPROM(byte range)
{
  EEPROM.update(EEPROM_ARP_RANGE, range);
}

FLASHMEM int8_t getArpRange()
{
  byte p = EEPROM.read(EEPROM_ARP_RANGE);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeArpDivisionToEEPROM(byte div)
{
  EEPROM.update(EEPROM_ARP_DIVISION, div);
}

FLASHMEM int8_t getArpDivision()
{
  byte p = EEPROM.read(EEPROM_ARP_DIVISION);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeArpCyclesToEEPROM(byte c)
{
  EEPROM.update(EEPROM_ARP_CYCLES, c);
}

FLASHMEM int8_t getArpCycles()
{
  byte p = EEPROM.read(EEPROM_ARP_CYCLES);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeArpBasisToEEPROM(byte c)
{
  EEPROM.update(EEPROM_ARP_BASIS, c);
}

FLASHMEM int8_t getArpBasis()
{
  byte p = EEPROM.read(EEPROM_ARP_BASIS);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeKeyboardBasisToEEPROM(byte c)
{
  EEPROM.update(EEPROM_KEYBOARD_BASIS, c);
}

FLASHMEM int8_t getKeyboardBasis()
{
  byte p = EEPROM.read(EEPROM_KEYBOARD_BASIS);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeKeyboardScaleToEEPROM(byte c)
{
  EEPROM.update(EEPROM_KEYBOARD_SCALE, c);
}

FLASHMEM int8_t getKeyboardScale()
{
  byte p = EEPROM.read(EEPROM_KEYBOARD_SCALE);
  if (p < 0 || p > 127)
    return 0;
  return p;
}

FLASHMEM void storeArpStyleToEEPROM(byte style)
{
  EEPROM.update(EEPROM_ARP_STYLE, style);
}

FLASHMEM void checkFirstRun()
{
  if (!getFirstRun())
  {
    if (DEBUG)
      Serial.println(F("First Run"));
    // Default the EEPROM contents
    storeMidiChannel(0);
    storeMidiOutCh(0);
    storeMidiThru(midi::Thru::Full);
    storeLastBankToEEPROM(0);
    storeLastPatchToEEPROM(0);
    storeLastSequenceToEEPROM(0);
    storeTuning(0);
    storeVUEnable(0);
    storeVolumeToEEPROM(currentVolume);
    storeScopeEnable(0);
    storeArpDivisionToEEPROM(1);
    storeArpCyclesToEEPROM(0);
    storeArpRangeToEEPROM(3);
    storeArpStyleToEEPROM(0);
    storeArpBasisToEEPROM(2);
    storeKeyboardScaleToEEPROM(0);
    storeKeyboardBasisToEEPROM(4);
    storeSendCCToEEPROM(0);
    storeSyncToMIDIClkToEEPROM(0);
    storeSameVoiceRetrigToEEPROM(0);
    storeFirstRun();
  }
}
