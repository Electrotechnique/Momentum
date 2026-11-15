#ifndef MOMENTUM_FIRMWARE_UPDATE_H
#define MOMENTUM_FIRMWARE_UPDATE_H

// Firmware update from the SD card
//
// 1. Put the new firmware .hex file in the root of the SD card, with any name ending .hex
//    (e.g. MomentumV2_45.hex)
// 2. At power up checkFirmwareFile() looks at every .hex file in the root (not ._ files made
//    by macOS) and reads its version from the Momentum marker (FIRMWARE_MARKER in
//    Constants.cpp) in the file's data:
//    - the same version as the running firmware: the update has just been done, so the file
//      is deleted
//    - no marker (not a Momentum firmware file) or damaged: renamed to .bad
//    - a different version (newer or older): the first one found is offered, Update or
//      Cancel. Cancel renames it to .old so it isn't offered again
// 3. performFirmwareUpdate() reads the file into the unused flash above the running firmware,
//    checking every line, the board (FlasherX target ID) and the Momentum marker and version.
//    Any problem: the copy is erased, the running firmware carries on and the file is renamed
//    to .bad. Otherwise the copy replaces the running firmware (FlasherX flash_move), and
//    Momentum restarts with the new firmware - which then deletes the file (step 2).
//
// Saved settings in EEPROM are kept (see FLASH_RESERVE in FlashTxx.h); patches, performances
// and sequences on the SD card aren't touched. If the power fails during the final copy the
// Teensy can always be reprogrammed over USB with the Teensy Loader.

#include <SD.h>
extern "C"
{
#include "FlashTxx.h"
}

bool firmwareUpdateRequested = false; // set by the Update encoder, run from loop()
elapsedMillis fwPageTimer;            // how long a firmware message page has been shown
#define FW_MESSAGE_MS 4000            // message pages (updated / failed) then return to the main page

// Reads the hex file a buffer at a time, one line per call without the line ending.
// (FlasherX's reader could wait for ever at the end of a file and parsed with sscanf, slowly.)
class HexFileReader
{
public:
    explicit HexFileReader(File &f) : file(f) {}
    // Returns false at the end of the file. Overlong lines are cut short and then fail their checksum.
    bool readLine(char *line, int max)
    {
        int n = 0;
        while (true)
        {
            if (pos >= len)
            {
                len = file.read(buf, sizeof(buf));
                pos = 0;
                if (len <= 0)
                {
                    len = 0;
                    line[n] = 0;
                    return n > 0;
                }
            }
            char c = buf[pos++];
            if (c == '\n' || c == '\r')
            {
                if (n > 0)
                {
                    line[n] = 0;
                    return true;
                }
                continue;
            }
            if (n < max - 1)
                line[n++] = c;
        }
    }

private:
    File &file;
    uint8_t buf[512];
    int len = 0;
    int pos = 0;
};

static int hexNibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static int hexByte(const char *p)
{
    int h = hexNibble(p[0]), l = hexNibble(p[1]);
    return (h < 0 || l < 0) ? -1 : (h << 4) | l;
}

// Intel hex record ":LLAAAATT<data>CC". False if it's malformed or the checksum is wrong.
FLASHMEM bool parseHexRecord(const char *line, uint8_t *data, uint8_t &count, uint16_t &addr, uint8_t &type)
{
    if (line[0] != ':')
        return false;
    size_t n = strlen(line);
    if (n < 11)
        return false;
    int len = hexByte(line + 1);
    if (len < 0 || n < (size_t)(11 + 2 * len))
        return false;
    int ah = hexByte(line + 3), al = hexByte(line + 5), ty = hexByte(line + 7);
    if (ah < 0 || al < 0 || ty < 0)
        return false;
    uint8_t sum = len + ah + al + ty;
    for (int i = 0; i < len; i++)
    {
        int b = hexByte(line + 9 + 2 * i);
        if (b < 0)
            return false;
        data[i] = b;
        sum += b;
    }
    int check = hexByte(line + 9 + 2 * len);
    if (check < 0 || (uint8_t)(sum + check) != 0)
        return false;
    count = len;
    addr = (ah << 8) | al;
    type = ty;
    return true;
}

// Returns the version following the marker prefix at p, or "" if it isn't a complete version
FLASHMEM String versionAfterMarker(const char *p, const char *end)
{
    const char *v = p + FIRMWARE_MARKER_PREFIX_LEN;
    const char *q = v;
    while (q < end && *q >= 0x21 && *q <= 0x7E && q - v < 16)
        q++;
    if (q == v || q >= end || *q != 0)
        return String();
    return String(v).substring(0, q - v);
}

// Finds the Momentum marker in the data of a hex file and returns the version ("" if not found).
// damaged is set if a line of the file is corrupt.
FLASHMEM String readFirmwareFileVersion(const String &fileName, bool &damaged)
{
    damaged = false;
    File f = SD.open(fileName.c_str());
    if (!f)
        return String();
    HexFileReader reader(f);
    char line[600];
    uint8_t data[256];
    uint8_t count, type;
    uint16_t addr;
    // Sliding window over the file's data. Marker + version + 0 is at most 37 bytes, and the
    // last 40 are kept when the window moves on, so a marker split across records is found.
    char win[320];
    int n = 0;
    String version;
    bool endRecord = false;
    while (version.length() == 0 && reader.readLine(line, sizeof(line)))
    {
        if (!parseHexRecord(line, data, count, addr, type))
        {
            damaged = true;
            break;
        }
        if (type == 1)
        {
            endRecord = true; // end of file
            break;
        }
        if (type != 0)
            continue;
        if (n + count > (int)sizeof(win))
        {
            memmove(win, win + n - 40, 40);
            n = 40;
        }
        memcpy(win + n, data, count);
        n += count;
        for (int i = 0; i + FIRMWARE_MARKER_PREFIX_LEN < n; i++)
        {
            if (memcmp(win + i, FIRMWARE_MARKER, FIRMWARE_MARKER_PREFIX_LEN) == 0)
            {
                version = versionAfterMarker(win + i, win + n);
                if (version.length() > 0)
                    break;
            }
        }
    }
    f.close();
    // A file that stops before its end record (and before a marker) has been cut short
    if (version.length() == 0 && !damaged && !endRecord)
        damaged = true;
    return version;
}

// The version in a copy of the firmware in flash ("" if the marker isn't there)
FLASHMEM String firmwareVersionInFlash(const char *start, uint32_t size)
{
    const char *end = start + size;
    for (const char *p = start; p + FIRMWARE_MARKER_PREFIX_LEN < end; p++)
    {
        if (*p == FIRMWARE_MARKER[0] && memcmp(p, FIRMWARE_MARKER, FIRMWARE_MARKER_PREFIX_LEN) == 0)
        {
            String v = versionAfterMarker(p, end);
            if (v.length() > 0)
                return v;
        }
    }
    return String();
}

// Renames a firmware file to the same name with another extension, e.g. Momentum.hex -> Momentum.old.
// Returns the new name.
FLASHMEM String renameFirmwareFile(const String &fileName, const char *newExt)
{
    int dot = fileName.lastIndexOf('.');
    String newName = (dot > 0 ? fileName.substring(0, dot) : fileName) + newExt;
    if (SD.exists(newName.c_str()))
        SD.remove(newName.c_str());
    SD.rename(fileName.c_str(), newName.c_str());
    return newName;
}

// Firmware files: name ends .hex (any case), and not a ._ file made by macOS
FLASHMEM bool isFirmwareFileName(const char *name)
{
    size_t n = strlen(name);
    size_t e = strlen(FW_FILE_EXT);
    return n > e && name[0] != '.' && strcasecmp(name + n - e, FW_FILE_EXT) == 0;
}

FLASHMEM void showFirmwarePage(State s)
{
    LockGuard lock(displayLock);
    state = s;
    setEncodersState(s);
    fwPageTimer = 0;
}

// At power up: are there firmware files on the SD card?
#define FW_MAX_FILES 8
FLASHMEM void checkFirmwareFile()
{
    if (!cardStatus)
        return;
    // Collect the names first: the directory isn't read while files are renamed or deleted
    String names[FW_MAX_FILES];
    int count = 0;
    File root = SD.open("/");
    if (!root)
        return;
    while (count < FW_MAX_FILES)
    {
        File entry = root.openNextFile();
        if (!entry)
            break;
        if (!entry.isDirectory() && isFirmwareFileName(entry.name()))
            names[count++] = entry.name();
        entry.close();
    }
    root.close();

    String offerName, offerVersion, deletedName, badName, badMessage, badRenamedTo;
    for (int i = 0; i < count; i++)
    {
        bool damaged;
        String version = readFirmwareFileVersion(names[i], damaged);
        if (version.length() == 0)
        {
            badName = names[i];
            badMessage = damaged ? F("The file is damaged") : F("Not a Momentum firmware file");
            badRenamedTo = renameFirmwareFile(names[i], FW_FILE_BAD_EXT);
        }
        else if (version == VERSION)
        {
            // This firmware came from the file: the update worked
            SD.remove(names[i].c_str());
            deletedName = names[i];
        }
        else if (offerName.length() == 0)
        {
            offerName = names[i];
            offerVersion = version;
        }
    }

    if (offerName.length() > 0)
    {
        fwFileName = offerName;
        fwFileVersion = offerVersion;
        showFirmwarePage(State::FIRMWARECONFIRM);
    }
    else if (deletedName.length() > 0)
    {
        fwFileName = deletedName;
        showFirmwarePage(State::FIRMWAREDELETE);
    }
    else if (badName.length() > 0)
    {
        fwFileName = badName;
        fwMessage = badMessage;
        fwRenamedTo = badRenamedTo;
        showFirmwarePage(State::FIRMWAREUPDATE_ABORT);
    }
}

// Update chosen (from loop(), not while holding the display lock, so progress is shown).
// Only returns if the update failed.
FLASHMEM void performFirmwareUpdate()
{
    fwProgress = 0;
    // Silence: the flash writes hold up the audio interrupt
    if (currentSequence.running)
        sequencerStop();
    silence();
    for (uint8_t i = 0; i < 4; i++)
    {
        global.effectMixerLM.gain(i, 0.0f);
        global.effectMixerRM.gain(i, 0.0f);
    }
    global.reverb.bypass_set(true);
    delay(50);

    const char *err = nullptr;
    uint32_t bufAddr = 0, bufSize = 0;
    uint32_t minAddr = 0xFFFFFFFF, maxAddr = 0;
    bool bufferInUse = false;

    File f = SD.open(fwFileName.c_str());
    if (!f)
        err = "Can't open the file";
    else if (firmware_buffer_init(&bufAddr, &bufSize) != FLASH_BUFFER_TYPE)
        err = "No space for the new firmware";
    else
    {
        bufferInUse = true;
        HexFileReader reader(f);
        char line[600];
        uint8_t data[256];
        uint8_t count, type;
        uint16_t addr;
        uint32_t base = 0, lines = 0;
        const uint32_t fileSize = f.size();
        bool eof = false;
        while (!eof && !err)
        {
            if (!reader.readLine(line, sizeof(line)))
                err = "The file ends early";
            else if (!parseHexRecord(line, data, count, addr, type))
                err = "Bad line in the file";
            else
            {
                switch (type)
                {
                case 0: // data
                {
                    const uint32_t a = base + addr;
                    if (a < FLASH_BASE_ADDR)
                        err = "Address out of range";
                    else if (a + count - FLASH_BASE_ADDR > bufSize)
                        err = "The file is too large";
                    else if (flash_write_block(bufAddr + a - FLASH_BASE_ADDR, (char *)data, count) != 0)
                        err = "Flash write failed";
                    if (a < minAddr)
                        minAddr = a;
                    if (a + count > maxAddr)
                        maxAddr = a + count;
                    break;
                }
                case 1: // end of file
                    eof = true;
                    break;
                case 2: // extended segment address
                    base = ((uint32_t)((data[0] << 8) | data[1])) << 4;
                    break;
                case 4: // extended linear address
                    base = ((uint32_t)((data[0] << 8) | data[1])) << 16;
                    break;
                case 3: // start addresses: not needed
                case 5:
                    break;
                default:
                    err = "Bad record in the file";
                }
            }
            if ((++lines & 255) == 0 && fileSize > 0)
                fwProgress = (uint8_t)((uint64_t)f.position() * 100 / fileSize);
        }
        f.close();
    }

    if (!err && minAddr != FLASH_BASE_ADDR)
        err = "Not a Teensy firmware file";
    if (!err && !check_flash_id(bufAddr, maxAddr - minAddr))
        err = "Not for Teensy MicroMod";
    if (!err && firmwareVersionInFlash((const char *)bufAddr, maxAddr - minAddr) != fwFileVersion)
        err = "Version check failed";

    if (err)
    {
        if (bufferInUse)
            firmware_buffer_free(bufAddr, bufSize);
        fwMessage = err;
        fwRenamedTo = renameFirmwareFile(fwFileName, FW_FILE_BAD_EXT);
        // Sound back as it was
        for (uint8_t i = 0; i < 4; i++)
        {
            global.effectMixerLM.gain(i, 1.0f);
            global.effectMixerRM.gain(i, 1.0f);
        }
        setCurrentPatchData();
        showFirmwarePage(State::FIRMWAREUPDATE_ABORT);
        return;
    }

    fwProgress = 100;
    showFirmwarePage(State::FIRMWAREINSTALL);
    delay(300); // the display thread draws the page

    // Stop every interrupt source. The core's flash routines turn interrupts back on after each
    // write, and an interrupt handler running code or reading data still in flash (the display
    // thread switch, audio, USB) would crash part way through the copy.
    __disable_irq();
    SYST_CSR = 0; // SysTick (millis, TeensyThreads)
    for (int i = 0; i < 8; i++)
        (&NVIC_ICER0)[i] = 0xFFFFFFFF;
    // Copy the new firmware over the running one, clear the copy area and restart
    flash_move(FLASH_BASE_ADDR, bufAddr, maxAddr - minAddr);
}

// Called from loop()
FLASHMEM void serviceFirmwareUpdate()
{
    if (firmwareUpdateRequested)
    {
        firmwareUpdateRequested = false;
        performFirmwareUpdate();
    }
    // Message pages return to the main page after a few seconds
    if ((state == State::FIRMWAREDELETE || state == State::FIRMWAREUPDATE_ABORT) && fwPageTimer > FW_MESSAGE_MS)
        showFirmwarePage(State::MAIN);
}

#endif
