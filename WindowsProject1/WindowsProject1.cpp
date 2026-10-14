/*
  ====================================================
   SNAKE GAME - DEMON EDITION
   C++ Windows API - Fullscreen - Creative
  ====================================================
  Controls:
    SPACE       = Start / Return to Menu
    Arrow Keys  = Move
    P           = Pause
    R           = Restart
    ESC         = Quit
  ====================================================
  Compile:
    g++ -o SnakeGame SnakeGame.cpp -lgdi32 -luser32 -mwindows
  Or use Visual Studio / Code::Blocks (Win32 Desktop App)
  ====================================================
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

// ═══════════════════════════════════════════════
//  Screen & Grid (calculated at runtime)
// ═══════════════════════════════════════════════
int SCREEN_W = 0;
int SCREEN_H = 0;
int CELL_SIZE = 28;
int GRID_COLS = 0;
int GRID_ROWS = 0;
int HUD_H = 50;

// ═══════════════════════════════════════════════
//  Game Constants
// ═══════════════════════════════════════════════
const int TIMER_GAME = 1;
const int TIMER_ANIM = 2;
const int BASE_SPEED = 140;

// ═══════════════════════════════════════════════
//  Types
// ═══════════════════════════════════════════════
enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum GameState { GS_START, GS_PLAYING, GS_PAUSED, GS_GAMEOVER };

struct Point2D { int x, y; };

// ═══════════════════════════════════════════════
//  Global Game State
// ═══════════════════════════════════════════════
std::vector<Point2D> snake;
int        snakeLen = 5;
Direction  dir = DIR_RIGHT;
Direction  nextDir = DIR_RIGHT;
Point2D    foodPos = { 5,5 };
int        score = 0;
int        highScore = 0;
GameState  gameState = GS_START;
int        animTick = 0;
int        gameSpeed = BASE_SPEED;
bool       deathAnim = false;
int        deathTick = 0;

struct Particle {
    float x, y, vx, vy;
    int   life, maxLife;
    COLORREF color;
};
std::vector<Particle> particles;

HWND    hWnd = nullptr;
HDC     hdcBuf = nullptr;
HBITMAP hbmBuf = nullptr;
HBITMAP hbmOld = nullptr;

// ═══════════════════════════════════════════════
//  Forward Declarations
// ═══════════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitGame();
void PlaceFood();
bool IsOnSnake(int x, int y);
void MoveSnake();
void CheckCollision();
void SpawnParticles(int gx, int gy);
void UpdateParticles();

void DrawFrame(HDC hdc);
void DrawStartScreen(HDC hdc);
void DrawPlayField(HDC hdc);
void DrawHUD(HDC hdc);
void DrawSnakeBody(HDC hdc);
void DrawSnakeHead(HDC hdc);
void DrawFood(HDC hdc);
void DrawParticles(HDC hdc);
void DrawGameOver(HDC hdc);
void DrawPaused(HDC hdc);
void DrawDeathEffect(HDC hdc);

HFONT MakeFont(int size, bool bold, const wchar_t* name = L"Impact");
void  GlowText(HDC hdc, const std::wstring& t, int x, int y,
    COLORREF main, COLORREF glow, int sz);
void  CenterText(HDC hdc, const std::wstring& t, int y,
    int sz, COLORREF c, bool bold = true,
    const wchar_t* face = L"Impact");
void  FillCell(HDC hdc, int gx, int gy, COLORREF c, int shrink = 2);

// ═══════════════════════════════════════════════
//  WinMain
// ═══════════════════════════════════════════════
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    srand((unsigned)time(nullptr));

    SCREEN_W = GetSystemMetrics(SM_CXSCREEN);
    SCREEN_H = GetSystemMetrics(SM_CYSCREEN);
    GRID_COLS = SCREEN_W / CELL_SIZE;
    GRID_ROWS = (SCREEN_H - HUD_H) / CELL_SIZE;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SnakeDemon";
    RegisterClassEx(&wc);

    hWnd = CreateWindowEx(
        WS_EX_TOPMOST,
        L"SnakeDemon",
        L"SNAKE - DEMON EDITION",
        WS_POPUP,
        0, 0,
        SCREEN_W, SCREEN_H,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ═══════════════════════════════════════════════
//  WndProc
// ═══════════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HDC hdc = GetDC(hwnd);
        hdcBuf = CreateCompatibleDC(hdc);
        hbmBuf = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
        hbmOld = (HBITMAP)SelectObject(hdcBuf, hbmBuf);
        ReleaseDC(hwnd, hdc);
        InitGame();
        SetTimer(hwnd, TIMER_ANIM, 50, nullptr);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_GAME);
        KillTimer(hwnd, TIMER_ANIM);
        SelectObject(hdcBuf, hbmOld);
        DeleteObject(hbmBuf);
        DeleteDC(hdcBuf);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wp == TIMER_ANIM)
        {
            ++animTick;
            UpdateParticles();
            if (deathAnim) { ++deathTick; if (deathTick > 30) deathAnim = false; }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else if (wp == TIMER_GAME && gameState == GS_PLAYING)
        {
            MoveSnake();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawFrame(hdcBuf);
        BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, hdcBuf, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
        switch (gameState)
        {
        case GS_START:
            if (wp == VK_SPACE) {
                InitGame(); gameState = GS_PLAYING;
                SetTimer(hwnd, TIMER_GAME, gameSpeed, nullptr);
            }
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            break;

        case GS_PLAYING:
            switch (wp) {
            case VK_UP:    nextDir = DIR_UP;    break;
            case VK_DOWN:  nextDir = DIR_DOWN;  break;
            case VK_LEFT:  nextDir = DIR_LEFT;  break;
            case VK_RIGHT: nextDir = DIR_RIGHT; break;
            case 'P': gameState = GS_PAUSED;
                KillTimer(hwnd, TIMER_GAME); break;
            case 'R': KillTimer(hwnd, TIMER_GAME);
                InitGame(); gameState = GS_PLAYING;
                SetTimer(hwnd, TIMER_GAME, gameSpeed, nullptr); break;
            case VK_ESCAPE: DestroyWindow(hwnd); break;
            }
            break;

        case GS_PAUSED:
            if (wp == 'P' || wp == VK_SPACE) {
                gameState = GS_PLAYING;
                SetTimer(hwnd, TIMER_GAME, gameSpeed, nullptr);
            }
            if (wp == 'R') {
                KillTimer(hwnd, TIMER_GAME);
                InitGame(); gameState = GS_PLAYING;
                SetTimer(hwnd, TIMER_GAME, gameSpeed, nullptr);
            }
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            break;

        case GS_GAMEOVER:
            if (wp == 'R') {
                InitGame(); gameState = GS_PLAYING;
                SetTimer(hwnd, TIMER_GAME, gameSpeed, nullptr);
            }
            if (wp == VK_SPACE) { InitGame(); gameState = GS_START; }
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            break;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ═══════════════════════════════════════════════
//  Game Logic
// ═══════════════════════════════════════════════
void InitGame()
{
    snakeLen = 5;
    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    score = 0;
    gameSpeed = BASE_SPEED;
    deathAnim = false;
    deathTick = 0;
    particles.clear();

    snake.assign(GRID_COLS * GRID_ROWS, { 0,0 });
    int mx = GRID_COLS / 2, my = GRID_ROWS / 2;
    for (int i = 0; i < snakeLen; ++i) { snake[i].x = mx - i; snake[i].y = my; }
    PlaceFood();
}

void PlaceFood()
{
    do { foodPos.x = rand() % GRID_COLS; foodPos.y = rand() % GRID_ROWS; } while (IsOnSnake(foodPos.x, foodPos.y));
}

bool IsOnSnake(int x, int y)
{
    for (int i = 0; i < snakeLen; ++i)
        if (snake[i].x == x && snake[i].y == y) return true;
    return false;
}

void SpawnParticles(int gx, int gy)
{
    float cx = (float)(gx * CELL_SIZE + CELL_SIZE / 2);
    float cy = (float)(gy * CELL_SIZE + CELL_SIZE / 2 + HUD_H);
    COLORREF cols[] = { RGB(255,0,0),RGB(255,100,0),RGB(255,200,0),RGB(200,0,0) };
    for (int i = 0; i < 18; ++i) {
        float angle = (float)(rand() % 360) * 3.14159f / 180.f;
        float speed = 2.0f + (rand() % 5);
        Particle p;
        p.x = cx; p.y = cy;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life = p.maxLife = 20 + rand() % 15;
        p.color = cols[rand() % 4];
        particles.push_back(p);
    }
}

void UpdateParticles()
{
    for (auto& p : particles) { p.x += p.vx; p.y += p.vy; p.vy += 0.3f; --p.life; }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) {return p.life <= 0; }),
        particles.end());
}

void MoveSnake()
{
    bool rev =
        (nextDir == DIR_UP && dir == DIR_DOWN) ||
        (nextDir == DIR_DOWN && dir == DIR_UP) ||
        (nextDir == DIR_LEFT && dir == DIR_RIGHT) ||
        (nextDir == DIR_RIGHT && dir == DIR_LEFT);
    if (!rev) dir = nextDir;

    Point2D nh = snake[0];
    switch (dir) {
    case DIR_UP:    --nh.y; break;
    case DIR_DOWN:  ++nh.y; break;
    case DIR_LEFT:  --nh.x; break;
    case DIR_RIGHT: ++nh.x; break;
    }
    for (int i = snakeLen - 1; i > 0; --i) snake[i] = snake[i - 1];
    snake[0] = nh;
    CheckCollision();
}

void CheckCollision()
{
    auto& h = snake[0];
    if (h.x < 0 || h.x >= GRID_COLS || h.y < 0 || h.y >= GRID_ROWS) {
        gameState = GS_GAMEOVER; deathAnim = true; deathTick = 0;
        KillTimer(hWnd, TIMER_GAME);
        if (score > highScore) highScore = score;
        return;
    }
    for (int i = 1; i < snakeLen; ++i)
        if (snake[i].x == h.x && snake[i].y == h.y) {
            gameState = GS_GAMEOVER; deathAnim = true; deathTick = 0;
            KillTimer(hWnd, TIMER_GAME);
            if (score > highScore) highScore = score;
            return;
        }
    if (h.x == foodPos.x && h.y == foodPos.y) {
        score += 10; ++snakeLen;
        SpawnParticles(foodPos.x, foodPos.y);
        PlaceFood();
        if (score % 50 == 0 && gameSpeed > 55) {
            gameSpeed -= 8;
            KillTimer(hWnd, TIMER_GAME);
            SetTimer(hWnd, TIMER_GAME, gameSpeed, nullptr);
        }
    }
}

// ═══════════════════════════════════════════════
//  Drawing Helpers
// ═══════════════════════════════════════════════
HFONT MakeFont(int size, bool bold, const wchar_t* name)
{
    return CreateFont(
        -MulDiv(size, GetDeviceCaps(hdcBuf, LOGPIXELSY), 72),
        0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, name);
}

void GlowText(HDC hdc, const std::wstring& t, int x, int y,
    COLORREF main, COLORREF glow, int sz)
{
    HFONT f = (HFONT)SelectObject(hdc, MakeFont(sz, true));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, glow);
    for (int o = 1; o <= 4; ++o) {
        TextOut(hdc, x - o, y, t.c_str(), (int)t.size());
        TextOut(hdc, x + o, y, t.c_str(), (int)t.size());
        TextOut(hdc, x, y - o, t.c_str(), (int)t.size());
        TextOut(hdc, x, y + o, t.c_str(), (int)t.size());
        TextOut(hdc, x - o, y - o, t.c_str(), (int)t.size());
        TextOut(hdc, x + o, y + o, t.c_str(), (int)t.size());
        TextOut(hdc, x - o, y + o, t.c_str(), (int)t.size());
        TextOut(hdc, x + o, y - o, t.c_str(), (int)t.size());
    }
    SetTextColor(hdc, main);
    TextOut(hdc, x, y, t.c_str(), (int)t.size());
    DeleteObject(SelectObject(hdc, f));
}

void CenterText(HDC hdc, const std::wstring& t, int y, int sz,
    COLORREF c, bool bold, const wchar_t* face)
{
    HFONT f = MakeFont(sz, bold, face);
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, c);
    SIZE s; GetTextExtentPoint32(hdc, t.c_str(), (int)t.size(), &s);
    TextOut(hdc, (SCREEN_W - s.cx) / 2, y, t.c_str(), (int)t.size());
    SelectObject(hdc, old); DeleteObject(f);
}

void FillCell(HDC hdc, int gx, int gy, COLORREF c, int shrink)
{
    int x = gx * CELL_SIZE + shrink;
    int y = gy * CELL_SIZE + HUD_H + shrink;
    RECT r = { x,y,x + CELL_SIZE - shrink * 2,y + CELL_SIZE - shrink * 2 };
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

void DrawScaleCell(HDC hdc, int gx, int gy, COLORREF body, COLORREF shine)
{
    int x = gx * CELL_SIZE + 2;
    int y = gy * CELL_SIZE + HUD_H + 2;
    int w = CELL_SIZE - 4, h = CELL_SIZE - 4;

    HBRUSH br = CreateSolidBrush(body);
    HPEN   pn = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN   op = (HPEN)SelectObject(hdc, pn);
    RoundRect(hdc, x, y, x + w, y + h, 8, 8);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(br); DeleteObject(pn);

    HBRUSH sh = CreateSolidBrush(shine);
    ob = (HBRUSH)SelectObject(hdc, sh);
    pn = CreatePen(PS_NULL, 0, 0);
    op = (HPEN)SelectObject(hdc, pn);
    Ellipse(hdc, x + 3, y + 3, x + 8, y + 8);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(sh); DeleteObject(pn);
}

// ═══════════════════════════════════════════════
//  Draw Snake
// ═══════════════════════════════════════════════
void DrawSnakeBody(HDC hdc)
{
    for (int i = snakeLen - 1; i >= 1; --i)
    {
        float t = (float)i / (float)snakeLen;
        int   r = 0;
        int   g = (int)(80 + (1.0f - t) * 175);
        int   b = (int)(t * 40);
        COLORREF body = RGB(r, g, b);
        COLORREF shine = RGB(r, min(255, g + 60), b);

        if ((snake[i].x + snake[i].y) % 2 == 0)
            body = RGB(r, max(40, g - 30), b);

        DrawScaleCell(hdc, snake[i].x, snake[i].y, body, shine);
    }
}

void DrawSnakeHead(HDC hdc)
{
    if (snakeLen < 1) return;
    int gx = snake[0].x, gy = snake[0].y;
    int x = gx * CELL_SIZE + 2;
    int y = gy * CELL_SIZE + HUD_H + 2;
    int w = CELL_SIZE - 4, h = CELL_SIZE - 4;

    HBRUSH br = CreateSolidBrush(RGB(20, 160, 20));
    HPEN   pn = CreatePen(PS_SOLID, 2, RGB(0, 80, 0));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN   op = (HPEN)SelectObject(hdc, pn);
    RoundRect(hdc, x, y, x + w, y + h, 6, 6);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(br); DeleteObject(pn);

    int eyeSize = max(4, CELL_SIZE / 5);
    int eyeOff = CELL_SIZE / 4;
    int eyeY = y + h / 3;
    int ex1 = x + eyeOff, ex2 = x + w - eyeOff - eyeSize;

    int eox = 0, eoy = 0;
    switch (dir) {
    case DIR_RIGHT: eox = w / 3;  eoy = 0;    break;
    case DIR_LEFT:  eox = -w / 3; eoy = 0;    break;
    case DIR_UP:    eox = 0;    eoy = -h / 3; break;
    case DIR_DOWN:  eox = 0;    eoy = h / 3;  break;
    }

    HBRUSH wh = CreateSolidBrush(RGB(255, 255, 200));
    HPEN bp = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    ob = (HBRUSH)SelectObject(hdc, wh);
    op = (HPEN)SelectObject(hdc, bp);
    Ellipse(hdc, ex1 + eox, eyeY + eoy, ex1 + eox + eyeSize + 2, eyeY + eoy + eyeSize + 2);
    Ellipse(hdc, ex2 + eox, eyeY + eoy, ex2 + eox + eyeSize + 2, eyeY + eoy + eyeSize + 2);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(wh); DeleteObject(bp);

    int ps = max(2, eyeSize / 2);
    HBRUSH rd = CreateSolidBrush(RGB(255, 0, 0));
    HPEN np = CreatePen(PS_NULL, 0, 0);
    ob = (HBRUSH)SelectObject(hdc, rd);
    op = (HPEN)SelectObject(hdc, np);
    Ellipse(hdc, ex1 + eox + 1, eyeY + eoy + 1, ex1 + eox + ps + 2, eyeY + eoy + ps + 2);
    Ellipse(hdc, ex2 + eox + 1, eyeY + eoy + 1, ex2 + eox + ps + 2, eyeY + eoy + ps + 2);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(rd); DeleteObject(np);

    HPEN tongue = CreatePen(PS_SOLID, 2, RGB(220, 0, 60));
    op = (HPEN)SelectObject(hdc, tongue);
    int tx = x + w / 2, ty = y + h / 2;
    switch (dir) {
    case DIR_RIGHT:
        MoveToEx(hdc, x + w, ty, nullptr); LineTo(hdc, x + w + 8, ty);
        LineTo(hdc, x + w + 12, ty - 4);
        MoveToEx(hdc, x + w + 8, ty, nullptr); LineTo(hdc, x + w + 12, ty + 4);
        break;
    case DIR_LEFT:
        MoveToEx(hdc, x, ty, nullptr); LineTo(hdc, x - 8, ty);
        LineTo(hdc, x - 12, ty - 4);
        MoveToEx(hdc, x - 8, ty, nullptr); LineTo(hdc, x - 12, ty + 4);
        break;
    case DIR_UP:
        MoveToEx(hdc, tx, y, nullptr); LineTo(hdc, tx, y - 8);
        LineTo(hdc, tx - 4, y - 12);
        MoveToEx(hdc, tx, y - 8, nullptr); LineTo(hdc, tx + 4, y - 12);
        break;
    case DIR_DOWN:
        MoveToEx(hdc, tx, y + h, nullptr); LineTo(hdc, tx, y + h + 8);
        LineTo(hdc, tx - 4, y + h + 12);
        MoveToEx(hdc, tx, y + h + 8, nullptr); LineTo(hdc, tx + 4, y + h + 12);
        break;
    }
    SelectObject(hdc, op); DeleteObject(tongue);
}

// ═══════════════════════════════════════════════
//  Draw Food
// ═══════════════════════════════════════════════
void DrawFood(HDC hdc)
{
    int cx = foodPos.x * CELL_SIZE + CELL_SIZE / 2;
    int cy = foodPos.y * CELL_SIZE + CELL_SIZE / 2 + HUD_H;
    int r = CELL_SIZE / 2 - 3;

    int pulse = (int)(3 * sin(animTick * 0.3f));

    HBRUSH gb = CreateSolidBrush(RGB(80, 0, 0));
    HPEN   gp = CreatePen(PS_NULL, 0, 0);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, gb);
    HPEN   op = (HPEN)SelectObject(hdc, gp);
    Ellipse(hdc, cx - r - 4 + pulse, cy - r - 4 + pulse, cx + r + 4 - pulse, cy + r + 4 - pulse);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(gb); DeleteObject(gp);

    HBRUSH rb = CreateSolidBrush(RGB(220, 20, 20));
    HPEN   rp = CreatePen(PS_SOLID, 1, RGB(140, 0, 0));
    ob = (HBRUSH)SelectObject(hdc, rb);
    op = (HPEN)SelectObject(hdc, rp);
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(rb); DeleteObject(rp);

    HBRUSH sh = CreateSolidBrush(RGB(255, 120, 120));
    ob = (HBRUSH)SelectObject(hdc, sh);
    gp = CreatePen(PS_NULL, 0, 0);
    op = (HPEN)SelectObject(hdc, gp);
    Ellipse(hdc, cx - r / 2, cy - r / 2, cx - r / 5, cy - r / 5);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(sh); DeleteObject(gp);
}

// ═══════════════════════════════════════════════
//  Draw Particles
// ═══════════════════════════════════════════════
void DrawParticles(HDC hdc)
{
    for (auto& p : particles)
    {
        float alpha = (float)p.life / (float)p.maxLife;
        int sz = max(2, (int)(5 * alpha));
        HBRUSH br = CreateSolidBrush(p.color);
        HPEN   pn = CreatePen(PS_NULL, 0, 0);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
        HPEN   op = (HPEN)SelectObject(hdc, pn);
        Ellipse(hdc, (int)p.x - sz, (int)p.y - sz, (int)p.x + sz, (int)p.y + sz);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(br); DeleteObject(pn);
    }
}

// ═══════════════════════════════════════════════
//  Draw HUD
// ═══════════════════════════════════════════════
void DrawHUD(HDC hdc)
{
    RECT bar = { 0,0,SCREEN_W,HUD_H };
    HBRUSH br = CreateSolidBrush(RGB(10, 10, 10));
    FillRect(hdc, &bar, br); DeleteObject(br);

    HPEN ln = CreatePen(PS_SOLID, 2, RGB(0, 200, 0));
    HPEN op = (HPEN)SelectObject(hdc, ln);
    MoveToEx(hdc, 0, HUD_H - 1, nullptr); LineTo(hdc, SCREEN_W, HUD_H - 1);
    SelectObject(hdc, op); DeleteObject(ln);

    std::wstring sc = L"  SCORE: " + std::to_wstring(score) +
        L"      BEST: " + std::to_wstring(highScore) +
        L"      [P] Pause    [R] Restart    [ESC] Quit";

    HFONT f = MakeFont(14, true, L"Consolas");
    HFONT of = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 80));
    TextOut(hdc, 10, 14, sc.c_str(), (int)sc.size());
    SelectObject(hdc, of); DeleteObject(f);
}

// ═══════════════════════════════════════════════
//  Draw Background Grid
// ═══════════════════════════════════════════════
void DrawPlayField(HDC hdc)
{
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(18, 18, 18));
    HPEN op = (HPEN)SelectObject(hdc, pen);
    for (int i = 0; i <= GRID_COLS; ++i) {
        MoveToEx(hdc, i * CELL_SIZE, HUD_H, nullptr);
        LineTo(hdc, i * CELL_SIZE, SCREEN_H);
    }
    for (int i = 0; i <= GRID_ROWS; ++i) {
        MoveToEx(hdc, 0, i * CELL_SIZE + HUD_H, nullptr);
        LineTo(hdc, SCREEN_W, i * CELL_SIZE + HUD_H);
    }
    SelectObject(hdc, op); DeleteObject(pen);
}

// ═══════════════════════════════════════════════
//  Death Effect
// ═══════════════════════════════════════════════
void DrawDeathEffect(HDC hdc)
{
    if (!deathAnim) return;
    float alpha = (float)(30 - deathTick) / 30.0f;
    if (alpha <= 0) return;

    HBRUSH rb = CreateSolidBrush(RGB(180, 0, 0));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, rb);

    for (int i = 0; i < (int)(alpha * 5); ++i)
        PatBlt(hdc, 0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, PATINVERT);

    SelectObject(hdc, ob); DeleteObject(rb);
}

// ═══════════════════════════════════════════════
//  Start Screen
// ═══════════════════════════════════════════════
void DrawStartScreen(HDC hdc)
{
    RECT bg = { 0,0,SCREEN_W,SCREEN_H };
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &bg, br); DeleteObject(br);

    // Animated dot grid
    for (int gx = 0; gx < GRID_COLS; gx += 3)
        for (int gy = 0; gy < GRID_ROWS; gy += 3)
        {
            float wave = sinf((gx + gy + animTick) * 0.15f);
            int   brightness = (int)(8 + wave * 6);
            HBRUSH db = CreateSolidBrush(RGB(0, brightness, 0));
            HPEN   np = CreatePen(PS_NULL, 0, 0);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, db);
            HPEN   op = (HPEN)SelectObject(hdc, np);
            int px = gx * CELL_SIZE + CELL_SIZE / 2;
            int py = gy * CELL_SIZE + CELL_SIZE / 2;
            Ellipse(hdc, px - 2, py - 2, px + 2, py + 2);
            SelectObject(hdc, ob); SelectObject(hdc, op);
            DeleteObject(db); DeleteObject(np);
        }

    // Animated snake strips
    COLORREF greens[] = { RGB(0,80,0),RGB(0,120,0),RGB(0,160,0),
                       RGB(0,200,0),RGB(0,255,0) };
    int offset = animTick % GRID_COLS;
    for (int i = 0; i < GRID_COLS; ++i) {
        int idx = (i + offset) % 5;
        int bx = i * CELL_SIZE + 2, by = 8;
        HBRUSH cb = CreateSolidBrush(greens[idx]);
        HPEN   np = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        HBRUSH ob = (HBRUSH)SelectObject(hdc, cb);
        HPEN   op = (HPEN)SelectObject(hdc, np);
        RoundRect(hdc, bx, by, bx + CELL_SIZE - 4, by + CELL_SIZE - 4, 6, 6);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(cb); DeleteObject(np);
    }
    for (int i = 0; i < GRID_COLS; ++i) {
        int idx = (GRID_COLS - 1 - i + offset) % 5;
        int bx = i * CELL_SIZE + 2, by = SCREEN_H - CELL_SIZE - 8;
        HBRUSH cb = CreateSolidBrush(greens[idx]);
        HPEN   np = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        HBRUSH ob = (HBRUSH)SelectObject(hdc, cb);
        HPEN   op = (HPEN)SelectObject(hdc, np);
        RoundRect(hdc, bx, by, bx + CELL_SIZE - 4, by + CELL_SIZE - 4, 6, 6);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(cb); DeleteObject(np);
    }

    // Title SNAKE
    int titleY = SCREEN_H / 2 - 160;
    std::wstring title = L"SNAKE";
    HFONT bigF = MakeFont(110, true, L"Impact");
    HFONT of = (HFONT)SelectObject(hdc, bigF);
    SIZE sz; GetTextExtentPoint32(hdc, title.c_str(), (int)title.size(), &sz);
    int tx = (SCREEN_W - sz.cx) / 2;
    SelectObject(hdc, of); DeleteObject(bigF);

    SetBkMode(hdc, TRANSPARENT);
    HFONT sf = MakeFont(110, true, L"Impact");
    of = (HFONT)SelectObject(hdc, sf);
    SetTextColor(hdc, RGB(0, 40, 0));
    TextOut(hdc, tx + 8, titleY + 8, title.c_str(), (int)title.size());
    SelectObject(hdc, of); DeleteObject(sf);

    GlowText(hdc, title, tx, titleY, RGB(0, 255, 0), RGB(0, 100, 0), 110);

    GlowText(hdc, L"DEMON  EDITION",
        (SCREEN_W - 420) / 2, titleY + 130,
        RGB(200, 0, 0), RGB(60, 0, 0), 28);

    // Divider
    HPEN dvd = CreatePen(PS_SOLID, 2, RGB(0, 120, 0));
    HPEN op2 = (HPEN)SelectObject(hdc, dvd);
    MoveToEx(hdc, SCREEN_W / 4, SCREEN_H / 2 - 5, nullptr);
    LineTo(hdc, SCREEN_W * 3 / 4, SCREEN_H / 2 - 5);
    SelectObject(hdc, op2); DeleteObject(dvd);

    CenterText(hdc, L"HOW  TO  PLAY", SCREEN_H / 2 + 10, 16, RGB(255, 220, 0), true, L"Impact");
    CenterText(hdc, L"Arrow Keys = Move       P = Pause       R = Restart",
        SCREEN_H / 2 + 50, 13, RGB(200, 200, 200), false, L"Consolas");
    CenterText(hdc, L"Eat the glowing orbs to grow stronger!",
        SCREEN_H / 2 + 80, 13, RGB(0, 200, 100), false, L"Consolas");

    dvd = CreatePen(PS_SOLID, 1, RGB(0, 80, 0));
    op2 = (HPEN)SelectObject(hdc, dvd);
    MoveToEx(hdc, SCREEN_W / 4, SCREEN_H / 2 + 115, nullptr);
    LineTo(hdc, SCREEN_W * 3 / 4, SCREEN_H / 2 + 115);
    SelectObject(hdc, op2); DeleteObject(dvd);

    if ((animTick % 12) < 7)
        GlowText(hdc, L"PRESS  SPACE  TO  START",
            (SCREEN_W - 700) / 2, SCREEN_H / 2 + 130,
            RGB(255, 255, 255), RGB(0, 60, 0), 22);

    if (highScore > 0) {
        std::wstring hs = L"BEST SCORE: " + std::to_wstring(highScore);
        CenterText(hdc, hs, SCREEN_H / 2 + 185, 15, RGB(255, 200, 0), true, L"Impact");
    }

    CenterText(hdc, L"ESC = Quit", SCREEN_H - 50, 10, RGB(80, 80, 80), false, L"Consolas");
}

// ═══════════════════════════════════════════════
//  Game Over Screen
// ═══════════════════════════════════════════════
void DrawGameOver(HDC hdc)
{
    RECT rc = { 0,HUD_H,SCREEN_W,SCREEN_H };
    HBRUSH br = CreateSolidBrush(RGB(5, 0, 0));
    FillRect(hdc, &rc, br); DeleteObject(br);

    int my = (SCREEN_H + HUD_H) / 2;
    int pulse = (int)(sinf(animTick * 0.2f) * 3);

    GlowText(hdc, L"GAME  OVER",
        (SCREEN_W - 800) / 2 + pulse, my - 120 + pulse,
        RGB(255, 0, 0), RGB(100, 0, 0), 80);

    std::wstring sc = L"SCORE: " + std::to_wstring(score);
    CenterText(hdc, sc, my - 10, 22, RGB(255, 220, 0), true, L"Impact");

    if (score >= highScore && score > 0) {
        GlowText(hdc, L"★  NEW  HIGH  SCORE  ★",
            (SCREEN_W - 600) / 2, my + 35,
            RGB(0, 255, 255), RGB(0, 60, 80), 18);
    }

    CenterText(hdc, L"R = Play Again          SPACE = Main Menu",
        my + 90, 13, RGB(180, 180, 180), false, L"Consolas");
}

// ═══════════════════════════════════════════════
//  Paused Screen
// ═══════════════════════════════════════════════
void DrawPaused(HDC hdc)
{
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    PatBlt(hdc, 0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, BLACKNESS);
    SelectObject(hdc, ob); DeleteObject(br);

    int my = (SCREEN_H + HUD_H) / 2;
    GlowText(hdc, L"PAUSED",
        (SCREEN_W - 380) / 2, my - 60,
        RGB(255, 220, 0), RGB(80, 60, 0), 52);

    CenterText(hdc, L"P / SPACE = Continue          R = Restart",
        my + 20, 14, RGB(200, 200, 200), false, L"Consolas");
}

// ═══════════════════════════════════════════════
//  Master Draw
// ═══════════════════════════════════════════════
void DrawFrame(HDC hdc)
{
    RECT rc = { 0,0,SCREEN_W,SCREEN_H };
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, br); DeleteObject(br);

    switch (gameState)
    {
    case GS_START:
        DrawStartScreen(hdc);
        break;
    case GS_PLAYING:
        DrawHUD(hdc);
        DrawPlayField(hdc);
        DrawFood(hdc);
        DrawParticles(hdc);
        DrawSnakeBody(hdc);
        DrawSnakeHead(hdc);
        DrawDeathEffect(hdc);
        break;
    case GS_PAUSED:
        DrawHUD(hdc);
        DrawPlayField(hdc);
        DrawFood(hdc);
        DrawSnakeBody(hdc);
        DrawSnakeHead(hdc);
        DrawPaused(hdc);
        break;
    case GS_GAMEOVER:
        DrawHUD(hdc);
        DrawParticles(hdc);
        DrawGameOver(hdc);
        DrawDeathEffect(hdc);
        break;
    }
}