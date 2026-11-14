#include "VoiceGroup.h"
#include "SettingsService.h"

FLASHMEM void settingsMIDICh(int index, const char *value);
FLASHMEM void settingsMIDIOutCh(int index, const char *value);
FLASHMEM void settingsMIDIThru(int index, const char *value);
FLASHMEM void settingsScopeEnable(int index, const char *value);
FLASHMEM void settingsVUEnable(int index, const char *value);
FLASHMEM void settingsTuning(int index, const char *value);
FLASHMEM void settingsSendCCEnable(int index, const char *value);

FLASHMEM int currentIndexMIDICh();
FLASHMEM int currentIndexMIDIOutCh();
FLASHMEM int currentIndexMIDIThru();
FLASHMEM int currentIndexScopeEnable();
FLASHMEM int currentIndexVUEnable();
FLASHMEM int currentIndexTuning();
FLASHMEM int currentIndexSendCCEnable();

FLASHMEM void settingsMIDICh(int index, const char *value)
{
  if (strcmp(value, "ALL") == 0)
  {
    midiChannel = MIDI_CHANNEL_OMNI;
  }
  else
  {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

FLASHMEM void settingsMIDIOutCh(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
  {
    midiOutCh = 0;
  }
  else
  {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

FLASHMEM void settingsMIDIThru(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
    MIDIThru = midi::Thru::Off;
  if (strcmp(value, "Full") == 0)
    MIDIThru = midi::Thru::Full;
  if (strcmp(value, "Same Ch.") == 0)
    MIDIThru = midi::Thru::SameChannel;
  if (strcmp(value, "Diff. Ch.") == 0)
    MIDIThru = midi::Thru::DifferentChannel;
  changeMIDIThruMode();
  storeMidiThru(MIDIThru);
}

FLASHMEM void settingsScopeEnable(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
  {
    enableScope(false);
    storeScopeEnable(0);
  }
  else
  {
    enableScope(true);
    storeScopeEnable(1);
  }
}

FLASHMEM void settingsVUEnable(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
  {
    vuMeter = false;
    storeVUEnable(0);
  }
  else
  {
    vuMeter = true;
    storeVUEnable(1);
  }
}

FLASHMEM void settingsSendCCEnable(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
  {
    sendCC = false;
    storeSendCCToEEPROM(0);
  }
  else
  {
    sendCC = true;
    storeSendCCToEEPROM(1);
  }
}

FLASHMEM void settingsSyncToMIDIClkEnable(int index, const char *value)
{
  if (strcmp(value, "Off") == 0)
  {
    syncToMIDIClk = false;
    storeSyncToMIDIClkToEEPROM(0);
  }
  else
  {
    syncToMIDIClk = true;
    storeSyncToMIDIClkToEEPROM(1);
  }
}

// Retrigger: Rotate (as before) takes another voice for a note played again while it is
// still sounding; Same Voice reuses that note's voice
FLASHMEM void settingsRetrig(int index, const char *value)
{
  bool same = strcmp(value, "Same Voice") == 0;
  groupvec[activeGroupIndex]->setSameVoiceRetrig(same);
  storeSameVoiceRetrigToEEPROM(same ? 1 : 0);
}

FLASHMEM int currentIndexRetrig()
{
  return getSameVoiceRetrig() ? 1 : 0;
}

FLASHMEM void settingsTuning(int index, const char *value)
{
  tuningCents = atoi(value);
  tuningCentsFrac = 1.0f + (tuningCents * CENTSFRAC);
  storeTuning(tuningCents);
}

FLASHMEM int currentIndexMIDICh()
{
  return getMIDIChannel();
}

FLASHMEM int currentIndexMIDIOutCh()
{
  return getMIDIOutCh();
}

FLASHMEM int currentIndexMIDIThru()
{
  if (MIDIThru == midi::Thru::Off)
    return 0;
  if (MIDIThru == midi::Thru::Full)
    return 1;
  if (MIDIThru == midi::Thru::SameChannel)
    return 2;
  if (MIDIThru == midi::Thru::DifferentChannel)
    return 3;
  return 0;
}

FLASHMEM int currentIndexScopeEnable()
{
  return getScopeEnable() ? 1 : 0;
}

FLASHMEM int currentIndexVUEnable()
{
  return getVUEnable() ? 1 : 0;
}

FLASHMEM int currentIndexTuning()
{
  return getTuningCents() + 50;
}

FLASHMEM int currentIndexSendCCEnable()
{
  return getSendCC() ? 1 : 0;
}

FLASHMEM int currentIndexSyncToMIDIClkEnable()
{
  return getSyncToMIDIClk() ? 1 : 0;
}

// add settings to the circular buffer
FLASHMEM void setUpSettings()
{
  settings::append(settings::SettingsOption{"Oscilloscope", {"Off", "On", "\0"}, settingsScopeEnable, currentIndexScopeEnable});
  settings::append(settings::SettingsOption{"VU Meter", {"Off", "On", "\0"}, settingsVUEnable, currentIndexVUEnable});
  settings::append(settings::SettingsOption{"Global Tuning (cents)", {"-50", "-49", "-48", "-47", "-46", "-45", "-44", "-43", "-42", "-41", "-40", "-39", "-38", "-37", "-36", "-35", "-34", "-33", "-32", "-31", "-30", "-29", "-28", "-27", "-26", "-25", "-24", "-23", "-22", "-21", "-20", "-19", "-18", "-17", "-16", "-15", "-14", "-13", "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "\0"}, settingsTuning, currentIndexTuning});
  settings::append(settings::SettingsOption{"Patch MIDI CC Out", {"Off", "On", "\0"}, settingsSendCCEnable, currentIndexSendCCEnable});
  settings::append(settings::SettingsOption{"Sync to MIDI Clk", {"Off", "On", "\0"}, settingsSyncToMIDIClkEnable, currentIndexSyncToMIDIClkEnable});
  settings::append(settings::SettingsOption{"Retrigger", {"Rotate", "Same Voice", "\0"}, settingsRetrig, currentIndexRetrig});
}
