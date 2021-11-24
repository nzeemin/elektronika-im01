/*  This file is part of ELEKTRONIKA-IM01.
    ELEKTRONIKA-IM01 is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    ELEKTRONIKA-IM01 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
ELEKTRONIKA-IM01. If not, see <http://www.gnu.org/licenses/>. */

// KeyboardView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"

//////////////////////////////////////////////////////////////////////


#define COLOR_BK_BACKGROUND     RGB(255,255,233)
#define COLOR_BK_PANEL          RGB(203,207,207)
#define COLOR_BK_INDICATOR      RGB(0,124,14)
#define COLOR_KEYBOARD_DIVIDER  RGB(184,169,123)
#define COLOR_KEYBOARD_GREEN    RGB(80,255,80)


HWND g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE m_nKeyboardKeyPressed = 0;  // BK scan-code for the key pressed, or 0
BYTE m_arrKeyboardSegmentsData[4] = { 0, 0156, 0354, 0 };

void KeyboardView_OnDraw(HDC hdc);
int KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////

const int KEYBOARD_KEYS_ARRAY_WIDTH = 5;

// Keyboard key mapping to bitmap for BK0010-01 keyboard
const WORD m_arrKeyboardKeys[] =
{
    /* x1, y1   w, h      code */
    58,  144,  28, 23,    0001, // h 8
    96,  144,  28, 23,    0003, // Фиг
    134, 144,  28, 23,    0213, // НП
    58,  188,  28, 23,    0026, // g 7
    96,  188,  28, 23,    0027, // И
    134, 188,  28, 23,    0202, // СД
    58,  232,  28, 23,    0204, // f 6
    96,  232,  28, 23,    0200, // Empty circle
    134, 232,  28, 23,    0014, // Black circle
    58,  276,  28, 23,    0007, // e 5
    96,  276,  28, 23,    0006, // РЗ
    134, 276,  28, 23,    0073, // Three lines
    58,  320,  28, 23,    0061, // d 4
    96,  320,  28, 23,    0062, // N
    134, 320,  28, 23,    0063, // Arrow back
    58,  364,  28, 23,    0064, // c 3
    96,  364,  28, 23,    0065, // Вар
    134, 364,  28, 23,    0066, // ПХ
    58,  408,  28, 23,    0067, // b 2
    96,  408,  28, 23,    0070, // ?
    134, 408,  28, 23,    0071, // СИ
    58,  452,  28, 23,    0060, // a 1
    96,  452,  28, 23,    0055, // Arrow up
    134, 452,  28, 23,    0057, // Arrow down
};
const int m_nKeyboardKeysCount = sizeof(m_arrKeyboardKeys) / sizeof(WORD) / KEYBOARD_KEYS_ARRAY_WIDTH;

struct IndicatorSegment
{
    int x0, y0, x1, y1, x2, y2, x3, y3;
}
m_arrIndicatorSegments[] =
{
    {  2, 14,  3, 12, 14, 12, 13, 14 }, // middle
    {  2,  0,  4,  2,  3, 11,  1, 13 }, // left-top
    {  0, 26,  2, 13,  4, 15,  3, 24 }, // left-bottom
    {  0, 26,  2, 24, 11, 24, 13, 26 }, // bottom
    { 14, 13, 13, 26, 11, 24, 12, 15 }, // right-bottom
    { 15,  0, 14, 13, 13, 11, 13,  2 }, // right-top
    {  3,  0, 15,  0, 13,  2,  5,  2 }, // top
};
const int m_nKeyboardSegmentsCount = sizeof(m_arrIndicatorSegments) / sizeof(IndicatorSegment);


//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = KeyboardViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Init()
{
}

void KeyboardView_Done()
{
}

void KeyboardView_Update()
{
    ::InvalidateRect(g_hwndKeyboard, NULL, TRUE);
}

void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndKeyboard = CreateWindow(
            CLASSNAME_KEYBOARDVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK KeyboardViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            KeyboardView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        {
            POINT ptCursor;  ::GetCursorPos(&ptCursor);
            ::ScreenToClient(g_hwndKeyboard, &ptCursor);
            int keyindex = KeyboardView_GetKeyByPoint(ptCursor.x, ptCursor.y);
            LPCTSTR cursor = (keyindex == -1) ? IDC_ARROW : IDC_HAND;
            ::SetCursor(::LoadCursor(NULL, cursor));
        }
        return (LRESULT)TRUE;
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            WORD fwkeys = (WORD)wParam;

            int keyindex = KeyboardView_GetKeyByPoint(x, y);
            if (keyindex == -1) break;
            BYTE keyscan = (BYTE) m_arrKeyboardKeys[keyindex * KEYBOARD_KEYS_ARRAY_WIDTH + 4];
            if (keyscan == 0) break;

            BOOL okShift = ((fwkeys & MK_SHIFT) != 0);
            if (okShift && keyscan >= 0100 && keyscan <= 0137)
                keyscan += 040;
            else if (okShift && keyscan >= 0060 && keyscan <= 0077)
                keyscan -= 020;

            // Fire keydown event and capture mouse
            Emulator_KeyEvent(keyscan, TRUE);
            ::SetCapture(g_hwndKeyboard);

            // Draw focus frame for the key pressed
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, keyscan);
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

            // Remember key pressed
            m_nKeyboardKeyPressed = keyscan;
        }
        break;
    case WM_LBUTTONUP:
        if (m_nKeyboardKeyPressed != 0)
        {
            // Fire keyup event and release mouse
            Emulator_KeyEvent(m_nKeyboardKeyPressed, FALSE);
            ::ReleaseCapture();

            // Draw focus frame for the released key
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

            m_nKeyboardKeyPressed = 0;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void Keyboard_DrawSegment(HDC hdc, IndicatorSegment* pSegment, int x, int y)
{
    POINT points[4];
    points[0].x = x + pSegment->x0;
    points[0].y = y + pSegment->y0;
    points[1].x = x + pSegment->x1;
    points[1].y = y + pSegment->y1;
    points[2].x = x + pSegment->x2;
    points[2].y = y + pSegment->y2;
    points[3].x = x + pSegment->x3;
    points[3].y = y + pSegment->y3;
    Polygon(hdc, points, 4);
}

void KeyboardView_OnDraw(HDC hdc)
{
    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    // Background
    HBRUSH hbrBackground = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hbrBackground);
    ::PatBlt(hdc, 0, 0, rc.right, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    VERIFY(::DeleteObject(hbrBackground));

    HPEN hpenDivider = ::CreatePen(PS_SOLID, 2, COLOR_KEYBOARD_DIVIDER);
    HGDIOBJ hOldPen = ::SelectObject(hdc, hpenDivider);
    ::MoveToEx(hdc, 1, 0, NULL);
    ::LineTo(hdc, 1, BK_SCREEN_HEIGHT);
    ::SelectObject(hdc, hOldPen);

    // Keyboard panel
    HBRUSH hbrBkPanel = ::CreateSolidBrush(COLOR_BK_PANEL);
    hOldBrush = ::SelectObject(hdc, hbrBkPanel);
    ::PatBlt(hdc, 14, 120, 168, 388, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    VERIFY(::DeleteObject(hbrBkPanel));

    // Indicator background
    HBRUSH hIndBrush = ::CreateSolidBrush(COLOR_BK_INDICATOR);
    hOldBrush = ::SelectObject(hdc, hIndBrush);
    ::PatBlt(hdc, 14, 0, 168, 120, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    VERIFY(::DeleteObject(hIndBrush));

    HBITMAP hBmpPanel = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_PANEL));
    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hBmpPanel);
    ::BitBlt(hdc, 26, 127, 141, 372, hdcMem, 0, 0, SRCCOPY);
    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::DeleteObject(hBmpPanel);

    if (m_nKeyboardKeyPressed != 0)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    ////DEBUG: Show key mappings
    //for (int i = 0; i < m_nKeyboardKeysCount; i++)
    //{
    //    RECT rcKey;
    //    rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
    //    rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
    //    rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
    //    rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];
    //    ::DrawFocusRect(hdc, &rcKey);
    //}

    // Draw segment indicators
    HBRUSH hbrGreen = ::CreateSolidBrush(COLOR_KEYBOARD_GREEN);
    HPEN hpenGreen = ::CreatePen(PS_SOLID, 1, COLOR_KEYBOARD_GREEN);
    hOldBrush = ::SelectObject(hdc, hbrGreen);
    hOldPen = ::SelectObject(hdc, hpenGreen);
    for (int ind = 0; ind < 4; ind++)
    {
        BYTE data = m_arrKeyboardSegmentsData[ind];
        int segmentsx = m_nKeyboardBitmapLeft + 54 + ind * 23;
        if (ind >= 2) segmentsx += 8;
        int segmentsy = m_nKeyboardBitmapTop + 50;
        for (int i = 0; i < m_nKeyboardSegmentsCount; i++)
        {
            if ((data & (1 << (i + 1))) == 0)
                continue;
            Keyboard_DrawSegment(hdc, m_arrIndicatorSegments + i, segmentsx, segmentsy);
        }
    }
    ::SelectObject(hdc, hOldPen);
    ::SelectObject(hdc, hOldBrush);

    ::DeleteObject(hbrGreen);
}

// Returns: index of key under the cursor position, or -1 if not found
int KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];

        if (x >= rcKey.left && x < rcKey.right && y >= rcKey.top && y < rcKey.bottom)
        {
            return i;
        }
    }
    return -1;
}

void Keyboard_DrawKey(HDC hdc, BYTE keyscan)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
        if (keyscan == m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 4])
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////
