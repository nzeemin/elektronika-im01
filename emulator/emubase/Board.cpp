/*  This file is part of ELEKTRONIKA-IM01.
    ELEKTRONIKA-IM01 is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    ELEKTRONIKA-IM01 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
ELEKTRONIKA-IM01. If not, see <http://www.gnu.org/licenses/>. */

// Board.cpp
//

#include "stdafx.h"
#include "Emubase.h"
#include "Board.h"

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, uint32_t dwTrace);


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);

    m_dwTrace = TRACE_NONE;
    m_SoundGenCallback = nullptr;
    m_SoundPrevValue = 0;
    m_SoundChanges = 0;
    m_CPUbps = nullptr;

    // Allocate memory for RAM and ROM
    m_pRAM = (uint8_t*) ::malloc(64 * 1024);  //::memset(m_pRAM, 0, 128 * 1024);
    m_pROM = (uint8_t*) ::calloc(64 * 1024, 1);

    SetConfiguration(BK_CONF_IM01);  // Default configuration

    Reset();
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;

    // Free memory
    ::free(m_pRAM);
    ::free(m_pROM);
}

void CMotherboard::SetConfiguration(uint16_t conf)
{
    m_Configuration = conf;

    // Define memory map
    m_MemoryMap = 0xfe;  // By default, 000000-077777 is RAM, 100000-177777 is ROM
    m_MemoryMapOnOff = 0xff;  // By default, all 8K blocks are valid

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 64 * 1024);
    ::memset(m_pROM, 0, 64 * 1024);

    //// Pre-fill RAM with "uninitialized" values
    //uint16_t * pMemory = (uint16_t *) m_pRAM;
    //uint16_t val = 0;
    //uint8_t flag = 0;
    //for (uint32_t i = 0; i < 128 * 1024; i += 2, flag--)
    //{
    //    *pMemory = val;  pMemory++;
    //    if (flag == 192)
    //        flag = 0;
    //    else
    //        val = ~val;
    //}
}

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
}

void CMotherboard::Reset ()
{
    m_pCPU->Stop();

    // Reset ports
    m_Port177560 = m_Port177562 = 0;
    m_Port177564 = 0200;
    m_Port177566 = 0;
    m_Port177660 = 0100;
    m_Port177662rd = 0;
    m_Port177662wr = 047400;
    m_Port177664 = 01330;
    m_Port177714in = m_Port177714out = 0;
    m_Port177716 = 0020000 | 0300;
    m_Port177716mem = 0000002;
    m_Port177716tap = 0200;

    m_timer = 0177777;
    m_timerdivider = 0;
    m_timerreload = 011000;
    m_timerflags = 0177400;

    ResetDevices();

    m_pCPU->Start();
}

// Load 8 KB ROM image from the buffer
//   bank - number of 8k ROM bank, 0..7
void CMotherboard::LoadROM(int bank, const uint8_t* pBuffer)
{
    ASSERT(bank >= 0 && bank <= 7);
    ::memcpy(m_pROM + 8192 * bank, pBuffer, 8192);
}

void CMotherboard::LoadRAM(int startbank, const uint8_t* pBuffer, int length)
{
    ASSERT(pBuffer != nullptr);
    ASSERT(startbank >= 0 && startbank < 15);
    int address = 8192 * startbank;
    ASSERT(address + length <= 128 * 1024);
    ::memcpy(m_pRAM + address, pBuffer, length);
}

// Работа с памятью //////////////////////////////////////////////////

uint16_t CMotherboard::GetRAMWord(uint16_t offset) const
{
    return *((uint16_t*)(m_pRAM + offset));
}
uint16_t CMotherboard::GetRAMWord(uint8_t chunk, uint16_t offset) const
{
    uint32_t dwOffset = (((uint32_t)chunk & 7) << 14) + offset;
    return *((uint16_t*)(m_pRAM + dwOffset));
}
uint8_t CMotherboard::GetRAMByte(uint16_t offset) const
{
    return m_pRAM[offset];
}
uint8_t CMotherboard::GetRAMByte(uint8_t chunk, uint16_t offset) const
{
    uint32_t dwOffset = (((uint32_t)chunk & 7) << 14) + offset;
    return m_pRAM[dwOffset];
}
void CMotherboard::SetRAMWord(uint16_t offset, uint16_t word)
{
    *((uint16_t*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetRAMWord(uint8_t chunk, uint16_t offset, uint16_t word)
{
    uint32_t dwOffset = (((uint32_t)chunk & 7) << 14) + offset;
    *((uint16_t*)(m_pRAM + dwOffset)) = word;
}
void CMotherboard::SetRAMByte(uint16_t offset, uint8_t byte)
{
    m_pRAM[offset] = byte;
}
void CMotherboard::SetRAMByte(uint8_t chunk, uint16_t offset, uint8_t byte)
{
    uint32_t dwOffset = (((uint32_t)chunk & 7) << 14) + offset;
    m_pRAM[dwOffset] = byte;
}

uint16_t CMotherboard::GetROMWord(uint16_t offset) const
{
    ASSERT(offset < 1024 * 64);
    return *((uint16_t*)(m_pROM + offset));
}
uint8_t CMotherboard::GetROMByte(uint16_t offset) const
{
    ASSERT(offset < 1024 * 64);
    return m_pROM[offset];
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetDevices()
{
    // Reset ports
    m_Port177560 = m_Port177562 = 0;
    m_Port177564 = 0200;
    m_Port177566 = 0;

    // Reset timer
    m_timerflags = 0177400;
    m_timer = 0177777;
    m_timerreload = 011000;
}

void CMotherboard::Tick50()  // 50 Hz timer
{
    if ((m_Port177662wr & 040000) == 0)
    {
        m_pCPU->TickIRQ2();
    }
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

void CMotherboard::TimerTick() // Timer Tick, 31250 Hz = 32 мкс (BK-0011), 23437.5 Hz = 42.67 мкс (BK-0010)
{
    if ((m_timerflags & 1) == 1)  // STOP, the timer stopped
    {
        m_timer = m_timerreload;
        return;
    }
    if ((m_timerflags & 16) == 0)  // Not RUN, the timer paused
        return;

    m_timerdivider++;

    bool flag = false;
    switch ((m_timerflags >> 5) & 3)  // bits 5,6 -- prescaler
    {
    case 0:  // 32 мкс
        flag = true;
        break;
    case 1:  // 32 * 16 = 512 мкс
        flag = (m_timerdivider >= 16);
        break;
    case 2: // 32 * 4 = 128 мкс
        flag = (m_timerdivider >= 4);
        break;
    case 3:  // 32 * 16 * 4 = 2048 мкс, 8129 тактов процессора
        flag = (m_timerdivider >= 64);
        break;
    }
    if (!flag)  // Nothing happened
        return;

    m_timerdivider = 0;
    m_timer--;
    if (m_timer == 0)
    {
        if ((m_timerflags & 2) == 0)  // If not WRAPAROUND
        {
            if ((m_timerflags & 8) != 0)  // If ONESHOT and not WRAPAROUND then reset RUN bit
                m_timerflags &= ~16;

            m_timer = m_timerreload;
        }

        if ((m_timerflags & 4) != 0)  // If EXPENABLE
            m_timerflags |= 128;  // Set EXPIRY bit
    }
}

void CMotherboard::SetTimerReload(uint16_t val)  // Sets timer reload value, write to port 177706
{
    //DebugPrintFormat(_T("SetTimerReload %06o\r\n"), val);
    m_timerreload = val;
}
void CMotherboard::SetTimerState(uint16_t val) // Sets timer state, write to port 177712
{
    //DebugPrintFormat(_T("SetTimerState %06o\r\n"), val);
    m_timer = m_timerreload;

    m_timerflags = 0177400 | val;
}

void CMotherboard::DebugTicks()
{
    m_pCPU->ClearInternalTick();
    m_pCPU->Execute();
}


/*
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.
12 МГц = 1 / 12000000 = 0.83(3) мкс
В каждый фрейм происходит:
* 120000 тиков ЦП - 6 раз за тик
* программируемый таймер - на каждый 128-й тик процессора; 42.6(6) мкс либо 32 мкс
* 2 тика IRQ2 50 Гц, в 0-й и 10000-й тик фрейма
*/
bool CMotherboard::SystemFrame()
{
    const int frameProcTicks = 6;
    const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);
    m_SoundChanges = 0;

    int timerTicks = 0;

    for (int frameticks = 0; frameticks < 20000; frameticks++)
    {
        for (int procticks = 0; procticks < frameProcTicks; procticks++)  // CPU ticks
        {
#if !defined(PRODUCT)
            if ((m_dwTrace & TRACE_CPU) && m_pCPU->GetInternalTick() == 0)
                TraceInstruction(m_pCPU, this, m_pCPU->GetPC(), m_dwTrace);
#endif
            m_pCPU->Execute();
            if (m_CPUbps != nullptr)  // Check for breakpoints
            {
                const uint16_t* pbps = m_CPUbps;
                while (*pbps != 0177777) { if (m_pCPU->GetPC() == *pbps++) return false; }
            }

            timerTicks++;
            if (timerTicks >= 128)
            {
                TimerTick();  // System timer tick: 31250 Hz = 32uS (BK-0011), 23437.5 Hz = 42.67 uS (BK-0010)
                timerTicks = 0;
            }
        }

        if (frameticks % 10000 == 0)
        {
            Tick50();  // 1/50 timer event
        }

        if (frameticks % audioticks == 0)  // AUDIO tick
            DoSound();
    }

    return true;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(uint8_t scancode, bool okPressed, bool okAr2)
{
    if (scancode == BK_KEY_STOP)
    {
        if (okPressed)
        {
            m_pCPU->AssertIRQ1();
        }
        return;
    }

    if (!okPressed)  // Key released
    {
        m_Port177716 |= 0100;  // Reset "Key pressed" flag in system register
        m_Port177716 |= 4;  // Set "ready" flag
        return;
    }

    m_Port177716 &= ~0100;  // Set "Key pressed" flag in system register
    m_Port177716 |= 4;  // Set "ready" flag

    if ((m_Port177660 & 0200) == 0)
    {
        m_Port177662rd = scancode & 0177;
        m_Port177660 |= 0200;  // "Key ready" flag in keyboard state register
        if ((m_Port177660 & 0100) == 0100)  // Keyboard interrupt enabled
        {
            uint16_t intvec = ((okAr2 || (scancode & 0200) != 0) ? 0274 : 060);
            m_pCPU->InterruptVIRQ(1, intvec);
        }
    }
}


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
uint16_t CMotherboard::GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pAddrType) const
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    *pAddrType = addrtype;

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        //return GetRAMWord(offset & 0177776);  //TODO: Use (addrtype & ADDRTYPE_RAMMASK) bits
        return GetRAMWord(addrtype & ADDRTYPE_RAMMASK, offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        return 0;  // I/O port, not memory
    case ADDRTYPE_DENY:
        return 0;  // This memory is inaccessible for reading
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint16_t CMotherboard::GetWord(uint16_t address, bool okHaltMode, bool okExec)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(addrtype & ADDRTYPE_RAMMASK, offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortWord(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint8_t CMotherboard::GetByte(uint16_t address, bool okHaltMode)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMByte(addrtype & ADDRTYPE_RAMMASK, offset);
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortByte(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(uint16_t address, bool okHaltMode, uint16_t word)
{
    uint16_t offset;

    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMWord(addrtype & ADDRTYPE_RAMMASK, offset & 0177776, word);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(uint16_t address, bool okHaltMode, uint8_t byte)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMByte(addrtype & ADDRTYPE_RAMMASK, offset, byte);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

int CMotherboard::TranslateAddress(uint16_t address, bool /*okHaltMode*/, bool /*okExec*/, uint16_t* pOffset) const
{
    uint16_t portStartAddr = 0164000;
    if (address >= portStartAddr)  // Port
    {
        *pOffset = address;
        return ADDRTYPE_IO;
    }

    int memoryBlock = (address >> 13) & 7;  // 8K block number 0..7
    bool okValid = (m_MemoryMapOnOff >> memoryBlock) & 1;  // 1 - OK, 0 - deny
    if (!okValid)
        return ADDRTYPE_DENY;
    bool okRom = (m_MemoryMap >> memoryBlock) & 1;  // 1 - ROM, 0 - RAM
    if (okRom)
        address -= 0020000;

    *pOffset = address;
    return (okRom) ? ADDRTYPE_ROM : ADDRTYPE_RAM;
}

uint8_t CMotherboard::GetPortByte(uint16_t address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (uint8_t) GetPortWord(address);
}

uint16_t CMotherboard::GetPortWord(uint16_t address)
{
    switch (address)
    {
    case 0177560:  // Serial port recieve status
        return m_Port177560;
    case 0177562:  // Serial port recieve data
        return m_Port177562;
    case 0177564:  // Serial port translate status
        return m_Port177564;
    case 0177566:  // Serial port interrupt vector
        return 060;

    case 0177700:  // Регистр режима (РР) ВМ1
        return 0177740;
    case 0177702:  // Регистр адреса прерывания (РАП) ВМ1
        return 0177777;
    case 0177704:  // Регистр ошибки (РОШ) ВМ1
        return 0177440;

    case 0177706:  // System Timer counter start value -- регистр установки таймера
        return m_timerreload;
    case 0177710:  // System Timer Counter -- регистр счетчика таймера
        return m_timer;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        return m_timerflags;

    case 0177660:  // Keyboard status register
        return m_Port177660;
    case 0177662:  // Keyboard register
        m_Port177660 &= ~0200;  // Reset "Ready" bit
        return m_Port177662rd;

    case 0177664:  // Scroll register
        return m_Port177664;

    case 0177714:  // Parallel port register: printer, joystick
        return m_Port177714in;

    case 0177716:  // System register
        {
            uint16_t value = m_Port177716;
            m_Port177716 &= ~4;  // Reset bit 2
            return value;
        }

    case 0164004:  // ???
        return 0;//STUB

    case 0164060:  // ???
        return 0;//STUB

    case 0164072:  // ???
        return 0;

    case 0164074:  // ???
        return 0;

    case 0177750:  // ???
        return 0;//STUB

    case 0177760:  // ???
        return 0;//STUB

    default:
        m_pCPU->MemoryError();
        return 0;
    }

    //return 0;
}

// Read word from port for debugger
uint16_t CMotherboard::GetPortView(uint16_t address) const
{
    switch (address)
    {
    case 0177560:  // Serial port recieve status
        return m_Port177560;
    case 0177562:  // Serial port recieve data
        return m_Port177562;
    case 0177564:  // Serial port translate status
        return m_Port177564;
    case 0177566:  // Serial port interrupt vector
        return 060;

    case 0177706:  // System Timer counter start value -- регистр установки таймера
        return m_timerreload;
    case 0177710:  // System Timer Counter -- регистр счетчика таймера
        return m_timer;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        return m_timerflags;

    case 0177660:  // Keyboard status register
        return m_Port177660;
    case 0177662:  // Keyboard data register
        return m_Port177662rd;

    case 0177664:  // Scroll register
        return m_Port177664;

    case 0177716:  // System register
        return m_Port177716;

    case 0164004:  // ???
        return 0;//STUB

    case 0164060:  // ???
        return 0;//STUB

    case 0164072:  // ???
        return 0;

    case 0164074:  // ???
        return 0;

    case 0177750:  // ???
        return 0;//STUB

    case 0177760:  // ???
        return 0;//STUB

    default:
        return 0;
    }
}

void CMotherboard::SetPortByte(uint16_t address, uint8_t byte)
{
    uint16_t word;
    if (address & 1)
    {
        word = GetPortWord(address & 0xfffe);
        word &= 0xff;
        word |= byte << 8;
        SetPortWord(address & 0xfffe, word);
    }
    else
    {
        word = GetPortWord(address);
        word &= 0xff00;
        SetPortWord(address, word | byte);
    }
}

//void DebugPrintFormat(LPCTSTR pszFormat, ...);  //DEBUG
void CMotherboard::SetPortWord(uint16_t address, uint16_t word)
{
    switch (address)
    {
    case 0177560:
        m_Port177560 = word;
        break;
    case 0177562:
        //TODO
        break;
    case 0177564:  // Serial port output status register
//        DebugPrintFormat(_T("177564 write '%06o'\r\n"), word);
        m_Port177564 = word;
        break;
    case 0177566:  // Serial port output data
//        DebugPrintFormat(_T("177566 write '%c'\r\n"), (uint8_t)word);
        m_Port177566 = word;
        m_Port177564 &= ~0200;
        break;

    case 0177700: case 0177702: case 0177704:  // Unknown something
        break;

    case 0177706:  // System Timer reload value -- регистр установки таймера
        SetTimerReload(word);
        break;
    case 0177710:  // System Timer Counter -- регистр реверсивного счетчика таймера
        //Do nothing: the register is read-only
        break;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        SetTimerState(word);
        break;

    case 0177714:  // Parallel port register
        m_Port177714out = word;
        break;

    case 0177716:  // System register - memory management, tape management
        m_Port177716 |= 4;  // Set bit 2
        if (word & 04000)
        {
            m_Port177716mem = word;

//            DebugLogFormat(_T("177716mem %06o\t\t%06o\r\n"), word, m_pCPU->GetInstructionPC());
        }
        else
        {
            m_Port177716tap = word;
        }
        break;

    case 0177660:  // Keyboard status register
        //TODO
        break;

    case 0177662:  // Palette register
        m_Port177662wr = word;
        break;

    case 0177664:  // Scroll register
        m_Port177664 = word & 01377;
        break;

    case 0164004:  // ???
        break;

    case 0164060:  // ???
        break;

    case 0164072:  // ???
        break;

    case 0164074:  // ???
        break;

    case 0177750:  // ???
        break;

    case 0177760:  // ???
        break;

    default:
        m_pCPU->MemoryError();
        break;
    }
}


//////////////////////////////////////////////////////////////////////
// Emulator image
//  Offset Length
//       0     32 bytes  - Header
//      32    128 bytes  - Board status
//     160     32 bytes  - CPU status
//     192   3904 bytes  - RESERVED
//    4096  65536 bytes  - ROM image 64K
//   69632 131072 bytes  - RAM image 128K
//  200704     --        - END

void CMotherboard::SaveToImage(uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*) (pImage + 32);
    *pwImage++ = m_Configuration;
    pwImage += 6;  // RESERVED
    *pwImage++ = m_Port177560;
    *pwImage++ = m_Port177562;
    *pwImage++ = m_Port177564;
    *pwImage++ = m_Port177566;
    *pwImage++ = m_Port177660;
    *pwImage++ = m_Port177662rd;
    *pwImage++ = m_Port177662wr;
    *pwImage++ = m_Port177664;
    *pwImage++ = m_Port177714in;
    *pwImage++ = m_Port177714out;
    *pwImage++ = m_Port177716;
    *pwImage++ = m_Port177716mem;
    *pwImage++ = m_Port177716tap;
    pwImage += 3;  // RESERVED
    *pwImage++ = m_timer;
    *pwImage++ = m_timerreload;
    *pwImage++ = m_timerflags;
    *pwImage++ = m_timerdivider;

    // CPU status
    uint8_t* pImageCPU = pImage + 160;
    m_pCPU->SaveToImage(pImageCPU);
    // ROM
    uint8_t* pImageRom = pImage + 4096;
    memcpy(pImageRom, m_pROM, 64 * 1024);
    // RAM
    uint8_t* pImageRam = pImage + 69632;
    memcpy(pImageRam, m_pRAM, 128 * 1024);
}
void CMotherboard::LoadFromImage(const uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*) (pImage + 32);
    m_Configuration = *pwImage++;
    pwImage += 6;  // RESERVED
    m_Port177560 = *pwImage++;
    m_Port177562 = *pwImage++;
    m_Port177564 = *pwImage++;
    m_Port177566 = *pwImage++;
    m_Port177660 = *pwImage++;
    m_Port177662rd = *pwImage++;
    m_Port177662wr = *pwImage++;
    m_Port177664 = *pwImage++;
    m_Port177714in = *pwImage++;
    m_Port177714out = *pwImage++;
    m_Port177716 = *pwImage++;
    m_Port177716mem = *pwImage++;
    m_Port177716tap = *pwImage++;
    pwImage += 3;  // RESERVED
    m_timer = *pwImage++;
    m_timerreload = *pwImage++;
    m_timerflags = *pwImage++;
    m_timerdivider = *pwImage++;

    // CPU status
    const uint8_t* pImageCPU = pImage + 160;
    m_pCPU->LoadFromImage(pImageCPU);

    // ROM
    const uint8_t* pImageRom = pImage + 4096;
    memcpy(m_pROM, pImageRom, 64 * 1024);
    // RAM
    const uint8_t* pImageRam = pImage + 69632;
    memcpy(m_pRAM, pImageRam, 128 * 1024);
}


//////////////////////////////////////////////////////////////////////

uint16_t CMotherboard::GetKeyboardRegister(void)
{
    uint16_t res = 0;

    uint16_t mem000042 = GetRAMWord(000042);
    res |= (mem000042 & 0100000) == 0 ? KEYB_LAT : KEYB_RUS;

    return res;
}

void CMotherboard::DoSound(void)
{
    uint8_t soundValue = (m_Port177716tap & 0100) != 0 ? 0xff : 0;

    if (m_SoundPrevValue == 0 && soundValue != 0)
        m_SoundChanges++;
    m_SoundPrevValue = soundValue;

    if (m_SoundGenCallback == nullptr)
        return;

    uint16_t value16 = soundValue << 5;
    (*m_SoundGenCallback)(value16, value16);
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == nullptr)  // Reset callback
    {
        m_SoundGenCallback = nullptr;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}


//////////////////////////////////////////////////////////////////////

#if !defined(PRODUCT)

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, uint32_t dwTrace)
{
    bool okHaltMode = pProc->IsHaltMode();

    uint16_t memory[4];
    int addrtype = ADDRTYPE_RAM;
    memory[0] = pBoard->GetWordView(address + 0 * 2, okHaltMode, true, &addrtype);
    if (!(addrtype == ADDRTYPE_RAM && (dwTrace & TRACE_CPURAM)) &&
        !(addrtype == ADDRTYPE_ROM && (dwTrace & TRACE_CPUROM)))
        return;
    memory[1] = pBoard->GetWordView(address + 1 * 2, okHaltMode, true, &addrtype);
    memory[2] = pBoard->GetWordView(address + 2 * 2, okHaltMode, true, &addrtype);
    memory[3] = pBoard->GetWordView(address + 3 * 2, okHaltMode, true, &addrtype);

    TCHAR bufaddr[7];
    PrintOctalValue(bufaddr, address);

    TCHAR instr[8];
    TCHAR args[32];
    DisassembleInstruction(memory, address, instr, args);
    TCHAR buffer[64];
    _sntprintf(buffer, 64, _T("%s\t%s\t%s\r\n"), bufaddr, instr, args);

    DebugLog(buffer);
}

#endif

//////////////////////////////////////////////////////////////////////
