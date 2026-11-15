#include <Arduino.h>
#include "Constants.h"
#include "MidiCC.h"

const uint8_t PARAMETERSFORENCS = 60;
const char *ParameterStrMap[256] = {};
uint8_t ParametersForPerformanceEncoders[PARAMETERSFORENCS] = {};

FLASHMEM void assignStrings()
{
    ParameterStrMap[CCosclfoamt] = "Pitch LFO Amount";
    ParameterStrMap[CCglide] = "Glide Time";
    ParameterStrMap[CCvolume] = "Volume";
    ParameterStrMap[CCoscwaveformA] = "Waveform 1";
    ParameterStrMap[CCoscwaveformB] = "Waveform 2";
    ParameterStrMap[CCfilterenv] = "Filter Envelope";
    ParameterStrMap[CCfiltermixer] = "Filter Type";
    ParameterStrMap[CCoscLevelA] = "Level 1";
    ParameterStrMap[CCoscLevelB] = "Level 2";
    ParameterStrMap[CCnoiseLevel] = "Noise Level";
    ParameterStrMap[CCoscfx] = "Osc Effect";
    ParameterStrMap[CCpitchA] = "Pitch 1";
    ParameterStrMap[CCpitchB] = "Pitch 2";
    ParameterStrMap[CCpitchenv] = "Pitch Envelope";
    ParameterStrMap[CCosclforetrig] = "Pitch LFO Retrig";
    ParameterStrMap[CCfilterlforetrig] = "Filter LFO Retrig";
    ParameterStrMap[CCfilterres] = "Resonance";
    ParameterStrMap[CCamprelease] = "Release";
    ParameterStrMap[CCampattack] = "Attack";
    ParameterStrMap[CCfilterfreq] = "Cutoff Freq";
    ParameterStrMap[filterfreq256] = "Cutoff Freq";
    ParameterStrMap[CCampdecay] = "Decay";
    ParameterStrMap[CCfilterlforate] = "Filter LFO Rate";
    ParameterStrMap[CCfilterlfoamt] = "Filter LFO Level";
    ParameterStrMap[CCampsustain] = "Sustain";
    ParameterStrMap[CCfilterattack] = "Filter Attack";
    ParameterStrMap[CCfilterdecay] = "Filter Decay";
    ParameterStrMap[CCfiltersustain] = "Filter Sustain";
    ParameterStrMap[CCfilterrelease] = "Filter Release";
    ParameterStrMap[CCpwA] = "Pulse Width";
    ParameterStrMap[CCpwB] = "Pulse Width";
    ParameterStrMap[CCpwmRateA] = "PWM LFO Rate 1";
    ParameterStrMap[CCpwmRateB] = "PWM LFO Rate 2";
    ParameterStrMap[CCpwmAmtA] = "PWM Level 1";
    ParameterStrMap[CCpwmAmtB] = "PWM Level 2";
    ParameterStrMap[CCkeytracking] = "Key Tracking";
    ParameterStrMap[CCfilterlfowaveform] = "Filter LFO Wave";
    ParameterStrMap[CCensemblefxamt] = "Ensemble Rate";
    ParameterStrMap[CCensemblefxmix] = "Ensemble Mix";
    ParameterStrMap[CCreverbfxtime] = "Room Size";
    ParameterStrMap[CCreverbfxmix] = "Reverb Mix";
    ParameterStrMap[CCensemblefxdepth] = "Ensemble Depth";
    ParameterStrMap[CCensemblefxdelay] = "Ensemble Delay";
    ParameterStrMap[CCreverbfxdamping] = "Reverb Damping";
    ParameterStrMap[CCreverbfxbasscut] = "Reverb Bass Cut";
    ParameterStrMap[firmwareupdate] = "Update";
    ParameterStrMap[firmwarecancel] = "Cancel";
    ParameterStrMap[CCdetune] = "Detune";
    ParameterStrMap[CCoscLfoRate] = "Pitch LFO Rate";
    ParameterStrMap[CCoscLfoWaveform] = "Pitch LFO Wave";
    ParameterStrMap[CCfilterLFOMidiClkSync] = "Filter LFO Sync";
    ParameterStrMap[CCoscLFOMidiClkSync] = "Pitch LFO Sync";
    ParameterStrMap[CCpwmSourceA] = "PWM Source 1";
    ParameterStrMap[CCpwmSourceB] = "PWM Source 2";
    ParameterStrMap[CCunison] = "Unison";
    ParameterStrMap[CCmonomode] = "Monophonic";
    ParameterStrMap[patchselect] = "Patch";
    ParameterStrMap[CCbankselectLSB] = "Bank";
    ParameterStrMap[savepatch] = "Save";
    ParameterStrMap[deletepatch] = "Delete";
    ParameterStrMap[renamepatch] = "Rename";
    ParameterStrMap[CCvelocitySens] = "Velocity Sens";
    ParameterStrMap[ampenvshape] = "Amp. Env. Shape";
    ParameterStrMap[filterenvshape] = "Filter Env. Shape";
    ParameterStrMap[CCFilterVelocitySens] = "Filter Vel. Sens";
    ParameterStrMap[glideshape] = "Glide Shape";
    ParameterStrMap[pitchbendrange] = "Pitch Bend";
    ParameterStrMap[pitchmodwheeldepth] = "Pitch Mod W. Dep";
    ParameterStrMap[filtermodwheeldepth] = "Filter Mod W. Dep";
    ParameterStrMap[savebankselect] = "Bank";
    ParameterStrMap[namepatch] = "Name Patch";
    ParameterStrMap[MIDIChIn] = "Channel In";
    ParameterStrMap[MIDIChOut] = "Channel Out";
    ParameterStrMap[MIDIThruMode] = "MIDI Thru Mode";
    ParameterStrMap[choosePerfMIDIChIn] = "Channel In";
    ParameterStrMap[choosePerfMIDIChOut] = "Channel Out";
    ParameterStrMap[choosePerfMIDIThruMode] = "MIDI Thru Mode";
    ParameterStrMap[PerfSelect] = "Performance";
    ParameterStrMap[PerfEdit] = "Edit";
    ParameterStrMap[deleteCharacterPerformance] = "Del. Char";
    ParameterStrMap[choosecharacterPerformance] = "Character";
    ParameterStrMap[cancel] = "Cancel";
    ParameterStrMap[goback] = "Back";
    ParameterStrMap[settingoption] = "Setting";
    ParameterStrMap[settingvalue] = "Value";
    ParameterStrMap[editbank] = "Edit Banks";
    ParameterStrMap[deletepatches] = "Delete";
    ParameterStrMap[deletebank] = "Del Bank";
    ParameterStrMap[renamebank] = "Rename Bank";
    ParameterStrMap[savebank] = "Save Bank";
    ParameterStrMap[bankeditselect] = "Bank";
    ParameterStrMap[choosecharacterPatch] = "Character";
    ParameterStrMap[choosecharacterBank] = "Character";
    ParameterStrMap[choosecharacterSequence] = "Character";
    ParameterStrMap[deleteCharacterPatch] = "Del. Char";
    ParameterStrMap[deleteCharacterBank] = "Del. Char";
    ParameterStrMap[deleteCharacterSequence] = "Del. Char";
    ParameterStrMap[SeqEdit] = "Edit";
    ParameterStrMap[SeqSelect] = "Sequence";
    ParameterStrMap[SeqTempo] = "Tempo";
    ParameterStrMap[SeqStartStop] = "Start/Stop";
    ParameterStrMap[SeqNote] = "Note";
    ParameterStrMap[SeqPosition] = "Position";
    ParameterStrMap[SeqLength] = "Length";
    ParameterStrMap[SeqDelete] = "Delete";
    ParameterStrMap[PerfDelete] = "Delete";
    ParameterStrMap[ArpDivision] = "Division";
    ParameterStrMap[ArpStyle] = "Style";
    ParameterStrMap[ArpRange] = "Range";
    ParameterStrMap[ArpBasis] = "Basis";
    ParameterStrMap[ArpCycle] = "Cycles";
    ParameterStrMap[kbdOct] = "Keyboard Basis";
    ParameterStrMap[kbdScale] = "Keyboard Scale";
    ParameterStrMap[noencoder] = "None";
}

FLASHMEM void assignParametersForPerformanceEncoders()
{
    uint8_t i = 0;
    ParametersForPerformanceEncoders[i] = noencoder;
    ParametersForPerformanceEncoders[++i] = CCglide;
    ParametersForPerformanceEncoders[++i] = CCoscwaveformA;
    ParametersForPerformanceEncoders[++i] = CCoscwaveformB;
    ParametersForPerformanceEncoders[++i] = CCoscLevelA;
    ParametersForPerformanceEncoders[++i] = CCoscLevelB;
    ParametersForPerformanceEncoders[++i] = CCnoiseLevel;
    ParametersForPerformanceEncoders[++i] = CCpitchA;
    ParametersForPerformanceEncoders[++i] = CCpitchB;
    ParametersForPerformanceEncoders[++i] = CCdetune;
    ParametersForPerformanceEncoders[++i] = CCunison;
    ParametersForPerformanceEncoders[++i] = CCpitchenv;
    ParametersForPerformanceEncoders[++i] = CCpwmSourceA;
    ParametersForPerformanceEncoders[++i] = CCpwmSourceB;
    ParametersForPerformanceEncoders[++i] = CCpwA;
    ParametersForPerformanceEncoders[++i] = CCpwB;
    ParametersForPerformanceEncoders[++i] = CCpwmRateA;
    ParametersForPerformanceEncoders[++i] = CCpwmRateB;
    ParametersForPerformanceEncoders[++i] = CCpwmAmtA;
    ParametersForPerformanceEncoders[++i] = CCpwmAmtB;
    ParametersForPerformanceEncoders[++i] = CCoscfx;
    ParametersForPerformanceEncoders[++i] = CCoscLfoWaveform;
    ParametersForPerformanceEncoders[++i] = CCoscLfoRate;
    ParametersForPerformanceEncoders[++i] = CCosclfoamt;
    ParametersForPerformanceEncoders[++i] = CCosclforetrig;
    ParametersForPerformanceEncoders[++i] = CCoscLFOMidiClkSync;
    ParametersForPerformanceEncoders[++i] = filterfreq256;
    ParametersForPerformanceEncoders[++i] = CCfilterres;
    ParametersForPerformanceEncoders[++i] = CCfiltermixer;
    ParametersForPerformanceEncoders[++i] = CCfilterattack;
    ParametersForPerformanceEncoders[++i] = CCfilterdecay;
    ParametersForPerformanceEncoders[++i] = CCfiltersustain;
    ParametersForPerformanceEncoders[++i] = CCfilterrelease;
    ParametersForPerformanceEncoders[++i] = CCfilterenv;
    ParametersForPerformanceEncoders[++i] = filterenvshape;
    ParametersForPerformanceEncoders[++i] = CCfilterlfowaveform;
    ParametersForPerformanceEncoders[++i] = CCfilterlforate;
    ParametersForPerformanceEncoders[++i] = CCfilterlfoamt;
    ParametersForPerformanceEncoders[++i] = CCfilterlforetrig;
    ParametersForPerformanceEncoders[++i] = CCfilterLFOMidiClkSync;
    ParametersForPerformanceEncoders[++i] = filtermodwheeldepth;
    ParametersForPerformanceEncoders[++i] = CCkeytracking;
    ParametersForPerformanceEncoders[++i] = CCampattack;
    ParametersForPerformanceEncoders[++i] = CCampdecay;
    ParametersForPerformanceEncoders[++i] = CCampsustain;
    ParametersForPerformanceEncoders[++i] = CCamprelease;
    ParametersForPerformanceEncoders[++i] = ampenvshape;
    ParametersForPerformanceEncoders[++i] = CCensemblefxamt;
    ParametersForPerformanceEncoders[++i] = CCensemblefxmix;
    ParametersForPerformanceEncoders[++i] = CCreverbfxtime;
    ParametersForPerformanceEncoders[++i] = CCreverbfxmix;
    ParametersForPerformanceEncoders[++i] = CCmonomode;
    ParametersForPerformanceEncoders[++i] = CCvelocitySens;
    ParametersForPerformanceEncoders[++i] = glideshape;
    ParametersForPerformanceEncoders[++i] = pitchbendrange;
    ParametersForPerformanceEncoders[++i] = pitchmodwheeldepth;
    // Added at the end so the existing entries keep their positions
    ParametersForPerformanceEncoders[++i] = CCensemblefxdepth;
    ParametersForPerformanceEncoders[++i] = CCensemblefxdelay;
    ParametersForPerformanceEncoders[++i] = CCreverbfxdamping;
    ParametersForPerformanceEncoders[++i] = CCreverbfxbasscut;
}

FLASHMEM uint8_t getIndexForParametersForPerformanceEncoders(uint8_t cc)
{
    for (uint8_t i = 0; i < PARAMETERSFORENCS; i++)
    {
        if (ParametersForPerformanceEncoders[i] == cc)
            return i;
    }
    if (DEBUG)
        Serial.println(F("getIndexForParametersForPerformanceEncoders() not matched:") + String(cc));
    return 0; // noencoder
}

FLASHMEM String
getWaveformStr(int value)
{
    switch (value)
    {
    case WAVEFORM_SILENT:
        return F("Off");
    case WAVEFORM_SAMPLE_HOLD:
        return F("Sample & Hold");
    case WAVEFORM_SINE:
        return F("Sine");
    case WAVEFORM_BANDLIMIT_SQUARE:
    case WAVEFORM_SQUARE:
        return F("Square");
    case WAVEFORM_TRIANGLE:
        return F("Triangle");
    case WAVEFORM_BANDLIMIT_SAWTOOTH:
    case WAVEFORM_SAWTOOTH:
        return F("Sawtooth");
    case WAVEFORM_SAWTOOTH_REVERSE:
        return F("Ramp");
    case WAVEFORM_BANDLIMIT_PULSE:
        return F("Variable Pulse");
    case WAVEFORM_TRIANGLE_VARIABLE:
        return F("Variable Triangle");
    case WAVEFORM_PARABOLIC:
        return F("Parabolic");
    case WAVEFORM_HARMONIC:
        return F("Harmonic");
    default:
        return F("ERR_WAVE");
    }
}

const char *CDT_STR[128] = {"Major", "Major", "Major", "Major", "Major", "Major", "Major", "Minor", "Minor", "Minor", "Minor", "Minor", "Minor", "Minor", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "6th", "6th", "6th", "6th", "6th", "6th", "6th", "7th", "7th", "7th", "7th", "7th", "7th", "7th", "9th", "9th", "9th", "9th", "9th", "9th", "9th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "All 12", "All 12"};
