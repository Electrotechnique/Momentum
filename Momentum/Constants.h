#pragma once

#include <stdint.h>
#include "Arduino.h"

#define PWMWAVEFORM WAVEFORM_TRIANGLE
#define BANKS_LIMIT 8          // 8 banks
#define PATCHES_LIMIT 128      // 128 patches per bank
#define PERFORMANCES_LIMIT 128 // 128 performances
#define SEQUENCES_LIMIT 128    // 128 sequences

// Used in myControlChange for 8 bit control of filter cutoff freq from front panel
#define filterfreq256 128
#define patchselect 129
#define savepatch 130
#define renamepatch 131
#define deletepatch 132
#define ampenvshape 134
#define filterenvshape 135
#define glideshape 136
#define pitchbendrange 137
#define pitchmodwheeldepth 138
#define namepatch 139
#define savebankselect 140
#define MIDIChIn 141
#define MIDIChOut 142
#define MIDIThruMode 143
#define PerfSelect 144
#define PerfEdit 145
#define cancel 146
#define settingoption 147
#define settingvalue 148
#define editbank 149
#define deletepatches 150
#define deletebank 151
#define renamebank 152
#define bankeditselect 153
#define choosecharacterPatch 154
#define choosecharacterBank 155
#define deleteCharacterPatch 156
#define deleteCharacterBank 157
#define savebank 158
#define chooseEncoderTL 159
#define chooseEncoderTR 160
#define chooseEncoderBL 161
#define chooseEncoderBR 162
#define chooseMIDIThruMode 163
#define chooseMIDIChIn 164
#define chooseMIDIChOut 165
#define deleteCharacterPerformance 166
#define choosecharacterPerformance 167
#define filtermodwheeldepth 168
#define choosePerfMIDIThruMode 169
#define choosePerfMIDIChIn 170
#define choosePerfMIDIChOut 171
#define SeqSelect 172
#define SeqEdit 173
#define deleteCharacterSequence 174
#define choosecharacterSequence 175
#define SeqTempo 176
#define SeqStartStop 177
#define goback 178
#define SeqNote 179
#define SeqPitch 180
#define SeqPosition 181
#define SeqLength 182
#define ArpStyle 183
#define ArpDivision 184
#define ArpRange 185
#define ArpCycle 186
#define ArpBasis 187
#define SeqDelete 188
#define PerfDelete 189
#define kbdOct 190
#define kbdScale 191
#define firmwareupdate 192 // SD card firmware update: confirm
#define firmwarecancel 193 // SD card firmware update: cancel
#define noencoder 255

const static char *SEQUENCE_FOLDER_NAME = "Sequences";
const static char *SEQUENCE_FOLDER_NAME_SLASH = "Sequences/";

const static char *BANKS_FILE_NAME = "Banknames";
const static char *BANK_FOLDER_NAMES[] = {"Bank1", "Bank2", "Bank3", "Bank4", "Bank5", "Bank6", "Bank7", "Bank8"};
const static char *BANK_FOLDER_NAMES_SLASH[] = {"Bank1/", "Bank2/", "Bank3/", "Bank4/", "Bank5/", "Bank6/", "Bank7/", "Bank8/"};
// Defaults to store on blank SD card
const static char *BANKS_FILE_CONTENTS = "{\"Bank1\": \"Strings\",\"Bank2\": \"Basses\", \"Bank3\": \"Keys\",\"Bank4\": \"FX\",\"Bank5\": \"User 1\",\"Bank6\": \"User 2\",\"Bank7\": \"User 3\",\"Bank8\": \"User 4\"}";

// Friendly bank names from Banknames file - defaults
static String bankNames[8] = {"Strings", "Basses", "Keys", "FX", "User 1", "User 2", "User 3", "User 4"};

const static char *PATCH_INDEX_FILE_NAME = "Patchindex";
// const static char *PATCH_INDEX_FILE_CONTENTS = "{\"1\":0,\"2\":0,\"3\":0,\"4\":0,\"5\":0,\"6\":0,\"7\":0,\"8\":0,\"9\":0,\"10\":0,\"11\":0,\"12\":0,\"13\":0,\"14\":0,\"15\":0,\"16\":0,\"17\":0,\"18\":0,\"19\":0,\"20\":0,\"21\":0,\"22\":0,\"23\":0,\"24\":0,\"25\":0,\"26\":0,\"27\":0,\"28\":0,\"29\":0,\"30\":0,\"31\":0,\"32\":0,\"33\":0,\"34\":0,\"35\":0,\"36\":0,\"37\":0,\"38\":0,\"39\":0,\"40\":0,\"41\":0,\"42\":0,\"43\":0,\"44\":0,\"45\":0,\"46\":0,\"47\":0,\"48\":0,\"49\":0,\"50\":0,\"51\":0,\"52\":0,\"53\":0,\"54\":0,\"55\":0,\"56\":0,\"57\":0,\"58\":0,\"59\":0,\"60\":0,\"61\":0,\"62\":0,\"63\":0,\"64\":0,\"65\":0,\"66\":0,\"67\":0,\"68\":0,\"69\":0,\"70\":0,\"71\":0,\"72\":0,\"73\":0,\"74\":0,\"75\":0,\"76\":0,\"77\":0,\"78\":0,\"79\":0,\"80\":0,\"81\":0,\"82\":0,\"83\":0,\"84\":0,\"85\":0,\"86\":0,\"87\":0,\"88\":0,\"89\":0,\"90\":0,\"91\":0,\"92\":0,\"93\":0,\"94\":0,\"95\":0,\"96\":0,\"97\":0,\"98\":0,\"99\":0,\"100\":0,\"101\":0,\"102\":0,\"103\":0,\"104\":0,\"105\":0,\"106\":0,\"107\":0,\"108\":0,\"109\":0,\"110\":0,\"111\":0,\"112\":0,\"113\":0,\"114\":0,\"115\":0,\"116\":0,\"117\":0,\"118\":0,\"119\":0,\"120\":0,\"121\":0,\"122\":0,\"123\":0,\"124\":0,\"125\":0,\"126\":0,\"127\":0,\"128\":0}";

const static char *PERFORMANCE_FOLDER_NAME = "Performances";
const static char *PERFORMANCE_FOLDER_NAME_SLASH = "Performances/";

const static char *EnvShapeStr[] = {"Lin", "Exp -8", "Exp -7", "Exp -6", "Exp -5", "Exp -4", "Exp -3", "Exp -2", "Exp -1", "Exp 0", "Exp +1", "Exp +2", "Exp +3", "Exp +4", "Exp +5", "Exp +6", "Exp +7", "Exp +8"};
const static char *GlideShapeStr[] = {"Lin", "Exp"};
const static char *MonophonicStr[] = {"Off", "Last", "First", "Highest", "Lowest"};

const static char *MIDIThruStr[] = {"Off", "Full", "Same Ch.", "Diff. Ch."};

#define TOTALCHARS 69
const static char *CHARACTERS[TOTALCHARS] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", " ", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", " ", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "!", "?", "+", ":"};

extern const char *VERSION;
extern const char FIRMWARE_MARKER[];  // "MOMENTUM_FW_VERSION:" followed by the version
#define FIRMWARE_MARKER_PREFIX_LEN 20 // length of "MOMENTUM_FW_VERSION:"
// SD card firmware update: any .hex file in the root of the SD card. Renamed to .old if the
// update is cancelled, .bad if the file is faulty (keeping the rest of the name)
#define FW_FILE_EXT ".hex"
#define FW_FILE_CANCELLED_EXT ".old"
#define FW_FILE_BAD_EXT ".bad"
extern const float PROGMEM FILTERFREQS256[256];
extern const float POWER[128];
extern const float NOTEFREQS[128];
extern const uint32_t ENVTIMES[128];
extern const float LFOTEMPO[128];
extern const String LFOTEMPOSTR[128];
extern const uint32_t OSCMIXA[128];
extern const uint32_t OSCMIXB[128];
extern const int PITCH[128];
extern const float KEYTRACKINGFACTOR[128];
extern const float KEYTRACKINGAMT[128];
extern const float CENTSFRAC;
extern const float DIV8192;
extern const float DIV127;
extern const float DIV12;
extern const float DIV24;
extern const float DIV1270;
extern const float DIV12xDIV8192;
extern const float LINEAR[128];
extern const float LINEARCENTREZERO[128];
extern const int8_t ENVSHAPE[18];

extern const float BANDPASS;
extern const float LOWPASS;
extern const float HIGHPASS;
extern const float LINEAR_FILTERMIXER[128];
extern const int16_t PROGMEM PARABOLIC_WAVE[256];
extern const int16_t PROGMEM HARMONIC_WAVE[256];
extern const int16_t PROGMEM PPG_WAVE[256];
extern const float AWFREQ;
extern const float PWMRATE[128];
extern const float PITCHLFOOCTAVERANGE;
extern const uint32_t MINUNISONVOICES;
extern const float LFOMAXRATE;
#define PWMSOURCELFO 0
#define PWMSOURCEFENV 1
#define PWMSOURCEMANUAL 2
#define OSCFXOFF 0
#define OSCFXXOR 1
#define OSCFXXMOD 2
extern const float ONE;
extern const float WAVEFORMLEVEL;
extern const float VOICEMIXERLEVEL;
extern const float MAX_VOL;
extern const float UNISONVOICEMIXERLEVEL;
extern const float UNISONNOISEMIXERLEVEL;
extern const float OSCMODMIXERMAX;
extern const float FILTERMODMIXERMAX;
extern const float GLIDEFACTOR;
extern const uint32_t NO_OF_PARAMS;
extern const char *INITPATCHNAME;
extern const char *EMPTYNAME;
extern const char *MISSINGNAME;
extern const uint8_t PATCHNAMEMAXLEN;
extern const uint8_t BANKNAMEMAXLEN;
extern const uint8_t PERFORMANCENAMEMAXLEN;
extern const uint8_t SEQUENCENAMEMAXLEN;
extern const uint32_t HOLD_DURATION;
extern const uint32_t CLICK_DURATION;
extern const float ENSEMBLE_LFO[128];
extern const String NOTENAME[128];
extern const String SEQPOSSTR[128];

typedef enum State
{
  MAIN,                  // The main page for displaying the current patch and control (parameter) changes
  PATCHLIST,             // Patches list
  SAVE,                  // Save patch page
  REINITIALISE,          // Reinitialise message
  PATCHSAVING,           // Patch naming page
  DELETEPATCH,           // Delete patch page
  DELETEMSG,             // Delete patch message page
  SETTINGS,              // Settings page
  SETTINGSVALUE,         // Settings page
  OSCPAGE1,              // Osc parameters 1
  OSCPAGE2,              // Osc parameters 2
  OSCPAGE3,              // Osc parameters 3
  OSCPAGE4,              // Osc parameters 4
  OSCMODPAGE1,           // Osc Mod parameters 1
  OSCMODPAGE2,           // Osc Mod parameters 2
  OSCMODPAGE3,           // Osc Mod parameters 3
  OSCMODPAGE4,           // Osc Mod parameters 4
  FILTERPAGE1,           // Filter parameters 1
  FILTERPAGE2,           // Filter parameters 2
  FILTERMODPAGE1,        // Filter Mod parameters 1
  FILTERMODPAGE2,        // Filter Mod parameters 2
  FILTERMODPAGE3,        // Filter Mod parameters 3
  AMPPAGE1,              // Amplifier parameters 1
  AMPPAGE2,              // Amplifier parameters 2
  AMPPAGE3,              // Amplifier parameters 3
  FXPAGE1,               // FX parameters
  FXPAGE2,               // FX parameters
  MIDIPAGE,              // MIDI parameters
  ARPPAGE1,              // Arpeggiator parameters
  ARPPAGE2,              // Arpeggiator parameters
  SEQUENCEPAGE,          // Sequencer parameters
  SEQUENCEEDIT,          // Sequence parameters
  SEQUENCERECALL,        // Sequence recall
  RENAMESEQUENCE,        // Sequence naming
  CHOOSECHARSEQUENCE,    // Choose character
  DELETECHARSEQUENCE,    // Delete character
  PERFORMANCERECALL,     // Performance parameters
  PERFORMANCEPAGE,       // Performance
  RENAMEPERFORMANCE,     // Performance naming
  DELETECHARPERFORMANCE, // Delete character
  CHOOSECHARPERFORMANCE, // Choose character
  PERFORMANCEPATCHEDIT,  // Performance patch edit
  PERFORMANCEENCEDIT,    // Performance encoder edit
  PERFORMANCEMIDIEDIT,   // Performance Midi edit
  EDITBANK,              // Edit the bank names
  RENAMEBANK,            // Rename a bank
  RENAMEPATCH,           // Rename a patch
  CHOOSECHARPATCH,       // Choose character
  DELETECHARPATCH,       // Delete character
  CHOOSECHARBANK,        // Choose character
  DELETECHARBANK,        // Delete character
  DELETEBANKMSG,         // Delete bank message page
  FIRMWAREUPDATE,        // Update firmware page (reading the file, with progress)
  FIRMWAREDELETE,        // Update complete, firmware file deleted
  FIRMWAREUPDATE_ABORT,  // Update firmware abort page
  FIRMWARECONFIRM,       // Firmware file found on the SD card: update or cancel
  FIRMWAREINSTALL        // Copying the new firmware into place (no screen updates after this)
} State;