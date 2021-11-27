/*  This file is part of ELEKTRONIKA-IM01.
    ELEKTRONIKA-IM01 is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    ELEKTRONIKA-IM01 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
ELEKTRONIKA-IM01. If not, see <http://www.gnu.org/licenses/>. */

// Board.h
//

#pragma once

#include "Defines.h"

class CProcessor;


//////////////////////////////////////////////////////////////////////

// Machine configurations
enum BKConfiguration
{
    BK_CONF_IM01 = 1,
    BK_CONF_IM01T = 2,
};


//////////////////////////////////////////////////////////////////////


// TranslateAddress result code
#define ADDRTYPE_RAM     0  // RAM
#define ADDRTYPE_ROM    32  // ROM
#define ADDRTYPE_IO     64  // I/O port
#define ADDRTYPE_DENY  128  // Access denied
#define ADDRTYPE_MASK  224  // RAM type mask
#define ADDRTYPE_RAMMASK 7  // RAM chunk number mask

// Trace flags
#define TRACE_NONE         0  // Turn off all tracing
#define TRACE_CPUROM       1  // Trace CPU instructions from ROM
#define TRACE_CPURAM       2  // Trace CPU instructions from RAM
#define TRACE_CPU          3  // Trace CPU instructions (mask)
#define TRACE_ALL    0177777  // Trace all

// Emulator image constants
#define BKIMAGE_HEADER_SIZE 32
#define BKIMAGE_SIZE 200704
#define BKIMAGE_HEADER1 0x30304B41  // "BK00"
#define BKIMAGE_HEADER2 0x214C5442  // "BTL!"
#define BKIMAGE_VERSION 0x00010000  // 1.0


//////////////////////////////////////////////////////////////////////


// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);


//////////////////////////////////////////////////////////////////////

class CMotherboard  // BK computer
{
private:  // Devices
    CProcessor* m_pCPU;  // CPU device
private:  // Memory
    uint16_t    m_Configuration;  // See BK_COPT_Xxx flag constants
    uint8_t     m_MemoryMap;      // Memory map, every bit defines how 8KB mapped: 0 - RAM, 1 - ROM
    uint8_t     m_MemoryMapOnOff; // Memory map, every bit defines how 8KB: 0 - deny, 1 - OK
    uint8_t*    m_pRAM;  // RAM, 4 KB
    uint8_t*    m_pROM;  // ROM, 8 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor* GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    uint16_t    GetRAMWord(uint16_t offset) const;
    uint16_t    GetRAMWord(uint8_t chunk, uint16_t offset) const;
    uint8_t     GetRAMByte(uint16_t offset) const;
    uint8_t     GetRAMByte(uint8_t chunk, uint16_t offset) const;
    void        SetRAMWord(uint16_t offset, uint16_t word);
    void        SetRAMWord(uint8_t chunk, uint16_t offset, uint16_t word);
    void        SetRAMByte(uint16_t offset, uint8_t byte);
    void        SetRAMByte(uint8_t chunk, uint16_t offset, uint8_t byte);
    uint16_t    GetROMWord(uint16_t offset) const;
    uint8_t     GetROMByte(uint16_t offset) const;
public:  // Debug
    void        DebugTicks();  // One Debug CPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoints(const uint16_t* bps) { m_CPUbps = bps; } // Set CPU breakpoint list
    uint32_t    GetTrace() const { return m_dwTrace; }
    void        SetTrace(uint32_t dwTrace);
public:  // System control
    void        SetConfiguration(uint16_t conf);
    uint16_t    GetConfiguration() const { return m_Configuration; }
    void        Reset();  // Reset computer
    void        LoadROM(int bank, const uint8_t* pBuffer);  // Load 8 KB ROM image from the biffer
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void        TimerTick();        // Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
    void        ResetDevices();     // INIT signal
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    bool        SystemFrame();  // Do one frame -- use for normal run
    uint8_t     GetIndicator(int pos);
    void        KeyboardEvent(uint8_t scancode, bool okPressed);  // Key pressed or released
    int         GetSoundChanges() const { return m_SoundChanges; }  ///< Sound signal 0 to 1 changes since the beginning of the frame
public:  // Callbacks
    void        SetSoundGenCallback(SOUNDGENCALLBACK callback);
public:  // Memory
    // Read command for execution
    uint16_t GetWordExec(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, true); }
    // Read word from memory
    uint16_t GetWord(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, false); }
    // Read word
    uint16_t GetWord(uint16_t address, bool okHaltMode, bool okExec);
    // Write word
    void SetWord(uint16_t address, bool okHaltMode, uint16_t word);
    // Read byte
    uint8_t GetByte(uint16_t address, bool okHaltMode);
    // Write byte
    void SetByte(uint16_t address, bool okHaltMode, uint8_t byte);
    // Read word from memory for debugger
    uint16_t GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pValid) const;
    // Read word from port for debugger
    uint16_t GetPortView(uint16_t address) const;
    // Read SEL register
    uint16_t GetSelRegister() const { return 0020000 | 0300; }
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - true: read instruction for execution; false: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset) const;
private:  // Access to I/O ports
    uint16_t    GetPortWord(uint16_t address);
    void        SetPortWord(uint16_t address, uint16_t word);
    uint8_t     GetPortByte(uint16_t address);
    void        SetPortByte(uint16_t address, uint8_t byte);
public:  // Saving/loading emulator status
    void        SaveToImage(uint8_t* pImage);
    void        LoadFromImage(const uint8_t* pImage);
private:  // Ports: implementation
    uint16_t    m_Port164060;       // Register A
    uint16_t    m_Port164074;       // Register D
    uint8_t     m_Indicator[5 * 8];
    void        UpdateIndicator(uint8_t mask, uint8_t value);
    uint8_t     m_KeyMatrix[6];     // Keyboard matrix 6x4
private:  // Timer implementation
    uint16_t    m_timer;
    uint16_t    m_timerreload;
    uint16_t    m_timerflags;
    uint16_t    m_timerdivider;
    void        SetTimerReload(uint16_t val);   //sets timer reload value
    void        SetTimerState(uint16_t val);    //sets timer state
private:
    const uint16_t* m_CPUbps;  // CPU breakpoint list, ends with 177777 value
    uint32_t    m_dwTrace;  // Trace flags
    int         m_SoundPrevValue;  // Previous value of the sound signal
    int         m_SoundChanges;  // Sound signal 0 to 1 changes since the beginning of the frame
private:
    SOUNDGENCALLBACK m_SoundGenCallback;
private:
    void        DoSound();
};


//////////////////////////////////////////////////////////////////////
