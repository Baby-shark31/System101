/*
 * =====================================================================
 *  GRAPHICAL PONG for Windows  (Win32 API + GDI, no external libraries)
 * =====================================================================
 *
 *  This produces a real windowed, graphical game - not a terminal
 *  program. It uses only the Windows API headers that ship with every
 *  Windows compiler toolchain, so the result is a single, standalone
 *  .exe with no DLLs to distribute beyond what Windows already has.
 *
 *  Controls:
 *      W / Up Arrow    -> move paddle up
 *      S / Down Arrow  -> move paddle down
 *      Esc             -> quit
 *      Space           -> restart after game over
 *
 *  ---------------------------------------------------------------
 *  HOW TO COMPILE (must be done on a Windows machine, or by
 *  cross-compiling with MinGW - this can't be built on Linux/macOS
 *  since it depends on the Windows API headers/libs):
 *  ---------------------------------------------------------------
 *
 *  Option A - MinGW-w64 (from a "Windows"/"MSYS2 MinGW64" shell):
 *      gcc pong_gui.c -o pong.exe -mwindows -lgdi32 -luser32
 *
 *      -mwindows   builds a GUI app (no console window pops up)
 *      -lgdi32     links the Graphics Device Interface library
 *      -luser32    links the window/message/input library
 *
 *  Option B - MSVC (from a "Developer Command Prompt for VS"):
 *      cl pong_gui.c /link user32.lib gdi32.lib
 *
 *  Either way you get pong.exe - just double-click it to play.
 *
 *  Design notes:
 *      - The game loop is driven by a Windows timer (SetTimer) that
 *        fires roughly 60 times/second and posts WM_TIMER messages.
 *      - All drawing happens in WM_PAINT using GDI: FillRect for
 *        paddles/ball, TextOut for the score. To avoid flicker, each
 *        frame is drawn to an off-screen memory bitmap first, then
 *        copied to the screen in one BitBlt ("double buffering").
 *      - Input uses GetAsyncKeyState() each tick to check paddle
 *        keys, rather than relying on WM_KEYDOWN messages, so holding
 *        a key down gives smooth continuous movement.
 * =====================================================================
 */

#include <windows.h>
#include <stdio.h>
#include <time.h>

/* ---------------------------------------------------------------------
 * Game configuration
 * ------------------------------------------------------------------- */
#define WIN_WIDTH      800
#define WIN_HEIGHT     500
#define PADDLE_WIDTH   14
#define PADDLE_HEIGHT  90
#define BALL_SIZE      16
#define PADDLE_SPEED   7.0f
#define AI_SPEED       5.0f
#define WIN_SCORE      5
#define TIMER_ID       1
#define TIMER_MS       16          /* ~60 FPS */

/* ---------------------------------------------------------------------
 * Game state (kept in globals for simplicity in a small Win32 app)
 * ------------------------------------------------------------------- */
static float player_y, ai_y;               /* paddle top-left Y            */
static float ball_x, ball_y;                /* ball top-left position       */
static float ball_dx, ball_dy;              /* ball velocity                */
static int   player_score = 0, ai_score = 0;
static int   game_over = 0;
static char  status_text[64] = "";

/* Off-screen buffer for flicker-free drawing (double buffering) */
static HBITMAP backBuffer   = NULL;
static HDC     backBufferDC = NULL;

/* ---------------------------------------------------------------------
 * Reset the ball to center with a pseudo-random direction.
 * towards_player = 1 sends it left (toward the player who was just
 * scored on... actually toward whichever side did NOT just score).
 * ------------------------------------------------------------------- */
void reset_ball(int towards_player) {
    ball_x = (WIN_WIDTH  - BALL_SIZE) / 2.0f;
    ball_y = (WIN_HEIGHT - BALL_SIZE) / 2.0f;
    ball_dx = towards_player ? -6.0f : 6.0f;
    ball_dy = (float)((rand() % 5) - 2) * 1.5f;
    if (ball_dy == 0.0f) ball_dy = 2.0f;
}

/* Reset entire game to starting state (used at launch and on replay) */
void reset_game(void) {
    player_y = (WIN_HEIGHT - PADDLE_HEIGHT) / 2.0f;
    ai_y     = (WIN_HEIGHT - PADDLE_HEIGHT) / 2.0f;
    player_score = 0;
    ai_score     = 0;
    game_over    = 0;
    status_text[0] = '\0';
    reset_ball(0);
}

/* ---------------------------------------------------------------------
 * Advance the simulation by one tick: input, AI, physics, scoring.
 * ------------------------------------------------------------------- */
void update_game(HWND hwnd) {
    if (game_over) return;

    /* --- Player input (held-key state, so movement is smooth) --- */
    if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000) {
        player_y -= PADDLE_SPEED;
    }
    if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000) {
        player_y += PADDLE_SPEED;
    }
    if (player_y < 0) player_y = 0;
    if (player_y > WIN_HEIGHT - PADDLE_HEIGHT) player_y = WIN_HEIGHT - PADDLE_HEIGHT;

    /* --- AI: chase the ball's Y position, capped speed --- */
    float ai_center   = ai_y + PADDLE_HEIGHT / 2.0f;
    float ball_center = ball_y + BALL_SIZE / 2.0f;
    if (ai_center < ball_center - 10) {
        ai_y += AI_SPEED;
    } else if (ai_center > ball_center + 10) {
        ai_y -= AI_SPEED;
    }
    if (ai_y < 0) ai_y = 0;
    if (ai_y > WIN_HEIGHT - PADDLE_HEIGHT) ai_y = WIN_HEIGHT - PADDLE_HEIGHT;

    /* --- Ball movement --- */
    ball_x += ball_dx;
    ball_y += ball_dy;

    /* Bounce off top/bottom walls */
    if (ball_y <= 0) {
        ball_y = 0;
        ball_dy = -ball_dy;
    } else if (ball_y >= WIN_HEIGHT - BALL_SIZE) {
        ball_y = WIN_HEIGHT - BALL_SIZE;
        ball_dy = -ball_dy;
    }

    /* Player paddle rect: x in [30, 30+PADDLE_WIDTH] */
    const float player_x = 30.0f;
    const float ai_x      = WIN_WIDTH - 30.0f - PADDLE_WIDTH;

    /* Bounce off player paddle */
    if (ball_dx < 0 &&
        ball_x <= player_x + PADDLE_WIDTH && ball_x + BALL_SIZE >= player_x &&
        ball_y + BALL_SIZE >= player_y && ball_y <= player_y + PADDLE_HEIGHT) {
        ball_x = player_x + PADDLE_WIDTH;
        ball_dx = -ball_dx * 1.03f; /* slight speed-up each hit */
        float offset = (ball_y + BALL_SIZE / 2.0f) - (player_y + PADDLE_HEIGHT / 2.0f);
        ball_dy += offset * 0.06f;
    }

    /* Bounce off AI paddle */
    if (ball_dx > 0 &&
        ball_x + BALL_SIZE >= ai_x && ball_x <= ai_x + PADDLE_WIDTH &&
        ball_y + BALL_SIZE >= ai_y && ball_y <= ai_y + PADDLE_HEIGHT) {
        ball_x = ai_x - BALL_SIZE;
        ball_dx = -ball_dx * 1.03f;
        float offset = (ball_y + BALL_SIZE / 2.0f) - (ai_y + PADDLE_HEIGHT / 2.0f);
        ball_dy += offset * 0.06f;
    }

    /* --- Scoring --- */
    if (ball_x < 0) {
        ai_score++;
        reset_ball(1);
    } else if (ball_x > WIN_WIDTH) {
        player_score++;
        reset_ball(0);
    }

    if (player_score >= WIN_SCORE) {
        game_over = 1;
        sprintf_s(status_text, sizeof(status_text), "YOU WIN!  Press SPACE to play again");
    } else if (ai_score >= WIN_SCORE) {
        game_over = 1;
        sprintf_s(status_text, sizeof(status_text), "AI WINS!  Press SPACE to play again");
    }

    /* Ask Windows to repaint the window with the new frame */
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ---------------------------------------------------------------------
 * Draw one frame into the given device context (the off-screen buffer).
 * ------------------------------------------------------------------- */
void draw_frame(HDC hdc) {
    /* Background */
    RECT full = {0, 0, WIN_WIDTH, WIN_HEIGHT};
    HBRUSH bg = CreateSolidBrush(RGB(10, 10, 20));
    FillRect(hdc, &full, bg);
    DeleteObject(bg);

    /* Center net (dashed line) */
    HBRUSH netBrush = CreateSolidBrush(RGB(70, 70, 90));
    for (int y = 0; y < WIN_HEIGHT; y += 24) {
        RECT dash = { WIN_WIDTH / 2 - 2, y, WIN_WIDTH / 2 + 2, y + 14 };
        FillRect(hdc, &dash, netBrush);
    }
    DeleteObject(netBrush);

    /* Paddles */
    HBRUSH paddleBrush = CreateSolidBrush(RGB(230, 230, 240));
    RECT playerRect = { 30, (int)player_y, 30 + PADDLE_WIDTH, (int)player_y + PADDLE_HEIGHT };
    RECT aiRect     = { WIN_WIDTH - 30 - PADDLE_WIDTH, (int)ai_y,
                         WIN_WIDTH - 30, (int)ai_y + PADDLE_HEIGHT };
    FillRect(hdc, &playerRect, paddleBrush);
    FillRect(hdc, &aiRect, paddleBrush);
    DeleteObject(paddleBrush);

    /* Ball */
    HBRUSH ballBrush = CreateSolidBrush(RGB(255, 200, 60));
    RECT ballRect = { (int)ball_x, (int)ball_y, (int)ball_x + BALL_SIZE, (int)ball_y + BALL_SIZE };
    FillRect(hdc, &ballRect, ballBrush);
    DeleteObject(ballBrush);

    /* Score text */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HFONT scoreFont = CreateFontA(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   ANTIALIASED_QUALITY, DEFAULT_PITCH, "Consolas");
    HFONT oldFont = (HFONT)SelectObject(hdc, scoreFont);

    char buf[32];
    sprintf_s(buf, sizeof(buf), "%d", player_score);
    TextOutA(hdc, WIN_WIDTH / 2 - 80, 20, buf, (int)strlen(buf));
    sprintf_s(buf, sizeof(buf), "%d", ai_score);
    TextOutA(hdc, WIN_WIDTH / 2 + 60, 20, buf, (int)strlen(buf));

    SelectObject(hdc, oldFont);
    DeleteObject(scoreFont);

    /* Status / game-over text */
    if (game_over) {
        HFONT bigFont = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     ANTIALIASED_QUALITY, DEFAULT_PITCH, "Consolas");
        HFONT oldBig = (HFONT)SelectObject(hdc, bigFont);
        SetTextColor(hdc, RGB(255, 220, 120));
        TextOutA(hdc, WIN_WIDTH / 2 - 220, WIN_HEIGHT / 2 - 20,
                 status_text, (int)strlen(status_text));
        SelectObject(hdc, oldBig);
        DeleteObject(bigFont);
    } else {
        HFONT smallFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       ANTIALIASED_QUALITY, DEFAULT_PITCH, "Consolas");
        HFONT oldSmall = (HFONT)SelectObject(hdc, smallFont);
        SetTextColor(hdc, RGB(150, 150, 170));
        const char *hint = "W / S or Arrow Keys to move   |   Esc to quit";
        TextOutA(hdc, WIN_WIDTH / 2 - 160, WIN_HEIGHT - 30, hint, (int)strlen(hint));
        SelectObject(hdc, oldSmall);
        DeleteObject(smallFont);
    }
}

/* ---------------------------------------------------------------------
 * Create/resize the off-screen back buffer used for double buffering.
 * ------------------------------------------------------------------- */
void init_backbuffer(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    backBufferDC = CreateCompatibleDC(hdc);
    backBuffer   = CreateCompatibleBitmap(hdc, WIN_WIDTH, WIN_HEIGHT);
    SelectObject(backBufferDC, backBuffer);
    ReleaseDC(hwnd, hdc);
}

/* ---------------------------------------------------------------------
 * Window procedure: handles creation, timer ticks, painting, input,
 * and cleanup. This is the standard Win32 message-loop callback.
 * ------------------------------------------------------------------- */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

        case WM_CREATE:
            srand((unsigned int)time(NULL));
            reset_game();
            init_backbuffer(hwnd);
            SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
            return 0;

        case WM_TIMER:
            update_game(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
            } else if (wParam == VK_SPACE && game_over) {
                reset_game();
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            /* Draw the frame into the off-screen buffer, then blit it
             * to the screen in one operation - eliminates flicker. */
            draw_frame(backBufferDC);
            BitBlt(hdc, 0, 0, WIN_WIDTH, WIN_HEIGHT, backBufferDC, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            if (backBuffer)   DeleteObject(backBuffer);
            if (backBufferDC) DeleteDC(backBufferDC);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* ---------------------------------------------------------------------
 * Entry point for a Windows GUI application.
 * ------------------------------------------------------------------- */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; /* unused parameters */

    const char CLASS_NAME[] = "TerminalPongGraphicalWindowClass";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    /* Adjust the window rect so the *client area* (playable area) is
     * exactly WIN_WIDTH x WIN_HEIGHT, not including title bar/borders. */
    RECT rect = {0, 0, WIN_WIDTH, WIN_HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "Pong",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}