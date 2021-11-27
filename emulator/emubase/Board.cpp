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
    m_pRAM = (uint8_t*) ::malloc(64 * 1024);
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
    m_MemoryMap = 0xfe;  // RAM/ROM mapping
    m_MemoryMapOnOff = 0x07;  // Only lower three 8K blocks are valid

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 64 * 1024);
    ::memset(m_pROM, 0, 64 * 1024);
}

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
}

void CMotherboard::Reset()
{
    m_pCPU->Stop();

    // Reset ports
    m_Port164060 = m_Port164074 = 0;
    memset(m_Indicator, 0, sizeof(m_Indicator));
    memset(m_KeyMatrix, 0, sizeof(m_KeyMatrix));

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
    m_Port164060 = m_Port164074 = 0;

    // Reset timer
    m_timerflags = 0177400;
    m_timer = 0177777;
    m_timerreload = 011000;
}

void CMotherboard::Tick50()  // 50 Hz timer
{
    for (int i = 0; i < sizeof(m_Indicator); i++)
    {
        if (m_Indicator[i] > 0)
            m_Indicator[i]--;
    }

    m_pCPU->TickIRQ2();
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

void CMotherboard::UpdateIndicator(uint8_t mask, uint8_t value)
{
    for (int m = 0; m < 5; m++)
    {
        if ((mask & (1 << m)) == 0)
            continue;

        for (int i = 0; i < 8; i++)
        {
            if ((value & (1 << i)) != 0)
                m_Indicator[m * 8 + i] = 10;
        }
    }
}

uint8_t CMotherboard::GetIndicator(int pos)
{
    if (pos < 0 || pos > 4)
        return 0;

    uint8_t value = 0;
    for (int i = 0; i < 8; i++)
    {
        if (m_Indicator[pos * 8 + i] != 0)
            value |= (1 << i);
    }
    return value;
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
void CMotherboard::KeyboardEvent(uint8_t scancode, bool okPressed)
{
    int col = (scancode & 0070) >> 3;  // 0..5
    int row = scancode & 0003;  // 0..3

    if (okPressed)
        m_KeyMatrix[col] |= (1 << row);
    else
        m_KeyMatrix[col] &= ~(1 << row);
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

    if (address < 0010000)  // 000000..007777 - 4K RAM
    {
        *pOffset = address;
        return ADDRTYPE_RAM;
    }

    if (address < 0020000)  // 010000..017777 - 4K none
        return ADDRTYPE_DENY;

    if (address < 0060000)
    {
        *pOffset = address - 0020000;
        return ADDRTYPE_ROM;
    }

    return ADDRTYPE_DENY;
}

uint8_t CMotherboard::GetPortByte(uint16_t address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (uint8_t) GetPortWord(address);
}

uint16_t CMotherboard::GetPortWord(uint16_t address)
{
    //DebugLogFormat(_T("READ PORT %06o PC=%06o\r\n"), address, m_pCPU->GetInstructionPC());

    switch (address)
    {
    case 0164004:  // ???
        return 0;//STUB

    case 0164060:  // ???
        return m_Port164060;

    case 0164072:  // ???
        return 0;

    case 0164074:  // ???
        return m_Port164074;

    case 0164076:  // ???
        {
            for (int col = 0; col < 6; col++)
            {
                if ((m_Port164060 & (1 << col)) != 0)
                {
                    //DebugLogFormat(_T("READ PORT 164076 PC=%06o, (164060)=%06o, value=%03o\r\n"), m_pCPU->GetInstructionPC(), m_Port164060, m_KeyMatrix[col]);
                    return m_KeyMatrix[col] << 8;
                }
            }
            return 0;
        }

    case 0177750:  // ???
        return 0;//STUB

    case 0177760:  // ???
        return 0;//STUB

    default:
        m_pCPU->MemoryError();
        DebugLogFormat(_T("READ UNKNOWN PORT %06o PC=%06o\r\n"), address, m_pCPU->GetInstructionPC());
        return 0;
    }

    //return 0;
}

// Read word from port for debugger
uint16_t CMotherboard::GetPortView(uint16_t address) const
{
    switch (address)
    {
    case 0164004:  // ???
        return 0;//STUB

    case 0164060:  // ???
        return m_Port164060;

    case 0164072:  // ???
        return 0;

    case 0164074:  // ???
        return 0;

    case 0164076:  // ???
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
    //DebugLogFormat(_T("WRITE PORT %06o value %06o PC=%06o\r\n"), address, word, m_pCPU->GetInstructionPC());

    switch (address)
    {
    case 0164004:  // ???
        break;

    case 0164060:  // ???
        m_Port164060 = word;
        break;

    case 0164072:  // ???
        break;

    case 0164074:  // ???
        m_Port164074 = word;
        UpdateIndicator((uint8_t)m_Port164060, (uint8_t)word);
        //DebugLogFormat(_T("WRITE PORT 164074 value %06o PC=%06o, (164060)=%06o\r\n"), word, m_pCPU->GetInstructionPC(), m_Port164060);
        break;

    case 0164076:  // ???
        break;

    case 0177750:  // ???
        break;

    case 0177760:  // ???
        break;

    default:
        m_pCPU->MemoryError();
        DebugLogFormat(_T("WRITE UNKNOWN PORT %06o PC=%06o value %06o\r\n"), address, m_pCPU->GetInstructionPC(), word);
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


void CMotherboard::DoSound(void)
{
    uint8_t soundValue = 0;//TODO

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
