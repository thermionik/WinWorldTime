#include <windows.h>
#include <time.h>
#include <string.h>
#include <algorithm>

HINSTANCE hInst;

#define TIMER_ID 1

typedef struct {
    const char* city;
    int utcOffsetHours;
    int utcOffsetMins;
    COLORREF color;
} CITY_TIMEZONE;

CITY_TIMEZONE cities[] = {
    {"Austin", -5, 0, RGB(255, 255, 255)},
    {"Tehran", 3, 30, RGB(255, 255, 0)},
    {"Beijing", 8, 0, RGB(255, 0, 0)},
    {"Pyongyang", 9, 0, RGB(0, 0, 255)},
    {"Moscow", 3, 0, RGB(0, 255, 0)}
};

const int cityCount = sizeof(cities) / sizeof(cities[0]);

const BYTE digitSegments[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
};

void DrawSegment(HDC hdc, int x, int y, int l, int t, BOOL vertical, COLORREF col) {
    POINT pts[6];
    if (!vertical) {
        pts[0] = (POINT){0, t / 2}; pts[1] = (POINT){t / 2, 0};
        pts[2] = (POINT){l - t / 2, 0}; pts[3] = (POINT){l, t / 2};
        pts[4] = (POINT){l - t / 2, t}; pts[5] = (POINT){t / 2, t};
    } else {
        pts[0] = (POINT){t / 2, 0}; pts[1] = (POINT){0, t / 2};
        pts[2] = (POINT){0, l - t / 2}; pts[3] = (POINT){t / 2, l};
        pts[4] = (POINT){t, l - t / 2}; pts[5] = (POINT){t, t / 2};
    }
    for (int i = 0; i < 6; ++i) { pts[i].x += x; pts[i].y += y; }
    POINT shadow[6];
    for (int i = 0; i < 6; ++i) {
        shadow[i].x = pts[i].x + 2;
        shadow[i].y = pts[i].y + 2;
    }
    COLORREF shadowColor = RGB(GetRValue(col) / 4, GetGValue(col) / 4, GetBValue(col) / 4);
    HBRUSH shadowBrush = CreateSolidBrush(shadowColor);
    HBRUSH old = (HBRUSH)SelectObject(hdc, shadowBrush);
    Polygon(hdc, shadow, 6);
    SelectObject(hdc, old);
    DeleteObject(shadowBrush);
    HBRUSH brush = CreateSolidBrush(col);
    old = (HBRUSH)SelectObject(hdc, brush);
    Polygon(hdc, pts, 6);
    SelectObject(hdc, old);
    DeleteObject(brush);
}

void DrawDigit(HDC hdc, int x, int y, int l, int t, int d, COLORREF col) {
    BYTE seg = digitSegments[d];
    int ht = t / 2;
    if (seg & 1) DrawSegment(hdc, x + ht, y, l, t, FALSE, col);
    if (seg & 2) DrawSegment(hdc, x + l + ht, y + t, l, t, TRUE, col);
    if (seg & 4) DrawSegment(hdc, x + l + ht, y + l + 2 * t, l, t, TRUE, col);
    if (seg & 8) DrawSegment(hdc, x + ht, y + 2 * l + 2 * t, l, t, FALSE, col);
    if (seg & 16) DrawSegment(hdc, x, y + l + 2 * t, l, t, TRUE, col);
    if (seg & 32) DrawSegment(hdc, x, y + t, l, t, TRUE, col);
    if (seg & 64) DrawSegment(hdc, x + ht, y + l + t, l, t, FALSE, col);
}

void DrawColon(HDC hdc, int x, int y, int s, COLORREF col) {
    int r = s / 3;
    HBRUSH brush = CreateSolidBrush(col);
    HBRUSH old = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, x - r, y - s, x + r, y - s + 2 * r);
    Ellipse(hdc, x - r, y + s, x + r, y + s + 2 * r);
    SelectObject(hdc, old);
    DeleteObject(brush);
}

void GetCityTime(const CITY_TIMEZONE* city, SYSTEMTIME* stOut) {
    SYSTEMTIME utc; GetSystemTime(&utc);
    FILETIME ft; SystemTimeToFileTime(&utc, &ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart += ((LONGLONG)city->utcOffsetHours * 3600 + city->utcOffsetMins * 60) * 10000000LL;
    ft.dwLowDateTime = uli.LowPart; ft.dwHighDateTime = uli.HighPart;
    FileTimeToSystemTime(&ft, stOut);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hwnd, TIMER_ID, 1000, NULL);
            return 0;
        case WM_TIMER:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, CreateSolidBrush(RGB(0,0,0)));

            int padding = 20;
            int availableHeight = rc.bottom - rc.top - (cityCount + 1) * padding;
            int clockHeight = availableHeight / cityCount;
            int segThickness = std::max(2, clockHeight / 15);
            int segLength = clockHeight / 5;
            int digitWidth = segLength + 2 * segThickness;
            int digitHeight = 2 * segLength + 3 * segThickness;
            int colonWidth = segThickness * 3;
            int totalClockWidth = digitWidth * 6 + colonWidth * 2 + segThickness * 6;
            int startX = (rc.right - totalClockWidth) / 2;

            for (int i = 0; i < cityCount; i++) {
                SYSTEMTIME st;
                GetCityTime(&cities[i], &st);
                int baseY = rc.top + padding + i * (clockHeight + padding);

                RECT box = {startX - 10, baseY - 10, startX + totalClockWidth + 10, baseY + digitHeight + 30};
                FrameRect(hdc, &box, (HBRUSH)GetStockObject(WHITE_BRUSH));

                int hT = st.wHour / 10, hO = st.wHour % 10;
                int mT = st.wMinute / 10, mO = st.wMinute % 10;
                int sT = st.wSecond / 10, sO = st.wSecond % 10;

                int x = startX;
                DrawDigit(hdc, x, baseY, segLength, segThickness, hT, cities[i].color); x += digitWidth + segThickness;
                DrawDigit(hdc, x, baseY, segLength, segThickness, hO, cities[i].color); x += digitWidth + segThickness;
                DrawColon(hdc, x + segThickness, baseY + digitHeight / 2 - 8, segThickness, cities[i].color); x += colonWidth + segThickness;
                DrawDigit(hdc, x, baseY, segLength, segThickness, mT, cities[i].color); x += digitWidth + segThickness;
                DrawDigit(hdc, x, baseY, segLength, segThickness, mO, cities[i].color); x += digitWidth + segThickness;
                DrawColon(hdc, x + segThickness, baseY + digitHeight / 2 - 8, segThickness, cities[i].color); x += colonWidth + segThickness;
                DrawDigit(hdc, x, baseY, segLength, segThickness, sT, cities[i].color); x += digitWidth + segThickness;
                DrawDigit(hdc, x, baseY, segLength, segThickness, sO, cities[i].color);

                HFONT font = CreateFontA(segLength, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Courier New");
                HFONT oldFont = (HFONT)SelectObject(hdc, font);
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                SIZE sz;
                GetTextExtentPoint32A(hdc, cities[i].city, (int)strlen(cities[i].city), &sz);
                TextOutA(hdc, startX + (totalClockWidth - sz.cx) / 2, baseY + digitHeight + 5, cities[i].city, (int)strlen(cities[i].city));
                SelectObject(hdc, oldFont);
                DeleteObject(font);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "SevenSegmentClockClass";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return -1;

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "World Clock - Vertical Stack",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 800,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return -1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
