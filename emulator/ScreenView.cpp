/*  This file is part of ELEKTRONIKA-IM01.
    ELEKTRONIKA-IM01 is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    ELEKTRONIKA-IM01 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
ELEKTRONIKA-IM01. If not, see <http://www.gnu.org/licenses/>. */

// ScreenView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"

//////////////////////////////////////////////////////////////////////


#define COLOR_BK_BACKGROUND RGB(255,255,233)
#define COLOR_BOARD_WHITE   RGB(200,200,180)
#define COLOR_BOARD_BLACK   RGB(100,100,100)
#define COLOR_BOARD_TEXT    RGB(140,140,140)

HWND g_hwndScreen = NULL;  // Screen View window handle

BYTE m_arrScreen_BoardData[64];

int m_cxScreenWidth = 320;
int m_cyScreenHeight = 354;

void ScreenView_OnDraw(HDC hdc);
BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed);


//////////////////////////////////////////////////////////////////////

void ScreenView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ScreenViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_SCREENVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    memset(m_arrScreen_BoardData, 0, sizeof(m_arrScreen_BoardData));
}

void ScreenView_Done()
{
}


// Create Screen View as child of Main Window
void ScreenView_Create(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != NULL);

    int xLeft = x;
    int yTop = y;
    int cyScreenHeight = 4 + m_cyScreenHeight + 4;
    int cyHeight = cyScreenHeight;
    int cxWidth = 4 + m_cxScreenWidth + 4;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        if ((lParam & (1 << 30)) != 0)
            return (LRESULT)TRUE;
        return (LRESULT)ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, TRUE);
    case WM_KEYUP:
        return (LRESULT)ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, FALSE);
    case WM_SETCURSOR:
        if (::GetFocus() == g_hwndScreen)
        {
            SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed)
{
    BYTE keyscan;
    switch (vkey)
    {
    case 0x31: case 0x41: keyscan = 0103;  break;  // "1", "A"
    case 0x32: case 0x42: keyscan = 0102;  break;  // "2", "B"
    case 0x33: case 0x43: keyscan = 0101;  break;  // "3", "C"
    case 0x34: case 0x44: keyscan = 0100;  break;  // "4", "D"
    case 0x35: case 0x45: keyscan = 0133;  break;  // "5", "E"
    case 0x36: case 0x46: keyscan = 0132;  break;  // "6", "F"
    case 0x37: case 0x47: keyscan = 0131;  break;  // "7", "G"
    case 0x38: case 0x48: keyscan = 0130;  break;  // "8", "H"
    case VK_RETURN: keyscan = 0123;  break;  // Arrow down
    default:
        return FALSE;
    }

    Emulator_KeyEvent(keyscan, okPressed);

    return TRUE;
}

void ScreenView_OnDraw(HDC hdc)
{
    RECT rc;  ::GetClientRect(g_hwndScreen, &rc);
    int boardx = 22;
    int boardy = 22;
    int cellsize = 60;

    // Empty border
    HBRUSH hBrush = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, rc.right, boardy, PATCOPY);
    PatBlt(hdc, 0, 0, boardx, rc.bottom, PATCOPY);
    PatBlt(hdc, boardx + m_cxScreenWidth, 0, rc.right - boardx - m_cxScreenWidth, rc.bottom, PATCOPY);
    PatBlt(hdc, 0, boardy + 8 * cellsize, rc.right, rc.bottom - boardy + 8 * cellsize, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    ::DeleteObject(hBrush);

    HBITMAP hBmpPieces = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_CHESS_PIECES));
    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hBmpPieces);

    HBRUSH hbrWhite = ::CreateSolidBrush(COLOR_BOARD_WHITE);
    HBRUSH hbrBlack = ::CreateSolidBrush(COLOR_BOARD_BLACK);
    for (int yy = 0; yy < 8; yy++)
    {
        int celly = boardy + (7 - yy) * cellsize;

        for (int xx = 0; xx < 8; xx++)
        {
            int cellx = boardx + xx * cellsize;

            // Board cell background
            HBRUSH hbr = ((xx ^ yy) & 1) ? hbrWhite : hbrBlack;
            ::SelectObject(hdc, hbr);
            PatBlt(hdc, cellx, celly, cellsize, cellsize, PATCOPY);

            int index = yy * 8 + xx;
            int figure = m_arrScreen_BoardData[index];
            if (figure != 0)
            {
                int figurex = (6 - ((figure & 0xf) >> 1)) * cellsize;
                int figurey = (figure & 0x80) ? 0 : cellsize;
                ::TransparentBlt(hdc, cellx, celly, cellsize, cellsize, hdcMem, figurex, figurey, cellsize, cellsize, RGB(128, 128, 128));

                //PrintHexValue(buffer, figure);
                //::DrawText(hdc, buffer + 2, 2, &rcText, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_BOTTOM);
            }
        }
    }
    ::SelectObject(hdc, hOldBrush);
    VERIFY(::DeleteObject(hbrWhite));
    VERIFY(::DeleteObject(hbrBlack));

    ::SelectObject(hdcMem, hOldBitmap);
    VERIFY(::DeleteDC(hdcMem));
    VERIFY(::DeleteObject(hBmpPieces));

    HFONT hfont = CreateDialogFont();
    HGDIOBJ hOldFont = ::SelectObject(hdc, hfont);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, COLOR_BOARD_TEXT);
    RECT rcText;
    TCHAR buffer[5];
    // Row numbers
    for (int yy = 0; yy < 8; yy++)
    {
        buffer[0] = _T('1') + yy;
        buffer[1] = 0;
        rcText.top = boardy + (7 - yy) * cellsize;
        rcText.bottom = rcText.top + cellsize;
        rcText.right = boardx - 4;
        rcText.left = rcText.right - cellsize;
        ::DrawText(hdc, buffer, 1, &rcText, DT_NOPREFIX | DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    }
    // Column letters
    for (int xx = 0; xx < 8; xx++)
    {
        buffer[0] = _T('a') + xx;
        buffer[1] = 0;
        rcText.left = boardx + xx * cellsize;
        rcText.right = rcText.left + cellsize;
        //rcText.bottom = boardy - 4;
        //rcText.top = rcText.bottom - cellsize;
        //::DrawText(hdc, buffer, 1, &rcText, DT_NOPREFIX | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
        rcText.top = boardy + 8 * cellsize + 4;
        rcText.bottom = rcText.top + cellsize;
        ::DrawText(hdc, buffer, 1, &rcText, DT_NOPREFIX | DT_SINGLELINE | DT_CENTER | DT_TOP);
    }
    ::SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hfont));
}

// Зная где лежат данные шахматной доски, отдаем номер фигуры для изображения
BYTE GetChessFugure(int row, int col)
{
    uint16_t baseaddr = 000660;
    if (g_nEmulatorConfiguration == EMU_CONF_IM01T) baseaddr = 000610;
    else if (g_nEmulatorConfiguration == EMU_CONF_IM05) baseaddr = 000670;

    uint16_t addr = baseaddr + (7 - row) * 8 + (7 - col);

    BYTE figure = g_pBoard->GetRAMByte(0, addr);

    bool isblack = (figure & 0200) == 0;
    switch (figure & 0177)
    {
    case 002: figure = 0x02; break;  // пешка
    case 004: figure = 0x04; break;  // конь
    case 006: figure = 0x06; break;  // слон
    case 010: figure = 0x08; break;  // ладья
    case 012: figure = 0x0a; break;  // ферзь
    case 014: figure = 0x0c; break;  // король
    default: figure = 0;
    }
    figure |= isblack ? 0x80 : 0;

    return figure;
};

void ScreenView_UpdateScreen()
{
    bool okChanged = false;
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            WORD index = row * 8 + col;
            BYTE oldfigure = m_arrScreen_BoardData[index];
            BYTE figure = GetChessFugure(row, col);
            okChanged |= (oldfigure != figure);
            m_arrScreen_BoardData[index] = figure;
        }
    }

    if (okChanged)
    {
        HDC hdc = ::GetDC(g_hwndScreen);
        ScreenView_OnDraw(hdc);
        ::ReleaseDC(g_hwndScreen, hdc);
    }
}

void ScreenView_RedrawScreen()
{
    ScreenView_UpdateScreen();
}


//////////////////////////////////////////////////////////////////////
