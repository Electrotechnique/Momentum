
/*
https://forum.pjrc.com/threads/66840-Roadmap-quot-Dynamic-Updates-quot-any-effort-going-on/page4?highlight=dynamic
*/

#ifndef MOMENTUM_AUDIO_PATCHING_H
#define MOMENTUM_AUDIO_PATCHING_H

#include <vector>
#include "Constants.h"

// waveformX      -->   waveformMixerX   -->   voiceMixer1-3   -->   voiceMixerM  --> volumeMixer
// WAVEFORMLEVEL        oscA/BLevel             VELOCITY    VOICEMIXERLEVEL/UNISONVOICEMIXERLEVEL    volume

class Mixer
{
private:
    AudioMixer4 &mixer;
    uint8_t index;

public:
    Mixer(AudioMixer4 &mixer_, uint8_t index_) : mixer(mixer_), index(index_) {}

    void gain(float value)
    {
        mixer.gain(index, value);
    }
};

struct PatchShared
{
    AudioSynthWaveformDcTS pitchBend;
    AudioSynthWaveformTS pitchLfo;
    AudioMixer4 pitchMixer;
    AudioSynthWaveformTS pwmLfoA;
    AudioSynthWaveformTS pwmLfoB;
    AudioSynthWaveformTS filterLfo;
    AudioSynthWaveformDcTS pwa;
    AudioSynthWaveformDcTS pwb;
    AudioMixer4 noiseMixer;

    AudioMixer4 voiceMixer[3];
    AudioMixer4 voiceMixerM;

    AudioEffectEnsemble ensemble;
    AudioFilterDCBlock dcOffsetFilter;
    AudioMixer4 volumeMixer;
    AudioMixer4 ensembleEffectMixerL;
    AudioMixer4 ensembleEffectMixerR;
    // The reverb is shared by all timbres and follows the ensemble (see Global).
    // Set by Global; one instance because each needs ~120KB of RAM2.
    AudioEffectPlateReverb *reverb = nullptr;

    AudioConnection connections[12] = {
        {pitchBend, 0, pitchMixer, 0},
        {pitchLfo, 0, pitchMixer, 1},
        {voiceMixer[0], 0, voiceMixerM, 0},
        {voiceMixer[1], 0, voiceMixerM, 1},
        {voiceMixer[2], 0, voiceMixerM, 2},
        {voiceMixerM, 0, dcOffsetFilter, 0},
        {dcOffsetFilter, 0, volumeMixer, 0},
        {volumeMixer, 0, ensemble, 0},
        {ensemble, 0, ensembleEffectMixerL, 1},
        {ensemble, 1, ensembleEffectMixerR, 1},
        {volumeMixer, 0, ensembleEffectMixerL, 0},
        {volumeMixer, 0, ensembleEffectMixerR, 0},
    };

private:
    AudioConnection *pinkNoiseConnection = nullptr;
    AudioConnection *whiteNoiseConnection = nullptr;
    AudioConnection *outputLConnection = nullptr;
    AudioConnection *outputRConnection = nullptr;

public:
    void connectNoise(AudioSynthNoisePink &pink, AudioSynthNoiseWhite &white)
    {
        delete pinkNoiseConnection;
        delete whiteNoiseConnection;
        pinkNoiseConnection = new AudioConnection(pink, 0, noiseMixer, 0);
        whiteNoiseConnection = new AudioConnection(white, 0, noiseMixer, 1);
    }

    void connectOutput(AudioMixer4 &left, AudioMixer4 &right, uint8_t index)
    {
        delete outputLConnection;
        delete outputRConnection;
        outputLConnection = new AudioConnection(ensembleEffectMixerL, 0, left, index);
        outputRConnection = new AudioConnection(ensembleEffectMixerR, 0, right, index);
    }
};

// Oscillator configurations.
struct Patch
{
    AudioEffectEnvelopeTS filterEnvelope_;

    AudioMixer4 pwMixer_a;
    AudioMixer4 pwMixer_b;

    AudioSynthWaveformDcTS glide_;

    AudioSynthWaveformDcTS filterVelocity_;

    AudioMixer4 oscModMixer_a;
    AudioMixer4 oscModMixer_b;

    AudioSynthWaveformModulatedTS waveformMod_a;
    AudioSynthWaveformModulatedTS waveformMod_b;

    AudioEffectDigitalCombine oscFX_;

    AudioMixer4 waveformMixer_;

    AudioMixer4 filterModMixer_;

    AudioFilterStateVariableTS filter_;

    AudioMixer4 filterMixer_;

    AudioEffectEnvelopeTS ampEnvelope_;

    AudioConnection connections[25] = {
        {filterVelocity_, 0, filterModMixer_, 3},
        {pwMixer_a, 0, waveformMod_a, 1},
        {pwMixer_b, 0, waveformMod_b, 1},
        {waveformMod_a, 0, waveformMixer_, 0},
        {waveformMod_a, 0, oscFX_, 0},
        {waveformMod_b, 0, waveformMixer_, 1},
        {waveformMod_b, 0, oscFX_, 1},
        {filterEnvelope_, 0, filterModMixer_, 0},
        {filterEnvelope_, 0, pwMixer_a, 2},
        {filterEnvelope_, 0, pwMixer_b, 2},
        {oscFX_, 0, waveformMixer_, 3},
        {waveformMixer_, 0, filter_, 0},
        {filterModMixer_, 0, filter_, 1},
        {filter_, 0, filterMixer_, 0},
        {filter_, 1, filterMixer_, 1},
        {filter_, 2, filterMixer_, 2},
        {filterMixer_, ampEnvelope_},
        // Mod sources
        {oscModMixer_a, 0, waveformMod_a, 0},
        {oscModMixer_b, 0, waveformMod_b, 0},
        // Pitch env
        {filterEnvelope_, 0, oscModMixer_a, 1},
        {filterEnvelope_, 0, oscModMixer_b, 1},
        // Glide
        {glide_, 0, oscModMixer_a, 2},
        {glide_, 0, oscModMixer_b, 2},
        // X Mod
        {waveformMod_a, 0, oscModMixer_b, 3},
        {waveformMod_b, 0, oscModMixer_a, 3}};

private:
    // When added to a voice group, connect PWA/PWB.
    AudioConnection *pitchMixerAConnection = nullptr;
    AudioConnection *pitchMixerBConnection = nullptr;
    AudioConnection *pwmLfoAConnection = nullptr;
    AudioConnection *pwmLfoBConnection = nullptr;
    AudioConnection *filterLfoConnection = nullptr;
    AudioConnection *pwaConnection = nullptr;
    AudioConnection *pwbConnection = nullptr;
    AudioConnection *noiseMixerConnection = nullptr;
    AudioConnection *ampConnection = nullptr;

public:
    // Connect the shared audio objects to the per-voice audio objects.
    Mixer *connectTo(PatchShared &shared, uint8_t index)
    {
        delete pitchMixerAConnection;
        delete pitchMixerBConnection;
        delete pwmLfoAConnection;
        delete pwmLfoBConnection;
        delete filterLfoConnection;
        delete pwaConnection;
        delete pwbConnection;
        delete noiseMixerConnection;
        delete ampConnection;

        pitchMixerAConnection = new AudioConnection(shared.pitchMixer, 0, oscModMixer_a, 0);
        pitchMixerBConnection = new AudioConnection(shared.pitchMixer, 0, oscModMixer_b, 0);
        pwmLfoAConnection = new AudioConnection(shared.pwmLfoA, 0, pwMixer_a, 0);
        pwmLfoBConnection = new AudioConnection(shared.pwmLfoB, 0, pwMixer_b, 0);
        filterLfoConnection = new AudioConnection(shared.filterLfo, 0, filterModMixer_, 1);
        pwaConnection = new AudioConnection(shared.pwa, 0, pwMixer_a, 1);
        pwbConnection = new AudioConnection(shared.pwb, 0, pwMixer_b, 1);
        noiseMixerConnection = new AudioConnection(shared.noiseMixer, 0, waveformMixer_, 2);
        uint8_t voiceMixerIndex = 0;
        uint8_t indexMod4 = index % 4;
        if (index != 0)
            voiceMixerIndex = index / 4;
        ampConnection = new AudioConnection(ampEnvelope_, 0, shared.voiceMixer[voiceMixerIndex], indexMod4);
        return new Mixer{shared.voiceMixer[voiceMixerIndex], indexMod4};
    }
};

struct Global
{
private:
    static const uint8_t MAX_NO_TIMBER = 2;
    static const uint8_t MAX_NO_VOICE = 12;

public:
    AudioOutputUSB usbAudio;
    AudioSynthWaveformDcTS constant1Dc;
    AudioSynthNoisePink pink;
    AudioSynthNoiseWhite white;
    AudioAnalyzePeak peak;
    Oscilloscope scope;
    AudioMixer4 ensembleEffectMixerR[3];
    AudioMixer4 effectMixerRM;
    AudioMixer4 ensembleEffectMixerL[3];
    AudioMixer4 effectMixerLM;
    AudioEffectPlateReverb reverb; // Stereo, in series after the ensemble
    AudioOutputI2S i2s;

    PatchShared SharedAudio[MAX_NO_TIMBER];
    Patch Oscillators[MAX_NO_VOICE];

    AudioConnection connectionsArray[14] = {
        {ensembleEffectMixerL[0], 0, effectMixerLM, 0},
        {ensembleEffectMixerL[1], 0, effectMixerLM, 1},
        {ensembleEffectMixerL[2], 0, effectMixerLM, 2},
        {ensembleEffectMixerR[0], 0, effectMixerRM, 0},
        {ensembleEffectMixerR[1], 0, effectMixerRM, 1},
        {ensembleEffectMixerR[2], 0, effectMixerRM, 2},
        {effectMixerLM, 0, reverb, 0},
        {effectMixerRM, 0, reverb, 1},
        {reverb, 0, scope, 0},
        {reverb, 0, peak, 0},
        {reverb, 1, usbAudio, 1},
        {reverb, 1, i2s, 1},
        {reverb, 0, i2s, 0},
        {reverb, 0, usbAudio, 0}};

    std::vector<AudioConnection *> connections;

    Global(float mixerLevel)
    {
        for (int i = 0; i < MAX_NO_VOICE; i++)
        {
            connections.push_back(new AudioConnection{constant1Dc, Oscillators[i].filterEnvelope_});
            connections.push_back(new AudioConnection{constant1Dc, 0, Oscillators[i].filterModMixer_, 2}); // Key tracking
        }

        // Fixed reverb tone settings (0.0 - 1.0). Size and mix are patch parameters.
        reverb.hidamp(0.5f);   // Treble loss in the tail
        reverb.lodamp(0.2f);   // Bass loss in the tail, keeps low notes from muddying
        reverb.lowpass(0.3f);  // Reverb output treble cut
        reverb.hipass(0.0f);   // Reverb output bass cut
        reverb.diffusion(1.0f);

        for (int i = 0; i < MAX_NO_TIMBER; i++)
        {
            SharedAudio[i].reverb = &reverb;
            SharedAudio[i].connectNoise(pink, white);

            uint8_t mixerIdx = 0;
            if (i > 0)
                mixerIdx = i / 12;
            SharedAudio[i].connectOutput(ensembleEffectMixerL[mixerIdx], ensembleEffectMixerR[mixerIdx], i % 4);

            SharedAudio[i].voiceMixerM.gain(0, mixerLevel);
            SharedAudio[i].voiceMixerM.gain(1, mixerLevel);
            SharedAudio[i].voiceMixerM.gain(2, mixerLevel);
            SharedAudio[i].voiceMixerM.gain(3, mixerLevel);

            SharedAudio[i].volumeMixer.gain(0, 1); // Set by VoiceGroup setVolume(float amount)
            SharedAudio[i].volumeMixer.gain(1, 0);
            SharedAudio[i].volumeMixer.gain(2, 0);
            SharedAudio[i].volumeMixer.gain(3, 0);

            // This removes dc offset (mostly from unison pulse waves) before the effects
            SharedAudio[i].dcOffsetFilter.frequency(7.0f);
        }

        constant1Dc.amplitude(1.0f);

        effectMixerLM.gain(0, 1.0f);
        effectMixerLM.gain(1, 1.0f);
        effectMixerLM.gain(2, 1.0f);
        effectMixerLM.gain(3, 1.0f);
        effectMixerRM.gain(0, 1.0f);
        effectMixerRM.gain(1, 1.0f);
        effectMixerRM.gain(2, 1.0f);
        effectMixerRM.gain(3, 1.0f);

        pink.amplitude(1.0f);
        white.amplitude(1.0f);
    }

    inline int maxVoices() { return MAX_NO_VOICE; }
    inline int maxTimbre() { return MAX_NO_TIMBER; }

    // Limited to 12 because we have 3 mixers funnelling into 1 mixer.
    inline int maxVoicesPerGroup() { return MAX_NO_VOICE; }
    inline int maxTimbres() { return MAX_NO_VOICE; }
};

#endif
