#ifndef MOMENTUM_VOICE_H
#define MOMENTUM_VOICE_H

#include <stdint.h>
#include <stddef.h>
#include "AudioPatching.h"
#include "Constants.h"
#include "Detune.h"
#include "Velocity.h"

// TODO: Store / update these in the VoiceGroup instead of re-initializing for each noteOn.
struct VoiceParams
{
    float keytrackingAmount;
    float mixerLevel;
    int prevNote;
    float glideSpeed;
    int unisonMode;
    uint8_t chordDetune;
    float detune;
    int oscPitchA;
    int oscPitchB;
};

class Voice
{
private:
    Patch &_oscillator;
    long _timeOn;
    uint8_t _note;
    float _velocity;
    bool _voiceOn;
    uint8_t _idx;
    uint8_t _noteId;
    Mixer *mixer = nullptr;

    // Note-on held back while a stolen voice fades out (see noteOn / service)
    bool _pending = false;
    float _pendingGain = 0;
    float _pendingFilterVelocity = 0;
    bool _pendingGlide = false;
    float _pendingGlideFrom = 0;
    float _pendingGlideTime = 0;
    uint8_t _pendingNotesOn = 0;
    VoiceParams *_pendingParams = nullptr;

    // Set the new note's level, filter velocity, key tracking, glide and pitch
    void applyNoteOn()
    {
        Patch &osc = this->patch();
        mixer->gain(_pendingGain);
        osc.filterVelocity_.amplitude(_pendingFilterVelocity);
        this->updateKeytracking(*_pendingParams);
        if (_pendingGlide)
        {
            osc.glide_.amplitude(_pendingGlideFrom);    // Set glide to previous note frequency (limited to 1 octave max)
            osc.glide_.amplitude(0, _pendingGlideTime); // Glide to current note
        }
        this->updateVoice(*_pendingParams, _pendingNotesOn);
    }

public:
    Voice(Patch &p, uint8_t i) : _oscillator(p), _timeOn(-1), _note(0), _velocity(0), _voiceOn(false), _idx(i), _noteId(0)
    {
        p.waveformMod_a.frequencyModulation(PITCHLFOOCTAVERANGE);
        p.waveformMod_a.begin(WAVEFORMLEVEL, 440.0f, WAVEFORM_SQUARE);
        p.waveformMod_b.frequencyModulation(PITCHLFOOCTAVERANGE);
        p.waveformMod_b.begin(WAVEFORMLEVEL, 440.0f, WAVEFORM_SQUARE);

        // Arbitary waveform needs initializing to something
        p.waveformMod_a.arbitraryWaveform(PARABOLIC_WAVE, AWFREQ);
        p.waveformMod_b.arbitraryWaveform(PARABOLIC_WAVE, AWFREQ);

        // Gain controls to signal input of the filter from a AudioMixer4 object are limited to +/-1
        //Key tracking needs +/-5, so divide it by 5 and multiply using the octavecontrol
        p.filter_.octaveControl(5.0f);
    }

    inline void setMixer(Mixer *mixer_)
    {
        delete mixer;
        mixer = mixer_;
    }

    inline uint8_t index()
    {
        return this->_idx;
    }

    inline bool on()
    {
        return this->_voiceOn;
    }

    inline uint8_t note()
    {
        return this->_note;
    }

    inline uint8_t noteId()
    {
        return this->_noteId;
    }

    inline uint8_t velocity()
    {
        return this->_velocity;
    }

    inline long timeOn()
    {
        return this->_timeOn;
    }

    inline Patch &patch()
    {
        return this->_oscillator;
    }

    // C3 (note 60) is the pivot key, unaffected by key tracking. KEYTRACKINGFACTOR is in octaves;
    // divided by 5 as the AudioMixer4 output is limited to +/-1, the filter's octaveControl(5) restores it
    void updateKeytracking(VoiceParams &params)
    {
        if (_pending)
            return; // Old note still fading out, applied when the new note starts
        this->patch().filterModMixer_.gain(2, KEYTRACKINGFACTOR[this->_note] * params.keytrackingAmount / 5.0f);
    }

    // True while the voice is audible (including its release tail)
    inline bool sounding()
    {
        return this->_oscillator.ampEnvelope_.isActive();
    }

    void updateVoice(VoiceParams &params, uint8_t notesOn)
    {
        Patch &osc = this->patch();

        if (params.unisonMode == 1)
        {
            int offset = 2 * this->index();
            osc.waveformMod_a.frequency(NOTEFREQS[this->_note + params.oscPitchA] * tuningCentsFrac * (params.detune + ((1 - params.detune) * DETUNE[notesOn - 1][offset])));
            osc.waveformMod_b.frequency(NOTEFREQS[this->_note + params.oscPitchB] * tuningCentsFrac * (params.detune + ((1 - params.detune) * DETUNE[notesOn - 1][offset + 1])));
        }
        else if (params.unisonMode == 2)
        {
            // TODO: This approach doesn't make sense with voices spread across multiple timbres.
            osc.waveformMod_a.frequency(NOTEFREQS[this->_note + params.oscPitchA + CHORD_DETUNE[this->index()][params.chordDetune]] * tuningCentsFrac);
            osc.waveformMod_b.frequency(NOTEFREQS[this->_note + params.oscPitchB + CHORD_DETUNE[this->index()][params.chordDetune]] * CDT_DETUNE * tuningCentsFrac);
        }
        else
        {
            osc.waveformMod_a.frequency(NOTEFREQS[this->_note + params.oscPitchA] * tuningCentsFrac);
            osc.waveformMod_b.frequency(NOTEFREQS[this->_note + params.oscPitchB] * params.detune * tuningCentsFrac);
        }
    }

    void noteOn(uint8_t note, int velocity, VoiceParams &params, uint8_t notesOn, uint8_t id)
    {
        Patch &osc = this->patch();
        bool stealing = osc.ampEnvelope_.isActive();

        // Work out the new note's settings now, glide depends on the previous note
        _pendingGain = VELOCITY[currentPatch.VelocitySensitivity][velocity] * params.mixerLevel;
        _pendingFilterVelocity = currentPatch.FilterVelocitySensitivity != 0
                                     ? 2.0f * (VELOCITY[currentPatch.FilterVelocitySensitivity][velocity]) - 1.0f //-1 to 1 (63=0)
                                     : 0.0f;
        _pendingGlide = params.glideSpeed > 0 && note != params.prevNote;
        _pendingGlideFrom = (params.prevNote - note) * DIV24;
        _pendingGlideTime = params.glideSpeed * GLIDEFACTOR;
        _pendingParams = &params;
        _pendingNotesOn = notesOn;

        this->_noteId = id;
        this->_voiceOn = true;
        this->_note = note;
        this->_velocity = velocity;
        this->_timeOn = millis();

        if (stealing)
        {
            // The envelopes fade the old note out first (5ms forced release). Changing
            // the level and pitch now would be heard on that fade as a click/blip, so
            // wait until it has finished: service() then applies them and starts the envelopes.
            _pending = true;
            osc.filterEnvelope_.noteOnDeferred();
            osc.ampEnvelope_.noteOnDeferred();
            return;
        }

        _pending = false;
        applyNoteOn();
        osc.filterEnvelope_.noteOn();
        osc.ampEnvelope_.noteOn();
    }

    // Call regularly from the main loop. Starts a deferred note-on once the old note has faded out.
    void service()
    {
        if (!_pending || !this->patch().ampEnvelope_.isWaiting())
            return;
        AudioNoInterrupts();
        _pending = false;
        applyNoteOn();
        this->patch().filterEnvelope_.start();
        this->patch().ampEnvelope_.start();
        AudioInterrupts();
    }

    void noteOff()
    {
        _pending = false;
        if (!this->_voiceOn)
            return;
        this->_voiceOn = false;
        this->_noteId = 0;
        this->patch().filterEnvelope_.noteOff();
        this->patch().ampEnvelope_.noteOff();
    }
};

#endif
