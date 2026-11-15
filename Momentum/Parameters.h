#pragma once
#include <stdint.h>
#include <MIDI.h>
#include <Constants.h>

// Globals used for Settings
extern byte midiChannel;
extern byte midiOutCh;
extern midi::Thru::Mode MIDIThru;
extern String patchName;
extern boolean encCW;
extern boolean vuMeter;
extern boolean sendCC;
extern boolean syncToMIDIClk;
extern int8_t tuningCents;
extern float tuningCentsFrac;

// Global patch modifiers
extern float lfoSyncFreq;
extern float lfoFilterTempoValue;
extern float lfoPitchTempoValue;

// Exponential envelopes
extern int8_t envTypeAmp;
extern int8_t envTypeFilt;
// Glide shape
extern int8_t glideShape;

extern uint8_t currentVolume;

extern int8_t charCursor;
extern int8_t nameCursor;

// extern uint8_t currentSettingsOption;
// extern uint8_t currentSettingsValue;

// *** Patch parameters to store the relative 7-bit values (Cutoff freq 8-bit) for patch store and modification by the encoders ***
// *** Initialised with a default patch to store when there are no patches on SD card ***
typedef struct PatchStruct
{
    uint32_t UID = 0;
    char PatchName[64] = "Init"; // INITPATCHNAME;
    uint8_t OscLevelA = 127;
    uint8_t OscLevelB = 127;
    uint8_t NoiseLevel = 63; // Centre off
    uint8_t Unison = 0;
    uint8_t OscFX = 0;
    uint8_t Detune = 20;      // 0.15%
    uint8_t KeyTracking = 63; // Half tracking, 0.5
    uint8_t Glide = 0;
    uint8_t PitchA = 0;     //-24
    uint8_t PitchB = 8;     //-12
    uint8_t WaveformA = 75; // Pulse
    uint8_t WaveformB = 75; // Pulse
    uint8_t PWMSourceA = 0;
    uint8_t PWMSourceB = 0;
    uint8_t PWMA_Amount = 63; // 0.50
    uint8_t PWMB_Amount = 63; // 0.50
    uint8_t PWMRateA = 10;    // 0.18Hz
    uint8_t PWMRateB = 10;    // 0.18Hz
    uint8_t PWA_Amount = 63;
    uint8_t PWB_Amount = 63;
    uint8_t FilterRes = 0;    // 0.71
    uint8_t FilterFreq = 255; // 12000Hz
    uint8_t FilterMixer = 0;
    uint8_t FilterEnv = 63;
    uint8_t PitchLFOAmt = 0;
    uint8_t PitchLFORate = 10;
    uint8_t PitchLFOWaveform = 0;
    uint8_t PitchLFORetrig = 0;
    uint8_t PitchLFOMidiClkSync = 0;
    uint8_t FilterLFORate = 10;
    uint8_t FilterLFORetrig = 0;
    uint8_t FilterLFOMidiClkSync = 0;
    uint8_t FilterLfoAmt = 0;
    uint8_t FilterLFOWaveform = 0;
    uint8_t FilterAttack = 0;
    uint8_t FilterDecay = 20;
    uint8_t FilterSustain = 80;
    uint8_t FilterRelease = 25;
    uint8_t Attack = 0;
    uint8_t Decay = 20;
    uint8_t Sustain = 80;
    uint8_t Release = 25;
    uint8_t EnsembleEffectAmt = 8;
    uint8_t EnsembleEffectMix = 31;
    uint8_t ReverbEffectTime = 8;
    uint8_t ReverbEffectMix = 0;
    // Defaults give the settings these used to be fixed at
    uint8_t EnsembleEffectDepth = 64;  // 64 = original depth (25-200%)
    uint8_t EnsembleEffectDelay = 64;  // 64 = 11.6ms, the original (2-40ms)
    uint8_t ReverbEffectDamping = 64;  // 64 = 0.5
    uint8_t ReverbEffectBassCut = 26;  // 26 = 0.2
    uint8_t PitchEnv = 63; // Centre off
    uint8_t VelocitySensitivity = 0;
    uint8_t FilterVelocitySensitivity = 0;
    uint8_t ChordDetune = 0;
    uint8_t MonophonicMode = 0;
    uint8_t FilterEnvShape = 9; // Exp 0
    uint8_t AmpEnvShape = 9;    // Exp 0
    uint8_t GlideShape = 1;     // Exp
    uint8_t PitchBend = 7;
    uint8_t PitchModWheelDepth = 1;
    uint8_t FilterModWheelDepth = 0;
} PatchStruct;