#ifndef MOMENTUM_VOICE_GROUP_H
#define MOMENTUM_VOICE_GROUP_H

#include <Arduino.h>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include "Voice.h"
#include "MonoNoteHistory.h"
#include "Constants.h"

#define VG_FOR_EACH_OSC(CMD) VG_FOR_EACH_VOICE(voices[i]->patch().CMD)
#define VG_FOR_EACH_VOICE(CMD)                  \
    for (uint8_t i = 0; i < voices.size(); i++) \
    {                                           \
        CMD;                                    \
    }

// These are here because of a Settings.h circular dependency.
// Maximum combined gain of Osc 1 + Osc 2 + XOR into the waveform mixer.
// Band-limited squares peak at ~0.95 FS, so a higher total clips the 16-bit
// mixer and the clipped edges alias badly.
#define OSCMIX_MAX_SUM 1.0f

#define LFO_SMOOTH_MS 1.0f // smoothing of stepped LFO waveforms

#define MONOPHONIC_OFF 0
#define MONOPHONIC_LAST 1
#define MONOPHONIC_FIRST 2
#define MONOPHONIC_HIGHEST 3
#define MONOPHONIC_LOWEST 4
// Legato isn't supported, the envelope state from the previous note needs to transfer to the new note
#define MONOPHONIC_LEGATO 5

const static uint32_t WAVEFORM_PARABOLIC = 103;
const static uint32_t WAVEFORM_HARMONIC = 104;

class VoiceGroup
{
private:
    String patchName;
    uint32_t patchIndex;
    uint32_t UID;

    // Audio Objects
    PatchShared &shared;
    std::vector<Voice *> voices;

    // Patch Configs
    bool midiClockSignal;
    bool filterLfoMidiClockSync;
    bool pitchLFOMidiClockSync;

    VoiceParams _params;
    uint8_t unisonNotesOn;
    uint8_t voicesOn;
    uint8_t monoNote;
    uint8_t monophonic;
    uint8_t waveformA;
    uint8_t waveformB;
    float pitchEnvelope;
    uint8_t pwmSourceA;
    uint8_t pwmSourceB;
    float pwmAmtA;
    float pwmAmtB;
    float pwA;
    float pwB;
    float pwmRateA;
    float pwmRateB;
    uint8_t oscFX;
    float oscLevelA;
    float oscLevelB;
    float waveformLevels[4] = {1.0f, 1.0f, 1.0f, 0.0f}; // Requested waveform mixer levels (before headroom scaling)
    float noiseContribution = 0.0f;                      // Noise level feeding waveform mixer channel 2
    float oscMixCompensation = 1.0f;                     // Restores loudness lost to headroom scaling, applied after the filter
    float cutoff;
    float resonance;
    float filterMixer;
    float filterEnvelope;
    float filterAttack;
    float filterDecay;
    float filterSustain;
    float filterRelease;
    float ampAttack;
    float ampDecay;
    float ampSustain;
    float ampRelease;
    float filterLfoRetrig;
    float filterLfoRate;
    float filterLfoAmt;
    uint8_t filterLfoWaveform;
    float pinkLevel;
    float whiteLevel;
    uint8_t pitchLfoWaveform;
    bool pitchLfoRetrig;
    bool sameVoiceRetrig = false;
    float pitchLfoAmount;
    float pitchLfoRate;
    float pitchModWheelDepth;
    float filterModWheelDepth;
    float ensembleEffectAmount;
    float ensembleEffectMix;
    float reverbEffectTime;
    float reverbEffectMix;

    // Used to remember active mono notes.
    MonoNoteHistory noteStack;

public:
    VoiceGroup(PatchShared &shared_) : patchName(""),
                                       patchIndex(0),
                                       shared(shared_),
                                       midiClockSignal(false),
                                       filterLfoMidiClockSync(false),
                                       pitchLFOMidiClockSync(false),
                                       unisonNotesOn(0),
                                       voicesOn(0),
                                       monophonic(0),
                                       waveformA(WAVEFORM_SQUARE),
                                       waveformB(WAVEFORM_SQUARE),
                                       pitchEnvelope(0.0f),
                                       pwmSourceA(PWMSOURCELFO),
                                       pwmSourceB(PWMSOURCELFO),
                                       pwmAmtA(1.0f),
                                       pwmAmtB(1.0f),
                                       pwA(0.0f),
                                       pwB(0.0f),
                                       pwmRateA(0.5),
                                       pwmRateB(0.5f),
                                       oscFX(0),
                                       oscLevelA(1.0f),
                                       oscLevelB(1.0f),
                                       cutoff(12000.0f),
                                       resonance(0.71f),
                                       filterMixer(0.0f),
                                       filterEnvelope(0.0f),
                                       filterAttack(100.0f),
                                       filterDecay(350.0f),
                                       filterSustain(0.7f),
                                       filterRelease(300.0f),
                                       ampAttack(10.0f),
                                       ampDecay(35.0f),
                                       ampSustain(1.0f),
                                       ampRelease(300.0f),
                                       filterLfoRetrig(false),
                                       filterLfoRate(2.0),
                                       filterLfoAmt(0.0f),
                                       filterLfoWaveform(WAVEFORM_SINE),
                                       pinkLevel(0.0f),
                                       whiteLevel(0.0f),
                                       pitchLfoWaveform(WAVEFORM_SINE),
                                       pitchLfoRetrig(false),
                                       pitchLfoAmount(0.0f),
                                       pitchLfoRate(4.0f),
                                       pitchModWheelDepth(0.0f),
                                       filterModWheelDepth(0.0f),
                                       ensembleEffectAmount(1.0f),
                                       ensembleEffectMix(0.0f),
                                       reverbEffectTime(1.0f),
                                       reverbEffectMix(0.0f)
    {
        _params.keytrackingAmount = 0.0f;
        _params.mixerLevel = 0.0f;
        _params.prevNote = 48;
        _params.glideSpeed = 0.0f;
        _params.unisonMode = 0;
        _params.chordDetune = 0;
        _params.detune = 0.0f;
        _params.oscPitchA = 0;
        _params.oscPitchB = 12;

        shared.noiseMixer.gain(0, 0.0f);
        shared.noiseMixer.gain(1, 0.0f);

        shared.pitchLfo.begin(WAVEFORM_SINE);
        shared.pwmLfoA.amplitude(ONE);
        shared.pwmLfoA.begin(PWMWAVEFORM);
        shared.pwmLfoB.amplitude(ONE);
        shared.pwmLfoB.begin(PWMWAVEFORM);

        // Round off the jumps of square, saw and sample & hold LFOs so they don't click
        shared.pitchLfo.smoothing(LFO_SMOOTH_MS);
        shared.filterLfo.smoothing(LFO_SMOOTH_MS);
        shared.pwmLfoA.smoothing(LFO_SMOOTH_MS);
        shared.pwmLfoB.smoothing(LFO_SMOOTH_MS);

        setEnsembleEffectAmount(ensembleEffectAmount);
        setEnsembleEffectMix(ensembleEffectMix);

        setReverbEffectTime(reverbEffectTime);
        setReverbEffectMix(reverbEffectMix);
    }

    inline uint8_t size() { return this->voices.size(); }
    inline String getPatchName() { return this->patchName; }
    bool getFilterLfoMidiClockSync() { return filterLfoMidiClockSync; }
    bool getPitchLfoMidiClockSync() { return pitchLFOMidiClockSync; }
    inline uint32_t getPatchIndex() { return this->patchIndex; }
    inline uint32_t getUID() { return this->UID; }
    float getKeytrackingAmount() { return this->_params.keytrackingAmount; }
    uint8_t getMonophonicMode() { return this->monophonic; }
    uint32_t getWaveformA() { return waveformA; }
    uint32_t getWaveformB() { return waveformB; }
    float getPitchEnvelope() { return pitchEnvelope; }
    uint8_t getPwmSourceA() { return pwmSourceA; }
    uint8_t getPwmSourceB() { return pwmSourceB; }
    float getPwA() { return pwA; }
    float getPwB() { return pwB; }
    float getPwmAmtA() { return pwmAmtA; }
    float getPwmAmtB() { return pwmAmtB; }
    float getPwmRateA() { return pwmRateA; }
    float getPwmRateB() { return pwmRateB; }
    uint8_t getOscFX() { return oscFX; }
    float getOscLevelA() { return oscLevelA; }
    float getOscLevelB() { return oscLevelB; }
    float getCutoff() { return cutoff; }
    float getResonance() { return resonance; }
    float getFilterMixer() { return filterMixer; }
    float getFilterEnvelope() { return filterEnvelope; }
    float getFilterAttack() { return filterAttack; }
    float getFilterDecay() { return filterDecay; }
    float getFilterSustain() { return filterSustain; }
    float getFilterRelease() { return filterRelease; }
    float getAmpAttack() { return ampAttack; }
    float getAmpDecay() { return ampDecay; }
    float getAmpSustain() { return ampSustain; }
    float getAmpRelease() { return ampRelease; }
    bool getFilterLfoRetrig() { return filterLfoRetrig; }
    float getFilterLfoRate() { return filterLfoRate; }
    float getFilterLfoAmt() { return filterLfoAmt; }
    uint32_t getFilterLfoWaveform() { return filterLfoWaveform; }
    float getPinkNoiseLevel() { return pinkLevel; }
    float getWhiteNoiseLevel() { return whiteLevel; }
    bool getPitchLfoRetrig() { return pitchLfoRetrig; }
    uint32_t getPitchLfoWaveform() { return pitchLfoWaveform; }
    float getPitchLfoAmount() { return pitchLfoAmount; }
    float getPitchLfoRate() { return pitchLfoRate; }
    float getPitchModWheelDepth() { return pitchModWheelDepth; }
    float getfilterModWheelDepth() { return filterModWheelDepth; }
    float getEnsebleEffectAmount() { return ensembleEffectAmount; }
    float getEnsembleEffectMix() { return ensembleEffectMix; }
    float getReverbEffectTime() { return reverbEffectTime; }
    float getReverbEffectMix() { return reverbEffectMix; }

    inline void setPatchName(String name)
    {
        this->patchName = name;
    }

    inline void setPatchIndex(uint32_t index)
    {
        this->patchIndex = index;
    }

    inline void setUID(uint32_t uid)
    {
        this->UID = uid;
    }

    void setWaveformA(uint32_t waveform)
    {
        if (waveformA == waveform)
            return;
        waveformA = waveform;

        int temp = waveform;
        if (waveform == WAVEFORM_PARABOLIC)
        {
            VG_FOR_EACH_OSC(waveformMod_a.arbitraryWaveform(PARABOLIC_WAVE, AWFREQ));
            temp = WAVEFORM_ARBITRARY;
        }
        if (waveform == WAVEFORM_HARMONIC)
        {
            VG_FOR_EACH_OSC(waveformMod_a.arbitraryWaveform(HARMONIC_WAVE, AWFREQ));
            temp = WAVEFORM_ARBITRARY;
        }

        VG_FOR_EACH_OSC(waveformMod_a.begin(temp))
    }

    void setWaveformB(uint32_t waveform)
    {
        if (waveformB == waveform)
            return;
        waveformB = waveform;

        int temp = waveform;
        if (waveform == WAVEFORM_PARABOLIC)
        {
            VG_FOR_EACH_OSC(waveformMod_b.arbitraryWaveform(PARABOLIC_WAVE, AWFREQ));
            temp = WAVEFORM_ARBITRARY;
        }
        if (waveform == WAVEFORM_HARMONIC)
        {
            VG_FOR_EACH_OSC(waveformMod_b.arbitraryWaveform(PPG_WAVE, AWFREQ));
            temp = WAVEFORM_ARBITRARY;
        }

        VG_FOR_EACH_OSC(waveformMod_b.begin(temp))
    }

    void setPwmRateA(float value)
    {
        pwmRateA = value;

        shared.pwmLfoA.frequency(pwmRateA);

        if (pwmSourceA == PWMSOURCEMANUAL)
        {
            // Set to fixed PW mode
            this->setPwmMixerALFO(0);  // LFO Source off
            this->setPwmMixerAFEnv(0); // Filter Env Source off
            this->setPwmMixerAPW(1);   // Manually adjustable pulse width on
        }
        else if (pwmSourceA == PWMSOURCEFENV)
        {
            this->setPwmMixerAFEnv(this->getPwmAmtA());
            this->setPwmMixerAPW(0);
        }
        else
        {
            this->setPwmMixerAPW(0);
        }
    }

    void setPwmRateB(float value)
    {
        pwmRateB = value;

        shared.pwmLfoB.frequency(pwmRateB);

        if (pwmSourceB == PWMSOURCEMANUAL)
        {
            // Set to fixed PW mode
            this->setPwmMixerBLFO(0);  // LFO Source off
            this->setPwmMixerBFEnv(0); // Filter Env Source off
            this->setPwmMixerBPW(1);   // Manually adjustable pulse width on
        }
        else if (pwmSourceB == PWMSOURCEFENV)
        {
            this->setPwmMixerBFEnv(this->getPwmAmtB());
            this->setPwmMixerBPW(0);
        }
        else
        {
            this->setPwmMixerBPW(0);
        }
    }

    void setPitchEnvelope(float value)
    {
        pitchEnvelope = value;
        VG_FOR_EACH_OSC(oscModMixer_a.gain(1, value))
        VG_FOR_EACH_OSC(oscModMixer_b.gain(1, value))
    }

    void setPwmMixerALFO(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_a.gain(0, value));
    }

    void setPwmMixerBLFO(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_b.gain(0, value));
    }

    void setPwmMixerAPW(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_a.gain(1, value));
    }

    void setPwmMixerBPW(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_b.gain(1, value));
    }

    void setPwmMixerAFEnv(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_a.gain(2, value));
    }

    void setPwmMixerBFEnv(float value)
    {
        VG_FOR_EACH_OSC(pwMixer_b.gain(2, value));
    }

    void setPWA(float valuePwA)
    {
        pwA = valuePwA;
        // Prevent silence when pw = +/-1.0 on pulse
        float pwA_Adj = pwA;
        if (pwA > 0.98)
            pwA_Adj = 0.98f;
        if (pwA < -0.98)
            pwA_Adj = -0.98f;
        shared.pwa.amplitude(pwA_Adj);
    }

    void setPWB(float valuePwB)
    {
        pwB = valuePwB;
        // Prevent silence when pw = +/-1.0 on pulse
        float pwB_Adj = pwB;
        if (pwB > 0.98)
            pwB_Adj = 0.98f;
        if (pwB < -0.98)
            pwB_Adj = -0.98f;
        shared.pwb.amplitude(pwB_Adj);
    }

    void setPWMSourceA(uint8_t value)
    {
        pwmSourceA = value;
        if (value == PWMSOURCELFO)
        {
            this->setPwmMixerAPW(0);        // Set PW man to zero
            this->setPwmMixerAFEnv(0);      // Set filter mod to zero
            this->setPwmMixerALFO(pwmAmtA); // Set LFO mod
        }
        else if (value == PWMSOURCEFENV)
        {
            this->setPwmMixerAPW(0);         // Set PW man to zero
            this->setPwmMixerALFO(0);        // Set LFO mod to zero
            this->setPwmMixerAFEnv(pwmAmtA); // Set filter mod
        }
        else
        {
            // PW fixed manual
            this->setPwmMixerAPW(1);   // Set PW man to on
            this->setPwmMixerALFO(0);  // Set LFO mod to zero
            this->setPwmMixerAFEnv(0); // Set filter mod zero
        }
    }

    void setPWMSourceB(uint8_t value)
    {
        pwmSourceB = value;
        if (value == PWMSOURCELFO)
        {
            this->setPwmMixerBPW(0);        // Set PW man to zero
            this->setPwmMixerBFEnv(0);      // Set filter mod to zero
            this->setPwmMixerBLFO(pwmAmtB); // Set LFO mod
        }
        else if (value == PWMSOURCEFENV)
        {
            this->setPwmMixerBPW(0);         // Set PW man to zero
            this->setPwmMixerBLFO(0);        // Set LFO mod to zero
            this->setPwmMixerBFEnv(pwmAmtB); // Set filter mod
        }
        else
        {
            // PW fixed manual
            this->setPwmMixerBPW(1);   // Set PW man to on
            this->setPwmMixerBLFO(0);  // Set LFO mod to zero
            this->setPwmMixerBFEnv(0); // Set filter mod zero
        }
    }

    void setWaveformMixerLevel(int channel, float level)
    {
        waveformLevels[channel] = level;
        applyWaveformMixerLevels();
    }

    // Scale the oscillator and noise channels together so their sum can't clip the mixer
    void applyWaveformMixerLevels()
    {
        float sum = waveformLevels[0] + waveformLevels[1] + waveformLevels[3] + waveformLevels[2] * noiseContribution;
        float scale = sum > OSCMIX_MAX_SUM ? OSCMIX_MAX_SUM / sum : 1.0f;
        // Uncorrelated sources add by power, so compensate by sqrt to keep perceived level
        oscMixCompensation = 1.0f / sqrtf(scale);
        VG_FOR_EACH_OSC(waveformMixer_.gain(0, waveformLevels[0] * scale))
        VG_FOR_EACH_OSC(waveformMixer_.gain(1, waveformLevels[1] * scale))
        VG_FOR_EACH_OSC(waveformMixer_.gain(2, waveformLevels[2] * scale))
        VG_FOR_EACH_OSC(waveformMixer_.gain(3, waveformLevels[3] * scale))
    }

    void updateNoiseContribution(float gain)
    {
        noiseContribution = (pinkLevel + whiteLevel) * gain;
        applyWaveformMixerLevels();
    }

    // Call regularly from the main loop: starts note-ons deferred for stolen voices
    void serviceVoices()
    {
        for (uint8_t i = 0; i < voices.size(); i++)
            voices[i]->service();
    }

    void setOscModMixerA(int channel, float level)
    {
        VG_FOR_EACH_OSC(oscModMixer_a.gain(channel, level))
    }

    void setOscModMixerB(int channel, float level)
    {
        VG_FOR_EACH_OSC(oscModMixer_b.gain(channel, level))
    }

    void setOscFXCombineMode(AudioEffectDigitalCombine::combineMode mode)
    {
        VG_FOR_EACH_OSC(oscFX_.setCombineMode(mode))
    }

    void setOscLevelA(float value)
    {
        if (oscFX == OSCFXXMOD)
        {
            value < 0.01f ? oscLevelA = 0.01f : oscLevelA = value;
        }
        else
        {
            oscLevelA = value;
        }
        switch (oscFX)
        {
        case OSCFXXOR:                                                // XOR
            setWaveformMixerLevel(0, oscLevelA);                      // Osc 1 (A)
            setWaveformMixerLevel(3, (oscLevelA + oscLevelB) / 2.0f); // oscFX XOR level
            break;
        case OSCFXXMOD: // XMod
            setOscModMixerB(3, 1 - oscLevelA);
            break;
        case OSCFXOFF:                           // None
            setOscModMixerA(3, 0);               // Feed from Osc 2 (B)
            setWaveformMixerLevel(0, oscLevelA); // Osc 1 (A)
            setWaveformMixerLevel(3, 0);         // XOR
            break;
        }
    }

    void setOscLevelB(float value)
    {
        if (oscFX == OSCFXXMOD)
        {
            value < 0.01f ? oscLevelB = 0.01f : oscLevelB = value;
        }
        else
        {
            oscLevelB = value;
        }

        switch (oscFX)
        {
        case OSCFXXOR:                                                // XOR
            setWaveformMixerLevel(1, oscLevelB);                      // Osc 2 (B)
            setWaveformMixerLevel(3, (oscLevelA + oscLevelB) / 2.0f); // oscFX XOR level
            break;
        case OSCFXXMOD:                        // XMod
            setOscModMixerA(3, 1 - oscLevelB); // Feed from Osc 2 (B)
            break;
        case OSCFXOFF:                           // None
            setOscModMixerB(3, 0);               // Feed from Osc 1 (A)
            setWaveformMixerLevel(1, oscLevelB); // Osc 2 (B)
            setWaveformMixerLevel(3, 0);         // XOR
            break;
        }
    }

    void setOscFX(uint8_t value)
    {
        oscFX = value;

        if (oscFX == 2)
        {
            // if (oscLevelA == 1.0f && oscLevelB <= 1.0f)
            if (oscLevelA > oscLevelB)
            {
                setOscModMixerA(3, 1 - oscLevelB); // Feed from Osc 2 (B)
                setWaveformMixerLevel(0, ONE);     // Osc 1 (A)
                setWaveformMixerLevel(1, 0);       // Osc 2 (B)
            }
            else
            {
                setOscModMixerB(3, 1 - oscLevelA); // Feed from Osc 1 (A)
                setWaveformMixerLevel(0, 0);       // Osc 1 (A)
                setWaveformMixerLevel(1, ONE);     // Osc 2 (B)
            }
            // Set XOR type off
            setOscFXCombineMode(AudioEffectDigitalCombine::OFF);
            setWaveformMixerLevel(3, 0); // XOR
        }
        else if (oscFX == 1)
        {
            setOscModMixerA(3, 0); // XMod off
            setOscModMixerB(3, 0); // XMod off
            // XOR 'Ring Mod' type effect
            setOscFXCombineMode(AudioEffectDigitalCombine::XOR);
            setWaveformMixerLevel(3, (oscLevelA + oscLevelB) / 2.0f); // XOR on
        }
        else
        {
            setOscModMixerA(3, 0);                               // XMod off
            setOscModMixerB(3, 0);                               // XMod off
            setOscFXCombineMode(AudioEffectDigitalCombine::OFF); // Set XOR type off
            setWaveformMixerLevel(0, oscLevelA);                 // Osc 1 (A)
            setWaveformMixerLevel(1, oscLevelB);                 // Osc 2 (B)
            setWaveformMixerLevel(3, 0);                         // XOR off
        }
    }

    void setCutoff(float value)
    {
        this->cutoff = value;

        VG_FOR_EACH_OSC(filter_.frequency(value))

        // float filterOctave = 0.0;
        // // Altering filterOctave to give more cutoff width for deeper bass, but sharper cutoff at higher frequncies
        // // This is how TSynth sounds different from other Teensy Audio Library based synths
        // if (value <= 2000)
        // {
        //     filterOctave = 4.0f + ((2000.0f - value) / 710.0f); // More bass
        // }
        // else if (value > 2000 && value <= 3500)
        // {
        //     filterOctave = 3.0f + ((3500.0f - value) / 1500.0f); // Sharper cutoff
        // }
        // else if (value > 3500 && value <= 7000)
        // {
        //     filterOctave = 2.0f + ((7000.0f - value) / 4000.0f); // Sharper cutoff
        // }
        // else
        // {
        //     filterOctave = 1.0f + ((12000.0f - value) / 5100.0f); // Sharper cutoff
        // }

        // VG_FOR_EACH_OSC(filter_.octaveControl(filterOctave))
    }

    void setResonance(float value)
    {
        resonance = value;
        VG_FOR_EACH_OSC(filter_.resonance(value))
    }

    void setFilterMixer(float value)
    {
        filterMixer = value;

        float LP = 1.0f;
        float BP = 0;
        float HP = 0;

        if (value == BANDPASS || value >= 2.0f)
        {
            // Bandpass
            LP = 0;
            BP = 1.0f;
            HP = 0;
        }
        else if (value > 1.0f)
        {
            // Highpass fading into bandpass (top of the knob)
            LP = 0;
            BP = value - 1.0f;
            HP = 1.0f - BP;
        }
        else
        {
            // LP-HP mix at constant level: from LP the highs come in until a full level notch
            // at the middle (LP + HP), then the lows go out to HP. (A plain crossfade was 6dB
            // down at the middle.)
            LP = value < 0.5f ? 1.0f : 2.0f * (1.0f - value);
            HP = value > 0.5f ? 1.0f : 2.0f * value;
        }

        // Mixed inside the filter: gains smoothed per sample, soft limiter on the mix
        VG_FOR_EACH_OSC(filter_.mix(LP, BP, HP))
    }

    void setFilterEnvelope(float value)
    {
        filterEnvelope = value;
        VG_FOR_EACH_OSC(filterModMixer_.gain(0, filterEnvelope))
    }

    void setFilterAttack(float value)
    {
        filterAttack = value;
        VG_FOR_EACH_OSC(filterEnvelope_.attack(value))
    }

    void setFilterDecay(float value)
    {
        filterDecay = value;
        VG_FOR_EACH_OSC(filterEnvelope_.decay(value))
    }

    void setFilterSustain(float value)
    {
        filterSustain = value;
        VG_FOR_EACH_OSC(filterEnvelope_.sustain(value))
    }

    void setFilterRelease(float value)
    {
        filterRelease = value;
        VG_FOR_EACH_OSC(filterEnvelope_.release(value))
    }

    void setAmpAttack(float value)
    {
        ampAttack = value;
        VG_FOR_EACH_OSC(ampEnvelope_.attack(value))
    }

    void setAmpDecay(float value)
    {
        ampDecay = value;
        VG_FOR_EACH_OSC(ampEnvelope_.decay(value))
    }

    void setAmpSustain(float value)
    {
        ampSustain = value;
        VG_FOR_EACH_OSC(ampEnvelope_.sustain(value))
    }

    void setAmpRelease(float value)
    {
        ampRelease = value;
        VG_FOR_EACH_OSC(ampEnvelope_.release(value))
    }

    void setKeytracking(float value)
    {
        _params.keytrackingAmount = value;
        VG_FOR_EACH_VOICE(voices[i]->updateKeytracking(_params)) // Keep held notes in step
    }

    void setFilterModWheelDepth(float value)
    {
        filterModWheelDepth = value;
    }

    void setFilterModWheelAmount(float value)
    {
        VG_FOR_EACH_OSC(filter_.frequency(value + cutoff))
    }

    void setFilterLfoRetrig(bool value)
    {
        filterLfoRetrig = value;
        shared.filterLfo.sync();
    }

    void setFilterLfoRate(float value)
    {
        filterLfoRate = value;
        shared.filterLfo.frequency(value);
    }

    void setFilterLfoAmt(float value)
    {
        filterLfoAmt = value;
        shared.filterLfo.amplitude(value);
    }

    void setFilterLfoWaveform(int waveform)
    {
        if (filterLfoWaveform == waveform)
            return;
        filterLfoWaveform = waveform;
        shared.filterLfo.begin(filterLfoWaveform);
    }

    void setPinkNoiseLevel(float value)
    {
        pinkLevel = value;
        float gain;
        if (_params.unisonMode == 0)
            gain = 1.0;
        else
            gain = UNISONNOISEMIXERLEVEL;
        shared.noiseMixer.gain(0, pinkLevel * gain);
        updateNoiseContribution(gain);
    }

    void setWhiteNoiseLevel(float value)
    {
        whiteLevel = value;
        float gain;
        if (_params.unisonMode == 0)
            gain = 1.0;
        else
            gain = UNISONNOISEMIXERLEVEL;
        shared.noiseMixer.gain(1, whiteLevel * gain);
        updateNoiseContribution(gain);
    }

    void setPitchLfoRetrig(bool value)
    {
        pitchLfoRetrig = value;
        shared.pitchLfo.sync();
    }

    void setPitchLfoWaveform(uint32_t waveform)
    {
        if (pitchLfoWaveform == waveform)
            return;
        pitchLfoWaveform = waveform;
        shared.pitchLfo.begin(waveform);
    }

    void setPitchLfoAmount(float value)
    {
        pitchLfoAmount = value;
        shared.pitchLfo.amplitude(value);
    }

    void setPitchLfoRate(float value)
    {
        pitchLfoRate = value;
        shared.pitchLfo.frequency(value);
    }

    void setPitchModWheelAmount(float value)
    {
        shared.pitchLfo.amplitude(value + pitchLfoAmount);
    }

    void setPitchModWheelDepth(float value)
    {
        pitchModWheelDepth = value;
    }

    void setEnsembleEffectAmount(float value)
    {
        ensembleEffectAmount = value;
        shared.ensemble.lfoRate(ensembleEffectAmount);
    }

    void setEnsembleEffectMix(float value)
    {
        ensembleEffectMix = value;
        shared.ensembleEffectMixerL.gain(0, 1.0f - ensembleEffectMix); // Dry
        shared.ensembleEffectMixerL.gain(1, ensembleEffectMix);        // Wet
        shared.ensembleEffectMixerR.gain(0, 1.0f - ensembleEffectMix); // Dry
        shared.ensembleEffectMixerR.gain(1, ensembleEffectMix);        // Wet
    }

    void setReverbEffectTime(float value)
    {
        reverbEffectTime = value;
        if (shared.reverb)
            shared.reverb->size(reverbEffectTime);
    }

    void setReverbEffectMix(float value)
    {
        reverbEffectMix = value;
        if (!shared.reverb)
            return;
        // Mix is up to 50:50, as with the previous reverb
        shared.reverb->mix(reverbEffectMix / 2.0f);
        // Bypassed at zero mix to save CPU time
        shared.reverb->bypass_set(reverbEffectMix <= 0.0f);
    }

    inline void setMonophonic(uint8_t mode)
    {
        if (mode != this->monophonic)
        {
            noteStack.clear();
        }
        this->monophonic = mode;
    }

    void setUnisonMode(uint8_t mode)
    {
        if (mode == 0)
            allNotesOff();

        this->_params.unisonMode = mode;
        this->unisonNotesOn = 0;

        // Update noise gain
        setPinkNoiseLevel(pinkLevel);
        setWhiteNoiseLevel(whiteLevel);
    }

    void setFilterLfoMidiClockSync(bool value)
    {
        filterLfoMidiClockSync = value;
        shared.filterLfo.frequency(lfoSyncFreq * lfoFilterTempoValue);
        // filterMidiClock(lfoSyncFreq * lfoFilterTempoValue);
    }

    void setPitchLfoMidiClockSync(bool value)
    {
        pitchLFOMidiClockSync = value;
        shared.pitchLfo.frequency(lfoSyncFreq * lfoPitchTempoValue);
        pitchMidiClock(lfoSyncFreq * lfoPitchTempoValue);
    }

    inline uint8_t unisonNotes()
    {
        return this->unisonNotesOn;
    }

    //
    // Configure the group
    //
    VoiceParams &params()
    {
        return _params;
    }

    //
    // Modify the group
    //

    // Splits out num voices from the current group into a new group.
    Voice *pop()
    {
        if (voices.size() == 0)
        {
            return nullptr;
        }
        Voice *result = voices.back();
        voices.pop_back();
        return result;
    }

    void add(Voice *v)
    {
        Mixer *m = v->patch().connectTo(shared, voices.size());
        v->setMixer(m);
        voices.push_back(v);
    }

    // Merges the other VoiceGroup into this one, making additional voices
    // available to this group.
    void merge(VoiceGroup other)
    {
    }

    //
    // Use the group
    //

    void noteOn(uint8_t note, uint8_t velocity)
    {
        if (this->monophonic)
        {
            handleMonophonicNoteOn(note, velocity);
            return;
        }
        noteOn(note, velocity, false);
    }

    void allNotesOn(uint8_t note, int velocity, uint8_t id)
    {
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            voices[i]->noteOn(note, velocity, this->_params, unisonNotesOn, id);
        }
    }

    void allNotesOff()
    {
        this->unisonNotesOn = 0;
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            voices[i]->noteOff();
        }
        // Keep the voice count and mono note history in step, or unison voice allocation
        // and mono retriggering work from stale notes
        this->voicesOn = 0;
        noteStack.clear();
    }

    // Silence everything at once: notes off and envelopes closed
    void panic()
    {
        allNotesOff();
        closeEnvelopes();
    }

    void updateVoices()
    {
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            voices[i]->updateVoice(this->_params, unisonNotesOn);
        }
    }

    void noteOff(uint8_t note)
    {
        if (this->monophonic)
        {
            handleMonophonicNoteOff(note);
            return;
        }

        uint8_t num = 0;
        switch (this->_params.unisonMode)
        {
        case 0:
            num = noteOff(note, false); // changed for experiment
            break;
        default:
            num = noteOff(note, true);
            break;
        }

        // Decrement unison note count if noteOff modified a voice.
        if (this->unisonNotesOn > 0 && num > 0)
            this->unisonNotesOn--;
    }

    void pitchBend(float amount)
    {
        shared.pitchBend.amplitude(amount);
    }

    void setVolume(float amount)
    {
        shared.volumeMixer.gain(0, amount);
    }

    void midiClockStart()
    {
        midiClockSignal = true;
        // Start of the song: synced LFOs start their cycle on the first beat
        // (previously only the filter LFO was reset)
        if (filterLfoMidiClockSync)
            shared.filterLfo.setPhase(0.0f);
        if (pitchLFOMidiClockSync)
            shared.pitchLfo.setPhase(0.0f);
    }

    // Called on every MIDI clock beat (24 clocks). Sets synced LFOs to the tempo and keeps
    // them in phase with the beat: the phase error is taken out over the next beat by
    // adjusting the rate (no jump), or by setting the phase if it is far out. Previously
    // only the rate was set, so a synced LFO slowly drifted against the music.
    // Divisions are lfoFilterTempoValue / lfoPitchTempoValue, in LFO cycles per beat.
    void midiClockBeat(uint32_t beat, float beatSeconds)
    {
        midiClockSignal = true;
        if (filterLfoMidiClockSync)
            lockLfo(shared.filterLfo, lfoFilterTempoValue, beat, beatSeconds, filterLfoRetrig);
        if (pitchLFOMidiClockSync)
            lockLfo(shared.pitchLfo, lfoPitchTempoValue, beat, beatSeconds, pitchLfoRetrig);
    }

    static void lockLfo(AudioSynthWaveformTS &lfo, float cyclesPerBeat, uint32_t beat, float beatSeconds, bool retrig)
    {
        if (beatSeconds <= 0.0f)
            return;
        const float rate = cyclesPerBeat / beatSeconds;
        if (retrig)
        {
            // Restarted by notes, so not locked to the beat
            lfo.frequency(rate);
            return;
        }
        const double target = (double)beat * cyclesPerBeat;
        float error = (float)(target - floor(target)) - lfo.getPhase();
        error -= roundf(error); // -0.5 .. 0.5 of a cycle
        if (fabsf(error) > 0.25f)
        {
            lfo.setPhase((float)(target - floor(target)));
            lfo.frequency(rate);
        }
        else
        {
            lfo.frequency(rate + error / beatSeconds);
        }
    }

    void setSameVoiceRetrig(bool same) { sameVoiceRetrig = same; }

    // Audio CPU overload (called from the audio interrupt): silence one voice to bring the
    // load down - a voice still in its release tail if there is one, else the oldest held note
    void shedVoice()
    {
        Voice *victim = nullptr;
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            Voice *v = voices[i];
            if (!v->on() && v->sounding() && (victim == nullptr || v->timeOn() < victim->timeOn()))
                victim = v;
        }
        if (victim == nullptr)
        {
            for (uint8_t i = 0; i < voices.size(); i++)
            {
                Voice *v = voices[i];
                if (v->on() && (victim == nullptr || v->timeOn() < victim->timeOn()))
                    victim = v;
            }
        }
        if (victim != nullptr && victim->forceSilence() && this->voicesOn > 0)
            this->voicesOn--;
    }

    void midiClockStop()
    {
        midiClockSignal = false;
    }

    void filterMidiClock(float frequency)
    {
        midiClockSignal = true;

        if (filterLfoMidiClockSync)
        {
            shared.filterLfo.frequency(frequency);
        }
    }

    void pitchMidiClock(float frequency)
    {
        midiClockSignal = true;

        if (pitchLFOMidiClockSync)
        {
            shared.pitchLfo.frequency(frequency);
        }
    }

    void closeEnvelopes()
    {
        VG_FOR_EACH_VOICE(
            voices[i]->patch().filterEnvelope_.close();
            voices[i]->patch().ampEnvelope_.close();)
    }

    // TODO: This helps during refactoring, maybe it will be removed later.
    Voice *operator[](int i) const { return voices[i]; }

private:
    void handleMonophonicNoteOn(uint8_t note, uint8_t velocity)
    {
        noteStack.push(note, velocity);
        if (monophonic)
        {
            MonoNoteHistory::Element nextNote{note, velocity};
            // If there is more than 1 note check for retrigger.
            if (noteStack.size() > 1)
            {
                switch (monophonic)
                {
                case MONOPHONIC_FIRST:
                    // Exit since there is already a note playing.
                    return;
                    break;
                case MONOPHONIC_LEGATO: // Not implemented
                case MONOPHONIC_LAST:
                    // Always play the last note.
                    break;
                case MONOPHONIC_HIGHEST:
                {
                    // Play if higher than active note.
                    if (note < monoNote)
                    {
                        return;
                    }
                    break;
                }
                case MONOPHONIC_LOWEST:
                {
                    if (note > monoNote)
                    {
                        return;
                    }
                    break;
                }
                }
            }

            // Make sure any active note is turned off.
            for (uint8_t i = 0; i < voices.size(); i++)
            {
                voices[i]->noteOff();
            }
            this->monoNote = nextNote.note;
            noteOn(nextNote.note, nextNote.velocity, false);
        }
        else
        {
            noteOn(note, velocity, false);
        }
    }

    void handleMonophonicNoteOff(uint8_t note)
    {
        if (!this->monophonic)
            return;

        // Remove turned-off note
        noteStack.erase(note);
        bool activeNoteTurnedOff = 0 != noteOff(note, true);

        // Active note still active, or no more notes.
        if (!activeNoteTurnedOff || noteStack.size() == 0)
            return;

        MonoNoteHistory::Element nextNote;
        switch (this->monophonic)
        {
        case MONOPHONIC_LAST:
            nextNote = noteStack.getLast();
            break;
        case MONOPHONIC_FIRST:
            nextNote = noteStack.getFirst();
            break;
        case MONOPHONIC_LOWEST:
            nextNote = noteStack.getLowest();
            break;
        case MONOPHONIC_HIGHEST:
            nextNote = noteStack.getHighest();
            break;
        case MONOPHONIC_LEGATO:
        default:
            // No retriggering.
            return;
            break;
        }
        this->monoNote = nextNote.note;
        noteOn(nextNote.note, nextNote.velocity, true);
        return;
    }

    // Prefer a silent voice, then the oldest released voice (still in its release tail),
    // then steal the oldest held note. Using silent voices first avoids cutting off release tails.
    // Note retrigger option (setSameVoiceRetrig): reuse the voice of a note that is played
    // again while still sounding (held or releasing), instead of taking another voice
    Voice *getVoice(uint8_t note)
    {
        if (sameVoiceRetrig && !this->monophonic)
        {
            for (uint8_t i = 0; i < voices.size(); i++)
                if (voices[i]->note() == note && (voices[i]->on() || voices[i]->sounding()))
                    return voices[i];
        }

        // Mono modes retrigger the same voice rather than overlapping the previous note's release
        if (this->monophonic)
        {
            for (uint8_t i = 0; i < voices.size(); i++)
                if (!voices[i]->on())
                    return voices[i];
        }

        Voice *released = nullptr;
        Voice *oldest = nullptr;
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            Voice *v = voices[i];
            if (!v->on())
            {
                if (!v->sounding())
                    return v;
                if (released == nullptr || v->timeOn() < released->timeOn())
                    released = v;
            }
            else if (oldest == nullptr || v->timeOn() < oldest->timeOn())
            {
                oldest = v;
            }
        }
        return released != nullptr ? released : oldest;
    }

    // Turn off one or more notes, return the number of notes turned off.
    int noteOff(uint8_t note, bool all)
    {
        int num = 0;
        for (uint8_t i = 0; i < voices.size(); i++)
        {
            if (voices[i]->note() == note && voices[i]->on() == true)
            {
                num++;
                voices[i]->noteOff();
                this->voicesOn--;
                if (!all)
                {
                    return 1;
                }
            }
        }
        return num;
    }

    void noteOn(uint8_t note, uint8_t velocity, bool monoRetrigger)
    {

        if (filterLfoRetrig)
        {
            shared.filterLfo.sync();
        }

        switch (this->_params.unisonMode)
        {
        case 0:
        {
            this->_params.mixerLevel = VOICEMIXERLEVEL * oscMixCompensation;
            Voice *v = this->getVoice(note);
            if (!v->on())
                this->voicesOn++;
            v->noteOn(note, velocity, this->_params, unisonNotesOn, 0);
            break;
        }
        case 1:
        {
            if (!this->monophonic)
                this->unisonNotesOn++;

            this->_params.mixerLevel = UNISONVOICEMIXERLEVEL * oscMixCompensation;

            uint8_t maxUnison = voices.size() / MINUNISONVOICES;
            uint8_t tally[maxUnison] = {};
            uint8_t oldestVoiceIndex = 0;

            // Figure out which note id to use.
            for (uint8_t i = 0; i < this->voices.size(); i++)
            {
                if (voices[i]->on())
                {
                    tally[voices[i]->noteId()]++;
                    if (voices[i]->timeOn() < voices[oldestVoiceIndex]->timeOn())
                    {
                        oldestVoiceIndex = i;
                    }
                }
            }

            uint8_t id = 0;
            for (uint8_t i = 0; i < maxUnison; i++)
            {
                if (tally[i] == 0)
                {
                    id = i;
                    break;
                }
            }

            // Replace oldest note if too many are playing.
            if (this->unisonNotesOn > maxUnison)
            {
                id = voices[oldestVoiceIndex]->noteId();
                noteOff(voices[oldestVoiceIndex]->note());
            }

            // Fill gaps if there are any.
            if (this->unisonNotesOn != 1 && this->voicesOn != this->voices.size())
            {
                for (uint8_t i = 0; i < this->voices.size(); i++)
                {
                    if (!voices[i]->on())
                    {
                        voices[i]->noteOn(note, velocity, this->_params, unisonNotesOn, id);
                        this->voicesOn++;
                    }
                }
                return;
            }

            // Start all voices or...
            // Steal voices until each has the right amount.
            uint8_t max = this->voices.size() / unisonNotesOn;
            for (uint8_t i = 0; i < voices.size(); i++)
            {
                if (!voices[i]->on() || tally[voices[i]->noteId()] > max)
                {
                    // underflow here when starting first unison note, but it still works.
                    tally[voices[i]->noteId()]--;
                    voices[i]->noteOn(note, velocity, this->_params, unisonNotesOn, id);
                }
            }
            voicesOn = voices.size();

            break;
        }
        case 2:
            this->_params.mixerLevel = UNISONVOICEMIXERLEVEL * oscMixCompensation;
            allNotesOn(note, velocity, 0);
            break;
        }
        this->_params.prevNote = note;
    }
};

FLASHMEM int getLFOWaveform(int value)
{
    if (value >= 0 && value < 8)
    {
        return WAVEFORM_SINE;
    }
    else if (value >= 8 && value < 30)
    {
        return WAVEFORM_TRIANGLE;
    }
    else if (value >= 30 && value < 63)
    {
        return WAVEFORM_SAWTOOTH_REVERSE;
    }
    else if (value >= 63 && value < 92)
    {
        return WAVEFORM_SAWTOOTH;
    }
    else if (value >= 92 && value < 111)
    {
        return WAVEFORM_SQUARE;
    }
    else
    {
        return WAVEFORM_SAMPLE_HOLD;
    }
}

FLASHMEM int getWaveformA(int value)
{
    if (value >= 0 && value < 7)
    {
        // This will turn the osc off
        return WAVEFORM_SILENT;
    }
    else if (value >= 7 && value < 23)
    {
        return WAVEFORM_TRIANGLE;
    }
    else if (value >= 23 && value < 40)
    {
        return WAVEFORM_BANDLIMIT_SQUARE;
    }
    else if (value >= 40 && value < 60)
    {
        return WAVEFORM_BANDLIMIT_SAWTOOTH;
    }
    else if (value >= 60 && value < 80)
    {
        return WAVEFORM_BANDLIMIT_PULSE;
    }
    else if (value >= 80 && value < 100)
    {
        return WAVEFORM_TRIANGLE_VARIABLE;
    }
    else if (value >= 100 && value < 120)
    {
        return WAVEFORM_PARABOLIC;
    }
    else
    {
        return WAVEFORM_HARMONIC;
    }
}

FLASHMEM int getWaveformB(int value)
{
    if (value >= 0 && value < 7)
    {
        // This will turn the osc off
        return WAVEFORM_SILENT;
    }
    else if (value >= 7 && value < 23)
    {
        return WAVEFORM_SAMPLE_HOLD;
    }
    else if (value >= 23 && value < 40)
    {
        return WAVEFORM_BANDLIMIT_SQUARE;
    }
    else if (value >= 40 && value < 60)
    {
        return WAVEFORM_BANDLIMIT_SAWTOOTH;
    }
    else if (value >= 60 && value < 80)
    {
        return WAVEFORM_BANDLIMIT_PULSE;
    }
    else if (value >= 80 && value < 100)
    {
        return WAVEFORM_TRIANGLE_VARIABLE;
    }
    else if (value >= 100 && value < 120)
    {
        return WAVEFORM_PARABOLIC;
    }
    else
    {
        return WAVEFORM_HARMONIC;
    }
}

#endif
