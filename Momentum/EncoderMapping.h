void setConfigurationForCC(uint8_t, uint8_t);

typedef struct EncoderMappingStruct
{
    boolean active = true;
    uint8_t Parameter = 255;        // MIDI cc (up to 127) or other code up to 255
    uint8_t ShowValue = false;      // Some parameters don't show value like patch no
    uint8_t Value = 0;              // Up to 255
    uint8_t DefaultValue = 0;       // Up to 255
    int8_t Counter = 0;             // For reduced sensitivity for small ranges - do not set
    String ValueStr = "";           // For transforming values in strings for display
    uint8_t Range = 127;            // Up to 255
    String ParameterStr = "";       // For display
    boolean Push = false;           // Pushing encoder button for further settings
    State PushAction = State::MAIN; // Next state when button pushed
} EncoderMappingStruct;

EncoderMappingStruct encMap[4] = {};

// The encoder value is a byte, but tempo goes up to 300bpm (and higher from MIDI clock).
// Only the text is shown; clamp the byte, as converting an out of range float is undefined.
inline uint8_t bpmToEncValue(float bpm)
{
    return bpm <= 0.0f ? 0 : (bpm >= 255.0f ? 255 : (uint8_t)bpm);
}

// Sets the value and value string of the encoder when this parameter is on the display
FLASHMEM boolean setEncValue(boolean act, uint8_t parameter, uint8_t value, String str, uint8_t newParameter)
{
    LockGuard lock(displayLock); // The display renders from encMap
    for (uint8_t i = 0; i < 4; i++)
    {
        if (encMap[i].Parameter == parameter)
        {
            if (act != encMap[i].active && state != State::MAIN)
                encMap[i].active = act;
            if (newParameter != encMap[i].Parameter)
                encMap[i].Parameter = newParameter;
            encMap[i].ParameterStr = ParameterStrMap[newParameter];
            if (value != encMap[i].Value)
                encMap[i].Value = value;
            encMap[i].ValueStr = str;
            return true; // True if this was set
        }
    }
    return false;
}

FLASHMEM boolean setEncValue(uint8_t parameter, uint8_t value, uint8_t range, String str)
{
    LockGuard lock(displayLock); // The display renders from encMap
    // Extra routine for settings which have variable size ranges
    for (uint8_t i = 0; i < 4; i++)
    {
        if (encMap[i].Parameter == parameter)
        {
            encMap[i].Range = range;
            break;
        }
    }
    return setEncValue(true, parameter, value, str, parameter);
}

FLASHMEM boolean setEncValue(uint8_t parameter, uint8_t value, String str)
{
    LockGuard lock(displayLock); // The display renders from encMap
    return setEncValue(true, parameter, value, str, parameter);
}

// For turning off
FLASHMEM boolean setEncInactive(uint8_t parameter)
{
    LockGuard lock(displayLock); // The display renders from encMap
    return setEncValue(false, parameter, 0, "", parameter);
}

FLASHMEM void configureOff(EncoderMappingStruct *enc)
{
    enc->active = false;
    enc->Parameter = 255;
}

FLASHMEM void configureKeyboardOct(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = kbdOct;
    enc->ShowValue = true;
    enc->Value = keyboardOct;
    enc->Range = 6;
    enc->ParameterStr = ParameterStrMap[kbdOct];
    enc->ValueStr = KEYBOARD_OCT_STR[keyboardOct];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureKeyboardScale(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = kbdScale;
    enc->ShowValue = true;
    enc->Value = keyboardScale;
    enc->Range = 23;
    enc->ParameterStr = ParameterStrMap[kbdScale];
    enc->ValueStr = SCALE_STR[keyboardScale];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCbankselectLSB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCbankselectLSB;
    enc->ShowValue = false;
    enc->Value = currentBankIndex;
    enc->Range = BANKS_LIMIT;
    enc->ParameterStr = ParameterStrMap[CCbankselectLSB];
    enc->ValueStr = "";
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurePatchselect(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = patchselect;
    enc->ShowValue = false;
    enc->Value = currentPatchIndex;
    enc->Range = patches.size();
    enc->ValueStr = "";
    enc->ParameterStr = ParameterStrMap[patchselect];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCancel(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = cancel;
    enc->ShowValue = false;
    enc->Value = 0;
    enc->Range = 0;
    enc->ValueStr = "";
    enc->ParameterStr = ParameterStrMap[cancel];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscwaveformA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscwaveformA;
    enc->ShowValue = true;
    enc->Value = currentPatch.WaveformA;
    enc->DefaultValue = 40;
    enc->ValueStr = getWaveformStr(groupvec[activeGroupIndex]->getWaveformA());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscwaveformA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLevelA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscLevelA;
    enc->ShowValue = true;
    enc->Value = currentPatch.OscLevelA;
    enc->DefaultValue = 127;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getOscLevelA());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLevelA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCdetune(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCdetune;
    enc->ShowValue = true;
    if (groupvec[activeGroupIndex]->params().unisonMode == 2)
    {
        enc->Value = currentPatch.ChordDetune;
        enc->DefaultValue = 0;
        enc->ValueStr = String(CDT_STR[groupvec[activeGroupIndex]->params().chordDetune]);
    }
    else
    {
        enc->Value = currentPatch.Detune;
        enc->DefaultValue = 20; // 0.15%
        enc->ValueStr = String((1 - groupvec[activeGroupIndex]->params().detune) * 100) + "%";
    }
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCdetune];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpitchA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCpitchA;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchA;
    enc->DefaultValue = 63;
    enc->ValueStr = (groupvec[activeGroupIndex]->params().oscPitchA > 0 ? "+" : "") + String(groupvec[activeGroupIndex]->params().oscPitchA);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpitchA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscwaveformB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscwaveformB;
    enc->ShowValue = true;
    enc->Value = currentPatch.WaveformB;
    enc->DefaultValue = 40;
    enc->ValueStr = getWaveformStr(groupvec[activeGroupIndex]->getWaveformB());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscwaveformB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLevelB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscLevelB;
    enc->ShowValue = true;
    enc->Value = currentPatch.OscLevelB;
    enc->DefaultValue = 127;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getOscLevelB());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLevelB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpitchB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCpitchB;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchB;
    enc->DefaultValue = 7;
    enc->ValueStr = (groupvec[activeGroupIndex]->params().oscPitchB > 0 ? "+" : "") + String(groupvec[activeGroupIndex]->params().oscPitchB);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpitchB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCnoiseLevel(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCnoiseLevel;
    enc->ShowValue = true;
    if (groupvec[activeGroupIndex]->getPinkNoiseLevel() > 0)
    {
        enc->Value = currentPatch.NoiseLevel;
        enc->ValueStr = "Pink " + String(groupvec[activeGroupIndex]->getPinkNoiseLevel());
    }
    else if (groupvec[activeGroupIndex]->getWhiteNoiseLevel() > 0)
    {
        enc->Value = currentPatch.NoiseLevel;
        enc->ValueStr = "White " + String(groupvec[activeGroupIndex]->getWhiteNoiseLevel());
    }
    else
    {
        enc->Value = 63;
        enc->ValueStr = "Off";
    }
    enc->Range = 127;
    enc->DefaultValue = 63;
    enc->ParameterStr = ParameterStrMap[CCnoiseLevel];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCunison(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCunison;
    enc->ShowValue = true;
    enc->Value = currentPatch.Unison;
    enc->DefaultValue = 0;

    if (groupvec[activeGroupIndex]->params().unisonMode == 0)
    {
        enc->ValueStr = "Off";
    }
    else if (groupvec[activeGroupIndex]->params().unisonMode == 1)
    {
        enc->ValueStr = "Dynamic";
    }
    else
    {
        enc->ValueStr = "Chord";
    }
    enc->Range = 2;
    enc->ParameterStr = ParameterStrMap[CCunison];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmSourceA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCpwmSourceA;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMSourceA;
    enc->DefaultValue = PWMSOURCELFO;
    switch (currentPatch.PWMSourceA)
    {
    case PWMSOURCEMANUAL:
        enc->ValueStr = "Manual";
        break;
    case PWMSOURCELFO:
        enc->ValueStr = "LFO";
        break;
    case PWMSOURCEFENV:
        enc->ValueStr = "Filter Env";
        break;
    }

    enc->Range = 2;
    enc->ParameterStr = ParameterStrMap[CCpwmSourceA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmAmtA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceA != PWMSOURCEMANUAL ? enc->active = true : enc->active = false;
    enc->Parameter = CCpwmAmtA;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMA_Amount;
    enc->DefaultValue = 63;
    enc->ValueStr = String(LINEAR[currentPatch.PWMA_Amount]);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpwmAmtA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmRateA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceA == PWMSOURCELFO ? enc->active = true : enc->active = false;
    enc->Parameter = CCpwmRateA;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMRateA;
    enc->DefaultValue = 11;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getPwmRateA()) + "Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpwmRateA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwA(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceA == PWMSOURCEMANUAL ? enc->active = true : enc->active = false;
    enc->Parameter = CCpwA;
    enc->ParameterStr = ParameterStrMap[CCpwA];
    enc->ShowValue = true;
    enc->Value = currentPatch.PWA_Amount;
    enc->DefaultValue = 63;
    if (groupvec[activeGroupIndex]->getWaveformA() == WAVEFORM_TRIANGLE_VARIABLE)
    {
        enc->ValueStr = "Tri " + String(groupvec[activeGroupIndex]->getPwA());
    }
    else
    {
        enc->ValueStr = "Pulse " + String(groupvec[activeGroupIndex]->getPwA());
    }
    enc->Range = 127;
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmSourceB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCpwmSourceB;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMSourceB;
    switch (currentPatch.PWMSourceB)
    {
    case PWMSOURCEMANUAL:
        enc->ValueStr = "Manual";
        break;
    case PWMSOURCELFO:
        enc->ValueStr = "LFO";
        break;
    case PWMSOURCEFENV:
        enc->ValueStr = "Filter Env";
        break;
    }
    enc->Range = 2;
    enc->ParameterStr = ParameterStrMap[CCpwmSourceB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmAmtB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceB != PWMSOURCEMANUAL
        ? enc->active = true
        : enc->active = false;
    enc->Parameter = CCpwmAmtB;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMB_Amount;
    enc->ValueStr = String(LINEAR[currentPatch.PWMB_Amount]);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpwmAmtB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwmRateB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceB == PWMSOURCELFO
        ? enc->active = true
        : enc->active = false;
    enc->Parameter = CCpwmRateB;
    enc->ShowValue = true;
    enc->Value = currentPatch.PWMRateB;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getPwmRateB()) + "Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpwmRateB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpwB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.PWMSourceB == PWMSOURCEMANUAL
        ? enc->active = true
        : enc->active = false;
    enc->Parameter = CCpwB;
    enc->ParameterStr = ParameterStrMap[CCpwB];
    enc->ShowValue = true;
    enc->Value = currentPatch.PWB_Amount;
    if (groupvec[activeGroupIndex]->getWaveformB() == WAVEFORM_TRIANGLE_VARIABLE)
    {
        enc->ValueStr = "Tri " + String(groupvec[activeGroupIndex]->getPwB());
    }
    else
    {
        enc->ValueStr = "Pulse " + String(groupvec[activeGroupIndex]->getPwB());
    }
    enc->Range = 127;
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLfoWaveform(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscLfoWaveform;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchLFOWaveform;
    enc->DefaultValue = WAVEFORM_SINE;
    enc->ValueStr = getWaveformStr(groupvec[activeGroupIndex]->getPitchLfoWaveform());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLfoWaveform];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCosclfoamt(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCosclfoamt;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchLFOAmt;
    enc->DefaultValue = 11;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getPitchLfoAmount(), 3);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCosclfoamt];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLfoRate(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscLfoRate;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchLFORate;
    enc->DefaultValue = 11;
    if (currentPatch.PitchLFOMidiClkSync > 0)
        enc->ValueStr = LFOTEMPOSTR[currentPatch.PitchLFORate];
    else
        enc->ValueStr = String(groupvec[activeGroupIndex]->getPitchLfoRate()) + " Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLfoRate];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCosclforetrig(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCosclforetrig;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchLFORetrig;
    enc->DefaultValue = 0;
    enc->ValueStr = groupvec[activeGroupIndex]->getPitchLfoRetrig() > 0 ? "On" : "Off";
    enc->Range = 1;
    enc->ParameterStr = ParameterStrMap[CCosclforetrig];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLFOMidiClkSync(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscLFOMidiClkSync;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchLFOMidiClkSync;
    enc->DefaultValue = 0;
    enc->ValueStr = groupvec[activeGroupIndex]->getPitchLfoMidiClockSync() > 0 ? "On" : "Off";
    enc->Range = 1;
    enc->ParameterStr = ParameterStrMap[CCoscLFOMidiClkSync];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterLFOMidiClkSync(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterLFOMidiClkSync;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterLFOMidiClkSync;
    enc->DefaultValue = 0;
    enc->ValueStr = groupvec[activeGroupIndex]->getFilterLfoMidiClockSync() > 0 ? "On" : "Off";
    enc->Range = 1;
    enc->ParameterStr = ParameterStrMap[CCfilterLFOMidiClkSync];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLevelB_OscFX(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.OscFX == OSCFXXMOD ? enc->active = true : enc->active = false;
    enc->Parameter = CCoscLevelB;
    enc->ShowValue = true;
    enc->Value = currentPatch.OscLevelB;
    enc->DefaultValue = 127;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getOscLevelB());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLevelB];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscfx(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCoscfx;
    enc->ShowValue = true;
    enc->Value = currentPatch.OscFX;
    enc->DefaultValue = 0;
    switch (groupvec[activeGroupIndex]->getOscFX())
    {
    case OSCFXXMOD:
        enc->ValueStr = "X Mod";
        break;
    case OSCFXXOR:
        enc->ValueStr = "XOR Mod";
        break;
    default:
        enc->ValueStr = "Off";
        break;
    }
    enc->Range = 2;
    enc->ParameterStr = ParameterStrMap[CCoscfx];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCpitchenv(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCpitchenv;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchEnv;
    enc->DefaultValue = 63;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getPitchEnvelope());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCpitchenv];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCoscLevelA_OscFX(EncoderMappingStruct *enc, State st = State::MAIN)
{
    currentPatch.OscFX == OSCFXXMOD ? enc->active = true : enc->active = false;
    enc->Parameter = CCoscLevelA;
    enc->ShowValue = true;
    enc->Value = currentPatch.OscLevelA;
    enc->DefaultValue = 63;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getOscLevelA());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCoscLevelA];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurefilterfreq256(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = filterfreq256; // 256 values
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterFreq;
    enc->DefaultValue = 255;
    enc->ValueStr = String(int(groupvec[activeGroupIndex]->getCutoff())) + " Hz";
    enc->Range = 255;
    enc->ParameterStr = ParameterStrMap[CCfilterfreq];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterres(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterres;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterRes;
    enc->DefaultValue = 0;
    enc->Range = 127;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getResonance());
    enc->ParameterStr = ParameterStrMap[CCfilterres];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfiltermixer(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfiltermixer;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterMixer;
    enc->DefaultValue = 0;
    if (groupvec[activeGroupIndex]->getFilterMixer() == BANDPASS)
    {
        enc->ValueStr = "Band Pass";
    }
    else
    {
        // LP-HP mix mode - a notch filter
        if (groupvec[activeGroupIndex]->getFilterMixer() == LOWPASS)
        {
            enc->ValueStr = "Low Pass";
        }
        else if (groupvec[activeGroupIndex]->getFilterMixer() == HIGHPASS)
        {
            enc->ValueStr = "High Pass";
        }
        else
        {
            enc->ValueStr = "Low " + String(100 - int(100 * groupvec[activeGroupIndex]->getFilterMixer())) + " - " + String(int(100 * groupvec[activeGroupIndex]->getFilterMixer())) + " High";
        }
    }
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfiltermixer];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterenv(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterenv;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterEnv;
    enc->DefaultValue = 63;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getFilterEnvelope());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterenv];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterattack(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterattack;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterAttack;
    enc->DefaultValue = 0;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getFilterAttack());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterattack];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterdecay(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterdecay;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterDecay;
    enc->DefaultValue = 20;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getFilterDecay());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterdecay];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfiltersustain(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfiltersustain;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterSustain;
    enc->DefaultValue = 63;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getFilterSustain());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfiltersustain];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterrelease(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterrelease;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterRelease;
    enc->DefaultValue = 20;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getFilterRelease());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterrelease];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterlfowaveform(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterlfowaveform;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterLFOWaveform;
    enc->DefaultValue = WAVEFORM_SINE;
    enc->ValueStr = getWaveformStr(groupvec[activeGroupIndex]->getFilterLfoWaveform());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterlfowaveform];
    enc->Push = true;
    enc->PushAction = st;
}
FLASHMEM void configureCCfilterlfoamt(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterlfoamt;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterLfoAmt;
    enc->DefaultValue = 0;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getFilterLfoAmt());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterlfoamt];
    enc->Push = true;
    enc->PushAction = st;
}
FLASHMEM void configureCCfilterlforate(EncoderMappingStruct *enc, State st)
{
    enc->active = true;
    enc->Parameter = CCfilterlforate;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterLFORate;
    enc->DefaultValue = 18;
    if (currentPatch.FilterLFOMidiClkSync)
        enc->ValueStr = LFOTEMPOSTR[currentPatch.FilterLFORate];
    else
        enc->ValueStr = String(groupvec[activeGroupIndex]->getFilterLfoRate()) + " Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCfilterlforate];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCfilterlforetrig(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCfilterlforetrig;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterLFORetrig;
    enc->DefaultValue = 0;
    enc->ValueStr = groupvec[activeGroupIndex]->getFilterLfoRetrig() > 0 ? "On" : "Off";
    enc->Range = 1;
    enc->ParameterStr = ParameterStrMap[CCfilterlforetrig];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCensemblefxamt(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxamt;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectAmt;
    enc->DefaultValue = 0;
    enc->ValueStr = String(ENSEMBLE_LFO[currentPatch.EnsembleEffectAmt]) + " Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxamt];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCensemblefxmix(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxmix;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectMix;
    enc->DefaultValue = 0;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getEnsembleEffectMix());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxmix];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxtime(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxtime;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectTime;
    enc->DefaultValue = 0;
    enc->ValueStr = String(LINEAR[currentPatch.ReverbEffectTime]);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxtime];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxmix(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxmix;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectMix;
    enc->DefaultValue = 0;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getReverbEffectMix());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxmix];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCkeytracking(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCkeytracking;
    enc->ShowValue = true;
    enc->Value = currentPatch.KeyTracking;
    enc->DefaultValue = 63;
    enc->ValueStr = String(LINEAR[currentPatch.KeyTracking]);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCkeytracking];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurefilterenvshape(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = filterenvshape;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterEnvShape;
    enc->DefaultValue = 9;
    enc->ValueStr = EnvShapeStr[currentPatch.FilterEnvShape];
    enc->Range = 17;
    enc->ParameterStr = ParameterStrMap[filterenvshape];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCampattack(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCampattack;
    enc->ShowValue = true;
    enc->Value = currentPatch.Attack;
    enc->DefaultValue = 0;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getAmpAttack());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCampattack];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCampdecay(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCampdecay;
    enc->ShowValue = true;
    enc->Value = currentPatch.Decay;
    enc->DefaultValue = 20;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getAmpDecay());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCampdecay];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCampsustain(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCampsustain;
    enc->ShowValue = true;
    enc->Value = currentPatch.Sustain;
    enc->DefaultValue = 63;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getAmpSustain());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCampsustain];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCamprelease(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCamprelease;
    enc->ShowValue = true;
    enc->Value = currentPatch.Release;
    enc->DefaultValue = 20;
    enc->ValueStr = milliToString(groupvec[activeGroupIndex]->getAmpRelease());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCamprelease];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurepitchbendrange(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = pitchbendrange;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchBend;
    enc->DefaultValue = 12;
    enc->ValueStr = String(currentPatch.PitchBend);
    enc->Range = 12;
    enc->ParameterStr = ParameterStrMap[pitchbendrange];
    enc->Push = true;
    enc->PushAction = st;
}
FLASHMEM void configurefiltermodwheeldepth(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = filtermodwheeldepth;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterModWheelDepth;
    enc->DefaultValue = 1;
    enc->ValueStr = String(currentPatch.FilterModWheelDepth / 10.0f);
    enc->Range = 10;
    enc->ParameterStr = ParameterStrMap[filtermodwheeldepth];
    enc->Push = true;
    enc->PushAction = st;
}
FLASHMEM void configurepitchmodwheeldepth(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = pitchmodwheeldepth;
    enc->ShowValue = true;
    enc->Value = currentPatch.PitchModWheelDepth;
    enc->DefaultValue = 1;
    enc->ValueStr = String(currentPatch.PitchModWheelDepth / 10.0f);
    enc->Range = 10;
    enc->ParameterStr = ParameterStrMap[pitchmodwheeldepth];
    enc->Push = true;
    enc->PushAction = st;
}
FLASHMEM void configureCCAmpVelocitySens(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCvelocitySens;
    enc->ShowValue = true;
    enc->Value = currentPatch.VelocitySensitivity;
    enc->DefaultValue = 0;
    enc->ValueStr = velocityStr[currentPatch.VelocitySensitivity];
    enc->Range = 4;
    enc->ParameterStr = ParameterStrMap[CCvelocitySens];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCFilterVelocitySens(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCFilterVelocitySens;
    enc->ShowValue = true;
    enc->Value = currentPatch.FilterVelocitySensitivity;
    enc->DefaultValue = 0;
    enc->ValueStr = velocityStr[currentPatch.FilterVelocitySensitivity];
    enc->Range = 4;
    enc->ParameterStr = ParameterStrMap[CCFilterVelocitySens];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCmonomode(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCmonomode;
    enc->ShowValue = true;
    enc->Value = currentPatch.MonophonicMode;
    enc->DefaultValue = 0;
    enc->ValueStr = MonophonicStr[currentPatch.MonophonicMode];
    enc->Range = 4;
    enc->ParameterStr = ParameterStrMap[CCmonomode];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureampenvshape(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = ampenvshape;
    enc->ShowValue = true;
    enc->Value = currentPatch.AmpEnvShape;
    enc->DefaultValue = 9;
    enc->ValueStr = EnvShapeStr[currentPatch.AmpEnvShape];
    enc->Range = 17;
    enc->ParameterStr = ParameterStrMap[ampenvshape];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCglide(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCglide;
    enc->ShowValue = true;
    enc->Value = currentPatch.Glide;
    enc->DefaultValue = 0;
    enc->ValueStr = milliToString(POWER[currentPatch.Glide] * GLIDEFACTOR);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCglide];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureglideshape(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = glideshape;
    enc->ShowValue = true;
    enc->Value = currentPatch.GlideShape;
    enc->DefaultValue = 0;
    enc->ValueStr = GlideShapeStr[currentPatch.GlideShape];
    enc->Range = 1;
    enc->ParameterStr = ParameterStrMap[glideshape];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void setDefaultValue(EncoderMappingStruct *enc)
{
    enc->Value = enc->DefaultValue;
}

// currentPatch.XXX stores relative values (0-127) like MIDI CC
// groupvec[activeGroupIndex]->XXX stores absolute, real values
FLASHMEM void setEncodersState(State s)
{
    LockGuard lock(displayLock); // The display renders from encMap
    switch (s)
    {
    case State::MAIN:
        if (keyboardActive)
        {
            configureKeyboardOct(&encMap[ENC_TL]);
            configureKeyboardScale(&encMap[ENC_TR]);
        }
        else
        {
            configureOff(&encMap[ENC_TL]);
            configureOff(&encMap[ENC_TR]);
        }
        configureCCbankselectLSB(&encMap[ENC_BL], State::PATCHLIST);
        configurePatchselect(&encMap[ENC_BR], State::PATCHLIST);
        break;
    case State::PATCHLIST:
        configureOff(&encMap[ENC_TL]);
        configureCancel(&encMap[ENC_TR]);
        configureCCbankselectLSB(&encMap[ENC_BL]);
        configurePatchselect(&encMap[ENC_BR]);
        break;
    case State::OSCPAGE1:
        configureCCoscwaveformA(&encMap[ENC_BL], State::OSCPAGE1);
        configureCCoscLevelA(&encMap[ENC_BR], State::OSCPAGE1);
        configureCCdetune(&encMap[ENC_TL], State::OSCPAGE1);
        configureCCpitchA(&encMap[ENC_TR], State::OSCPAGE1);
        break;
    case State::OSCPAGE2:
        configureCCoscwaveformB(&encMap[ENC_BL], State::OSCPAGE2);
        configureCCoscLevelB(&encMap[ENC_BR], State::OSCPAGE2);
        configureCCdetune(&encMap[ENC_TL], State::OSCPAGE2);
        configureCCpitchB(&encMap[ENC_TR], State::OSCPAGE2);
        break;
    case State::OSCPAGE3:
        configureCCnoiseLevel(&encMap[ENC_BL], State::OSCPAGE3);
        configureCCunison(&encMap[ENC_BR], State::OSCPAGE3);
        configureCCdetune(&encMap[ENC_TL], State::OSCPAGE3);
        configureOff(&encMap[ENC_TR]);
        break;
    case State::OSCPAGE4:
        configureCCoscLevelB_OscFX(&encMap[ENC_BR], State::OSCPAGE4);
        configureCCoscfx(&encMap[ENC_BL], State::OSCPAGE4);
        configureOff(&encMap[ENC_TL]);
        configureCCoscLevelA_OscFX(&encMap[ENC_TR], State::OSCPAGE4);
        break;
    case State::OSCMODPAGE1:
        configureCCpwmSourceA(&encMap[ENC_BL], State::OSCMODPAGE1);
        configureCCpwmAmtA(&encMap[ENC_BR], State::OSCMODPAGE1);
        configureCCpwmRateA(&encMap[ENC_TL], State::OSCMODPAGE1);
        configureCCpwA(&encMap[ENC_TR], State::OSCMODPAGE1);
        break;
    case State::OSCMODPAGE2:
        configureCCpwmSourceB(&encMap[ENC_BL], State::OSCMODPAGE2);
        configureCCpwmAmtB(&encMap[ENC_BR], State::OSCMODPAGE2);
        configureCCpwmRateB(&encMap[ENC_TL], State::OSCMODPAGE2);
        configureCCpwB(&encMap[ENC_TR], State::OSCMODPAGE2);
        break;
    case State::OSCMODPAGE3:
        configureCCoscLfoWaveform(&encMap[ENC_BL], State::OSCMODPAGE3);
        configureCCosclfoamt(&encMap[ENC_BR], State::OSCMODPAGE3);
        configureCCoscLfoRate(&encMap[ENC_TL], State::OSCMODPAGE3);
        configureCCoscLFOMidiClkSync(&encMap[ENC_TR], State::OSCMODPAGE3);
        break;
    case State::OSCMODPAGE4:
        configureCCpitchenv(&encMap[ENC_TL], State::OSCMODPAGE4);
        configureCCosclforetrig(&encMap[ENC_TR], State::OSCMODPAGE4);
        configurepitchbendrange(&encMap[ENC_BL], State::AMPPAGE2);
        configurepitchmodwheeldepth(&encMap[ENC_BR], State::OSCMODPAGE4);
        break;
    case State::FILTERPAGE1:
        configurefilterfreq256(&encMap[ENC_BL], State::FILTERPAGE1);
        configureCCfilterres(&encMap[ENC_BR], State::FILTERPAGE1);
        configureCCfiltermixer(&encMap[ENC_TL], State::FILTERPAGE1);
        configureCCfilterenv(&encMap[ENC_TR], State::FILTERPAGE1);
        break;
    case State::FILTERPAGE2:
        configureCCfilterattack(&encMap[ENC_TL], State::FILTERPAGE2);
        configureCCfilterdecay(&encMap[ENC_TR], State::FILTERPAGE2);
        configureCCfiltersustain(&encMap[ENC_BL], State::FILTERPAGE2);
        configureCCfilterrelease(&encMap[ENC_BR], State::FILTERPAGE2);
        break;
    case State::FILTERMODPAGE1:
        configureCCfilterlfowaveform(&encMap[ENC_BL], State::FILTERMODPAGE1);
        configureCCfilterlfoamt(&encMap[ENC_BR], State::FILTERMODPAGE1);
        configureCCfilterlforate(&encMap[ENC_TL], State::FILTERMODPAGE1);
        configureCCfilterLFOMidiClkSync(&encMap[ENC_TR], State::FILTERMODPAGE1);
        break;
    case State::FILTERMODPAGE2:
        configureCCfilterlfowaveform(&encMap[ENC_BL], State::FILTERMODPAGE2);
        configureCCfilterlfoamt(&encMap[ENC_BR], State::FILTERMODPAGE2);
        configureCCfilterlforate(&encMap[ENC_TL], State::FILTERMODPAGE2);
        configureCCfilterlforetrig(&encMap[ENC_TR], State::FILTERMODPAGE2);
        break;
    case State::FILTERMODPAGE3:
        configureCCkeytracking(&encMap[ENC_BR], State::FILTERMODPAGE3);
        configureCCFilterVelocitySens(&encMap[ENC_BL], State::FILTERMODPAGE3);
        configurefiltermodwheeldepth(&encMap[ENC_TR], State::FILTERMODPAGE3);
        configurefilterenvshape(&encMap[ENC_TL], State::FILTERMODPAGE3);
        break;
    case State::AMPPAGE1:
        configureCCampattack(&encMap[ENC_TL], State::AMPPAGE1);
        configureCCampdecay(&encMap[ENC_TR], State::AMPPAGE1);
        configureCCampsustain(&encMap[ENC_BL], State::AMPPAGE1);
        configureCCamprelease(&encMap[ENC_BR], State::AMPPAGE1);
        break;

    case State::AMPPAGE2:
        configureOff(&encMap[ENC_TR]);
        configureampenvshape(&encMap[ENC_TL], State::AMPPAGE2);
        configureCCAmpVelocitySens(&encMap[ENC_BL], State::AMPPAGE2);
        configureCCmonomode(&encMap[ENC_BR], State::AMPPAGE2);
        break;

    case State::AMPPAGE3:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);
        configureCCglide(&encMap[ENC_BL], State::AMPPAGE3);
        configureglideshape(&encMap[ENC_BR], State::AMPPAGE3);
        break;
    case State::FXPAGE1:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);
        configureCCensemblefxamt(&encMap[ENC_BL], State::FXPAGE1);
        configureCCensemblefxmix(&encMap[ENC_BR], State::FXPAGE1);
        break;
    case State::FXPAGE2:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);
        configureCCreverbfxtime(&encMap[ENC_BL], State::FXPAGE2);
        configureCCreverbfxmix(&encMap[ENC_BR], State::FXPAGE2);
        break;
    case State::SAVE:
        encMap[ENC_TL].active = false;

        encMap[ENC_TR].active = false;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = savepatch;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 127;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[savepatch];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::MAIN;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = renamepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = 127;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[renamepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::PATCHSAVING:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = editbank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = currentBankIndex;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[editbank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::EDITBANK;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = savebankselect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[savebankselect];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = namepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[namepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::RENAMEPATCH;
        break;

    case State::RENAMEPATCH:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterPatch;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 4; // More encoder rotation for accuracy
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterPatch];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARPATCH;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterPatch;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterPatch];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARPATCH;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = savepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[savepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::DELETEPATCH:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deletepatch;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deletepatch];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::MAIN;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = CCbankselectLSB;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[CCbankselectLSB];
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = patchselect;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[patchselect];
        encMap[ENC_BR].Push = false;
        break;

    case State::MIDIPAGE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = MIDIThruMode;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = MIDIThru;
        encMap[ENC_TL].Range = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[MIDIThruMode];
        encMap[ENC_TL].ValueStr = MIDIThruStr[MIDIThru];
        encMap[ENC_TL].Push = false;

        encMap[ENC_TR].active = false;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = MIDIChIn;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = midiChannel;
        encMap[ENC_BL].Range = 16;
        if (midiChannel == 0)
        {
            encMap[ENC_BL].ValueStr = "All";
        }
        else
        {
            encMap[ENC_BL].ValueStr = String(midiChannel);
        }
        encMap[ENC_BL].ParameterStr = ParameterStrMap[MIDIChIn];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = MIDIChOut;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = midiOutCh;
        encMap[ENC_BR].Range = 16;
        if (midiOutCh == 0)
        {
            encMap[ENC_BR].ValueStr = "Off";
        }
        else
        {
            encMap[ENC_BR].ValueStr = String(midiOutCh);
        }
        encMap[ENC_BR].ParameterStr = ParameterStrMap[MIDIChOut];
        encMap[ENC_BR].Push = false;
        break;

    case State::PERFORMANCEENCEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = chooseEncoderTL;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.TL);
        encMap[ENC_TL].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_TL].Value]];
        encMap[ENC_TL].DefaultValue = 0;
        encMap[ENC_TL].Range = PARAMETERSFORENCS - 1;
        encMap[ENC_TL].ParameterStr = "";
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = chooseEncoderTR;
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.TR);
        encMap[ENC_TR].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_TR].Value]];
        encMap[ENC_TR].DefaultValue = 0;
        encMap[ENC_TR].Range = PARAMETERSFORENCS - 1;
        encMap[ENC_TR].ParameterStr = "";
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = chooseEncoderBL;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.BL);
        encMap[ENC_BL].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_BL].Value]];
        encMap[ENC_BL].DefaultValue = 0;
        encMap[ENC_BL].Range = PARAMETERSFORENCS - 1;
        encMap[ENC_BL].ParameterStr = "";
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = chooseEncoderBR;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.BR);
        encMap[ENC_BR].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_BR].Value]];
        encMap[ENC_BR].DefaultValue = 0;
        encMap[ENC_BR].Range = PARAMETERSFORENCS - 1;
        encMap[ENC_BR].ParameterStr = "";
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::PERFORMANCEENCEDIT;
        break;
    case State::PERFORMANCEMIDIEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = choosePerfMIDIThruMode;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = currentPerformance.patches[0].midiThru; // Multitimbrality not supported yet
        encMap[ENC_TL].Range = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[choosePerfMIDIThruMode];
        encMap[ENC_TL].ValueStr = MIDIThruStr[currentPerformance.patches[0].midiThru]; // Multitimbrality not supported yet
        encMap[ENC_TL].Push = false;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosePerfMIDIChIn;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = currentPerformance.patches[0].midiCh; // Multitimbrality not supported yet
        encMap[ENC_BL].Range = 16;
        if (currentPerformance.patches[0].midiCh == 0) // Multitimbrality not supported yet
        {
            encMap[ENC_BL].ValueStr = "All";
        }
        else
        {
            encMap[ENC_BL].ValueStr = String(currentPerformance.patches[0].midiCh); // Multitimbrality not supported yet
        }
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosePerfMIDIChIn];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = choosePerfMIDIChOut;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = currentPerformance.patches[0].midiChOut; // Multitimbrality not supported yet
        encMap[ENC_BR].Range = 16;
        if (currentPerformance.patches[0].midiChOut == 0) // Multitimbrality not supported yet
        {
            encMap[ENC_BR].ValueStr = "Off";
        }
        else
        {
            encMap[ENC_BR].ValueStr = String(currentPerformance.patches[0].midiChOut); // Multitimbrality not supported yet
        }
        encMap[ENC_BR].ParameterStr = ParameterStrMap[choosePerfMIDIChOut];
        encMap[ENC_BR].Push = false;
        break;
    case State::PERFORMANCEPATCHEDIT:
        encMap[ENC_TL].active = false;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = CCbankselectLSB;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[CCbankselectLSB];
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Push = true;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = patchselect;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[patchselect];
        encMap[ENC_BR].Push = true;
        break;
    case State::RENAMEPERFORMANCE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterPerformance;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 4; // More encoder rotation for accuracy
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterPerformance];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARPERFORMANCE;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterPerformance;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterPerformance];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARPERFORMANCE;

        encMap[ENC_BR].active = false;
        break;
    case State::PERFORMANCERECALL:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = PerfEdit;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[PerfEdit];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::PERFORMANCEPATCHEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = PerfSelect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = PERFORMANCES_LIMIT - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[PerfSelect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::PERFORMANCEPAGE;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = PerfDelete;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = PERFORMANCES_LIMIT - 1;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[PerfDelete];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::PERFORMANCERECALL;
        break;

    case State::PERFORMANCEPAGE:
        setConfigurationForCC(ENC_TL, currentPerformance.TL);
        setConfigurationForCC(ENC_TR, currentPerformance.TR);
        setConfigurationForCC(ENC_BL, currentPerformance.BL);
        setConfigurationForCC(ENC_BR, currentPerformance.BR);
        break;

    case State::SETTINGS:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = settingoption;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = settings::getSettingsOptionsSize() - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[settingoption];
        encMap[ENC_BL].ValueStr = settings::current_setting();
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = settingvalue;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = (int)settings::index();
        encMap[ENC_BR].Range = settings::current_setting_size() - 1;
        encMap[ENC_BR].ValueStr = settings::current_setting_value();
        encMap[ENC_BR].ParameterStr = ParameterStrMap[settingvalue];
        encMap[ENC_BR].Push = false;
        break;

    case State::ARPPAGE1:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::ARPPAGE1;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = ArpStyle; // Style
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = arpStyle;
        encMap[ENC_TR].ValueStr = ARP_STYLES[arpStyle];
        encMap[ENC_TR].Range = 5;
        encMap[ENC_TR].DefaultValue = 0;
        encMap[ENC_TR].ParameterStr = ParameterStrMap[ArpStyle];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::ARPPAGE1;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = ArpDivision; // Sixteenth, Eighth...
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].ValueStr = ARP_DIVISION_STR[arpDivision];
        encMap[ENC_BL].Value = arpDivision;
        encMap[ENC_BL].Range = 9;
        encMap[ENC_BL].DefaultValue = 2;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[ArpDivision];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::ARPPAGE1;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = ArpCycle;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].ValueStr = ARP_CYCLES[arpCycles];
        encMap[ENC_BR].Value = arpCycles;
        encMap[ENC_BR].Range = 4;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[ArpCycle];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::ARPPAGE1;
        break;
    case State::ARPPAGE2:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = ArpRange;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = ARP_RANGE_STR[arpRange];
        encMap[ENC_TL].Value = arpRange;
        encMap[ENC_TL].Range = 6;
        encMap[ENC_TL].DefaultValue = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[ArpRange];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::ARPPAGE2;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = ArpBasis;
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = arpBasis;
        encMap[ENC_TR].ValueStr = ARP_BASIS_STR[arpBasis];
        encMap[ENC_TR].Range = 4;
        encMap[ENC_TR].DefaultValue = 2;
        encMap[ENC_TR].ParameterStr = ParameterStrMap[ArpBasis];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::ARPPAGE2;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = ArpDivision; // Sixteenth, Eighth...
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].ValueStr = ARP_DIVISION_STR[arpDivision];
        encMap[ENC_BL].Value = arpDivision;
        encMap[ENC_BL].Range = 9;
        encMap[ENC_BL].DefaultValue = 2;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[ArpDivision];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::ARPPAGE2;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = ArpCycle;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].ValueStr = ARP_CYCLES[arpCycles];
        encMap[ENC_BR].Value = arpCycles;
        encMap[ENC_BR].Range = 4;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[ArpCycle];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::ARPPAGE2;
        break;
    case State::SEQUENCERECALL:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqEdit;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqEdit];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = goback;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[goback];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqSelect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = SEQUENCES_LIMIT - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqSelect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = SeqDelete;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = SEQUENCES_LIMIT - 1;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[SeqDelete];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::SEQUENCERECALL;
        break;
    case State::SEQUENCEPAGE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = goback;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 127;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[goback];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCERECALL;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqStartStop;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 0;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqStartStop];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_BR].active = false;
        break;

    case State::SEQUENCEEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = false; // value is shown in a different position
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = SeqLength;
        encMap[ENC_TR].ShowValue = false; // value is shown in a different position
        encMap[ENC_TR].Value = currentSequence.length - 1;
        encMap[ENC_TR].Range = 63;
        encMap[ENC_TR].ValueStr = SEQPOSSTR[currentSequence.length - 1];
        encMap[ENC_TR].ParameterStr = ParameterStrMap[SeqLength];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqPosition;
        encMap[ENC_BL].ShowValue = false; // value is shown in a different position
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 0;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqPosition];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = SeqNote;
        encMap[ENC_BR].ShowValue = false; // value is shown in a different position
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = 0;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[SeqNote];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::SEQUENCEEDIT;
        break;
    case State::EDITBANK:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deletebank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deletebank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::MAIN;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = bankeditselect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[bankeditselect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::MAIN;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = renamebank;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[renamebank];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::RENAMEBANK;
        break;

    case State::RENAMEBANK:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterBank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 0;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterBank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARBANK;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterBank;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterBank];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARBANK;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = savebank;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[savebank];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::RENAMESEQUENCE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterSequence;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 0;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterSequence];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARSEQUENCE;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterSequence;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterSequence];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARSEQUENCE;

        encMap[ENC_BR].active = false;
        break;
    default:
        break;
    }
}

FLASHMEM void setConfigurationForCC(uint8_t enc, uint8_t cc)
{
    LockGuard lock(displayLock); // The display renders from encMap
    switch (cc)
    {
    case CCunison:
        configureCCunison(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCglide:
        configureCCglide(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpitchenv:
        configureCCpitchenv(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscwaveformA:
        configureCCoscwaveformA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscwaveformB:
        configureCCoscwaveformB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpitchA:
        configureCCpitchA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpitchB:
        configureCCpitchB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCdetune:
        configureCCdetune(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmSourceA:
        configureCCpwmSourceA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmSourceB:
        configureCCpwmSourceB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmRateA:
        configureCCpwmRateA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmRateB:
        configureCCpwmRateB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmAmtA:
        configureCCpwmAmtA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwmAmtB:
        configureCCpwmAmtB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwA:
        configureCCpwA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCpwB:
        configureCCpwB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscLevelA:
        configureCCoscLevelA(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscLevelB:
        configureCCoscLevelB(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCnoiseLevel:
        configureCCnoiseLevel(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case filterfreq256:
        configurefilterfreq256(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterres:
        configureCCfilterres(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfiltermixer:
        configureCCfiltermixer(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterenv:
        configureCCfilterenv(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCkeytracking:
        configureCCkeytracking(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCosclfoamt:
        configureCCosclfoamt(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscLfoRate:
        configureCCoscLfoRate(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscLfoWaveform:
        configureCCoscLfoWaveform(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCosclforetrig:
        configureCCosclforetrig(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterLFOMidiClkSync:
        configureCCfilterLFOMidiClkSync(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterlforate:
        configureCCfilterlforate(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterlfoamt:
        configureCCfilterlfoamt(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterlfowaveform:
        configureCCfilterlfowaveform(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterlforetrig:
        configureCCfilterlforetrig(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscLFOMidiClkSync:
        configureCCoscLFOMidiClkSync(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterattack:
        configureCCfilterattack(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterdecay:
        configureCCfilterdecay(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfiltersustain:
        configureCCfiltersustain(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterrelease:
        configureCCfilterrelease(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCampattack:
        configureCCampattack(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCampdecay:
        configureCCampdecay(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCampsustain:
        configureCCampsustain(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCamprelease:
        configureCCamprelease(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCoscfx:
        configureCCoscfx(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxamt:
        configureCCensemblefxamt(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxmix:
        configureCCensemblefxmix(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxtime:
        configureCCreverbfxtime(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxmix:
        configureCCreverbfxmix(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCvelocitySens:
        configureCCAmpVelocitySens(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCmonomode:
        configureCCmonomode(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case ampenvshape:
        configureampenvshape(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case filterenvshape:
        configurefilterenvshape(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case glideshape:
        configureglideshape(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case pitchbendrange:
        configurepitchbendrange(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case pitchmodwheeldepth:
        configurepitchmodwheeldepth(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case filtermodwheeldepth:
        configurefiltermodwheeldepth(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    default:
        configureOff(&encMap[enc]);
        break;
    }
}