/*
This code is taken from:

    MicroDexed - https://codeberg.org/positionhigh/MicroDexed-touch
   and
    https://github.com/wbajzek/arduino-arpeggiator/blob/master/arpeggiator.ino

        *** Heavily modified by Electrotechnique 2023 ***

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6/4.x with audio shield.
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-android

   (c)2021-2022 H. Wirtz <wirtz@parasitstudio.de>, M. Koslowski <positionhigh@gmx.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Janelia Array available in libraries manager
#include <Array.h>
#include "Constants.h"
#include "Utils.h"
#include <ArduinoJson.h>
#include "Parameters.h"

#define SEQ_PATTERN_LEN 64
#define ARP_INF 0
#define ARP_HOLD 4
#define LATCHED_NOTE 130 // TODO not used currently

typedef enum ArpStyles
{
    UP,
    DOWN,
    BOUNCE,
    UPDOWN,
    RANDOMPLAY,
    PLAYORDER
} ArpStyles;

extern void myNoteOff(byte channel, byte note, byte velocity);
extern void noteOff(byte channel, byte note, byte velocity);
extern void noteOn(byte channel, byte note, byte velocity);

void noteOffRoutine();

uint8_t currentSeqPosition = 0; // 1.1.1
uint8_t currentSeqNote = 60;    // C3
uint8_t seqCurrentOctPos = 3;   // C3

boolean arpRunning = false;
bool seqSwapper = false;

bool sequencerTimerStarted = false;

boolean arpUp = true;
uint8_t playBeat = 0;
uint8_t previousArpNote = 0;
uint8_t arpStyle = UP;
boolean arpHold = false;
boolean arpStarted = false;
uint8_t arpCycles = 0;
uint8_t arpCycleCount = 0;
byte arpNotes[12] = {'\0'};
byte arpVels[12];
uint8_t arpNotesHeld = 0;
uint8_t arpDivision = 1;
uint8_t arpRange = 3;
uint8_t arpBasis = 2;
uint8_t playingArpRange = 3;
int8_t currentArpOct = 0;
int8_t arpPlayCount = 0;
boolean upDownExtraNote = false;

const static char *ARP_STYLES[6] = {"Up", "Down", "Bounce", "Up & Down", "Random", "Play Order"};
const static char *ARP_CYCLES[5] = {"Inf", "1", "2", "3", "Hold"};
const static char *ARP_DIVISION_STR[10] = {"1/32", "1/24", "1/16", "1/12", "1/8", "1/6", "1/4", "1/3", "1/2", "1"};
const static uint8_t ARP_DIVISION_24PPQ[10] = {3, 4, 6, 8, 12, 16, 24, 32, 48, 96};
const static char *ARP_RANGE_STR[7] = {"4 Oct Down", "3 Oct Down", "2 Oct Down", "Base Oct", "2 Oct Up", "3 Oct Up", "4 Oct Up"};
const static int8_t ARP_RANGE[7] = {-3, -2, -1, 0, 1, 2, 3};
const static char *ARP_BASIS_STR[5] = {"2 Below", "1 Below", "Base Oct", "1 Above", "2 Above"};
const static int8_t ARP_BASIS[5] = {-2, -1, 0, 1, 2};

FLASHMEM void resetArp()
{
    playBeat = 0;
    currentArpOct = 0;
    arpCycleCount = 0;
    playingArpRange = arpRange;
    upDownExtraNote = false;
    arpPlayCount = 0;
    arpStarted = false;
    if ((arpStyle == BOUNCE || arpStyle == UPDOWN) && ARP_RANGE[arpRange] < 0)
    {
        arpUp = false;
    }
    else
    {
        arpUp = true;
    }
}

// empties out the arpeggio. used when switching modes, when in hold mode and
// a new arpeggio is started.
FLASHMEM void resetNotes()
{
    // noteOffRoutine();
    // sequencerTimerStarted = false;
    // sequencer_timer.setPeriod(10000);
    resetArp();
    for (uint8_t i = 0; i < sizeof(arpNotes); i++)
        arpNotes[i] = '\0';
}

FLASHMEM uint8_t getNotesInArp()
{
    if (arpNotes[0] == '\0')
        return 0;
    uint8_t a = sizeof(arpNotes) - 1;
    while (arpNotes[a] == '\0')
    {
        a--;
    }
    return a + 1;
}

FLASHMEM void up()
{
    playBeat++;
    if (arpNotes[playBeat] == '\0')
    {
        playBeat = 0;
        if (ARP_RANGE[arpRange] > -1 && currentArpOct < ARP_RANGE[arpRange])
            currentArpOct++;
        else if (ARP_RANGE[arpRange] < 0 && currentArpOct > ARP_RANGE[arpRange])
            currentArpOct--;
        else
        {
            currentArpOct = 0;
        }
    }
}

FLASHMEM void down()
{
    if (playBeat == 0)
    {
        playBeat = sizeof(arpNotes) - 1;
        while (arpNotes[playBeat] == '\0')
        {
            playBeat--;
        }

        if (ARP_RANGE[arpRange] > -1 && currentArpOct < ARP_RANGE[arpRange])
            currentArpOct++;
        else if (ARP_RANGE[arpRange] < 0 && currentArpOct > ARP_RANGE[arpRange])
            currentArpOct--;
        else
        {
            currentArpOct = 0;
        }
    }
    else
        playBeat--;
}

FLASHMEM void upDown()
{
    if (ARP_RANGE[playingArpRange] == 0 && getNotesInArp() == 1)
    {
        // Special case - one note, base range
        playBeat = 0;
    }
    else if (ARP_RANGE[playingArpRange] > -1)
    {
        if (arpUp)
        {
            if (upDownExtraNote && currentArpOct == 0)
            {
                upDownExtraNote = false;
                return;
            }
            else
            {
                playBeat++;
            }
            if (arpNotes[playBeat] == '\0')
            {
                playBeat = 0;
                if (ARP_RANGE[playingArpRange] != 0)
                    currentArpOct++;
            }
            if (playBeat == getNotesInArp() - 1 && currentArpOct == ARP_RANGE[playingArpRange])
            {
                arpUp = false;
                upDownExtraNote = true;
            }
        }
        else
        {
            if (upDownExtraNote && currentArpOct == ARP_RANGE[playingArpRange])
            {
                upDownExtraNote = false;
                return;
            }

            if (playBeat == 0)
            {
                playBeat = sizeof(arpNotes) - 1;
                while (arpNotes[playBeat] == '\0')
                {
                    playBeat--;
                }
                if (ARP_RANGE[playingArpRange] != 0)
                    currentArpOct--;
            }
            else
            {
                playBeat--;
            }
            if (playBeat == 0 && currentArpOct == 0)
            {
                arpUp = true;
                upDownExtraNote = true;
            }
        }
    }
    else
    {
        if (arpUp)
        {
            if (upDownExtraNote && currentArpOct == ARP_RANGE[playingArpRange])
            {
                upDownExtraNote = false;
                return;
            }
            else
            {
                playBeat++;
            }
            if (arpNotes[playBeat] == '\0')
            {
                playBeat = 0;
                currentArpOct++;
            }
            if (playBeat == getNotesInArp() - 1 && currentArpOct == 0)
            {
                arpUp = false;
                upDownExtraNote = true;
            }
        }
        else
        {
            if (upDownExtraNote && currentArpOct == 0)
            {
                upDownExtraNote = false;
                return;
            }
            if (playBeat == 0)
            {
                playBeat = sizeof(arpNotes) - 1;
                while (arpNotes[playBeat] == '\0')
                {
                    playBeat--;
                }
                currentArpOct--;
            }
            else
                playBeat--;
            if (playBeat == 0 && currentArpOct == ARP_RANGE[playingArpRange])
            {
                arpUp = true;
                upDownExtraNote = true;
            }
        }
    }
}

FLASHMEM void bounce()
{
    if (ARP_RANGE[playingArpRange] == 0 && getNotesInArp() == 1)
    {
        // Special case - one note, base range
        playBeat = 0;
    }
    else if (ARP_RANGE[playingArpRange] > -1)
    {
        if (arpUp)
        {
            playBeat++;
            if (arpNotes[playBeat] == '\0')
            {
                playBeat = 0;
                if (ARP_RANGE[playingArpRange] != 0)
                    currentArpOct++;
            }
            if (playBeat == getNotesInArp() - 1 && currentArpOct == ARP_RANGE[playingArpRange])
                arpUp = false;
        }
        else
        {
            if (playBeat == 0)
            {
                playBeat = sizeof(arpNotes) - 1;
                while (arpNotes[playBeat] == '\0')
                {
                    playBeat--;
                }
                if (ARP_RANGE[playingArpRange] != 0)
                    currentArpOct--;
            }
            else
                playBeat--;
            if (playBeat == 0 && currentArpOct == 0)
                arpUp = true;
        }
    }
    else
    {
        if (arpUp)
        {
            playBeat++;
            if (arpNotes[playBeat] == '\0')
            {
                playBeat = 0;
                currentArpOct++;
            }
            if (playBeat == getNotesInArp() - 1 && currentArpOct == 0)
                arpUp = false;
        }
        else
        {
            if (playBeat == 0)
            {
                playBeat = sizeof(arpNotes) - 1;
                while (arpNotes[playBeat] == '\0')
                {
                    playBeat--;
                }
                currentArpOct--;
            }
            else
                playBeat--;
            if (playBeat == 0 && currentArpOct == ARP_RANGE[playingArpRange])
                arpUp = true;
        }
    }
}

FLASHMEM void randomPlay()
{
    if (arpNotes[1] == '\0')
        playBeat = 0;
    else
        playBeat = random(getNotesInArp());

    if (ARP_RANGE[arpRange] > -1)
        currentArpOct = random(ARP_RANGE[arpRange] + 1);
    else
        currentArpOct = -1 * random(-1 * (ARP_RANGE[arpRange] - 1));
}

typedef struct SequenceStruct
{
    uint32_t UID = 0;
    String SequenceName = EMPTYNAME;
    float bpm = 120.0f;
    uint8_t length = SEQ_PATTERN_LEN;
    uint8_t Notes[SEQ_PATTERN_LEN] = {0};
    uint8_t Velocities[SEQ_PATTERN_LEN] = {0};
    boolean noteoffsent = false;
    uint8_t step = 0;
    boolean running = false;
    boolean recording = false;

    uint8_t note_in;
    uint8_t note_in_velocity;
    int8_t transpose;

    uint32_t tempo_us = 125'000; // 60s/bpm

    uint8_t prev_note; // note_offs for every instrument track
    uint8_t prev_vel;
} SequenceStruct;

uint8_t currentSequenceIndex = 0;

boolean seqMidiSync = false;

SequenceStruct currentSequence;

String sequences[SEQUENCES_LIMIT];

FLASHMEM void concatSequenceFolderAndFilename(uint8_t filename, char *result)
{
    char buf[20];
    strcpy(result, SEQUENCE_FOLDER_NAME_SLASH); // copy string one into the result.
    strcat(result, utoa(filename, buf, 10));    // append string two to the result
}

// Filename is Index
FLASHMEM boolean loadSequence(uint8_t filename)
{
    if (!cardStatus)
        return false;
    // Open file for reading - UID is Filename
    char result[30];
    concatSequenceFolderAndFilename(filename, result);
    File file = SD.open(result);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<3000> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        if (DEBUG)
            Serial.print(F("loadSequence() - Failed to read file:"));
        if (DEBUG)
            Serial.println(filename);
        return false;
    }
    // Copy values from the JsonDocument to the SequenceStruct
    currentSequence.SequenceName = doc["Name"].as<String>();
    currentSequence.UID = doc["UID"];
    currentSequence.bpm = doc["bpm"];
    currentSequence.length = doc["Length"];

    JsonArray Notes = doc["Notes"];
    JsonArray Vel = doc["Vel"];

    for (uint8_t j = 0; j < currentSequence.length; j++)
    {
        currentSequence.Notes[j] = Notes[j];
        currentSequence.Velocities[j] = Vel[j];
    }
    file.close();
    currentSeqPosition = 0;
    return true;
}

FLASHMEM void saveSequence()
{
    if (!cardStatus)
        return;
    //Delete existing sequence
    char resultdel[30];
    concatSequenceFolderAndFilename(currentSequenceIndex + 1, resultdel);
    SD.remove(resultdel);

    StaticJsonDocument<3000> doc;
    // Need to generate a new UID as the sequence settings may have changed if overwriting an existing sequence
    String output;
    if (DEBUG)
        Serial.println("Saving seq:" + currentSequence.SequenceName);
    doc["Name"] = currentSequence.SequenceName;
    doc["Length"] = currentSequence.length;
    doc["bpm"] = currentSequence.bpm;
    JsonArray Notes = doc.createNestedArray("Notes");
    JsonArray Vel = doc.createNestedArray("Vel");

    for (uint8_t j = 0; j < currentSequence.length; j++)
    {
        Notes.add(currentSequence.Notes[j]);
        Vel.add(currentSequence.Velocities[j]);
    }

    serializeJson(doc, output);   // Generate complete JSON to save
    doc["UID"] = getHash(output); // Insert UID
    char result[30];
    concatSequenceFolderAndFilename(currentSequenceIndex + 1, result);
    File file = SD.open(result, FILE_WRITE_BEGIN);
    if (!file)
    {
        if (DEBUG)
            Serial.println(F("Failed to create file"));
        return;
    }
    if (serializeJson(doc, file) == 0)
    {
        if (DEBUG)
            Serial.println(F("Failed to write to file"));
        return;
    }
}

FLASHMEM char *getSequenceName(File file)
{
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<3000> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        if (DEBUG)
            Serial.println(F("getSequenceName() - Failed to read file"));
        return "Failed to read file";
    }
    // Copy values from the JsonDocument to the SequenceStruct
    return doc["Name"];
}

FLASHMEM void loadSequenceNames()
{
    if (!cardStatus)
        return;
    File sequenceDir = SD.open(SEQUENCE_FOLDER_NAME_SLASH);
    File sequenceFile;
    for (uint8_t i = 0; i < SEQUENCES_LIMIT; i++)
    {
        sequences[i] = {EMPTYNAME};
    }

    while (sequenceFile = sequenceDir.openNextFile())
    {
        if (!sequenceFile)
            break;
        uint8_t number = strtoul(sequenceFile.name(), NULL, 0);
        if (number < 1 || number > 128)
        {
            if (DEBUG)
                Serial.print(F("Sequence number is outside 1-128:"));
            if (DEBUG)
                Serial.println(number);
        }
        else
        {
            sequences[number - 1] = getSequenceName(sequenceFile);
        }
    }
    sequenceFile.close();
}

FLASHMEM void deleteSequence(uint8_t filename)
{
    if (!cardStatus)
        return;
    char result[30];
    concatSequenceFolderAndFilename(filename, result);
    if (!SD.remove(result))
    {
        // Sequence missing
        if (DEBUG)
            Serial.print(F("Sequence Missing:"));
        if (DEBUG)
            Serial.println(filename);
    }
    loadSequenceNames();
}

FLASHMEM uint8_t incSequenceIndex()
{
    if (currentSequenceIndex < SEQUENCES_LIMIT - 1)
    {
        return ++currentSequenceIndex;
    }
    else
    {
        // Go back to Start from front of array
        currentSequenceIndex = 0;
        return currentSequenceIndex;
    }
}

FLASHMEM uint8_t decSequenceIndex()
{
    if (currentSequenceIndex > 0)
    {
        return --currentSequenceIndex;
    }
    else
    {
        // Go to back of array
        currentSequenceIndex = SEQUENCES_LIMIT - 1;
        return currentSequenceIndex;
    }
}

FLASHMEM void seq_live_recording()
{
    // record to sequencer if recording is active
    if (currentSequence.note_in > 0 && currentSequence.recording)
    {
        currentSequence.Notes[currentSequence.step] = currentSequence.note_in;
        currentSequence.Velocities[currentSequence.step] = currentSequence.note_in_velocity;

        currentSequence.note_in = 0;
        currentSequence.note_in_velocity = 0;
    }
}

// Sequencer/arpeggiator step code runs from the timer ISR, so keep it in RAM for steady timing
void noteOnRoutine()
{
    if (arpRunning)
    {
        if ((arpHold && getNotesInArp() > 0) || arpNotesHeld > 0)
        {
            // if(DEBUG) Serial.printf("arpCycleCount:%d  arpPlayCount:%d Target:%d\n", arpCycleCount, arpPlayCount, 2 * (abs(ARP_RANGE[playingArpRange]) + 1 * getNotesInArp()));
            if (arpCycles != ARP_HOLD && arpCycles != ARP_INF && arpCycleCount >= arpCycles)
            {
                return;
            }
            if (arpNotes[playBeat] != '\0')
            {
                if (!arpStarted)
                {
                    arpStarted = true;

                    if (((arpStyle == BOUNCE || arpStyle == UPDOWN) && ARP_RANGE[playingArpRange] < 0) ||
                        arpStyle == DOWN)
                        playBeat = getNotesInArp() - 1;
                }

                arpPlayCount++;

                // Calling here to ensure first note played is random
                if (arpStyle == RANDOMPLAY)
                    randomPlay();
                noteOn(midiChannel, arpNotes[playBeat] + (currentArpOct * 12) + (ARP_BASIS[arpBasis] * 12), arpVels[playBeat]);
                previousArpNote = arpNotes[playBeat] + (currentArpOct * 12) + (ARP_BASIS[arpBasis] * 12);
            }
            // if(DEBUG) Serial.printf("%d:%d %d  held:%d\n", currentArpOct, playBeat, arpUp, arpNotesHeld);
            //     decide what the next note is based on the mode.
            switch (arpStyle)
            {
            case UP:
            case PLAYORDER:
                up();
                if ((abs(ARP_RANGE[arpRange]) + 1) * getNotesInArp() == arpPlayCount)
                {
                    arpCycleCount++;
                    arpPlayCount = 0;
                }
                break;
            case DOWN:
                down();
                if ((abs(ARP_RANGE[arpRange]) + 1) * getNotesInArp() == arpPlayCount)
                {
                    arpCycleCount++;
                    arpPlayCount = 0;
                }
                break;
            case BOUNCE:
                // Avoid problems when changing range
                if (playingArpRange != arpRange && ((arpUp && currentArpOct == 0 && playBeat == 0) || (!arpUp && currentArpOct == 0 && playBeat == getNotesInArp() - 1)))
                    playingArpRange = arpRange;
                // base,2up,3up: [1note]1,3,5  [2notes]3,7,11 [3notes]5,11,17
                if ((2 * (abs(ARP_RANGE[playingArpRange]) + 1) * getNotesInArp() - 1) == arpPlayCount)
                {
                    arpCycleCount++;
                    arpPlayCount = 0;
                }
                bounce();
                break;
            case UPDOWN:
                // Avoid problems when changing range
                if (playingArpRange != arpRange && ((arpUp && currentArpOct == 0 && playBeat == 0) || (!arpUp && currentArpOct == 0 && playBeat == getNotesInArp() - 1)))
                    playingArpRange = arpRange;
                // base,2up,3up: [1note]1,4,6  [2notes]4,8,12 [3notes]6,12,18
                if ((getNotesInArp() == 1 && ARP_RANGE[playingArpRange] == 0) || (2 * (abs(ARP_RANGE[playingArpRange]) + 1) * getNotesInArp() == arpPlayCount))
                {
                    arpCycleCount++;
                    arpPlayCount = 0;
                }
                upDown();
                break;
            case RANDOMPLAY:
                if (arpPlayCount == getNotesInArp())
                {
                    arpCycleCount++;
                    arpPlayCount = 0;
                }
                break;
            }
        }
    }
    else
    {
        if (currentSequence.note_in > 0 && currentSequence.note_in < 62 && currentSequence.recording == false)
        {
            myNoteOff(midiChannel, currentSequence.Notes[currentSequence.step] + currentSequence.transpose, 0);
            myNoteOff(midiChannel, currentSequence.Notes[currentSequence.step - 1] + currentSequence.transpose, 0);
            if (currentSequence.note_in > 65)
                currentSequence.note_in = currentSequence.note_in - 12;
            currentSequence.transpose = currentSequence.note_in % 12;
            currentSequence.transpose = currentSequence.transpose - 12;
            currentSequence.note_in = 0;
        }

        seq_live_recording();

        if (currentSequence.Notes[currentSequence.step] > 0)
        {
            if (currentSequence.running)
            {
                // TODO - NOT USED - latched note
                if (currentSequence.Notes[currentSequence.step] != LATCHED_NOTE)
                {
                    noteOn(midiChannel, currentSequence.Notes[currentSequence.step], currentSequence.Velocities[currentSequence.step]);
                    currentSequence.prev_note = currentSequence.Notes[currentSequence.step];
                    currentSequence.prev_vel = currentSequence.Velocities[currentSequence.step];
                }
            }
        }

        currentSequence.noteoffsent = false;

        if (++currentSequence.step == currentSequence.length)
            currentSequence.step = 0;
    }
    seqSwapper = false;
}

void noteOffRoutine()
{
    if (arpRunning)
    {
        // Turn off previous note
        noteOff(midiChannel, previousArpNote, 0);
    }
    else
    {
        seq_live_recording();

        if (currentSequence.noteoffsent == false)
        {
            if (currentSequence.prev_note > 0)
            {
                // TODO - NOT USED -  Note not held
                if (currentSequence.Notes[currentSequence.step] != LATCHED_NOTE)
                {
                    myNoteOff(midiChannel, currentSequence.prev_note, 0);
                    currentSequence.noteoffsent = true;
                }
            }
        }
    }
    seqSwapper = true;
}

// Runs in Interrupt Timer. Switches between the Noteon and Noteoff Task, each cycle
void sequencer()
{
    // if (currentSequence.running && state == SEQUENCEEDIT)
    //     currentSequence.recording = true;
    // else
    //     currentSequence.recording = false;
    if (seqSwapper)
        noteOnRoutine();
    else
        noteOffRoutine();
}