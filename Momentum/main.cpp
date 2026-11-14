/*
   MIT License

  Copyright (c) 2023 ElectroTechnique

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

------------------------------------------------
            ELECTROTECHNIQUE MOMENTUM
  Teensy MicroMod based Synthesizer - 12 voices
------------------------------------------------

  Arduino IDE Tools Settings:
    Board: "Teensy MicroMod"
    USB Type: "Serial + MIDI + Audio"
    CPU Speed: "600MHz"
    Optimize: "Faster"

  This code is based on TSynth V2.32 firmware - ElectroTechnique 2021
  and includes code by:
    Dave Benn -  A few bits and original inspiration  https://www.notesandvolts.com/2019/01/teensy-synth-part-10-hardware.html
    Alexander Davis / Vince R. Pearson - Stereo ensemble chorus effect https://github.com/quarterturn/teensy3-ensemble-chorus
    Will Winder - Major refactoring and monophonic mode
    Vince R. Pearson - Exponential envelopes & glide
    Github members fab672000 & CDW2000 - General improvements to code
    Github/PJRC forum member luni64 - EncoderTool library modified https://github.com/luni64/EncoderTool
    PJRC forum member Mark Tillotson - Special thanks for band-limiting the waveforms in the Audio Library

    Any problems with code design and functionality are my own.

  Additional libraries:
    Janila Array, Adafruit_GFX, ArduinoJSON, TeensyTimerTool, TeensyThreads, RokkitHash (available in Arduino libraries manager)
*/
#define DEBUG 0

#include <Adafruit_GFX.h>
#include <ILI9341_t3n.h>
#include <vector>
#include "Audio.h" //Using local version to override Teensyduino version
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include <TeensyThreads.h>
#include "ThreadSafety.h"
#include <TeensyTimerTool.h>
using namespace TeensyTimerTool;
#include "MidiCC.h"
#include "SettingsService.h"
#include "AudioPatching.h"
#include "Constants.h"
#include "Parameters.h"

FLASHMEM void silence();
volatile boolean cardStatus = false;
volatile boolean sdCardInterrupt = false;
boolean usbHostPluggedIn = false;
elapsedMillis usb_host_wait_timer;
boolean MIDIClkSignal = false;
// MIDI clock counts as lost after this long without a tick (24ppq at 20bpm is 125ms per tick)
#define MIDI_CLOCK_TIMEOUT_MS 500
elapsedMillis sinceMidiClock;

State state = State::MAIN;
boolean keyboardActive = false;

#include "PatchMgr.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "Detune.h"
#include "Keyboard.h"
#include "utils.h"
#include "Voice.h"
#include "VoiceGroup.h"

uint8_t majMin = MajMin::MAJOR;
uint8_t keyboardOct = 3;
uint8_t keyboardScale = 0;

// Initialize the audio configuration.
Global global{VOICEMIXERLEVEL};
std::vector<VoiceGroup *> groupvec;
uint8_t activeGroupIndex = 0;

#include "StringMapping.h"
#include "PerformanceMgr.h"

PeriodicTimer sequencer_timer(TCK); // TCK PIT GPT2
#include "SequenceMgr.h"
#include "EncoderMapping.h"

extern void sequencer();
extern void noteOnRoutine();

// USB HOST MIDI Class Compliant
USBHost myusb;
// MIDIDevice usbHostMIDI(myusb);
MIDIDevice_BigBuffer usbHostMIDI(myusb); // Try this if your MIDI Compliant controller has problems

// MIDI 5 Pin DIN - (TRS MIDI)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

FLASHMEM void changeMIDIThruMode()
{
    MIDI.turnThruOn(MIDIThru);
}

FLASHMEM void myProgramChange(byte channel, byte program);
void myPitchBend(byte channel, int bend);
void myNoteOff(byte channel, byte note, byte velocity);
void myNoteOn(byte channel, byte note, byte velocity);
FLASHMEM void playNote(uint8_t note);
FLASHMEM void setCurrentPatchData();
FLASHMEM void recallPatch(uint8_t bank, long patchUID);
FLASHMEM void recallPerformance(uint8_t filename);
FLASHMEM void myControlChange(byte channel, byte control, byte value);
FLASHMEM void usbHostControlChange(byte channel, byte control, byte value);
void myMIDIClock();
FLASHMEM void myMIDIClockContinue();
FLASHMEM void myMIDIClockStart();
FLASHMEM void myMIDIClockStop();
FLASHMEM void sequencerStart(boolean cont = false);
FLASHMEM void sequencerStop();
FLASHMEM void setSeqTimerPeriod(float bpm);

void midiCCOut(byte cc, byte value);
void encoderCallback(unsigned enc_idx, int value, int delta);
void encoderButtonCallback(unsigned button_idx, int state);
void buttonCallback(unsigned button_idx, int state);

#include "Display.h"
#include "Settings.h"

boolean firstPatchLoaded = false;

uint32_t previousMillis = millis(); // For MIDI Clk Sync
uint32_t timeNow = millis();        // For MIDI Clk Sync

uint8_t midiClkCount = 0;    // For MIDI Clk Sync
uint8_t midiClkArpCount = 0; // For MIDI Clk Sync with Arp
int voiceToReturn = -1;      // Initialise

uint8_t seq_last_step = 0;
uint8_t loopCount = 0;
#define MIDI_READS_PER_LOOP 16 // Max pending messages handled per MIDI input each time they're read
// Set while flushing MIDI input on Panic: incoming messages are read and dropped
volatile boolean midiDiscard = false;

// Blocks the audio update while note changes are applied, so envelope triggers and pitch
// changes land in the same audio block. Safe to nest: the audio interrupt is only re-enabled
// if it was enabled on entry.
class AudioUpdateLock
{
public:
    AudioUpdateLock() : wasEnabled(NVIC_IS_ENABLED(IRQ_SOFTWARE)) { NVIC_DISABLE_IRQ(IRQ_SOFTWARE); }
    ~AudioUpdateLock()
    {
        if (wasEnabled)
            NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
    }

private:
    const bool wasEnabled;
};

FLASHMEM void setup()
{
    while (DEBUG == 1 && !Serial)
    {
    }
    AudioMemory(60);
    sequencer_timer.begin(sequencer, currentSequence.tempo_us / 2.0f, false);
    checkFirstRun();
    // Initialize the voice groups.
    uint8_t total = 0;
    while (total < global.maxVoices())
    {
        VoiceGroup *currentGroup = new VoiceGroup{global.SharedAudio[groupvec.size()]};

        for (uint8_t i = 0; total < global.maxVoices() && i < global.maxVoicesPerGroup(); i++)
        {
            Voice *v = new Voice(global.Oscillators[i], i);
            currentGroup->add(v);
            total++;
        }
        groupvec.push_back(currentGroup);
    }
    setUpSettings();
    SD.setMediaDetectPin(pinCD);
    cardStatus = SD.begin(BUILTIN_SDCARD);
    if (SD.mediaPresent()) // Card is detected by CD pin
    {
        // ++++ For older sd cards - keep trying
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        //++++
    }
    if (cardStatus)
    {
        if (DEBUG)
            Serial.println(F("SD card is connected"));
        loadLastPatchUsed();
        currentSequenceIndex = getLastSequence();
        // Get patch numbers and names from SD card
        checkSDCardStructure();
        loadBankNames();
        loadPatchNamesFromBank(currentBankIndex);
        // If current patch is empty in this bank, go to previous bank with valid patch
        while (patches[0].patchUID == 0)
        {
            tempBankIndex = decTempBankIndex();
            loadPatchNamesFromBank(tempBankIndex);
            currentBankIndex = tempBankIndex;
            currentPatchIndex = 0;
        }
        recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
    }
    else
    {
        setCurrentPatchData(); // Initialise to default
        if (DEBUG)
            Serial.println(F("SD card is not connected or unusable"));
    }

    // Read MIDI Channel from EEPROM
    midiChannel = getMIDIChannel();
    if (DEBUG)
        Serial.println(F("MIDI In Channel:") + String(midiChannel) + F(" (0 is Omni On)"));

    // USB Client MIDI
    usbMIDI.setHandleControlChange(myControlChange);
    usbMIDI.setHandleNoteOff(myNoteOff);
    usbMIDI.setHandleNoteOn(myNoteOn);
    usbMIDI.setHandlePitchChange(myPitchBend);
    usbMIDI.setHandleProgramChange(myProgramChange);
    usbMIDI.setHandleClock(myMIDIClock);
    usbMIDI.setHandleContinue(myMIDIClockContinue);
    usbMIDI.setHandleStart(myMIDIClockStart);
    usbMIDI.setHandleStop(myMIDIClockStop);
    if (DEBUG)
        Serial.println(F("USB Client MIDI Listening"));

    // MIDI 5 Pin DIN
    MIDI.begin();
    MIDI.setHandleNoteOn(myNoteOn);
    MIDI.setHandleNoteOff(myNoteOff);
    MIDI.setHandlePitchBend(myPitchBend);
    MIDI.setHandleControlChange(myControlChange);
    MIDI.setHandleProgramChange(myProgramChange);
    MIDI.setHandleClock(myMIDIClock);
    MIDI.setHandleContinue(myMIDIClockContinue);
    MIDI.setHandleStart(myMIDIClockStart);
    MIDI.setHandleStop(myMIDIClockStop);
    if (DEBUG)
        Serial.println(F("MIDI In DIN Listening"));

    // USB HOST MIDI Class Compliant
    myusb.begin();
    usbHostMIDI.setHandleControlChange(usbHostControlChange);
    usbHostMIDI.setHandleNoteOff(myNoteOff);
    usbHostMIDI.setHandleNoteOn(myNoteOn);
    usbHostMIDI.setHandlePitchChange(myPitchBend);
    usbHostMIDI.setHandleProgramChange(myProgramChange);
    usbHostMIDI.setHandleClock(myMIDIClock);
    usbHostMIDI.setHandleContinue(myMIDIClockContinue);
    usbHostMIDI.setHandleStart(myMIDIClockStart);
    usbHostMIDI.setHandleStop(myMIDIClockStop);
    if (DEBUG)
        Serial.println(F("USB HOST MIDI Class Compliant Listening"));

    // Read MIDI Out Channel from EEPROM
    midiOutCh = getMIDIOutCh();
    // Read MIDI Thru mode from EEPROM
    MIDIThru = getMidiThru();
    changeMIDIThruMode();
    // Read oscilloscope enable from EEPROM
    enableScope(getScopeEnable());
    // Read VU enable from EEPROM
    vuMeter = getVUEnable();
    // Read volume  from EEPROM
    currentVolume = getVolume();
    // Read global tuning from EEPROM
    tuningCents = getTuningCents();
    tuningCentsFrac = 1.0f + (tuningCents * CENTSFRAC);
    // Read arpeggiator settings from EEPROM
    arpDivision = getArpDivision();
    arpRange = getArpRange();
    arpStyle = getArpStyle();
    arpCycles = getArpCycles();
    if (arpCycles == ARP_HOLD)
        arpHold = true;
    arpBasis = getArpBasis();
    keyboardScale = getKeyboardScale();
    keyboardOct = getKeyboardBasis();
    sendCC = getSendCC();
    syncToMIDIClk = getSyncToMIDIClk();

    assignStrings();
    assignParametersForPerformanceEncoders();
    setupDisplay();
    setEncodersState(state);
    setupHardware(encoderCallback, encoderButtonCallback, buttonCallback);
}

// Specifically for Sequencer, bypassing recording note in and arp
// Note handlers run from RAM: flash cache misses add latency/jitter (also called by the sequencer/arp)
void noteOn(byte channel, byte note, byte velocity)
{
    // Check for out of range notes. Less than 20Hz isn't really audible
    if (note + groupvec[activeGroupIndex]->params().oscPitchA < 10 ||
        note + groupvec[activeGroupIndex]->params().oscPitchA > 127 ||
        note + groupvec[activeGroupIndex]->params().oscPitchB < 10 ||
        note + groupvec[activeGroupIndex]->params().oscPitchB > 127)
        return;
    AudioUpdateLock lock;
    groupvec[activeGroupIndex]->noteOn(note, velocity);
}

void myNoteOn(byte channel, byte note, byte velocity)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    // Seed random() (Random arp style) from the CPU cycle count at the first key press, so
    // it doesn't play the same sequence after every power-up
    static bool randomSeeded = false;
    if (!randomSeeded)
    {
        randomSeed(ARM_DWT_CYCCNT | 1);
        randomSeeded = true;
    }
    if (arpRunning)
    {
        // If it's in hold mode and you are not holding any notes down,
        // it continues to play the previous arpeggio. Once you press
        // a new note, it resets the arpeggio and starts a new one.
        // Also resets arpeggio from starting note.
        if (arpNotesHeld == 0)
        {
            resetNotes();
            // if (getNotesInArp() == 0)
            // {
            //     noteOn(midiChannel, note, velocity);
            //     setSeqTimerPeriod(currentSequence.bpm);
            //     sequencerStart();
            // }
        }
        if (isArpKeyHeld(note))
            return; // Duplicate note-on (e.g. two merged MIDI sources)
        if (arpNotesHeld < sizeof(arpNotes) - 1)
        {
            arpNotesHeld++;
            setArpKeyHeld(note, true);
        }
        else
            return;

        // find the right place to insert the note in the notes array
        if (arpStyle == PLAYORDER)
        {
            for (uint8_t i = 0; i < sizeof(arpNotes); i++)
            {
                if (arpNotes[i] == note)
                    return; // already in arpeggio
                else if (arpNotes[i] != '\0')
                    continue;
                else
                {
                    arpNotes[i] = note;
                    arpVels[i] = velocity;
                    return;
                }
            }
        }
        else
        {
            for (uint8_t i = 0; i < sizeof(arpNotes); i++)
            {
                if (arpNotes[i] == note)
                    return; // already in arpeggio
                else if (arpNotes[i] != '\0' && arpNotes[i] < note)
                    continue; // ignore the notes below it
                else
                {
                    // once we reach the first note in the arpeggio that's higher
                    // than the new one, move the rest of the arpeggio array over
                    // to the right
                    for (uint8_t j = sizeof(arpNotes) - 1; j > i; j--)
                    {
                        arpNotes[j] = arpNotes[j - 1];
                        arpVels[j] = arpVels[j - 1];
                    }

                    // and insert the note
                    arpNotes[i] = note;
                    arpVels[i] = velocity;
                    return;
                }
            }
        }
    }
    else
    {
        if (currentSequence.recording && currentSequence.running)
        {
            currentSequence.note_in = note;
            currentSequence.note_in_velocity = velocity;
        }
        else if (currentSequence.recording && !currentSequence.running && state == SEQUENCEEDIT)
        {
            currentSequence.Notes[currentSeqPosition] = note;
            currentSequence.Velocities[currentSeqPosition] = velocity;
            if (currentSeqPosition < currentSequence.length - 1)
                currentSeqPosition++;
            else
                currentSeqPosition = 0;

            currentSeqNote = note;
            if (currentSeqNote < 108)
                seqCurrentOctPos = nearbyint((currentSeqNote + 4) / 12) - 2;
        }
        noteOn(midiChannel, note, velocity);
    }
}

void noteOff(byte channel, byte note, byte velocity)
{
    AudioUpdateLock lock;
    groupvec[activeGroupIndex]->noteOff(note);
}

void myNoteOff(byte channel, byte note, byte velocity)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    // Only keys the arp counted: not ones held before it started or rejected when it was full
    if (arpRunning && isArpKeyHeld(note))
    {
        setArpKeyHeld(note, false);
        if (arpNotesHeld > 0)
            arpNotesHeld--;
        // Remove exactly the released note (the old shift-based removal assumed a sorted
        // arpeggio, so it removed the wrong note in Play Order)
        if (!arpHold)
            removeArpNote(note);
        // The arp plays its own notes: this key never sounded, and a note-off here cut
        // off an arp note of the same pitch
        return;
    }
    noteOff(channel, note, velocity); // Includes keys held before the arp started
}

FLASHMEM void updatePatchnameAndIndex(String name, uint32_t index, uint32_t UID)
{
    groupvec[activeGroupIndex]->setPatchName(name);
    groupvec[activeGroupIndex]->setPatchIndex(index);
    groupvec[activeGroupIndex]->setUID(UID);
    setPatchNoAndNameForDisp(String(index), name);
}

void myPitchBend(byte channel, int bend)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    groupvec[activeGroupIndex]->pitchBend(bend * currentPatch.PitchBend * DIV12xDIV8192);
}

// MIDI CC
FLASHMEM void myControlChange(byte channel, byte control, byte value)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (DEBUG)
        Serial.printf("Ch:%u: cc=%d, v=%d\n", channel, control, value);
    if (sendCC)
        midiCCOut(control, value); // Send patch parameters

    switch (control)
    {
    case CCvolume:
        currentVolume = value;
        storeVolumeToEEPROM(value);
        // Audio taper: level = knob^2, so half way is -12dB rather than -6dB (most of the
        // change used to be in the bottom fifth of the travel). Maximum is unchanged.
        groupvec[activeGroupIndex]->setVolume(LINEAR[currentVolume] * LINEAR[currentVolume] * MAX_VOL);
        showCurrentParameterOverlay(ParameterStrMap[CCvolume], LINEAR[currentVolume]);
        break;
    case CCunison:
    {
        currentPatch.Unison = value;
        groupvec[activeGroupIndex]->setUnisonMode(value);

        if (value == 0)
        {
            setEncValue(CCdetune, currentPatch.Detune, String((1 - groupvec[activeGroupIndex]->params().detune) * 100) + F("%"));
            if (!setEncValue(CCunison, value, F("Off")))
                showCurrentParameterOverlay(F("Unison"), F("Off"));
        }
        else if (value == 1)
        {
            setEncValue(CCdetune, currentPatch.Detune, String((1 - groupvec[activeGroupIndex]->params().detune) * 100) + "%");
            if (!setEncValue(CCunison, value, F("Dynamic")))
                showCurrentParameterOverlay(F("Dynamic"), F("On"));
        }
        else
        {
            setEncValue(CCdetune, currentPatch.ChordDetune, String(CDT_STR[groupvec[activeGroupIndex]->params().chordDetune]));
            if (!setEncValue(CCunison, value, F("Chord")))
                showCurrentParameterOverlay(F("Chord"), F("On"));
        }
        break;
    }
    case CCglide:
        currentPatch.Glide = value;
        groupvec[activeGroupIndex]->params().glideSpeed = POWER[value];
        if (!setEncValue(CCglide, value, milliToString(POWER[value] * GLIDEFACTOR)))
            showCurrentParameterOverlay(ParameterStrMap[CCglide], milliToString(POWER[value] * GLIDEFACTOR));
        break;

    case CCpitchenv:
    {
        currentPatch.PitchEnv = value;
        float env = LINEARCENTREZERO[value] * OSCMODMIXERMAX;
        groupvec[activeGroupIndex]->setPitchEnvelope(env);
        if (!setEncValue(CCpitchenv, value, String(env)))
            showCurrentParameterOverlay(ParameterStrMap[CCpitchenv], String(env));
        break;
    }
    case CCoscwaveformA:
        currentPatch.WaveformA = value;
        groupvec[activeGroupIndex]->setWaveformA(getWaveformA(value));
        if (!setEncValue(CCoscwaveformA, value, getWaveformStr(getWaveformA(value))))
            showCurrentParameterOverlay(ParameterStrMap[CCoscwaveformA] + String(" Osc 1"), getWaveformStr(getWaveformA(value)));
        break;
    case CCoscwaveformB:
        currentPatch.WaveformB = value;
        groupvec[activeGroupIndex]->setWaveformB(getWaveformB(value));
        if (!setEncValue(CCoscwaveformB, value, getWaveformStr(getWaveformB(value))))
            showCurrentParameterOverlay(ParameterStrMap[CCoscwaveformB] + String(" Osc 2"), getWaveformStr(getWaveformB(value)));
        break;

    case CCpitchA:
        currentPatch.PitchA = value;
        groupvec[activeGroupIndex]->params().oscPitchA = PITCH[value];
        groupvec[activeGroupIndex]->updateVoices();
        if (!setEncValue(CCpitchA, value, (PITCH[value] > 0 ? F("+") : F("")) + String(PITCH[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCpitchA] + String(" Osc 1"), (PITCH[value] > 0 ? F("+") : F("")) + String(PITCH[value]));
        break;

    case CCpitchB:
        currentPatch.PitchB = value;
        groupvec[activeGroupIndex]->params().oscPitchB = PITCH[value];
        groupvec[activeGroupIndex]->updateVoices();
        if (!setEncValue(CCpitchB, value, (PITCH[value] > 0 ? F("+") : F("")) + String(PITCH[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCpitchB] + String(" Osc 2"), (PITCH[value] > 0 ? F("+") : F("")) + String(PITCH[value]));
        break;

    case CCdetune:
    {
        float detune = 1.0f - (MAXDETUNE * POWER[value]);
        groupvec[activeGroupIndex]->params().detune = detune;
        groupvec[activeGroupIndex]->params().chordDetune = value;
        groupvec[activeGroupIndex]->updateVoices();

        if (groupvec[activeGroupIndex]->params().unisonMode == 2) // Chord Detune
        {
            currentPatch.ChordDetune = value;
            if (!setEncValue(CCdetune, value, CDT_STR[value]))
                showCurrentParameterOverlay(F("Chord"), CDT_STR[value]);
        }
        else
        {
            currentPatch.Detune = value;
            if (!setEncValue(CCdetune, value, String((1 - detune) * 100) + F("%")))
                showCurrentParameterOverlay(ParameterStrMap[CCdetune], String((1 - detune) * 100) + F("%"));
        }
        break;
    }
    case CCpwmSourceA:
        currentPatch.PWMSourceA = value;
        groupvec[activeGroupIndex]->setPWMSourceA(value);

        if (value == PWMSOURCELFO)
        {
            if (!setEncValue(CCpwmSourceA, value, F("LFO")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceA] + String(" Osc 1"), F("LFO"));
            // Turn on rate control
            setEncValue(true, CCpwmRateA, currentPatch.PWMRateA, String(PWMRATE[currentPatch.PWMRateA]) + F("Hz"), CCpwmRateA);
            setEncInactive(CCpwA);
        }
        else if (value == PWMSOURCEFENV)
        {
            if (!setEncValue(CCpwmSourceA, value, F("Filter Env")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceA] + String(" Osc 1"), F("Filter Env"));
            setEncInactive(CCpwmRateA);
            setEncInactive(CCpwA);
            // Turn on pwm amt control
            setEncValue(true, CCpwmAmtA, currentPatch.PWMA_Amount, String(LINEAR[currentPatch.PWMA_Amount]), CCpwmAmtA);
        }
        else
        {
            if (!setEncValue(CCpwmSourceA, value, F("Manual")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceA] + String(" Osc 1"), F("Manual"));
            setEncInactive(CCpwmRateA);
            setEncInactive(CCpwmAmtA);
            if (groupvec[activeGroupIndex]->getWaveformA() == WAVEFORM_TRIANGLE_VARIABLE)
            {
                setEncValue(true, CCpwA, currentPatch.PWA_Amount, F("Tri ") + String(groupvec[activeGroupIndex]->getPwA()), CCpwA);
            }
            else
            {
                setEncValue(true, CCpwA, currentPatch.PWA_Amount, F("Pulse ") + String(groupvec[activeGroupIndex]->getPwA()), CCpwA);
            }
        }
        break;
    case CCpwmSourceB:
        currentPatch.PWMSourceB = value;
        groupvec[activeGroupIndex]->setPWMSourceB(value);

        if (value == PWMSOURCELFO)
        {
            if (!setEncValue(CCpwmSourceB, value, F("LFO")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceB] + String(" Osc 2"), F("LFO"));
            // Turn on rate control
            setEncValue(true, CCpwmRateB, currentPatch.PWMRateB, String(PWMRATE[currentPatch.PWMRateB]) + F("Hz"), CCpwmRateB);
            setEncInactive(CCpwB);
        }
        else if (value == PWMSOURCEFENV)
        {
            if (!setEncValue(CCpwmSourceB, value, F("Filter Env")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceB] + String(" Osc 2"), F("Filter Env"));
            setEncInactive(CCpwB);
            setEncInactive(CCpwmRateB);
            // Turn on pwm amt controls
            setEncValue(true, CCpwmAmtB, currentPatch.PWMB_Amount, String(LINEAR[currentPatch.PWMB_Amount]), CCpwmAmtB);
        }
        else
        {
            if (!setEncValue(CCpwmSourceB, value, F("Manual")))
                showCurrentParameterOverlay(ParameterStrMap[CCpwmSourceB], F("Manual"));
            setEncInactive(CCpwmRateB);
            setEncInactive(CCpwmAmtB);
            if (groupvec[activeGroupIndex]->getWaveformB() == WAVEFORM_TRIANGLE_VARIABLE)
            {
                setEncValue(true, CCpwB, currentPatch.PWB_Amount, F("Tri ") + String(groupvec[activeGroupIndex]->getPwB()), CCpwB);
            }
            else
            {
                setEncValue(true, CCpwB, currentPatch.PWB_Amount, F("Pulse ") + String(groupvec[activeGroupIndex]->getPwB()), CCpwB);
            }
        }
        break;
    case CCpwmRateA:
        currentPatch.PWMRateA = value;
        groupvec[activeGroupIndex]->setPwmRateA(PWMRATE[value]);
        if (!setEncValue(CCpwmRateA, value, String(2 * PWMRATE[value]) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCpwmRateA] + String(" Osc 1"), String(2 * PWMRATE[value]) + F(" Hz")); // PWM goes through mid to maximum, sounding effectively twice as fast
        break;
    case CCpwmRateB:
        currentPatch.PWMRateB = value;
        groupvec[activeGroupIndex]->setPwmRateB(PWMRATE[value]);
        if (!setEncValue(CCpwmRateB, value, String(2 * PWMRATE[value]) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCpwmRateB] + String(" Osc 2"), String(2 * PWMRATE[value]) + F(" Hz")); // PWM goes through mid to maximum, sounding effectively twice as fast
        break;

    case CCpwmAmtA:
        currentPatch.PWMA_Amount = value;
        if (currentPatch.PWMSourceA == PWMSOURCELFO)
        {
            groupvec[activeGroupIndex]->setPwmMixerALFO(LINEAR[value]);
        }
        else if (currentPatch.PWMSourceA == PWMSOURCEFENV)
        {
            groupvec[activeGroupIndex]->setPwmMixerAFEnv(LINEAR[value]);
        }
        if (!setEncValue(CCpwmAmtA, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCpwmAmtA] + String(" Osc 1"), String(LINEAR[value]));
        break;

    case CCpwmAmtB:
        currentPatch.PWMB_Amount = value;
        if (currentPatch.PWMSourceB == PWMSOURCELFO)
        {
            groupvec[activeGroupIndex]->setPwmMixerBLFO(LINEAR[value]);
        }
        else if (currentPatch.PWMSourceB == PWMSOURCEFENV)
        {
            groupvec[activeGroupIndex]->setPwmMixerBFEnv(LINEAR[value]);
        }
        if (!setEncValue(CCpwmAmtB, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCpwmAmtB] + String(" Osc 2"), String(LINEAR[value]));
        break;

    case CCpwA:
        currentPatch.PWA_Amount = value;
        groupvec[activeGroupIndex]->setPWA(LINEARCENTREZERO[value]);
        if (groupvec[activeGroupIndex]->getWaveformA() == WAVEFORM_TRIANGLE_VARIABLE)
        {
            if (!setEncValue(CCpwA, value, F("Tri ") + String(groupvec[activeGroupIndex]->getPwA())))
                showCurrentParameterOverlay2("Var Triangle Osc 1", groupvec[activeGroupIndex]->getPwA(), VAR_TRI);
        }
        else
        {
            if (!setEncValue(CCpwA, value, F("Pulse ") + String(groupvec[activeGroupIndex]->getPwA())))
                showCurrentParameterOverlay2("Pulse Width Osc 1", groupvec[activeGroupIndex]->getPwA(), PULSE);
        }
        break;

    case CCpwB:
        currentPatch.PWB_Amount = value;
        groupvec[activeGroupIndex]->setPWB(LINEARCENTREZERO[value]);
        if (groupvec[activeGroupIndex]->getWaveformB() == WAVEFORM_TRIANGLE_VARIABLE)
        {
            if (!setEncValue(CCpwB, value, F("Tri ") + String(groupvec[activeGroupIndex]->getPwB())))
                showCurrentParameterOverlay2("Variable Triangle Osc 2", groupvec[activeGroupIndex]->getPwB(), VAR_TRI);
        }
        else
        {
            if (!setEncValue(CCpwB, value, F("Pulse ") + String(groupvec[activeGroupIndex]->getPwB())))
                showCurrentParameterOverlay2("Pulse Width Osc 2", groupvec[activeGroupIndex]->getPwB(), PULSE);
        }
        break;

    case CCoscLevelA:
        if (currentPatch.OscFX == OSCFXXMOD)
        {
            value < 0.01f ? currentPatch.OscLevelA = 0.01f : currentPatch.OscLevelA = value; // Avoid problems with noise when level is zero
        }
        else
        {
            currentPatch.OscLevelA = value;
        }
        groupvec[activeGroupIndex]->setOscLevelA(LINEAR[value]);
        switch (groupvec[activeGroupIndex]->getOscFX())
        {
        case OSCFXXMOD: // XMod
            if (!setEncValue(CCoscLevelA, value, String(groupvec[activeGroupIndex]->getOscLevelA())))
                showCurrentParameterOverlay(F("X-Mod Osc 1"), F("by Osc 2: ") + String(1 - groupvec[activeGroupIndex]->getOscLevelB()));
            break;
        case OSCFXOFF: // None
        case OSCFXXOR: // XOR
            if (!setEncValue(CCoscLevelA, value, String(groupvec[activeGroupIndex]->getOscLevelA())))
                showCurrentParameterOverlay(ParameterStrMap[CCoscLevelA] + String(" Osc 1"), String(groupvec[activeGroupIndex]->getOscLevelA()));
            break;
        }
        break;

    case CCoscLevelB:
        if (currentPatch.OscFX == OSCFXXMOD)
        {
            value < 0.01f ? currentPatch.OscLevelB = 0.01f : currentPatch.OscLevelB = value; // Avoid problems with noise when level is zero
        }
        else
        {
            currentPatch.OscLevelB = value;
        }
        groupvec[activeGroupIndex]->setOscLevelB(LINEAR[value]);
        switch (groupvec[activeGroupIndex]->getOscFX())
        {
        case OSCFXXMOD: // XMod
            if (!setEncValue(CCoscLevelB, value, String(groupvec[activeGroupIndex]->getOscLevelB())))
                showCurrentParameterOverlay(F("X-Mod Osc 2"), F("by Osc 1: ") + String(1 - groupvec[activeGroupIndex]->getOscLevelA()));
            break;
        case OSCFXOFF: // None
        case OSCFXXOR: // XOR
            if (!setEncValue(CCoscLevelB, value, String(groupvec[activeGroupIndex]->getOscLevelB())))
                showCurrentParameterOverlay(ParameterStrMap[CCoscLevelB] + String(" Osc 2"), String(groupvec[activeGroupIndex]->getOscLevelB()));
            break;
        }
        break;

    case CCnoiseLevel:
    {
        currentPatch.NoiseLevel = value;
        float noise = LINEARCENTREZERO[value];
        float pink = 0.0;
        float white = 0.0;
        if (noise > 0)
        {
            pink = noise;
        }
        else if (noise < 0)
        {
            white = fabs(noise);
        }
        groupvec[activeGroupIndex]->setPinkNoiseLevel(pink);
        groupvec[activeGroupIndex]->setWhiteNoiseLevel(white);

        if (noise > 0)
        {
            if (!setEncValue(CCnoiseLevel, value, F("Pink ") + String(pink)))
                showCurrentParameterOverlay(ParameterStrMap[CCnoiseLevel], F("Pink ") + String(pink));
        }
        else if (noise < 0)
        {
            if (!setEncValue(CCnoiseLevel, value, F("White ") + String(white)))
                showCurrentParameterOverlay(ParameterStrMap[CCnoiseLevel], F("White ") + String(white));
        }
        else
        {
            if (!setEncValue(CCnoiseLevel, value, F("Off")))
                showCurrentParameterOverlay(ParameterStrMap[CCnoiseLevel], F("Off"));
        }
        break;
    }
    case CCbankselectLSB:
        if (value < BANKS_LIMIT)
        {
            currentBankIndex = value;
            if (!cardStatus)
                return;
            loadPatchNamesFromBank(currentBankIndex);
            if (patches.size() > 0 && patches[0].patchUID > 0)
            {
                currentPatchIndex = 0;
                recallPatch(currentBankIndex, patches[0].patchUID);
            }
        }
        break;

    case CCfilterfreq:
        // MIDI is 7 bit, 128 values and needs to choose alternate filterfreqs(8 bit) by multiplying by 2
        currentPatch.FilterFreq = value << 1; // FilterFreq is 8 bit
        groupvec[activeGroupIndex]->setCutoff(FILTERFREQS256[currentPatch.FilterFreq]);
        if (!setEncValue(filterfreq256, currentPatch.FilterFreq, String(int(FILTERFREQS256[currentPatch.FilterFreq])) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterfreq], String(int(FILTERFREQS256[currentPatch.FilterFreq])) + F(" Hz"));
        break;

    case filterfreq256:
        // 8 bit from panel control for smoothness
        currentPatch.FilterFreq = value;
        groupvec[activeGroupIndex]->setCutoff(FILTERFREQS256[value]);
        if (!setEncValue(filterfreq256, value, String(int(FILTERFREQS256[value])) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterfreq], String(int(FILTERFREQS256[value])) + F(" Hz"));
        break;

    case CCfilterres:
    {
        currentPatch.FilterRes = value;
        float res = (14.29f * POWER[value]) + 0.71f;
        groupvec[activeGroupIndex]->setResonance(res);
        if (!setEncValue(CCfilterres, value, res))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterres], res);
        break;
    }
    case CCfiltermixer:
    {
        currentPatch.FilterMixer = value;
        groupvec[activeGroupIndex]->setFilterMixer(LINEAR_FILTERMIXER[value]);

        String filterStr;
        if (LINEAR_FILTERMIXER[value] == BANDPASS)
        {
            filterStr = F("Band Pass");
        }
        else
        {
            // LP-HP mix mode - a notch filter
            if (LINEAR_FILTERMIXER[value] == LOWPASS)
            {
                filterStr = F("Low Pass");
            }
            else if (LINEAR_FILTERMIXER[value] == HIGHPASS)
            {
                filterStr = F("High Pass");
            }
            else
            {
                filterStr = F("Low ") + String(100 - int(100 * LINEAR_FILTERMIXER[value])) + F(" - ") + String(int(100 * LINEAR_FILTERMIXER[value])) + F(" High");
            }
        }
        if (!setEncValue(CCfiltermixer, value, filterStr))
            showCurrentParameterOverlay(ParameterStrMap[CCfiltermixer], filterStr);

        break;
    }

    case CCfilterenv:
    {
        currentPatch.FilterEnv = value;
        float env = LINEARCENTREZERO[value] * FILTERMODMIXERMAX;
        groupvec[activeGroupIndex]->setFilterEnvelope(env);
        if (!setEncValue(CCfilterenv, value, String(env)))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterenv], String(env));
        break;
    }
    case CCkeytracking:
        currentPatch.KeyTracking = value;
        groupvec[activeGroupIndex]->setKeytracking(LINEAR[value]);
        if (!setEncValue(CCkeytracking, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCkeytracking], String(LINEAR[value]));
        break;

    case CCmodwheel:
        // Variable LFO and filter cutoff amount from mod wheel
        groupvec[activeGroupIndex]->setPitchModWheelAmount(POWER[value] * currentPatch.PitchModWheelDepth / 10.0f);
        groupvec[activeGroupIndex]->setFilterModWheelAmount(LINEAR[value] * currentPatch.FilterModWheelDepth * 120.0f);
        break;

    case CCosclfoamt:
        currentPatch.PitchLFOAmt = value;
        groupvec[activeGroupIndex]->setPitchLfoAmount(POWER[value]);
        char buf[10];
        if (!setEncValue(CCosclfoamt, value, dtostrf(POWER[value], 4, 3, buf)))
            showCurrentParameterOverlay(ParameterStrMap[CCosclfoamt], dtostrf(POWER[value], 4, 3, buf));
        break;

    case CCoscLfoRate:
    {
        currentPatch.PitchLFORate = value;
        float rate = 0.0;
        if (groupvec[activeGroupIndex]->getPitchLfoMidiClockSync())
        {
            // TODO: Multitimbral - MIDI Tempo stuff remains global?
            lfoPitchTempoValue = LFOTEMPO[value];
            rate = lfoSyncFreq * LFOTEMPO[value];
            if (!setEncValue(CCoscLfoRate, value, LFOTEMPOSTR[value]))
                showCurrentParameterOverlay(ParameterStrMap[CCoscLfoRate], LFOTEMPOSTR[value]);
        }
        else
        {
            rate = LFOMAXRATE * POWER[value];
            if (!setEncValue(CCoscLfoRate, value, String(rate) + F(" Hz")))
                showCurrentParameterOverlay(ParameterStrMap[CCoscLfoRate], String(rate) + F(" Hz"));
        }
        groupvec[activeGroupIndex]->setPitchLfoRate(rate);

        break;
    }

    case CCoscLfoWaveform:
        currentPatch.PitchLFOWaveform = value;
        groupvec[activeGroupIndex]->setPitchLfoWaveform(getLFOWaveform(value));
        if (!setEncValue(CCoscLfoWaveform, value, getWaveformStr(getLFOWaveform(value))))
            showCurrentParameterOverlay(ParameterStrMap[CCoscLfoWaveform], getWaveformStr(getLFOWaveform(value)));
        break;

    case CCosclforetrig:
        currentPatch.PitchLFORetrig = value;
        groupvec[activeGroupIndex]->setPitchLfoRetrig(value > 0);
        if (!setEncValue(CCosclforetrig, value, value > 0 ? F("On") : F("Off")))
            showCurrentParameterOverlay(ParameterStrMap[CCosclforetrig], value > 0 ? F("On") : F("Off"));
        break;

    case CCfilterLFOMidiClkSync:
        currentPatch.FilterLFOMidiClkSync = value;
        groupvec[activeGroupIndex]->setFilterLfoMidiClockSync(value > 0);
        if (!setEncValue(CCfilterLFOMidiClkSync, value, value > 0 ? F("On") : F("Off")))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterLFOMidiClkSync], value > 0 ? F("On") : F("Off"));

        if (value > 0)
            setEncValue(CCfilterlforate, LFOTEMPO[currentPatch.FilterLFORate], LFOTEMPOSTR[currentPatch.FilterLFORate]);
        else
            setEncValue(CCfilterlforate, currentPatch.FilterLFORate, String(groupvec[activeGroupIndex]->getFilterLfoRate()) + " Hz");
        break;

    case CCfilterlforate:
    {
        currentPatch.FilterLFORate = value;
        float rate;
        String timeDivStr = "";
        if (groupvec[activeGroupIndex]->getFilterLfoMidiClockSync())
        {
            lfoFilterTempoValue = LFOTEMPO[value];
            rate = lfoSyncFreq * LFOTEMPO[value];
            timeDivStr = LFOTEMPOSTR[value];
            if (!setEncValue(CCfilterlforate, value, timeDivStr))
                showCurrentParameterOverlay(F("LFO Time Division"), timeDivStr);
        }
        else
        {
            rate = LFOMAXRATE * POWER[value];
            if (!setEncValue(CCfilterlforate, value, String(rate) + F(" Hz")))
                showCurrentParameterOverlay(ParameterStrMap[CCfilterlforate], String(rate) + F(" Hz"));
        }

        groupvec[activeGroupIndex]->setFilterLfoRate(rate);
        break;
    }

    case CCfilterlfoamt:
    {
        currentPatch.FilterLfoAmt = value;
        float amt = LINEAR[value] * FILTERMODMIXERMAX;
        groupvec[activeGroupIndex]->setFilterLfoAmt(amt);
        if (!setEncValue(CCfilterlfoamt, value, String(amt)))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterlfoamt], String(amt));
        break;
    }
    case CCfilterlfowaveform:
        currentPatch.FilterLFOWaveform = value;
        groupvec[activeGroupIndex]->setFilterLfoWaveform(getLFOWaveform(value));
        if (!setEncValue(CCfilterlfowaveform, value, getWaveformStr(getLFOWaveform(value))))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterlfowaveform], getWaveformStr(getLFOWaveform(value)));
        break;

    case CCfilterlforetrig:
        currentPatch.FilterLFORetrig = value;
        groupvec[activeGroupIndex]->setFilterLfoRetrig(value > 0);
        if (!setEncValue(CCfilterlforetrig, value, value > 0 ? F("On") : F("Off")))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterlforetrig], value > 0 ? F("On") : F("Off"));
        break;

    case CCoscLFOMidiClkSync:
        currentPatch.PitchLFOMidiClkSync = value;
        groupvec[activeGroupIndex]->setPitchLfoMidiClockSync(value > 0);
        if (!setEncValue(CCoscLFOMidiClkSync, value, value > 0 ? F("On") : F("Off")))
            showCurrentParameterOverlay(ParameterStrMap[CCoscLFOMidiClkSync], value > 0 ? F("On") : F("Off"));

        if (value > 0)
            setEncValue(CCoscLfoRate, LFOTEMPO[currentPatch.PitchLFORate], LFOTEMPOSTR[currentPatch.PitchLFORate]);
        else
            setEncValue(CCoscLfoRate, currentPatch.PitchLFORate, String(groupvec[activeGroupIndex]->getPitchLfoRate()) + " Hz");
        break;

    case CCfilterattack:
        currentPatch.FilterAttack = value;
        groupvec[activeGroupIndex]->setFilterAttack(ENVTIMES[value]);
        if (!setEncValue(CCfilterattack, value, milliToString(ENVTIMES[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterattack], milliToString(ENVTIMES[value]), FILTER_ENV);
        break;

    case CCfilterdecay:
        currentPatch.FilterDecay = value;
        groupvec[activeGroupIndex]->setFilterDecay(ENVTIMES[value]);
        if (!setEncValue(CCfilterdecay, value, milliToString(ENVTIMES[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterdecay], milliToString(ENVTIMES[value]), FILTER_ENV);
        break;

    case CCfiltersustain:
        currentPatch.FilterSustain = value;
        groupvec[activeGroupIndex]->setFilterSustain(LINEAR[value]);
        if (!setEncValue(CCfiltersustain, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCfiltersustain], String(LINEAR[value]), FILTER_ENV);
        break;

    case CCfilterrelease:
        currentPatch.FilterRelease = value;
        groupvec[activeGroupIndex]->setFilterRelease(ENVTIMES[value]);
        if (!setEncValue(CCfilterrelease, value, milliToString(ENVTIMES[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterrelease], milliToString(ENVTIMES[value]), FILTER_ENV);
        break;

    case CCampattack:
        currentPatch.Attack = value;
        groupvec[activeGroupIndex]->setAmpAttack(ENVTIMES[value]);
        if (!setEncValue(CCampattack, value, milliToString(ENVTIMES[value])))
        {
            showCurrentParameterOverlay(ParameterStrMap[CCampattack], milliToString(ENVTIMES[value]), AMP_ENV);
        }
        break;

    case CCampdecay:
        currentPatch.Decay = value;
        groupvec[activeGroupIndex]->setAmpDecay(ENVTIMES[value]);
        if (!setEncValue(CCampdecay, value, milliToString(ENVTIMES[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCampdecay], milliToString(ENVTIMES[value]), AMP_ENV);
        break;

    case CCampsustain:
        currentPatch.Sustain = value;
        groupvec[activeGroupIndex]->setAmpSustain(LINEAR[value]);
        if (!setEncValue(CCampsustain, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCampsustain], String(LINEAR[value]), AMP_ENV);
        break;
    case CCamprelease:
        currentPatch.Release = value;
        groupvec[activeGroupIndex]->setAmpRelease(ENVTIMES[value]);
        if (!setEncValue(CCamprelease, value, milliToString(ENVTIMES[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCamprelease], milliToString(ENVTIMES[value]), AMP_ENV);
        break;
    case CCoscfx:
    {
        currentPatch.OscFX = value;
        groupvec[activeGroupIndex]->setOscFX(value);
        if (value == OSCFXXMOD)
        {
            if (!setEncValue(CCoscfx, value, F("X Mod")))
                showCurrentParameterOverlay(ParameterStrMap[CCoscfx], F("X Mod"));
            // Turn osc level controls on
            setEncValue(true, CCoscLevelA, currentPatch.OscLevelA, groupvec[activeGroupIndex]->getOscLevelA(), CCoscLevelA);
            setEncValue(true, CCoscLevelB, currentPatch.OscLevelB, groupvec[activeGroupIndex]->getOscLevelB(), CCoscLevelB);
        }
        else if (value == OSCFXXOR)
        {
            if (!setEncValue(CCoscfx, value, F("XOR Mod")))
                showCurrentParameterOverlay(ParameterStrMap[CCoscfx], F("XOR Mod"));
            // Turn osc level controls off
            setEncInactive(CCoscLevelA);
            setEncInactive(CCoscLevelB);
        }
        else
        {
            if (!setEncValue(CCoscfx, value, F("Off")))
                showCurrentParameterOverlay(ParameterStrMap[CCoscfx], F("Off"));
            // Turn osc level controls off
            setEncInactive(CCoscLevelA);
            setEncInactive(CCoscLevelB);
        }
        break;
    }
    case CCensemblefxamt:
        currentPatch.EnsembleEffectAmt = value;
        groupvec[activeGroupIndex]->setEnsembleEffectAmount(ENSEMBLE_LFO[value]);
        if (!setEncValue(CCensemblefxamt, value, String(ENSEMBLE_LFO[value]) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxamt], String(ENSEMBLE_LFO[value]) + F(" Hz"));
        break;
    case CCensemblefxmix:
        currentPatch.EnsembleEffectMix = value;
        groupvec[activeGroupIndex]->setEnsembleEffectMix(LINEAR[value]);
        if (!setEncValue(CCensemblefxmix, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxmix], String(LINEAR[value]));
        break;
    case CCreverbfxtime:
        currentPatch.ReverbEffectTime = value;
        groupvec[activeGroupIndex]->setReverbEffectTime((LINEAR[value]) + 0.01f); // Doesn't like 0.0
        if (!setEncValue(CCreverbfxtime, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxtime], String(LINEAR[value]));
        break;
    case CCreverbfxmix:
        currentPatch.ReverbEffectMix = value;
        groupvec[activeGroupIndex]->setReverbEffectMix(LINEAR[value]);
        if (!setEncValue(CCreverbfxmix, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxmix], String(LINEAR[value]));
        break;
    case CCallnotesoff:
        groupvec[activeGroupIndex]->allNotesOff();
        break;

    case CCvelocitySens:
        currentPatch.VelocitySensitivity = value;
        if (!setEncValue(CCvelocitySens, value, velocityStr[currentPatch.VelocitySensitivity]))
            showCurrentParameterOverlay(ParameterStrMap[CCvelocitySens], velocityStr[currentPatch.VelocitySensitivity]);
        break;
    case CCFilterVelocitySens:
        currentPatch.FilterVelocitySensitivity = value;
        if (!setEncValue(CCFilterVelocitySens, value, velocityStr[currentPatch.FilterVelocitySensitivity]))
            showCurrentParameterOverlay(ParameterStrMap[CCFilterVelocitySens], velocityStr[currentPatch.FilterVelocitySensitivity]);
        break;
    case CCmonomode:
        currentPatch.MonophonicMode = value;
        groupvec[activeGroupIndex]->setMonophonic(value);
        if (!setEncValue(CCmonomode, value, MonophonicStr[value]))
            showCurrentParameterOverlay(ParameterStrMap[CCmonomode], MonophonicStr[value]);
        break;
    case ampenvshape:
        if (currentPatch.AmpEnvShape != 0 && value == 0)
            silence(); // Prevent noise when going to Linear
        currentPatch.AmpEnvShape = value;
        for (uint8_t i = 0; i < global.maxVoices(); i++)
        {
            global.Oscillators[i].ampEnvelope_.setEnvType(ENVSHAPE[value]);
        }
        if (!setEncValue(ampenvshape, value, EnvShapeStr[value]))
            showCurrentParameterOverlay(ParameterStrMap[ampenvshape], EnvShapeStr[value]);
        break;
    case filterenvshape:
        currentPatch.FilterEnvShape = value;
        for (uint8_t i = 0; i < global.maxVoices(); i++)
        {
            global.Oscillators[i].filterEnvelope_.setEnvType(ENVSHAPE[value]);
        }
        if (!setEncValue(filterenvshape, value, EnvShapeStr[value]))
            showCurrentParameterOverlay(ParameterStrMap[filterenvshape], EnvShapeStr[value]);
        break;
    case glideshape:
        currentPatch.GlideShape = value;
        for (uint8_t i = 0; i < global.maxVoices(); i++)
        {
            global.Oscillators[i].glide_.setMode(glideShape);
        }
        if (!setEncValue(glideshape, value, GlideShapeStr[value]))
            showCurrentParameterOverlay(ParameterStrMap[glideshape], GlideShapeStr[value]);
        break;
    case pitchbendrange:
        currentPatch.PitchBend = value;
        if (!setEncValue(pitchbendrange, value, String(value)))
            showCurrentParameterOverlay(ParameterStrMap[pitchbendrange], String(value));
        break;
    case filtermodwheeldepth:
        currentPatch.FilterModWheelDepth = value;
        groupvec[activeGroupIndex]->setFilterModWheelDepth(currentPatch.FilterModWheelDepth / 10.0f);
        if (!setEncValue(filtermodwheeldepth, value, String(currentPatch.FilterModWheelDepth / 10.0f)))
            showCurrentParameterOverlay(ParameterStrMap[filtermodwheeldepth], String(currentPatch.FilterModWheelDepth / 10.0f));
        break;
    case pitchmodwheeldepth:
        currentPatch.PitchModWheelDepth = value;
        groupvec[activeGroupIndex]->setPitchModWheelDepth(currentPatch.PitchModWheelDepth / 10.0f);
        if (!setEncValue(pitchmodwheeldepth, value, String(currentPatch.PitchModWheelDepth / 10.0f)))
            showCurrentParameterOverlay(ParameterStrMap[pitchmodwheeldepth], String(currentPatch.PitchModWheelDepth / 10.0f));
        break;
    case MIDIThruMode:
        MIDIThru = value;
        storeMidiThru(MIDIThru);
        changeMIDIThruMode();
        if (!setEncValue(MIDIThruMode, value, MIDIThruStr[MIDIThru]))
            showCurrentParameterOverlay(ParameterStrMap[MIDIThruMode], MIDIThruStr[MIDIThru]);
        break;
    case MIDIChOut:
    {
        midiOutCh = value;
        storeMidiOutCh(midiOutCh);
        String sOut = F("Off");
        if (midiOutCh > 0)
            sOut = String(midiOutCh);
        if (!setEncValue(MIDIChOut, value, sOut))
            showCurrentParameterOverlay(ParameterStrMap[MIDIChOut], sOut);
    }
    break;
    case MIDIChIn:
    {
        midiChannel = value;
        storeMidiChannel(midiChannel);
        String sIn = F("All");
        if (midiChannel > MIDI_CHANNEL_OMNI)
            sIn = String(midiChannel);
        if (!setEncValue(MIDIChIn, value, sIn))
            showCurrentParameterOverlay(ParameterStrMap[MIDIChIn], sIn);
    }
    break;
    }
}

FLASHMEM void myProgramChange(byte channel, byte program)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (state == State::PERFORMANCEPAGE || state == State::PERFORMANCERECALL)
    {
        if (program < PERFORMANCES_LIMIT)
        {
            if (state == State::PERFORMANCERECALL)
                state = State::PERFORMANCEPAGE;
            currentPerformanceIndex = program;
            recallPerformance(currentPerformanceIndex + 1);
            setEncodersState(State::PERFORMANCEPAGE);
            if (DEBUG)
                Serial.print(F("MIDI Pgm Change Performance:"));
            if (DEBUG)
                Serial.println(performances[currentPerformanceIndex]);
        }
    }
    else
    {
        if (program < patches.size())
        {
            currentPatchIndex = program;
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            if (DEBUG)
                Serial.print(F("MIDI Pgm Change:"));
            if (DEBUG)
                Serial.println(patches[currentPatchIndex].patchUID);
        }
    }
}

// Arp/Sequencer
FLASHMEM void playNote(uint8_t note)
{
    noteOn(midiChannel, note, 90);
    delay(300);
    noteOff(midiChannel, note, 0);
}

FLASHMEM void setSeqTimerPeriod(float bpm)
{
    currentSequence.bpm = bpm;
    currentSequence.tempo_us = 15'000'000 / bpm; // beats per bar microseconds
    if (arpRunning)
        currentSequence.tempo_us = currentSequence.tempo_us * (ARP_DIVISION_24PPQ[arpDivision] / 6.0f);
    sequencer_timer.setNextPeriod(currentSequence.tempo_us / 2);

    lfoSyncFreq = 60.0f / currentSequence.bpm;
    groupvec[activeGroupIndex]->filterMidiClock(lfoSyncFreq * lfoFilterTempoValue);
    groupvec[activeGroupIndex]->pitchMidiClock(lfoSyncFreq * lfoPitchTempoValue);
}

FLASHMEM void sequencerStart(boolean cont = false)
{
    setSeqTimerPeriod(currentSequence.bpm);
    if (!cont)
    {
        currentSequence.step = 0;
    }

    currentSequence.running = true;

    if (!seqMidiSync)
    {
        sequencer_timer.start();
    }
}

FLASHMEM void sequencerStop()
{
    sequencer_timer.stop();
    groupvec[activeGroupIndex]->allNotesOff();
    currentSequence.running = false;
    if (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE || state == State::SEQUENCERECALL)
        singleLED(ledColour::GREEN, 3);
    // currentSequence.recording = false;
    currentSequence.note_in = 0;
}

// The arp carries on with its own timer when MIDI clock stops or is lost
FLASHMEM void arpToInternalClock()
{
    if (arpRunning && !seqMidiSync)
    {
        midiClkArpCount = 0;
        sequencerStart(); // Starts the internal timer, as seqMidiSync is false
    }
}

// MIDI Stop, or the clock was lost (see loop()). seqMidiSync is only ever set while
// "sync to MIDI clock" is on, and is cleared here so the internal clock works again.
FLASHMEM void midiClockStopped()
{
    bool wasSynced = seqMidiSync;
    if (seqMidiSync)
    {
        sequencerStop();
        groupvec[activeGroupIndex]->allNotesOff();
        resetNotes();
    }
    seqMidiSync = false;
    MIDIClkSignal = false;
    if (wasSynced)
        arpToInternalClock();
}

FLASHMEM void myMIDIClockContinue()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    sinceMidiClock = 0;
    if (!syncToMIDIClk)
        return;
    if (!currentSequence.running)
    {
        seqMidiSync = true;
        sequencerStart(true);
    }
}

FLASHMEM void myMIDIClockStart()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    midiClkCount = 0;
    sinceMidiClock = 0;
    // Only follow the clock when "sync to MIDI clock" is on: the arp used to switch to it
    // regardless, and was then left with no timer at all after a MIDI Stop
    if (syncToMIDIClk)
    {
        if ((!currentSequence.running && !seqMidiSync) || arpRunning)
            seqMidiSync = true;
        if ((state == State::SEQUENCEPAGE || state == State::SEQUENCEEDIT) && seqMidiSync)
            sequencerStart();
        else if (arpRunning)
        {
            sequencer_timer.stop();
            midiClkArpCount = 0;
            seqSwapper = true;
        }
    }
    // Resync LFOs when MIDI Clock starts.
    // When there's a jump to a different
    // part of a track, such as in a DAW, the DAW must have same
    // rhythmic quantisation as Tempo Div.

    // TODO: Apply to all groupvec[activeGroupIndex]-> Maybe check channel?
    groupvec[activeGroupIndex]->midiClockStart();
    MIDIClkSignal = true;
}

FLASHMEM void myMIDIClockStop()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    midiClockStopped();
}

void myMIDIClock()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    sinceMidiClock = 0;
    MIDIClkSignal = true;
    if (seqMidiSync && !syncToMIDIClk)
        midiClockStopped(); // "Sync to MIDI clock" turned off while following the clock
    // 24ppq this is for 4/4 time, alternates between noteon and noteoff, 8 times
    if (seqMidiSync && syncToMIDIClk && currentSequence.running && (midiClkCount % 3) == 0)
    {
        sequencer();
        if (state == SEQUENCEPAGE || state == SEQUENCEEDIT || state == ARPPAGE1 || state == ARPPAGE2)
            setEncValue(SeqTempo, lfoSyncFreq * 60, String(lfoSyncFreq * 60, 1));
    }

    if (seqMidiSync && arpRunning)
    {
        const uint8_t division = ARP_DIVISION_24PPQ[arpDivision];
        // The division can be shortened while running: the count used to run on to 255
        // (seconds of silence, with the last note left on)
        if (midiClkArpCount >= division)
            midiClkArpCount = 0;
        if (midiClkArpCount == 0)
        {
            if (!seqSwapper)
                noteOffRoutine(); // Note-off was skipped by a division change
            noteOnRoutine();
        }
        else if (midiClkArpCount == division - 2 && !seqSwapper)
        {
            noteOffRoutine();
        }

        if (++midiClkArpCount >= division)
            midiClkArpCount = 0;
    }

    // This recalculates the LFO frequencies if the tempo changes (MIDI cLock is 24ppq)
    // At 120bpm, a quarter note is every 500ms, 24ppq may have an error of 20ms giving an error in calculated bpm of up to 5bpm
    if (++midiClkCount > 23)
    {
        // TODO: Multitimbral Most of this needs to move into the VoiceGroup
        timeNow = millis();
        lfoSyncFreq = 1000.0f / (timeNow - previousMillis);
        previousMillis = timeNow;
        groupvec[activeGroupIndex]->filterMidiClock(lfoSyncFreq * lfoFilterTempoValue);
        groupvec[activeGroupIndex]->pitchMidiClock(lfoSyncFreq * lfoPitchTempoValue);
        midiClkCount = 0;
    }
}

FLASHMEM void silence()
{
    groupvec[activeGroupIndex]->allNotesOff();
    groupvec[activeGroupIndex]->closeEnvelopes();
}

// Debounced patch recall, see selectPatchForRecall()
#define PATCH_RECALL_DELAY_MS 150
boolean patchRecallPending = false;
boolean patchRecallUpdatesPerformance = false;
elapsedMillis patchRecallTimer;

FLASHMEM void recallPatch(uint8_t bank, long patchUID)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    patchRecallPending = false;  // A direct recall replaces a pending one
    if (patchUID == 0)
        return;
    silence();
    if (loadPatch(bank, patchUID))
    {
        setCurrentPatchData();
        previousPatchIndex = currentPatchIndex;
        previousBankIndex = currentBankIndex;
        storeLastPatchToEEPROM(currentPatchIndex);
        storeLastBankToEEPROM(currentBankIndex);
        // Reset Midi channels - if coming from performance mode
        midiChannel = getMIDIChannel();
        midiOutCh = getMIDIOutCh();
        MIDIThru = getMidiThru();
        changeMIDIThruMode();
    }
}

// Turning the patch or bank encoder only selects and shows the patch. It is recalled once the
// encoder has been still for PATCH_RECALL_DELAY_MS, rather than on every detent: each recall
// reads the SD card, sets every parameter and writes EEPROM, which a fast turn did dozens of
// times a second (freezes on TSynth).
FLASHMEM void selectPatchForRecall(boolean updatePerformance)
{
    setPatchNoAndNameForDisp(String(currentPatchIndex + 1), patches[currentPatchIndex].patchName);
    patchRecallPending = true;
    patchRecallUpdatesPerformance = updatePerformance;
    patchRecallTimer = 0;
}

FLASHMEM void checkPendingPatchRecall()
{
    if (!patchRecallPending || patchRecallTimer < PATCH_RECALL_DELAY_MS)
        return;
    LockGuard lock(displayLock);
    patchRecallPending = false;
    recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
    if (patchRecallUpdatesPerformance)
    {
        currentPerformance.patches[0].bankIndex = currentBankIndex;
        currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
        currentPerformance.patches[0].patchName = currentPatchName;
    }
}

FLASHMEM void recallPerformance(uint8_t filename)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    silence();
    if (loadPerformance(filename))
    {
        //*** THIS IS HARDCODED TO THE FIRST PATCH AS MULTITIMBRALITY ISN'T FUNCTIONING ***
        if (loadPatch(currentPerformance.patches[0].bankIndex, currentPerformance.patches[0].UID))
        {
            midiChannel = currentPerformance.patches[0].midiCh;
            midiOutCh = currentPerformance.patches[0].midiChOut;
            MIDIThru = currentPerformance.patches[0].midiThru;
            changeMIDIThruMode();

            if (DEBUG)
                Serial.println(midiChannel);
            // TODO Set min and max note range but this is only useful for split and layer(multitimbral functionality)
            setCurrentPatchData();
            currentBankIndex = currentPerformance.patches[0].bankIndex;
            loadPatchNamesFromBank(currentBankIndex);
            currentPatchIndex = findPatchIndex(currentPerformance.patches[0].UID);
        }
        else
        {
            // Patch missing
            if (DEBUG)
                Serial.print(F("Patch Missing:"));
            if (DEBUG)
                Serial.println(currentPerformance.patches[0].UID);
            currentPerformance.patches[0].UID = 0;
        }
    }
    else
    {
        // Performance missing
        if (DEBUG)
            Serial.print(F("Performance Missing:"));
        if (DEBUG)
            Serial.println(filename);
    }
}

FLASHMEM void recallSequence(uint8_t filename)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    silence();
    if (!loadSequence(filename))
    {
        // Sequence missing
        if (DEBUG)
            Serial.print(F("Sequence Missing:"));
        if (DEBUG)
            Serial.println(filename);
        // Initialise sequence
        SequenceStruct ss;
        currentSequence = ss;
    }
    storeLastSequenceToEEPROM(currentSequenceIndex);
}

FLASHMEM void setCurrentPatchData()
{
    updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
    updateDisplay = false; // Don't show each parameter on the display as they are all updated.
    myControlChange(midiChannel, CCoscLevelA, currentPatch.OscLevelA);
    myControlChange(midiChannel, CCoscLevelB, currentPatch.OscLevelB);
    myControlChange(midiChannel, CCnoiseLevel, currentPatch.NoiseLevel);
    myControlChange(midiChannel, CCunison, currentPatch.Unison);
    myControlChange(midiChannel, CCoscfx, currentPatch.OscFX);
    myControlChange(midiChannel, CCdetune, currentPatch.Detune);
    myControlChange(midiChannel, CCoscLfoRate, currentPatch.PitchLFORate);
    myControlChange(midiChannel, CCkeytracking, currentPatch.KeyTracking);
    myControlChange(midiChannel, CCglide, currentPatch.Glide);
    myControlChange(midiChannel, CCpitchA, currentPatch.PitchA);
    myControlChange(midiChannel, CCpitchB, currentPatch.PitchB);
    myControlChange(midiChannel, CCoscwaveformA, currentPatch.WaveformA);
    myControlChange(midiChannel, CCoscwaveformB, currentPatch.WaveformB);
    myControlChange(midiChannel, CCpwmSourceA, currentPatch.PWMSourceA);
    myControlChange(midiChannel, CCpwmSourceB, currentPatch.PWMSourceB);
    myControlChange(midiChannel, CCpwA, currentPatch.PWA_Amount);
    myControlChange(midiChannel, CCpwmAmtA, currentPatch.PWMA_Amount);
    myControlChange(midiChannel, CCpwB, currentPatch.PWB_Amount);
    myControlChange(midiChannel, CCpwmAmtB, currentPatch.PWMB_Amount);
    myControlChange(midiChannel, CCpwmRateA, currentPatch.PWMRateA);
    myControlChange(midiChannel, CCpwmRateB, currentPatch.PWMRateB);
    myControlChange(midiChannel, CCfilterres, currentPatch.FilterRes);
    myControlChange(midiChannel, filterfreq256, currentPatch.FilterFreq); // 8 bit
    myControlChange(midiChannel, CCfiltermixer, currentPatch.FilterMixer);
    myControlChange(midiChannel, CCfilterenv, currentPatch.FilterEnv);
    myControlChange(midiChannel, CCosclfoamt, currentPatch.PitchLFOAmt);
    myControlChange(midiChannel, CCoscLFOMidiClkSync, currentPatch.PitchLFOMidiClkSync);
    myControlChange(midiChannel, CCoscLfoRate, currentPatch.PitchLFORate);
    myControlChange(midiChannel, CCoscLfoWaveform, currentPatch.PitchLFOWaveform);
    myControlChange(midiChannel, CCosclforetrig, currentPatch.PitchLFORetrig);
    myControlChange(midiChannel, CCfilterLFOMidiClkSync, currentPatch.FilterLFOMidiClkSync);
    myControlChange(midiChannel, CCfilterlforate, currentPatch.FilterLFORate);
    myControlChange(midiChannel, CCfilterlforetrig, currentPatch.FilterLFORetrig);
    myControlChange(midiChannel, CCfilterlfoamt, currentPatch.FilterLfoAmt);
    myControlChange(midiChannel, CCfilterlfowaveform, currentPatch.FilterLFOWaveform);
    myControlChange(midiChannel, CCfilterattack, currentPatch.FilterAttack);
    myControlChange(midiChannel, CCfilterdecay, currentPatch.FilterDecay);
    myControlChange(midiChannel, CCfiltersustain, currentPatch.FilterSustain);
    myControlChange(midiChannel, CCfilterrelease, currentPatch.FilterRelease);
    myControlChange(midiChannel, CCampattack, currentPatch.Attack);
    myControlChange(midiChannel, CCampdecay, currentPatch.Decay);
    myControlChange(midiChannel, CCampsustain, currentPatch.Sustain);
    myControlChange(midiChannel, CCamprelease, currentPatch.Release);
    myControlChange(midiChannel, CCensemblefxamt, currentPatch.EnsembleEffectAmt);
    myControlChange(midiChannel, CCensemblefxmix, currentPatch.EnsembleEffectMix);
    myControlChange(midiChannel, CCreverbfxtime, currentPatch.ReverbEffectTime);
    myControlChange(midiChannel, CCreverbfxmix, currentPatch.ReverbEffectMix);
    myControlChange(midiChannel, CCpitchenv, currentPatch.PitchEnv);
    myControlChange(midiChannel, CCvelocitySens, currentPatch.VelocitySensitivity);
    myControlChange(midiChannel, pitchmodwheeldepth, currentPatch.PitchModWheelDepth);
    myControlChange(midiChannel, filtermodwheeldepth, currentPatch.FilterModWheelDepth);
    myControlChange(midiChannel, pitchbendrange, currentPatch.PitchBend);
    myControlChange(midiChannel, ampenvshape, currentPatch.AmpEnvShape);
    myControlChange(midiChannel, filterenvshape, currentPatch.FilterEnvShape);
    myControlChange(midiChannel, glideshape, currentPatch.GlideShape);

    groupvec[activeGroupIndex]->params().chordDetune = currentPatch.ChordDetune;
    groupvec[activeGroupIndex]->setMonophonic(currentPatch.MonophonicMode);
    updateDisplay = true;

    if (DEBUG)
        Serial.print(F("Set Patch: "));
    if (DEBUG)
        Serial.print(currentPatch.PatchName);
    if (DEBUG)
        Serial.print(F(" UID: "));
    if (DEBUG)
        Serial.println(groupvec[activeGroupIndex]->getUID());
}

// Scales the encoder inc/decrement depending on the range to traverse
FLASHMEM int8_t encScaling(EncoderMappingStruct *enc, int8_t delta)
{
    int8_t countLimit = 0;
    switch (enc->Range)
    {
    case 64 ... 127:
        countLimit = 3;
        break;
    case 32 ... 63:
        countLimit = 4;
        break;
    case 16 ... 31:
        countLimit = 6;
        break;
    case 8 ... 15:
        countLimit = 9;
        // Less acceleration
        if (delta > 3)
            delta = 2;
        if (delta < -3)
            delta = -2;
        break;
    case 4 ... 7:
        countLimit = 12;
        // Less acceleration
        if (delta > 1)
            delta = 1;
        if (delta < -1)
            delta = -1;
        break;
    case 1 ... 3:
        countLimit = 15;
        // Less acceleration
        if (delta > 1)
            delta = 1;
        if (delta < -1)
            delta = -1;
        break;
    default: // over 127
        countLimit = 2;
        break;
    }

    enc->Counter += delta;
    if (enc->Counter < (countLimit * -1) || enc->Counter > countLimit)
    {
        enc->Counter = 0;
        if ((enc->Value + delta > -1) && (enc->Value + delta < enc->Range + 1))
        {
            enc->Value += delta;
        }
    }
    else
    {
        delta = 0;
    }

    return delta;
}

FLASHMEM void encoderCallback(unsigned enc_idx, int value, int delta)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    // --- Value isn't used because it's absolute values that don't change with changes to the encoder assignment---
    if (DEBUG)
        Serial.printf("enc[%u]: v=%d, d=%d\n", enc_idx, value, delta);
    //  Subtract 4 from encoder index due to numbering on shift registers
    enc_idx -= 4;
    if (encMap[enc_idx].active)
    {
        int8_t newDelta = encScaling(&encMap[enc_idx], delta);
        switch (encMap[enc_idx].Parameter)
        {
        case kbdOct:
            if (newDelta == 0 || keyboardOct + newDelta < 0 || keyboardOct + newDelta > encMap[enc_idx].Range)
                break;
            groupvec[activeGroupIndex]->allNotesOff();
            keyboardOct += newDelta;
            storeKeyboardBasisToEEPROM(keyboardOct);
            setEncValue(kbdOct, encMap[enc_idx].Value, KEYBOARD_OCT_STR[keyboardOct]);
            break;
        case kbdScale:
            if (newDelta == 0 || keyboardScale + newDelta < 0 || keyboardScale + newDelta > encMap[enc_idx].Range)
                break;
            groupvec[activeGroupIndex]->allNotesOff();
            keyboardScale += newDelta;
            majMin = keyboardScale % 2;
            storeKeyboardScaleToEEPROM(keyboardScale);
            setEncValue(kbdScale, encMap[enc_idx].Value, SCALE_STR[keyboardScale]);
            break;
        case patchselect:
        case namepatch:
        case deletepatch:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incCurrentPatchIndex() : decCurrentPatchIndex();
            if (state == State::MAIN || state == State::DELETEPATCH)
            {
                selectPatchForRecall(false);
            }
            else if (state == State::PATCHSAVING)
            {
                currentPatchName = patches[currentPatchIndex].patchName;
            }
            else if (state == State::PERFORMANCEPATCHEDIT)
            {
                selectPatchForRecall(true);
            }
            break;
        case CCbankselectLSB:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            if (state == State::PATCHLIST || state == State::EDITBANK || state == State::DELETEPATCH)
            {
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            else if (state == State::PERFORMANCEPATCHEDIT)
            {
                currentBankIndex = tempBankIndex;
                currentPatchIndex = patches.size() - 1;
                selectPatchForRecall(false);
            }
            else
            {
                while (patches[0].patchUID == 0)
                {
                    loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
                }
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
                selectPatchForRecall(false);
            }
            break;
        case savebankselect:
        case editbank:
        case deletebank:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            currentBankIndex = tempBankIndex;
            if (patches[0].patchUID != 0)
                patches.push_back(PatchUIDAndName{0, EMPTYNAME});
            currentPatchIndex = patches.size() - 1;
            break;
        case bankeditselect:
        case renamebank:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            break;
        case choosecharacterPatch:
        case choosecharacterBank:
        case choosecharacterPerformance:
        case choosecharacterSequence:
            charCursor += newDelta;
            if (charCursor >= TOTALCHARS)
                charCursor = 0;
            if (charCursor <= -1)
                charCursor = TOTALCHARS - 1;
            break;
        case deleteCharacterBank:
            if (newDelta > 0 && ++nameCursor > bankNames[currentBankIndex].length() - 1)
            {
                nameCursor = bankNames[currentBankIndex].length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterPatch:
            if (newDelta > 0 && ++nameCursor > currentPatchName.length() - 1)
            {
                nameCursor = currentPatchName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterPerformance:
            if (newDelta > 0 && ++nameCursor > currentPerformance.performanceName.length() - 1)
            {
                nameCursor = currentPerformance.performanceName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterSequence:
            if (newDelta > 0 && ++nameCursor > currentSequence.SequenceName.length() - 1)
            {
                nameCursor = currentSequence.SequenceName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case PerfSelect:
        case PerfEdit:
        case PerfDelete:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incPerformanceIndex() : decPerformanceIndex();
            break;
        case SeqSelect:
        case SeqEdit:
        case SeqDelete:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incSequenceIndex() : decSequenceIndex();
            break;
        case chooseEncoderTL:
        case chooseEncoderTR:
        case chooseEncoderBL:
        case chooseEncoderBR:
            if (newDelta == 0 || encMap[enc_idx].Value < 0 || encMap[enc_idx].Value > encMap[enc_idx].Range)
                break;
            setEncValue(encMap[enc_idx].Parameter, encMap[enc_idx].Value, ParameterStrMap[ParametersForPerformanceEncoders[encMap[enc_idx].Value]]);
            currentPerformance.TL = ParametersForPerformanceEncoders[encMap[ENC_TL].Value];
            currentPerformance.TR = ParametersForPerformanceEncoders[encMap[ENC_TR].Value];
            currentPerformance.BL = ParametersForPerformanceEncoders[encMap[ENC_BL].Value];
            currentPerformance.BR = ParametersForPerformanceEncoders[encMap[ENC_BR].Value];
            break;
        case chooseMIDIChIn:
        case choosePerfMIDIChIn:
        {
            String sIn = F("All");
            if (encMap[enc_idx].Value > MIDI_CHANNEL_OMNI)
                sIn = String(encMap[enc_idx].Value);
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiCh = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIChIn, encMap[enc_idx].Value, sIn);
            }
            else if (!setEncValue(chooseMIDIChIn, encMap[enc_idx].Value, sIn))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIChIn], sIn);
            }
        }
        break;
        case chooseMIDIChOut:
        case choosePerfMIDIChOut:
        {
            String sOut = F("Off");
            if (encMap[enc_idx].Value > 0)
                sOut = String(encMap[enc_idx].Value);
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiChOut = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIChOut, encMap[enc_idx].Value, sOut);
            }
            else if (!setEncValue(chooseMIDIChOut, encMap[enc_idx].Value, sOut))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIChOut], sOut);
            }
        }
        break;
        case chooseMIDIThruMode:
        case choosePerfMIDIThruMode:
        {
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiThru = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIThruMode, encMap[enc_idx].Value, MIDIThruStr[encMap[enc_idx].Value]);
            }
            else if (!setEncValue(chooseMIDIThruMode, encMap[enc_idx].Value, MIDIThruStr[encMap[enc_idx].Value]))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIThruMode], MIDIThruStr[encMap[enc_idx].Value]);
            }
        }
        break;
        case settingoption:
            if (newDelta == 0)
                break;
            newDelta > 0 ? settings::increment_setting() : settings::decrement_setting();
            setEncValue(settingoption, value, settings::current_setting());
            setEncValue(settingvalue, (int)settings::index(), settings::current_setting_size() - 1, settings::current_setting_value());
            settings::save_current_value();
            break;
        case settingvalue:
            if (newDelta == 0)
                break;
            newDelta > 0 ? settings::increment_setting_value() : settings::decrement_setting_value();
            setEncValue(settingvalue, value, settings::current_setting_value());
            settings::save_current_value();
            break;
        case SeqTempo:
            if ((seqMidiSync && !syncToMIDIClk) || newDelta == 0 || currentSequence.bpm + (newDelta / 10.0f) < 20.0f || currentSequence.bpm + (newDelta / 10.0f) > 300.0)
                break;
            currentSequence.bpm += (newDelta / 10.0f);
            setSeqTimerPeriod(currentSequence.bpm);
            setEncValue(SeqTempo, currentSequence.bpm, String(currentSequence.bpm, 1));
            break;
        case SeqLength:
            if (newDelta == 0 || currentSequence.length + newDelta < 1 || currentSequence.length + newDelta > 64)
                break;
            currentSequence.length += newDelta;
            if (currentSeqPosition > currentSequence.length - 1)
            {
                currentSeqPosition = currentSequence.length - 1;
            }
            if (currentSequence.running && currentSequence.step > currentSequence.length - 1)
            {
                sequencerStart();
            }
            break;
        case SeqPosition:
            if (newDelta == 0 || currentSeqPosition + newDelta < 0 || currentSeqPosition + newDelta > currentSequence.length - 1)
                break;
            currentSeqPosition += newDelta;
            break;
        case SeqNote:
            if (newDelta == 0 || currentSeqNote + newDelta < 12 || currentSeqNote + newDelta > 119)
                break;
            currentSeqNote += newDelta;
            if (currentSeqNote < 108)
                seqCurrentOctPos = nearbyint((currentSeqNote + 4) / 12) - 2;
            break;
        case ArpCycle:
            if (newDelta == 0 || arpCycles + newDelta < ARP_INF || arpCycles + newDelta > ARP_HOLD)
                break;
            arpCycles += newDelta;
            if (arpCycles != ARP_HOLD)
            {
                resetNotes();
                arpHold = false;
            }
            else
                arpHold = true;
            storeArpCyclesToEEPROM(arpCycles);
            setEncValue(ArpCycle, arpCycles, ARP_CYCLES[arpCycles]);
            break;
        case ArpStyle:
            if (newDelta == 0 || arpStyle + newDelta < 0 || arpStyle + newDelta > 5)
                break;
            arpStyle += newDelta;
            resetArp();
            storeArpStyleToEEPROM(arpStyle);
            setEncValue(ArpStyle, arpStyle, ARP_STYLES[arpStyle]);
            break;
        case ArpDivision:
            if (newDelta == 0 || arpDivision + newDelta < 0 || arpDivision + newDelta > 9)
                break;
            arpDivision += newDelta;
            setSeqTimerPeriod(currentSequence.bpm);
            storeArpDivisionToEEPROM(arpDivision);
            setEncValue(ArpDivision, arpDivision, ARP_DIVISION_STR[arpDivision]);
            break;
        case ArpRange:
            if (newDelta == 0 || arpRange + newDelta < 0 || arpRange + newDelta > 6)
                break;
            arpRange += newDelta;
            storeArpRangeToEEPROM(arpRange);
            setEncValue(ArpRange, arpRange, ARP_RANGE_STR[arpRange]);
            break;
        case ArpBasis:
            if (newDelta == 0 || arpBasis + newDelta < 0 || arpBasis + newDelta > 4)
                break;
            arpBasis += newDelta;
            storeArpBasisToEEPROM(arpBasis);
            setEncValue(ArpBasis, arpBasis, ARP_BASIS_STR[arpBasis]);
            break;
        default:
            // if(DEBUG) Serial.printf("enc[%u]: v=%d, d=%d\n", enc_idx, encMap[enc_idx].Value, newDelta);
            myControlChange(midiChannel, encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            midiCCOut(encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            break;
        }
    }
}

FLASHMEM void encoderButtonCallback(unsigned enc_idx, int buttonState)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    // Subtract 4 from encoder index due to numbering on shift registers
    enc_idx -= 4;

    // Ignore encoder button held repeatedly
    if (buttonState == HELD_REPEAT)
        return;

    if (encMap[enc_idx].Push && encMap[enc_idx].active)
    {
        switch (encMap[enc_idx].Parameter)
        {
        case kbdOct:
        case kbdScale:
            keyboardActive = !keyboardActive;
            break;
        case patchselect:
        case CCbankselectLSB:
            if (cardStatus)
            {
                // If current patch is empty in this bank, go to previous bank with valid patch
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                if (state == State::PERFORMANCEPATCHEDIT)
                {
                    currentPerformance.patches[0].bankIndex = currentBankIndex;
                    currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
                    savePerformance();
                    state = State::PERFORMANCEENCEDIT;
                    singleLED(ledColour::GREEN, 1);
                    setEncodersState(state);
                    return;
                }
            }
            else
            {
                return;
            }
            ledsOff();
            break;
        case namepatch:
            if (patches[currentPatchIndex].patchUID == 0)
                currentPatchName = "";
            nameCursor = currentPatchName.length() - 1;
            break;
        case editbank:
            nameCursor = bankNames[tempBankIndex].length() - 1;
            break;
        case cancel:
            currentPatchIndex = previousPatchIndex;
            currentBankIndex = previousBankIndex;
            tempBankIndex = currentBankIndex;
            loadPatchNamesFromBank(tempBankIndex);
            if (state != State::PATCHSAVING &&
                state != State::PATCHLIST &&
                state != State::PERFORMANCERECALL &&
                state != State::SEQUENCERECALL)
            {
                loadBankNames(); // If in the middle of bank renaming
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            }
            ledsOff();
            break;
        case goback:
            if (state != SEQUENCEPAGE)
                ledsOff();
            break;
        case choosecharacterPatch:
            if (currentPatchName.length() < PATCHNAMEMAXLEN)
            {
                if (currentPatchName.length() > 0)
                {
                    currentPatchName = currentPatchName.substring(0, nameCursor + 1) +
                                       CHARACTERS[charCursor] + currentPatchName.substring(nameCursor + 1, currentPatchName.length());
                }
                else
                {
                    currentPatchName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterBank:
            if (bankNames[tempBankIndex].length() < BANKNAMEMAXLEN)
            {
                if (bankNames[tempBankIndex].length() > 0)
                {
                    bankNames[tempBankIndex] = bankNames[tempBankIndex].substring(0, nameCursor + 1) +
                                               CHARACTERS[charCursor] + bankNames[tempBankIndex].substring(nameCursor + 1, bankNames[tempBankIndex].length());
                }
                else
                {
                    bankNames[tempBankIndex] = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterPerformance:
            if (currentPerformance.performanceName.length() < PERFORMANCENAMEMAXLEN)
            {
                if (bankNames[tempBankIndex].length() > 0)
                {
                    currentPerformance.performanceName = currentPerformance.performanceName.substring(0, nameCursor + 1) +
                                                         CHARACTERS[charCursor] + currentPerformance.performanceName.substring(nameCursor + 1, currentPerformance.performanceName.length());
                }
                else
                {
                    currentPerformance.performanceName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterSequence:
            if (currentSequence.SequenceName.length() < SEQUENCENAMEMAXLEN)
            {
                if (currentSequence.SequenceName.length() > 0)
                {
                    currentSequence.SequenceName = currentSequence.SequenceName.substring(0, nameCursor + 1) +
                                                   CHARACTERS[charCursor] + currentSequence.SequenceName.substring(nameCursor + 1, currentSequence.SequenceName.length());
                }
                else
                {
                    currentSequence.SequenceName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case deleteCharacterPatch:
            if (buttonState == HELD)
            {
                currentPatchName = "";
                nameCursor = 0;
                break;
            }
            if (currentPatchName.length() == 0)
                break;
            currentPatchName = currentPatchName.substring(0, nameCursor) + currentPatchName.substring(nameCursor + 1, currentPatchName.length());
            nameCursor--;
            break;
        case deleteCharacterBank:
            if (buttonState == HELD)
            {
                bankNames[tempBankIndex] = "";
                nameCursor = 0;
                break;
            }
            if (bankNames[tempBankIndex].length() == 0)
                break;
            nameCursor--;
            bankNames[tempBankIndex] = String(bankNames[tempBankIndex]).substring(0, bankNames[tempBankIndex].length() - 1);
            break;
        case deleteCharacterSequence:
            if (buttonState == HELD)
            {
                currentSequence.SequenceName = "";
                nameCursor = 0;
                break;
            }
            if (currentSequence.SequenceName.length() == 0)
                break;
            currentSequence.SequenceName = currentSequence.SequenceName.substring(0, nameCursor) +
                                           currentSequence.SequenceName.substring(nameCursor + 1, currentSequence.SequenceName.length());
            nameCursor--;
            break;
        case deleteCharacterPerformance:
            if (buttonState == HELD)
            {
                currentPerformance.performanceName = "";
                nameCursor = 0;
                break;
            }
            if (currentPerformance.performanceName.length() == 0)
                break;
            currentPerformance.performanceName = currentPerformance.performanceName.substring(0, nameCursor) +
                                                 currentPerformance.performanceName.substring(nameCursor + 1, currentPerformance.performanceName.length());
            nameCursor--;
            break;
        case savepatch:
            state = State::SAVE;
            strncpy(currentPatch.PatchName, currentPatchName.c_str(), 64);
            savePatch(currentBankIndex, currentPatchIndex);
            updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case deletepatch:
            if (patches[0].patchUID != 0)
            {
                state = State::DELETEMSG;
                if (findPatchIndex(patches[currentPatchIndex].patchUID, currentPatchIndex) == -1)
                    deletePatch(currentBankIndex, currentPatchIndex);
                // Remove from array if patch is in current bank
                patches.remove(currentPatchIndex);
                recreatePatchIndexFile(currentBankIndex);
            }
            currentPatchIndex = 0;
            // First Patch is empty in this bank, go to previous bank with valid patch
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case savebank:
            state = State::MAIN;
            saveBankName(bankNames[tempBankIndex]);
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case deletebank:
            state = State::DELETEBANKMSG;
            deleteBank(tempBankIndex);
            currentPatchIndex = 0;
            // First Patch is empty in this bank, go to previous bank with valid patch
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case PerfSelect:
        case PerfEdit:
            recallPerformance(currentPerformanceIndex + 1);
            singleLED(ledColour::GREEN, 1); // Led 1 stays green
            break;
        case PerfDelete:
            deletePerformance(currentPerformanceIndex + 1);
            break;
        case SeqSelect:
        case SeqEdit:
            recallSequence(currentSequenceIndex + 1);
            singleLED(ledColour::GREEN, 3); // Led 3 stays green
            break;
        case SeqDelete:
            deleteSequence(currentSequenceIndex + 1);
            break;
        case SeqStartStop:
            if (currentSequence.running)
            {
                sequencerStop();
            }
            else
            {
                sequencerStart();
            }
            singleLED(ledColour::GREEN, 3); // Led 3 stays green
            break;
        case SeqTempo:
            currentSequence.bpm = 120.0f;
            setSeqTimerPeriod(currentSequence.bpm);
            break;
        case SeqLength:
            if (buttonState == HELD)
            {
                // Toggle record sequence
                currentSequence.recording = !currentSequence.recording;
                break;
            }
            switch (currentSequence.length)
            {
            case 64:
                currentSequence.length = 8;
                break;
            case 48:
                currentSequence.length = SEQ_PATTERN_LEN;
                break;
            case 32:
                currentSequence.length = 48;
                break;
            case 16:
                currentSequence.length = 32;
                break;
            case 8:
                currentSequence.length = 16;
                break;
            default:
                currentSequence.length = SEQ_PATTERN_LEN;
                break;
            }
            currentSeqPosition = 0;
            if (currentSequence.running && currentSequence.step > currentSequence.length - 1)
            {
                sequencerStart();
            }
            break;
        case SeqPosition:
            if (buttonState == HELD)
            {
                currentSeqPosition = 0;
                break;
            }
            if (currentSeqPosition < currentSequence.length - 1)
                currentSeqPosition++;
            break;
        case SeqNote:
            if (buttonState == HELD)
            {
                // Delete all notes
                uint8_t n = 0;
                while (n < currentSequence.length)
                {
                    currentSequence.Notes[n] = 0;
                    n++;
                }
                saveSequence();
                break;
            }
            if (currentSequence.Notes[currentSeqPosition] == 0 || currentSequence.Notes[currentSeqPosition] != currentSeqNote)
            {
                currentSequence.Notes[currentSeqPosition] = currentSeqNote;
                currentSequence.Velocities[currentSeqPosition] = 64; // Default velocity
                playNote(currentSeqNote);
            }
            else
            {
                currentSequence.Notes[currentSeqPosition] = 0;
                currentSequence.Velocities[currentSeqPosition] = 0;
            }
            saveSequence();
            break;
        case ArpCycle:
            arpHold = !arpHold;
            if (!arpHold)
            {
                resetNotes();
                arpCycles = ARP_INF;
            }
            else
                arpCycles = ARP_HOLD;
            storeArpCyclesToEEPROM(arpCycles);
            break;

        case ArpStyle:
            arpStyle = encMap[enc_idx].DefaultValue;
            resetArp();
            storeArpStyleToEEPROM(arpStyle);
            setEncValue(ArpStyle, arpStyle, ARP_STYLES[arpStyle]);
            break;
        case ArpDivision:
            arpDivision = encMap[enc_idx].DefaultValue;
            setSeqTimerPeriod(currentSequence.bpm);
            storeArpDivisionToEEPROM(arpDivision);
            setEncValue(ArpDivision, arpDivision, ARP_DIVISION_STR[arpDivision]);
            break;
        case ArpRange:
            arpRange = encMap[enc_idx].DefaultValue;
            storeArpRangeToEEPROM(arpRange);
            setEncValue(ArpRange, arpRange, ARP_RANGE_STR[arpRange]);
            break;
        case ArpBasis:
            arpBasis = encMap[enc_idx].DefaultValue;
            storeArpBasisToEEPROM(arpBasis);
            setEncValue(ArpBasis, arpBasis, ARP_BASIS_STR[arpBasis]);
            break;
        default:
            setDefaultValue(&encMap[enc_idx]);
            myControlChange(midiChannel, encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            midiCCOut(encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            break;
        }
        state = encMap[enc_idx].PushAction;
        setEncodersState(state);
    }
}

// Read and drop everything waiting in the MIDI inputs, so a backlog from a flood of MIDI
// doesn't keep playing after Panic. Bounded in time in case the sender keeps going.
#define MIDI_FLUSH_MAX_MS 50
FLASHMEM void flushMidiInput()
{
    midiDiscard = true;
    elapsedMillis t;
    bool more = true;
    while (more && t < MIDI_FLUSH_MAX_MS)
    {
        more = false;
        myusb.Task();
        if (usbHostMIDI.read()) // USB host
            more = true;
        if (usbMIDI.read()) // USB device
            more = true;
    }
    // 5 pin DIN: drop the unread bytes without parsing them, so they aren't sent to MIDI thru
    Serial1.clear();
    midiDiscard = false;
}

// Panic: drop any MIDI waiting to be read, then silence all voices at once
FLASHMEM void midiPanic()
{
    flushMidiInput();
    AudioNoInterrupts();
    groupvec[activeGroupIndex]->panic();
    AudioInterrupts();
    showCurrentParameterOverlay(F("Panic"), F("All notes off"));
}

FLASHMEM void buttonCallback(unsigned button_idx, int buttonStatus)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    /*
      Button1 SR1 D4  10
      Button2 SR2 D4  9
      Button3 SR3 D4  11
      Button4 SR1 D5  7
      Button5 SR2 D5  6
      Button6 SR3 D5  8
      Button7 SR1 D6  4
      Button8 SR2 D6  3
      ButtonUp SR1 D7 1
      ButtonDown SR2 D7 0
    */
    switch (button_idx)
    {
    case 22: // VOL_UP & VOL_DOWN Held
        keyboardActive = !keyboardActive;
        break;
    case 33: // Arp & Seq Held
        if (currentSequence.running)
        {
            sequencerStop();
            arpRunning = false;
            if (state == State::ARPPAGE1 || state == State::ARPPAGE2)
            {
                state = State::MAIN;
                ledsOff();
            }
            if (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE || state == State::SEQUENCERECALL)
                singleLED(ledColour::GREEN, 3);
        }
        else if (!currentSequence.running && (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE))
        {
            sequencerStart();
        }
        break;
    case VOL_UP:
        if (buttonStatus == HIGH || buttonStatus == HELD || buttonStatus == HELD_REPEAT)
        {
            myControlChange(midiChannel, CCvolume, clampToRange<int>(currentVolume, 1, 0, 127));
        }
        break;
    case VOL_DOWN:
        if (buttonStatus == HIGH || buttonStatus == HELD || buttonStatus == HELD_REPEAT)
        {
            myControlChange(midiChannel, CCvolume, clampToRange<int>(currentVolume, -1, 0, 127));
        }
        break;
        // Osc / Performance
    case BUTTON_1:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }

            switch (state)
            {
            case State::OSCPAGE1:
                state = State::OSCPAGE2;
                singleLED(ledColour::RED, 1);
                break;
            case State::OSCPAGE2:
                state = State::OSCPAGE3;
                singleLED(ledColour::RED, 1);
                break;
            case State::OSCPAGE3:
                state = State::OSCPAGE4;
                singleLED(ledColour::RED, 1);
                break;
            case State::OSCPAGE4:
                state = State::OSCPAGE1;
                singleLED(ledColour::RED, 1);
                break;
            case State::PERFORMANCEPAGE:
                state = State::PERFORMANCERECALL;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCERECALL:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            case State::PERFORMANCEPATCHEDIT:
                if (currentPerformance.patches[0].UID == 0) // Patch is missing
                {
                    recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                    currentPerformance.patches[0].bankIndex = currentBankIndex;
                    currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
                    currentPerformance.patches[0].patchName = currentPatchName;
                }
                savePerformance();
                state = State::PERFORMANCEENCEDIT;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCEENCEDIT:
                savePerformance();
                state = State::PERFORMANCEMIDIEDIT;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCEMIDIEDIT:
                savePerformance();
                if (currentPerformance.performanceName.equals(EMPTYNAME))
                    currentPerformance.performanceName = "";
                nameCursor = currentPerformance.performanceName.length() - 1;
                state = State::RENAMEPERFORMANCE;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::RENAMEPERFORMANCE:
            case State::CHOOSECHARPERFORMANCE:
            case State::DELETECHARPERFORMANCE:
                savePerformance();
                loadPerformanceNames();
                state = State::PERFORMANCERECALL;
                singleLED(ledColour::GREEN, 1);
                break;
            default:
                state = State::OSCPAGE1; // show osc parameters
                singleLED(ledColour::RED, 1);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (cardStatus && state != State::PERFORMANCERECALL && state != State::PERFORMANCEPAGE &&
                state != State::OSCPAGE1 && state != State::OSCPAGE2 &&
                state != State::OSCPAGE3 && state != State::OSCPAGE4)
            {
                state = State::PERFORMANCERECALL;
                // Get performances from SD card
                loadPerformanceNames();
                singleLED(ledColour::GREEN, 1);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
            }
        }
        break;

        // Osc Mod / Arp
    case BUTTON_2:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::OSCMODPAGE1:
                state = State::OSCMODPAGE2;
                singleLED(ledColour::RED, 2);
                break;
            case State::OSCMODPAGE2:
                state = State::OSCMODPAGE3;
                singleLED(ledColour::RED, 2);
                break;
            case State::OSCMODPAGE3:
                state = State::OSCMODPAGE4;
                singleLED(ledColour::RED, 2);
                break;
            case State::OSCMODPAGE4:
                state = State::OSCMODPAGE1;
                singleLED(ledColour::RED, 2);
                break;
            case State::ARPPAGE1:
                state = State::ARPPAGE2;
                singleLED(ledColour::GREEN, 2);
                break;
            case State::ARPPAGE2:
                state = State::ARPPAGE1;
                singleLED(ledColour::GREEN, 2);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::OSCMODPAGE1; // show osc mod parameters
                singleLED(ledColour::RED, 2);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::ARPPAGE1 && state != State::ARPPAGE2 &&
                state != State::OSCMODPAGE1 && state != State::OSCMODPAGE2 &&
                state != State::OSCMODPAGE3 && state != State::OSCMODPAGE4)
            {
                if (!arpRunning)
                {
                    if (currentSequence.running)
                        sequencerStop();
                    arpRunning = true;
                    setSeqTimerPeriod(currentSequence.bpm);
                    resetNotes();
                    sequencerStart();
                }
                state = State::ARPPAGE1;
                ledsOff(); // Led 1 stays on after sequencerStart()
                singleLED(ledColour::GREEN, 2);
            }
            else if (state == State::ARPPAGE1 || state == State::ARPPAGE2)
            {
                if (arpRunning)
                {
                    arpRunning = false;
                    sequencerStop();
                    state = State::MAIN;
                    singleLED(ledColour::OFF, 2);
                }
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 2);
            }
        }
        break;

        // Filter / Seq
    case BUTTON_3:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::FILTERPAGE1:
                state = State::FILTERPAGE2;
                singleLED(ledColour::RED, 3);
                break;
            case State::FILTERPAGE2:
                state = State::FILTERPAGE1;
                singleLED(ledColour::RED, 3);
                break;
            case State::SEQUENCEPAGE:
            case State::SEQUENCERECALL:
                state = State::MAIN;
                singleLED(ledColour::OFF, 3);
                break;
            case State::SEQUENCEEDIT:
                saveSequence();
                currentSequence.recording = false;
                if (currentSequence.SequenceName.equals(EMPTYNAME))
                    currentSequence.SequenceName = "";
                nameCursor = currentSequence.SequenceName.length() - 1;
                state = State::RENAMESEQUENCE;
                singleLED(ledColour::GREEN, 3);
                break;
            case State::RENAMESEQUENCE:
            case State::CHOOSECHARSEQUENCE:
            case State::DELETECHARSEQUENCE:
                saveSequence();
                loadSequenceNames();
                state = State::SEQUENCERECALL;
                singleLED(ledColour::GREEN, 3);
                break;
            default:
                state = State::FILTERPAGE1; // show filter parameters
                singleLED(ledColour::RED, 3);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (arpRunning)
            {
                sequencerStop();
                arpRunning = false;
            }

            if (cardStatus && state != State::SEQUENCERECALL && state != State::SEQUENCEEDIT && state != State::FILTERPAGE1 && state != State::FILTERPAGE2 && !currentSequence.running)
            {
                state = State::SEQUENCERECALL;
                loadSequenceNames();
                currentSequence.recording = true;
                singleLED(ledColour::GREEN, 3);
            }
            else if (currentSequence.running && state != State::SEQUENCEPAGE)
            {
                state = State::SEQUENCEPAGE;
                singleLED(ledColour::GREEN, 3);
            }
            else if (currentSequence.running && state == State::SEQUENCEPAGE)
            {
                state = State::SEQUENCEEDIT;
                singleLED(ledColour::GREEN, 3);
            }
            else if (state == State::SEQUENCEEDIT)
            {
                saveSequence();
                state = State::SEQUENCERECALL;
                singleLED(ledColour::GREEN, 3);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 3);
            }
        }
        break;

        // Filter Mod / MIDI
    case BUTTON_4:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::FILTERMODPAGE1:
                state = State::FILTERMODPAGE2;
                singleLED(ledColour::RED, 4);
                break;
            case State::FILTERMODPAGE2:
                state = State::FILTERMODPAGE3;
                singleLED(ledColour::RED, 4);
                break;
            case State::FILTERMODPAGE3:
                state = State::FILTERMODPAGE1;
                singleLED(ledColour::RED, 4);
                break;
            case State::MIDIPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 4);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::FILTERMODPAGE1; // show filter mod parameters
                singleLED(ledColour::RED, 4);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::MIDIPAGE && state != State::FILTERMODPAGE1 && state != State::FILTERMODPAGE2 && state != State::FILTERMODPAGE3)
            {
                state = State::MIDIPAGE;
                singleLED(ledColour::GREEN, 4);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 4);
            }
        }
        break;
        // Amp / Settings
    case BUTTON_5:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::AMPPAGE1:
                state = State::AMPPAGE2; // show amplifier parameters
                singleLED(ledColour::RED, 5);
                break;
            case State::AMPPAGE2:
                state = State::AMPPAGE3; // show amplifier parameters
                singleLED(ledColour::RED, 5);
                break;
            case State::AMPPAGE3:
                state = State::AMPPAGE1; // show amplifier parameters
                singleLED(ledColour::RED, 5);
                break;
            case State::SETTINGS:
                state = State::MAIN;
                singleLED(ledColour::OFF, 5);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::AMPPAGE1; // show amplifier parameters
                singleLED(ledColour::RED, 5);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::SETTINGS && state != State::AMPPAGE1 && state != State::AMPPAGE2 && state != State::AMPPAGE3)
            {
                switch (state)
                {
                case State::MAIN:
                    state = SETTINGS;
                    break;
                case SETTINGS:
                    state = State::MAIN;
                    break;
                default:
                    state = SETTINGS;
                    break;
                }
                singleLED(ledColour::GREEN, 5);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 5);
            }
        }
        break;
        // FX / Panic
    case BUTTON_6:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }

            switch (state)
            {
            case State::FXPAGE1:
                state = State::FXPAGE2; // show FX parameters
                break;
            case State::FXPAGE2:
                state = State::FXPAGE1; // show FX parameters
                break;
            default:
                state = State::FXPAGE1; // show FX parameters
                break;
            }

            singleLED(ledColour::RED, 6);
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::FXPAGE1 && state != State::FXPAGE2)
            {
                // If Back button held, Panic: drop any MIDI waiting to be read, then all notes off
                midiPanic();
                flashLED(GREEN, 6, 250);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 6);
            }
        }
        break;

        // Save / Del
    case BUTTON_7:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            // Cancel out of Delete
            if (state == State::DELETEPATCH)
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
            else if (state == State::PERFORMANCEPAGE)
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
            }
            else if (state == State::RENAMEBANK)
            {
                // Save bank name
                state = State::MAIN;
                saveBankName(bankNames[tempBankIndex]);
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                ledsOff();
            }
            // SAVE
            else if (cardStatus && state != State::PATCHSAVING && state != State::CHOOSECHARPATCH && state != State::DELETECHARPATCH)
            {
                singleLED(ledColour::RED, 7);
                loadBankNames(); // If in the middle of bank renaming and cancelling
                tempBankIndex = currentBankIndex;
                if (patches.size() < PATCHES_LIMIT)
                {
                    // Prompt for patch name
                    currentPatchName = currentPatch.PatchName;
                    currentPatchIndex++;
                    state = State::PATCHSAVING;
                    if (patches[0].patchUID != 0 && patches.back().patchUID != 0)
                        patches.push_back(PatchUIDAndName{0, EMPTYNAME});
                    currentPatchIndex = patches.size() - 1;
                    if (DEBUG)
                        Serial.printf(F("currentPatchIndex: %i\n"), currentPatchIndex);
                }
                else
                {
                    if (DEBUG)
                        Serial.println(F("Over patch limit on this bank"));
                }
            }
            else if (state == State::PATCHSAVING)
            {
                // Save without patch naming
                state = State::SAVE;
                savePatch(currentBankIndex, currentPatchIndex);
                updatePatchnameAndIndex(currentPatchName, currentPatchIndex + 1, patches[currentPatchIndex].patchUID);
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
            else if (state == State::CHOOSECHARPATCH || state == State::DELETECHARPATCH)
            {
                // Save after patch naming
                state = State::SAVE;
                strncpy(currentPatch.PatchName, currentPatchName.c_str(), 64);
                savePatch(currentBankIndex, currentPatchIndex);
                updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
                break;
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (cardStatus && state != State::PATCHSAVING && state != State::DELETEPATCH && state != State::SAVE && state != State::EDITBANK && state != State::RENAMEBANK)
            {
                // DELETE
                state = State::DELETEPATCH;
                loadBankNames(); // If in the middle of bank renaming and cancelling
                tempBankIndex = currentBankIndex;
                singleLED(ledColour::GREEN, 7);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
        }
        break;

        // Recall / Init
    case BUTTON_8:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            // RECALL
            if (!cardStatus)
            {
                state = State::MAIN;
                ledsOff();
                return;
            }
            singleLED(ledColour::RED, 8);
            switch (state)
            {
            case State::PATCHLIST:
                // If current patch is empty in this bank, go to previous bank with valid patch
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 8);
                break;
            default:
                currentPatchIndex = previousPatchIndex; // when coming from cancelled save on last Empty patch
                currentBankIndex = previousBankIndex;
                tempBankIndex = currentBankIndex;
                loadPatchNamesFromBank(tempBankIndex);
                state = State::PATCHLIST; // show patch list
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::PATCHLIST)
            {
                // Reinitialise to default patch
                State prevState = state;
                state = State::REINITIALISE;
                reinitialisePatch();
                setCurrentPatchData();
                flashLED(GREEN, 8, 250);
                state = prevState;
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 8);
            }
        }
        break;
    }
    if (state == State::MAIN)
    {
        // Reset MIDI if changed by performance
        midiChannel = getMIDIChannel();
        midiOutCh = getMIDIOutCh();
        MIDIThru = getMidiThru();
    }
    setEncodersState(state);
}

void midiCCOut(byte cc, byte value)
{
    if (midiOutCh > 0 && cc < 129) // Misusing CC for other parameters above 127/128
    {
        if (cc == filterfreq256) // This is CC 128
        {
            // Turn cutoff freq back to 7 bit for MIDI
            cc = CCfilterfreq;
            value = value >> 1;
        }
        usbMIDI.sendControlChange(cc, value, midiOutCh);
        usbHostMIDI.sendControlChange(cc, value, midiOutCh);
        if (MIDIThru == midi::Thru::Off)
            MIDI.sendControlChange(cc, value, midiOutCh); // MIDI DIN is set to Out
    }
}

FLASHMEM void sdCardDetect()
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (sdCardInterrupt)
    {
        silence();
        delayMicroseconds(100'000); // Switch bounce
        if (!digitalReadFast(pinCD))
        {
            cardStatus = SD.begin(BUILTIN_SDCARD); // Reinitialise when card inserted
            // ++++ For older sd cards, retry three times
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            //++++
            checkSDCardStructure(); // May be a new card inserted
            if (cardStatus)
            {
                loadBankNames();
                loadPatchNamesFromBank(currentBankIndex);
            }
        }
        else
        {
            cardStatus = false;
            state = State::MAIN;
            // arpRunning = false;
            // sequencerStop();
            setEncodersState(state);
            ledsOff();
        }
        sdCardInterrupt = false;
        attachInterrupt(digitalPinToInterrupt(pinCD), sdCardInteruptRoutine, CHANGE);
    }
}

// Ignores CC messages for four seconds after USB Host is plugged in
// Some MIDI controllers send data when connected that changes synth settings
FLASHMEM void usbHostControlChange(byte channel, byte control, byte value)
{
    if (usbHostPluggedIn && usb_host_wait_timer > 4000) // wait 4s
    {
        // When waiting time has expired, all CC messages go through to the normal handler
        myControlChange(channel, control, value);
    }
}

FLASHMEM void checkUSBHostStatus()
{
    if (usbHostMIDI && !usbHostPluggedIn)
    {
        usb_host_wait_timer = 0;
        usbHostPluggedIn = true;
    }
    else if (!usbHostMIDI && usbHostPluggedIn)
    {
        usbHostPluggedIn = false;
    }
}

FLASHMEM void sequencerLEDs()
{
    if (currentSequence.running && currentSequence.step != seq_last_step)
    {
        seq_last_step = currentSequence.step;
        if (currentSequence.step % 16 < 8)
        {
            if ((currentSequence.step % 8 + 1) == 3)
                ledsOff();
            seqLED(RED, currentSequence.step % 8 + 1);
        }
        else
        {
            seqLED(GREEN, currentSequence.step % 8 + 1);
        }
    }
}

FLASHMEM void CPUMonitor()
{
    // Once a second: printing every loop floods USB serial and can stall the main loop
    static elapsedMillis sincePrint;
    if (sincePrint < 1000)
        return;
    sincePrint = 0;
    // Memory is in audio blocks (an integer), out of the AudioMemory() allocation in setup().
    // Stack is the most the display thread has used, out of DISPLAY_THREAD_STACK.
    Serial.printf(" CPU:%.1f (%.1f)  MEM:%u (%u)  Display stack:%u/%u\n", AudioProcessorUsage(), AudioProcessorUsageMax(),
                  (unsigned)AudioMemoryUsage(), (unsigned)AudioMemoryUsageMax(), (unsigned)displayStackUsed(), (unsigned)DISPLAY_THREAD_STACK);
}

// Prints the details of the last crash (if any) once the Serial Monitor is connected
FLASHMEM void crashReportCheck()
{
    static bool checked = false;
    if (checked || !Serial)
        return;
    checked = true;
    if (CrashReport)
        Serial.print(CrashReport);
}

void loop()
{
    // Improves USB digital audio - Checking USB client MIDI frequently causes drop-outs...
    if (loopCount++ > 15)
    {
        checkUSBHostStatus();
        // Drain pending MIDI messages (bounded) so notes aren't queued behind clock/CC traffic.
        // read() returns false straight away when nothing is waiting.
        // USB Host MIDI Class Compliant
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && usbHostMIDI.read(midiChannel); i++)
            ;
        // USB Client MIDI
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && usbMIDI.read(midiChannel); i++)
            ;
        // MIDI 5 Pin DIN
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && MIDI.read(midiChannel); i++)
            ;
        sdCardDetect();
        sequencerLEDs();
        // MIDI clock lost without a Stop (cable pulled, DAW closed): stop following it,
        // otherwise the sequencer waits for it for ever
        if (MIDIClkSignal && sinceMidiClock > MIDI_CLOCK_TIMEOUT_MS)
        {
            if (seqMidiSync)
                midiClockStopped();
            else
                MIDIClkSignal = false;
        }
        crashReportCheck();
        // Uncomment for CPU, audio memory and display stack figures (printed once a second)
        // CPUMonitor();
        loopCount = 0;
    }
    encoders.read();
    buttons.read();
    // Start note-ons that were waiting for a stolen voice to fade out
    groupvec[activeGroupIndex]->serviceVoices();
    checkPendingPatchRecall();
}
