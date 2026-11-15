// MIDI CC control numbers
// These broadly follow standard CC assignments
#define CCbankselectMSB 0
#define CCmodwheel 1  // pitch LFO amount and filter mod amount
#define CCosclfoamt 3 // pitch LFO amount - panel control
#define CCglide 5
#define CCvolume 7
#define CCoscwaveformA 14
#define CCoscwaveformB 15
#define CCfilterenv 16
#define CCfiltermixer 19
#define CCoscLevelA 20
#define CCoscLevelB 21
#define CCnoiseLevel 23
#define CCoscfx 24
#define CCpitchA 26
#define CCpitchB 27
#define CCpitchenv 28
#define CCosclforetrig 30    // Off/On
#define CCfilterlforetrig 31 // Off/On
#define CCbankselectLSB 32
#define CCfilterres 71
#define CCamprelease 72
#define CCampattack 73
#define CCfilterfreq 74
#define CCampdecay 75
#define CCfilterlforate 76
#define CCfilterlfoamt 77
#define CCampsustain 79
#define CCfilterattack 80
#define CCfilterdecay 81
#define CCfiltersustain 82
#define CCfilterrelease 83
#define CCpwA 85
#define CCpwB 86
#define CCpwmRateA 87
#define CCpwmAmtA 88
#define CCkeytracking 89
#define CCfilterlfowaveform 90
#define CCensemblefxamt 91
#define CCensemblefxmix 93
#define CCdetune 94
#define CCpwmAmtB 95
#define CCpwmRateB 96
#define CCreverbfxtime 97
#define CCreverbfxmix 98
#define CCoscLfoRate 102
#define CCoscLfoWaveform 103
#define CCfilterLFOMidiClkSync 104 // Off/On
#define CCoscLFOMidiClkSync 105    // Off/On
#define CCpwmSourceA 106           // Osc1 LFO/Filter Env
#define CCunison 107
#define CCpwmSourceB 108         // Osc2 LFO/Filter Env
#define CCvelocitySens 109       // Velocity sensitivity
#define CCFilterVelocitySens 110 // Filter Velocity sensitivity
#define CCensemblefxdepth 111    // Ensemble modulation depth
#define CCensemblefxdelay 112    // Ensemble delay time
#define CCreverbfxdamping 113    // Reverb tail damping (treble loss)
#define CCreverbfxbasscut 114    // Reverb tail bass cut
#define CCallnotesoff 123        // Panic button includes sound off
#define CCmonomode 126

/*
    0 Bank Select (MSB)
    1 Modulation Wheel
    2 Breath controller
    3 = Undefined
    4 Foot Pedal (MSB)
    5 Portamento Time (MSB)
    6 Data Entry (MSB)
    7 Volume (MSB)
    8 Balance (MSB
    9 = Undefined
    10 Pan position (MSB)
    11 Expression (MSB)
    12 Effect Control 1 (MSB)
    13 Effect Control 2 (MSB)
    14 = Undefined
    15 = Undefined
    16-19 = General Purpose
    20-31 = Undefined
    32-63 = Controller 0-31
    64 Hold Pedal (on/off)
    65 Portamento (on/off)
    66 Sostenuto Pedal (on/off)
    67 Soft Pedal (on/off)
    68 Legato Pedal (on/off)
    69 Hold 2 Pedal (on/off)
    70 Sound Variation
    71 Resonance (Timbre)
    72 Sound Release Time
    73 Sound Attack Time
    74 Frequency Cutoff (Brightness)
    75 Sound Control 6
    76 Sound Control 7
    77 Sound Control 8
    78 Sound Control 9
    79 Sound Control 10
    80 Decay or General Purpose Button 1 (on/off) Roland Tone level 1
    81 Hi Pass Filter Frequency or General Purpose Button 2 (on/off) Roland Tone level 2
    82 General Purpose Button 3 (on/off) Roland Tone level 3
    83 General Purpose Button 4 (on/off) Roland Tone level 4
    84 Portamento Amount
    85-90 = Undefined
    91 Reverb Level
    92 Tremolo Level
    93 Chorus Level
    94 Detune Level
    95 Phaser Level
    96 Data Button increment
    97 Data Button decrement
    98 Non-registered Parameter (LSB)
    99 Non-registered Parameter (MSB)
    100 Registered Parameter (LSB)
    101 Registered Parameter (MSB)
    102-119 = Undefined
    120 All Sound Off
    121 All Controllers Off
    122 Local Keyboard (on/off)
    123 All Notes Off
    124 Omni Mode Off
    125 Omni Mode On
    126 Mono Operation
    127 Poly Mode

*/