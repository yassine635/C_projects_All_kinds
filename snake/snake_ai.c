/*
 * ============================================================
 *  SNAKE GAME — Complete implementation using raylib
 *  Features:
 *    - Main menu with 3 game modes
 *    - 3 map styles (Classic, Walls, Maze)
 *    - Smooth snake rendering with rounded segments
 *    - Animated food with glow effect
 *    - Score, hi-score, speed levels
 *    - Game-over / pause screens
 * ============================================================
 */

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ─── Constants ─────────────────────────────────────────── */
#define SCREEN_W      1000
#define SCREEN_H      800
#define CELL           40          /* grid cell size in pixels */
#define COLS          (SCREEN_W / CELL)   /* 40 */
#define ROWS          (SCREEN_H / CELL)   /* 30 */
#define MAX_SNAKE     (COLS * ROWS)
#define FPS            60

/* ─── Game-mode IDs ─────────────────────────────────────── */
typedef enum {
    MODE_CLASSIC  = 0,   /* wrap-around walls          */
    MODE_WALLS    = 1,   /* solid walls → game over     */
    MODE_MAZE     = 2,   /* static maze obstacles       */
} GameMode;

/* ─── Map IDs ────────────────────────────────────────────── */
typedef enum {
    MAP_PLAIN  = 0,
    MAP_GRID   = 1,
    MAP_DARK   = 2,
} MapStyle;

/* ─── Direction ─────────────────────────────────────────── */
typedef enum { DIR_RIGHT=0, DIR_LEFT, DIR_UP, DIR_DOWN } Direction;

/* ─── App state ─────────────────────────────────────────── */
typedef enum {
    STATE_MENU = 0,
    STATE_MAPSEL,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER,
} AppState;

/* ─── Structures ─────────────────────────────────────────── */
typedef struct { int x, y; } Cell;

typedef struct {
    Cell      body[MAX_SNAKE];   /* body[0] = head */
    int       length;
    Direction dir;
    Direction nextDir;           /* queued direction change */
} Snake;

typedef struct {
    Cell   pos;
    float  pulse;                /* animation timer [0..2π] */
    int    bonus;                /* point value             */
} Food;

/* Maze obstacle list (static) */
#define MAX_WALLS 80
typedef struct {
    Cell  cells[MAX_WALLS];
    int   count;
} Maze;

/* ─── Globals ────────────────────────────────────────────── */
static AppState   gState       = STATE_MENU;
static GameMode   gMode        = MODE_CLASSIC;
static MapStyle   gMap         = MAP_PLAIN;
static Snake      gSnake;
static Food       gFood;
static Maze       gMaze;
static int        gScore       = 0;
static int        gHiScore     = 0;
static float      gMoveTimer   = 0.0f;   /* accumulates dt */
static float      gMoveInterval= 0.15f;  /* seconds/step   */
static int        gLevel       = 1;
static bool       gRunning     = true;

/* ─── Colour palette ────────────────────────────────────── */
static const Color COL_BG_PLAIN  = {  15,  15,  25, 255 };
static const Color COL_BG_GRID   = {  10,  20,  10, 255 };
static const Color COL_BG_DARK   = {   5,   5,   5, 255 };
static const Color COL_GRID_LINE = {  30,  30,  50, 255 };
static const Color COL_HEAD      = {  80, 220,  80, 255 };
static const Color COL_BODY_A    = {  60, 180,  60, 255 };
static const Color COL_BODY_B    = {  40, 140,  40, 255 };
static const Color COL_EYE       = { 255, 255, 255, 255 };
static const Color COL_PUPIL     = {  20,  20,  20, 255 };
static const Color COL_FOOD      = { 255,  80,  80, 255 };
static const Color COL_FOOD_GLOW = { 255, 120, 120,  60 };
static const Color COL_WALL      = {  80,  80, 100, 255 };
static const Color COL_MAZE      = { 120,  90,  50, 255 };

/* ═══════════════════════════════════════════════════════════
 *  UTILITY HELPERS
 * ═══════════════════════════════════════════════════════════ */

/* Convert grid cell to pixel center */
static Vector2 CellCenter(Cell c) {
    return (Vector2){ c.x * CELL + CELL / 2.0f,
                      c.y * CELL + CELL / 2.0f };
}

/* Returns true if position (x,y) is occupied by the snake */
static bool SnakeOccupies(int x, int y) {
    for (int i = 0; i < gSnake.length; i++)
        if (gSnake.body[i].x == x && gSnake.body[i].y == y) return true;
    return false;
}

/* Returns true if position (x,y) is a maze wall */
static bool MazeOccupies(int x, int y) {
    for (int i = 0; i < gMaze.count; i++)
        if (gMaze.cells[i].x == x && gMaze.cells[i].y == y) return true;
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  MAZE BUILDER
 *  Draws a simple symmetric maze inside the grid.
 * ═══════════════════════════════════════════════════════════ */
static void BuildMaze(void) {
    gMaze.count = 0;
    /* Outer border (only for MAP wall modes) handled by collision */
    /* Inner maze: two horizontal bars with a gap, two vertical bars */
    int cx = COLS / 2, cy = ROWS / 2;
    /* Horizontal bars */
    for (int x = 5; x < COLS - 5; x++) {
        if (abs(x - cx) > 3) { /* leave a gap in the middle */
            if (gMaze.count < MAX_WALLS)
                gMaze.cells[gMaze.count++] = (Cell){x, cy - 5};
            if (gMaze.count < MAX_WALLS)
                gMaze.cells[gMaze.count++] = (Cell){x, cy + 5};
        }
    }
    /* Vertical bars */
    for (int y = 5; y < ROWS - 5; y++) {
        if (abs(y - cy) > 3) {
            if (gMaze.count < MAX_WALLS)
                gMaze.cells[gMaze.count++] = (Cell){cx - 8, y};
            if (gMaze.count < MAX_WALLS)
                gMaze.cells[gMaze.count++] = (Cell){cx + 8, y};
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  FOOD SPAWNER
 *  Places food on an empty cell.
 * ═══════════════════════════════════════════════════════════ */
static void SpawnFood(void) {
    Cell c;
    do {
        c.x = GetRandomValue(1, COLS - 2);
        c.y = GetRandomValue(1, ROWS - 2);
    } while (SnakeOccupies(c.x, c.y) || MazeOccupies(c.x, c.y));
    gFood.pos   = c;
    gFood.pulse = 0.0f;
    gFood.bonus = 10 * gLevel;
}

/* ═══════════════════════════════════════════════════════════
 *  GAME INITIALISER
 *  Resets all game state; called on new game.
 * ═══════════════════════════════════════════════════════════ */
static void InitGame(void) {
    /* Place snake in the centre of the grid heading right */
    int startX = COLS / 2, startY = ROWS / 2;
    gSnake.length  = 4;
    gSnake.dir     = DIR_RIGHT;
    gSnake.nextDir = DIR_RIGHT;
    for (int i = 0; i < gSnake.length; i++) {
        gSnake.body[i].x = startX - i;
        gSnake.body[i].y = startY;
    }

    if (gMode == MODE_MAZE) BuildMaze();
    else                    gMaze.count = 0;

    gScore        = 0;
    gLevel        = 1;
    gMoveTimer    = 0.0f;
    gMoveInterval = 0.15f;

    SpawnFood();
}

/* ═══════════════════════════════════════════════════════════
 *  INPUT HANDLER
 *  Queues a direction change; prevents 180° reversal.
 * ═══════════════════════════════════════════════════════════ */
static void HandleInput(void) {
    /* ESC / P toggles pause */
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        if (gState == STATE_PLAYING) gState = STATE_PAUSED;
        else if (gState == STATE_PAUSED) gState = STATE_PLAYING;
        return;
    }

    Direction d = gSnake.nextDir;
    if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && gSnake.dir != DIR_LEFT)  d = DIR_RIGHT;
    if ((IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) && gSnake.dir != DIR_RIGHT) d = DIR_LEFT;
    if ((IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) && gSnake.dir != DIR_DOWN)  d = DIR_UP;
    if ((IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) && gSnake.dir != DIR_UP)    d = DIR_DOWN;
    gSnake.nextDir = d;
}

/* ═══════════════════════════════════════════════════════════
 *  SNAKE STEP
 *  Advances the snake by one cell; handles collisions, food.
 * ═══════════════════════════════════════════════════════════ */
static void StepSnake(void) {
    gSnake.dir = gSnake.nextDir;

    /* Compute new head position */
    Cell head = gSnake.body[0];
    switch (gSnake.dir) {
        case DIR_RIGHT: head.x++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
    }

    /* ── Wall collision ── */
    if (gMode == MODE_CLASSIC) {
        /* Wrap around */
        head.x = (head.x + COLS) % COLS;
        head.y = (head.y + ROWS) % ROWS;
    } else {
        /* Solid walls → game over */
        if (head.x < 0 || head.x >= COLS || head.y < 0 || head.y >= ROWS) {
            if (gScore > gHiScore) gHiScore = gScore;
            gState = STATE_GAMEOVER;
            return;
        }
    }

    /* ── Maze wall collision ── */
    if (gMode == MODE_MAZE && MazeOccupies(head.x, head.y)) {
        if (gScore > gHiScore) gHiScore = gScore;
        gState = STATE_GAMEOVER;
        return;
    }

    /* ── Self collision (skip tail — it will move) ── */
    for (int i = 0; i < gSnake.length - 1; i++) {
        if (gSnake.body[i].x == head.x && gSnake.body[i].y == head.y) {
            if (gScore > gHiScore) gHiScore = gScore;
            gState = STATE_GAMEOVER;
            return;
        }
    }

    /* ── Food eaten ── */
    bool ate = (head.x == gFood.pos.x && head.y == gFood.pos.y);

    /* Shift body forward */
    if (ate && gSnake.length < MAX_SNAKE) gSnake.length++;
    for (int i = gSnake.length - 1; i > 0; i--)
        gSnake.body[i] = gSnake.body[i - 1];
    gSnake.body[0] = head;

    if (ate) {
        gScore += gFood.bonus;
        if (gScore > gHiScore) gHiScore = gScore;
        /* Level up every 50 pts → speed increases */
        gLevel = 1 + gScore / 50;
        if (gLevel > 10) gLevel = 10;
        gMoveInterval = 0.15f - (gLevel - 1) * 0.012f;
        if (gMoveInterval < 0.04f) gMoveInterval = 0.04f;
        SpawnFood();
    }
}

/* ═══════════════════════════════════════════════════════════
 *  DRAW HELPERS
 * ═══════════════════════════════════════════════════════════ */

/* Draw background based on map style */
static void DrawBackground(void) {
    switch (gMap) {
        case MAP_PLAIN:
            ClearBackground(COL_BG_PLAIN);
            break;
        case MAP_GRID:
            ClearBackground(COL_BG_GRID);
            /* Draw subtle grid lines */
            for (int x = 0; x <= COLS; x++)
                DrawLine(x * CELL, 0, x * CELL, SCREEN_H, COL_GRID_LINE);
            for (int y = 0; y <= ROWS; y++)
                DrawLine(0, y * CELL, SCREEN_W, y * CELL, COL_GRID_LINE);
            break;
        case MAP_DARK:
            ClearBackground(COL_BG_DARK);
            /* Faint dot grid */
            for (int x = 1; x < COLS; x++)
                for (int y = 1; y < ROWS; y++)
                    DrawPixel(x * CELL, y * CELL, (Color){60,60,60,255});
            break;
    }
}

/* Draw the maze walls */
static void DrawMaze(void) {
    for (int i = 0; i < gMaze.count; i++) {
        int px = gMaze.cells[i].x * CELL;
        int py = gMaze.cells[i].y * CELL;
        DrawRectangle(px, py, CELL, CELL, COL_MAZE);
        DrawRectangleLines(px, py, CELL, CELL, (Color){160,120,70,255});
    }
}

/* Draw a single snake segment with rounded look */
static void DrawSegment(Cell c, Color col, float shrink) {
    float pad = shrink;
    float sz  = CELL - pad * 2.0f;
    DrawRectangleRounded(
        (Rectangle){ c.x * CELL + pad, c.y * CELL + pad, sz, sz },
        0.45f, 6, col
    );
}

/* Draw the snake (head + body with alternating shades + eyes) */
static void DrawSnake(void) {
    /* Body — draw tail → neck so head is always on top */
    for (int i = gSnake.length - 1; i >= 1; i--) {
        Color col = (i % 2 == 0) ? COL_BODY_A : COL_BODY_B;
        /* Taper near tail */
        float shrink = 2.5f + (float)(gSnake.length - i) * 0.04f;
        if (shrink > 4.5f) shrink = 4.5f;
        DrawSegment(gSnake.body[i], col, shrink);
    }

    /* Head */
    DrawSegment(gSnake.body[0], COL_HEAD, 2.0f);

    /* Eyes — offset based on direction */
    Vector2 hc = CellCenter(gSnake.body[0]);
    float ex1x = hc.x, ex1y = hc.y;
    float ex2x = hc.x, ex2y = hc.y;
    float eyeOff = 3.5f, fwdOff = 3.5f;
    switch (gSnake.dir) {
        case DIR_RIGHT:
            ex1x += fwdOff; ex1y -= eyeOff;
            ex2x += fwdOff; ex2y += eyeOff; break;
        case DIR_LEFT:
            ex1x -= fwdOff; ex1y -= eyeOff;
            ex2x -= fwdOff; ex2y += eyeOff; break;
        case DIR_UP:
            ex1x -= eyeOff; ex1y -= fwdOff;
            ex2x += eyeOff; ex2y -= fwdOff; break;
        case DIR_DOWN:
            ex1x -= eyeOff; ex1y += fwdOff;
            ex2x += eyeOff; ex2y += fwdOff; break;
    }
    DrawCircle((int)ex1x, (int)ex1y, 3.2f, COL_EYE);
    DrawCircle((int)ex2x, (int)ex2y, 3.2f, COL_EYE);
    DrawCircle((int)ex1x, (int)ex1y, 1.5f, COL_PUPIL);
    DrawCircle((int)ex2x, (int)ex2y, 1.5f, COL_PUPIL);
}

/* Draw food — pulsing apple shape */
static void DrawFood(void) {
    gFood.pulse += GetFrameTime() * 3.0f;
    float scale = 1.0f + sinf(gFood.pulse) * 0.12f;
    Vector2 fc  = CellCenter(gFood.pos);

    /* Outer glow */
    float glowR = (CELL / 2.0f + 6.0f) * scale;
    DrawCircle((int)fc.x, (int)fc.y, glowR, COL_FOOD_GLOW);
    DrawCircle((int)fc.x, (int)fc.y, glowR * 0.6f,
               (Color){255,160,80,40});

    /* Main body — two overlapping circles for apple silhouette */
    float r = (CELL / 2.0f - 1.0f) * scale;
    DrawCircle((int)fc.x, (int)(fc.y + 1), (int)r, COL_FOOD);
    /* Shine */
    DrawCircle((int)(fc.x - r * 0.28f), (int)(fc.y - r * 0.25f),
               (int)(r * 0.28f), (Color){255,220,220,180});
    /* Stem */
    DrawRectangle((int)(fc.x), (int)(fc.y - r - 4), 2, 5,
                  (Color){100,200,60,255});
    /* Leaf */
    DrawCircle((int)(fc.x + 4), (int)(fc.y - r - 3), 3,
               (Color){80,200,50,200});
}

/* Draw a border (for WALLS / MAZE modes) */
static void DrawBorder(void) {
    Color bc = (gMode == MODE_MAZE) ? COL_MAZE : COL_WALL;
    DrawRectangleLines(0, 0, SCREEN_W, SCREEN_H, bc);
    DrawRectangleLines(1, 1, SCREEN_W-2, SCREEN_H-2, bc);
    DrawRectangleLines(2, 2, SCREEN_W-4, SCREEN_H-4, bc);
}

/* Draw the HUD (score / level / mode) */
static void DrawHUD(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE: %d", gScore);
    DrawText(buf, 10, 6, 18, (Color){200,255,200,255});

    snprintf(buf, sizeof(buf), "BEST: %d", gHiScore);
    DrawText(buf, 10, 26, 14, (Color){150,200,150,180});

    snprintf(buf, sizeof(buf), "LVL %d", gLevel);
    DrawText(buf, SCREEN_W - 70, 6, 18, (Color){200,220,255,255});

    const char* modeNames[] = {"CLASSIC", "WALLS", "MAZE"};
    DrawText(modeNames[gMode], SCREEN_W / 2 - MeasureText(modeNames[gMode],14)/2,
             6, 14, (Color){160,160,200,180});
}

/* ─── Overlay helpers ─────────────────────────────────────── */

/* Darkened semi-transparent overlay */
static void DrawOverlay(void) {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,160});
}

/* Centred text helper */
static void DrawCentred(const char* text, int y, int size, Color col) {
    int tw = MeasureText(text, size);
    DrawText(text, SCREEN_W/2 - tw/2, y, size, col);
}

/* ═══════════════════════════════════════════════════════════
 *  MENU / MAP-SELECT SCREENS
 * ═══════════════════════════════════════════════════════════ */

/* A clickable/keyboard button — returns true if activated */
static bool MenuButton(const char* label, int cx, int y, int w, int h,
                       bool highlighted) {
    Rectangle r = { (float)(cx - w/2), (float)y, (float)w, (float)h };
    Color bgCol  = highlighted ? (Color){80,180,80,230} : (Color){40,60,40,180};
    Color txtCol = highlighted ? BLACK : (Color){200,255,200,255};

    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) { bgCol = (Color){100,210,100,255}; txtCol = BLACK; }

    DrawRectangleRounded(r, 0.3f, 8, bgCol);
    DrawRectangleRoundedLines(r, 0.3f, 8, highlighted ?
        (Color){150,255,150,255} : (Color){80,120,80,200});

    int tw = MeasureText(label, 20);
    DrawText(label, cx - tw/2, y + h/2 - 10, 20, txtCol);

    return (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ||
           (highlighted && IsKeyPressed(KEY_ENTER));
}

/* Main menu */
static void UpdateDrawMenu(void) {
    static int sel = 0;  /* selected mode index */

    /* keyboard navigation */
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) sel = (sel+1)%3;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) sel = (sel+2)%3;

    ClearBackground(COL_BG_PLAIN);

    /* Title */
    DrawCentred("SNAKE", SCREEN_H/2 - 180, 72, (Color){80,220,80,255});
    DrawCentred("Select Mode", SCREEN_H/2 - 100, 22, (Color){160,200,160,200});

    struct { const char* name; const char* desc; GameMode id; } modes[] = {
        { "CLASSIC",  "Wrap-around walls",   MODE_CLASSIC },
        { "WALLS",    "Hit a wall — die!",   MODE_WALLS   },
        { "MAZE",     "Navigate the maze",   MODE_MAZE    },
    };

    for (int i = 0; i < 3; i++) {
        int y = SCREEN_H/2 - 50 + i * 70;
        if (MenuButton(modes[i].name, SCREEN_W/2, y, 260, 50, sel == i)) {
            gMode  = modes[i].id;
            gState = STATE_MAPSEL;
            sel    = 0;
            return;
        }
        /* Description under button */
        int dw = MeasureText(modes[i].desc, 14);
        DrawText(modes[i].desc, SCREEN_W/2 - dw/2, y + 54, 14,
                 (Color){120,160,120,160});
    }

    DrawCentred("↑/↓ navigate   ENTER select", SCREEN_H - 28, 14,
                (Color){100,120,100,160});
}

/* Map selection screen */
static void UpdateDrawMapSel(void) {
    static int sel = 0;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) sel = (sel+1)%3;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) sel = (sel+2)%3;

    ClearBackground(COL_BG_PLAIN);
    DrawCentred("Choose Map", SCREEN_H/2 - 150, 40, (Color){80,220,80,255});

    struct { const char* name; MapStyle id; } maps[] = {
        { "PLAIN",  MAP_PLAIN },
        { "GRID",   MAP_GRID  },
        { "DARK",   MAP_DARK  },
    };

    for (int i = 0; i < 3; i++) {
        int y = SCREEN_H/2 - 80 + i * 70;
        if (MenuButton(maps[i].name, SCREEN_W/2, y, 200, 50, sel == i)) {
            gMap   = maps[i].id;
            InitGame();
            gState = STATE_PLAYING;
            return;
        }
    }

    DrawCentred("↑/↓ navigate   ENTER select", SCREEN_H - 28, 14,
                (Color){100,120,100,160});

    if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_MENU; sel = 0; }
}

/* Pause screen */
static void DrawPauseScreen(void) {
    DrawOverlay();
    DrawCentred("PAUSED", SCREEN_H/2 - 40, 48, (Color){200,255,200,255});
    DrawCentred("Press P or ESC to continue", SCREEN_H/2 + 20, 18,
                (Color){160,200,160,200});
    DrawCentred("Press M for main menu", SCREEN_H/2 + 46, 16,
                (Color){140,180,140,180});
    if (IsKeyPressed(KEY_M)) gState = STATE_MENU;
}

/* Game over screen */
static void DrawGameOverScreen(void) {
    DrawOverlay();

    DrawCentred("GAME OVER", SCREEN_H/2 - 80, 52, (Color){255,80,80,255});

    char buf[64];
    snprintf(buf, sizeof(buf), "Score: %d", gScore);
    DrawCentred(buf, SCREEN_H/2 - 10, 30, (Color){220,220,220,255});

    snprintf(buf, sizeof(buf), "Best:  %d", gHiScore);
    DrawCentred(buf, SCREEN_H/2 + 30, 22, (Color){180,180,180,200});

    DrawCentred("ENTER — Play again", SCREEN_H/2 + 80, 20,
                (Color){160,220,160,220});
    DrawCentred("M — Main menu",      SCREEN_H/2 + 108, 20,
                (Color){160,200,160,200});

    if (IsKeyPressed(KEY_ENTER)) { InitGame(); gState = STATE_PLAYING; }
    if (IsKeyPressed(KEY_M))       gState = STATE_MENU;
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Snake");
    SetTargetFPS(FPS);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        BeginDrawing();

        switch (gState) {

            /* ── MENU ── */
            case STATE_MENU:
                UpdateDrawMenu();
                break;

            /* ── MAP SELECT ── */
            case STATE_MAPSEL:
                UpdateDrawMapSel();
                break;

            /* ── PLAYING ── */
            case STATE_PLAYING:
                HandleInput();

                /* Advance snake on timer */
                gMoveTimer += dt;
                if (gMoveTimer >= gMoveInterval) {
                    gMoveTimer -= gMoveInterval;
                    StepSnake();           /* may change gState */
                }

                /* Draw world */
                DrawBackground();
                if (gMode == MODE_MAZE)  DrawMaze();
                if (gMode != MODE_CLASSIC) DrawBorder();
                DrawFood();
                DrawSnake();
                DrawHUD();
                break;

            /* ── PAUSED ── */
            case STATE_PAUSED:
                HandleInput();   /* listens for P/ESC to unpause */
                DrawBackground();
                if (gMode == MODE_MAZE)    DrawMaze();
                if (gMode != MODE_CLASSIC) DrawBorder();
                DrawFood();
                DrawSnake();
                DrawHUD();
                DrawPauseScreen();
                break;

            /* ── GAME OVER ── */
            case STATE_GAMEOVER:
                DrawBackground();
                if (gMode == MODE_MAZE)    DrawMaze();
                if (gMode != MODE_CLASSIC) DrawBorder();
                DrawFood();
                DrawSnake();
                DrawHUD();
                DrawGameOverScreen();
                break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}