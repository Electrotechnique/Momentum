// Janelia Array available in libraries manager
#include <Array.h>
#include "Constants.h"
#include "Utils.h"
#include <ArduinoJson.h>

uint8_t currentPerformanceIndex = 0;

typedef struct PatchesInPerfStruct
{
    uint8_t bankIndex = 0;
    uint32_t UID = 0;
    String patchName = "";
    uint8_t midiCh = 0;
    uint8_t midiChOut = 0;
    midi::Thru::Mode midiThru = midi::Thru::Off;
    uint8_t min = 0;
    uint8_t max = 127;
} PatchesInPerfStruct;

typedef enum PerformanceMode
{
    Single,
    Layer, // NOT USED - Requires multitimbral functionality to be finished
    Split  // NOT USED - Requires multitimbral functionality to be finished
} PerformanceMode;

typedef struct PerformanceStruct
{
    String performanceName = EMPTYNAME;
    PerformanceMode mode = PerformanceMode::Single;
    Array<PatchesInPerfStruct, 2> patches;
    uint8_t TL = noencoder;
    uint8_t TR = noencoder;
    uint8_t BL = noencoder;
    uint8_t BR = noencoder;
} PerformanceStruct;

PerformanceStruct currentPerformance;

String performances[PERFORMANCES_LIMIT];

FLASHMEM void concatPerformanceFolderAndFilename(uint32_t filename, char *result)
{
    char buf[20];
    strcpy(result, PERFORMANCE_FOLDER_NAME_SLASH); // copy string one into the result.
    strcat(result, utoa(filename, buf, 10));       // append string two to the result
}

// Performancename from a files and return it
// Returns a copy: the JsonDocument is on this function's stack (see getPatchName)
FLASHMEM String getPerformanceName(File file)
{
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<2048> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        if (DEBUG)
            Serial.println(F("getPerformanceName() - Failed to read file"));
        return F("Failed to read file");
    }
    return String(doc["PerformanceName"] | "");
}

FLASHMEM void loadPerformanceNames()
{
    if (!cardStatus)
        return;
    File performanceDir = SD.open(PERFORMANCE_FOLDER_NAME_SLASH);
    File performanceFile;
    for (uint8_t i = 0; i < PERFORMANCES_LIMIT; i++)
    {
        performances[i] = {EMPTYNAME};
    }

    while (performanceFile = performanceDir.openNextFile())
    {
        if (!performanceFile)
            break;
        uint8_t number = strtoul(performanceFile.name(), NULL, 0);
        if (number < 1 || number > 128)
        {
            if (DEBUG)
                Serial.print(F("Performance number is outside 1-128:"));
            if (DEBUG)
                Serial.println(number);
        }
        else
        {
            performances[number - 1] = getPerformanceName(performanceFile);
        }
    }
    performanceFile.close();
}

// Filename is index +1 (1-128)
FLASHMEM boolean loadPerformance(uint8_t filename)
{
    // Initial empty
    PerformanceStruct ps;
    currentPerformance = ps;
    if (!cardStatus)
        return false;

    // Open file for reading - Filename is index +1 (1-128)
    char result[30];
    concatPerformanceFolderAndFilename(filename, result);
    File file = SD.open(result);

    if (!file)
    {
        if (DEBUG)
            Serial.println(F("Performance file not found:") + String(filename));
        file.close();
        return false;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        if (DEBUG)
            Serial.print(F("loadPerformance() - Failed to read file:"));
        if (DEBUG)
            Serial.println(filename);
        return false;
    }
    // Copy values from the JsonDocument to the PerformanceStruct
    currentPerformance.performanceName = doc["PerformanceName"].as<String>();
    currentPerformance.mode = doc["Mode"];
    JsonObject Encoders = doc["Encoders"];
    currentPerformance.TL = Encoders["TL"];
    currentPerformance.TR = Encoders["TR"];
    currentPerformance.BL = Encoders["BL"];
    currentPerformance.BR = Encoders["BR"];

    uint8_t i = 0;
    for (JsonObject P : doc["Patches"].as<JsonArray>())
    {
        currentPerformance.patches[i].bankIndex = P["Bank"];
        currentPerformance.patches[i].UID = P["UID"];
        currentPerformance.patches[i].patchName = getPatchName(currentPerformance.patches[i].bankIndex, currentPerformance.patches[i].UID);
        currentPerformance.patches[i].midiCh = P["MidiChIn"];
        currentPerformance.patches[i].midiChOut = P["MidiChOut"];
        currentPerformance.patches[i].midiThru = P["MidiThru"];
        currentPerformance.patches[i].min = P["Min"];
        currentPerformance.patches[i].max = P["Max"];
        i++;
    }
    file.close();
    return true;
}

FLASHMEM void savePerformance()
{
    if (!cardStatus)
        return;
    // Delete existing performance
    char resultdel[30];
    concatPerformanceFolderAndFilename(currentPerformanceIndex + 1, resultdel);
    SD.remove(resultdel);

    StaticJsonDocument<1024> doc;
    doc["PerformanceName"] = currentPerformance.performanceName;
    doc["Mode"] = currentPerformance.mode;

    JsonArray Patches = doc.createNestedArray("Patches");

    JsonObject Patches_0 = Patches.createNestedObject();
    Patches_0["Bank"] = currentPerformance.patches[0].bankIndex;
    Patches_0["UID"] = currentPerformance.patches[0].UID;
    Patches_0["MidiChIn"] = currentPerformance.patches[0].midiCh;
    Patches_0["MidiChOut"] = currentPerformance.patches[0].midiChOut;
    Patches_0["MidiThru"] = currentPerformance.patches[0].midiThru;
    Patches_0["Min"] = currentPerformance.patches[0].min;
    Patches_0["Max"] = currentPerformance.patches[0].max;

    // NOT USED  Multimbrality isn't supported yet
    JsonObject Patches_1 = Patches.createNestedObject();
    Patches_1["Bank"] = currentPerformance.patches[1].bankIndex;
    Patches_1["UID"] = currentPerformance.patches[1].UID;
    Patches_1["MidiChIn"] = currentPerformance.patches[1].midiCh;
    Patches_1["MidiChOut"] = currentPerformance.patches[1].midiChOut;
    Patches_1["MidiThru"] = currentPerformance.patches[1].midiThru;
    Patches_1["Min"] = currentPerformance.patches[1].min;
    Patches_1["Max"] = currentPerformance.patches[1].max;

    JsonObject Encoders = doc.createNestedObject("Encoders");
    Encoders["TL"] = currentPerformance.TL;
    Encoders["TR"] = currentPerformance.TR;
    Encoders["BL"] = currentPerformance.BL;
    Encoders["BR"] = currentPerformance.BR;

    // Need to generate a new UID as the performance settings may have changed if overwriting an existing performance
    String output;
    serializeJson(doc, output); // Generate JSON without UID

    char result[30];
    concatPerformanceFolderAndFilename(currentPerformanceIndex + 1, result);
    SD.remove(result); // Delete Performance file
    File file = SD.open(result, FILE_WRITE);
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
    if (DEBUG)
        Serial.println(F("Save Performance:") + String(currentPerformanceIndex + 1));
}

FLASHMEM void deletePerformance(uint8_t filename)
{
    if (!cardStatus)
        return;
    char result[30];
    concatPerformanceFolderAndFilename(filename, result);
    if (!SD.remove(result))
    {
        // Performance missing
        if (DEBUG)
            Serial.print(F("Performance Missing:"));
        if (DEBUG)
            Serial.println(filename);
    }
    loadPerformanceNames();
}

FLASHMEM uint8_t incPerformanceIndex()
{
    if (currentPerformanceIndex < PERFORMANCES_LIMIT - 1)
    {
        return ++currentPerformanceIndex;
    }
    else
    {
        // Go back to Start from front of array
        currentPerformanceIndex = 0;
        return currentPerformanceIndex;
    }
}

FLASHMEM uint8_t decPerformanceIndex()
{
    if (currentPerformanceIndex > 0)
    {
        return --currentPerformanceIndex;
    }
    else
    {
        // Go to back of array
        currentPerformanceIndex = PERFORMANCES_LIMIT - 1;
        return currentPerformanceIndex;
    }
}