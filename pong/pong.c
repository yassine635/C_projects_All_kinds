#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* ── updated window & layout matching web canvas boundaries ──────────────── */
#define SCREEN_W        800
#define SCREEN_H        500
#define TARGET_FPS      60

/* ── gameplay constants ──────────────────────────────── */
#define PADDLE_W        14
#define PADDLE_H        90
#define PADDLE_SPEED    420.0f
#define BALL_SIZE       14
#define BALL_INIT_SPEED 350.0f
#define BALL_SPEEDUP    18.0f     
#define BALL_MAX_SPEED  780.0f
#define SCORE_TO_WIN    7

/* ── particle system ─────────────────────────────────── */
#define MAX_PARTICLES   120

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float   life;       
    float   size;
    Color   color;
    bool    active;
} Particle;

/* ── difficulty levels ───────────────────────────────── */
typedef enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD } Difficulty;

/* ── game state ──────────────────────────────────────── */
typedef enum { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_WIN } GameState;

typedef struct {
    Rectangle p1, p2;
    Vector2   ball;
    Vector2   ballVel;
    float     ballAngle;    
    int       score1, score2;
    GameState state;
    int       winner;       
    float     flashTimer;
    float     shakeTimer;
    Vector2   trail[20];
    int       trailIdx;
    Particle  particles[MAX_PARTICLES];
    float     serveTimer;
    bool      serving;
    Difficulty difficulty;
} Game;

// Global variables for Emscripten frame loop compatibility
Game g = {0};
RenderTexture2D screen;

/* ──────────────────────────────────────────────────────
   HELPERS / LOGIC
   ────────────────────────────────────────────────────── */

static void SpawnParticles(Game *g, Vector2 pos, Color base, int count)
{
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++) {
        if (!g->particles[i].active) {
            float angle = (float)(rand() % 360) * DEG2RAD;
            float speed = 80.0f + (rand() % 220);
            g->particles[i].pos   = pos;
            g->particles[i].vel   = (Vector2){ cosf(angle)*speed, sinf(angle)*speed };
            g->particles[i].life  = 1.0f;
            g->particles[i].size  = 3.0f + (rand() % 6);
            g->particles[i].color = base;
            g->particles[i].active = true;
            spawned++;
        }
    }
}

static void UpdateParticles(Game *g, float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g->particles[i].active) continue;
        g->particles[i].pos.x += g->particles[i].vel.x * dt;
        g->particles[i].pos.y += g->particles[i].vel.y * dt;
        g->particles[i].vel.x *= 0.92f;
        g->particles[i].vel.y *= 0.92f;
        g->particles[i].life  -= dt * 1.4f;
        if (g->particles[i].life <= 0.0f) g->particles[i].active = false;
    }
}

static void DrawParticles(const Game *g)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g->particles[i].active) continue;
        float t   = g->particles[i].life;
        Color col = g->particles[i].color;
        col.a     = (unsigned char)(t * 255);
        float sz  = g->particles[i].size * t;
        DrawCircleV(g->particles[i].pos, sz, col);
    }
}

static void PushTrail(Game *g)
{
    g->trail[g->trailIdx] = g->ball;
    g->trailIdx = (g->trailIdx + 1) % 20;
}

static void DrawTrail(const Game *g)
{
    for (int i = 0; i < 20; i++) {
        int idx = (g->trailIdx - 1 - i + 20) % 20;
        float t = 1.0f - (i / 20.0f);
        Color c = { 0, 220, 255, (unsigned char)(t * t * 130) };
        float sz = (BALL_SIZE * 0.5f) * t;
        DrawCircleV(g->trail[idx], sz, c);
    }
}

/* ── BOT MOVEMENT ── */
static void UpdateBot(Game *g, float dt)
{
    float botSpeed = PADDLE_SPEED;
    float targetY = g->ball.y - g->p2.height / 2.0f;
    
    switch (g->difficulty) {
        case DIFF_EASY:
            botSpeed *= 0.45f;      
            if ((rand() % 60) < 2) targetY += (rand() % 60) - 30;
            break;
        case DIFF_NORMAL:
            botSpeed *= 0.85f;      
            break;
        case DIFF_HARD:
            botSpeed *= 1.4f;       
            targetY = g->ball.y + g->ballVel.y * 0.2f - g->p2.height / 2.0f;
            break;
    }
    
    if (g->p2.y < targetY) {
        g->p2.y += botSpeed * dt;
        if (g->p2.y > targetY) g->p2.y = targetY;
    } else if (g->p2.y > targetY) {
        g->p2.y -= botSpeed * dt;
        if (g->p2.y < targetY) g->p2.y = targetY;
    }
    
    if (g->p2.y < 0) g->p2.y = 0;
    if (g->p2.y + g->p2.height > SCREEN_H) g->p2.y = SCREEN_H - g->p2.height;
}

static void ResetBall(Game *g, int towardsPlayer)
{
    g->ball = (Vector2){ SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    float dir = (towardsPlayer == 2) ? 1.0f : -1.0f;
    float angle = ((float)(rand() % 60) - 30.0f) * DEG2RAD;
    g->ballVel = (Vector2){
        dir * cosf(angle) * BALL_INIT_SPEED,
        sinf(angle) * BALL_INIT_SPEED
    };
    g->serving   = true;
    g->serveTimer = 1.2f;
    for (int i = 0; i < 20; i++) g->trail[i] = g->ball;
}

static void InitGame(Game *g)
{
    g->p1 = (Rectangle){ 30, SCREEN_H/2.0f - PADDLE_H/2.0f, PADDLE_W, PADDLE_H };
    g->p2 = (Rectangle){ SCREEN_W - 30 - PADDLE_W, SCREEN_H/2.0f - PADDLE_H/2.0f, PADDLE_W, PADDLE_H };
    g->score1    = 0;
    g->score2    = 0;
    g->winner    = 0;
    g->flashTimer = 0;
    g->shakeTimer = 0;
    g->ballAngle  = 0;
    g->trailIdx   = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) g->particles[i].active = false;
    ResetBall(g, 1);
    g->state = STATE_PLAYING;
}

static void UpdateGame(Game *g, float dt)
{
    if (g->state == STATE_PAUSED) {
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) g->state = STATE_PLAYING;
        return;
    }
    if (g->state == STATE_WIN) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) InitGame(g);
        return;
    }

    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        g->state = STATE_PAUSED;
        return;
    }

    if (g->serving) {
        g->serveTimer -= dt;
        if (g->serveTimer <= 0) g->serving = false;
    }

    float move = PADDLE_SPEED * dt;
    if (IsKeyDown(KEY_W) && g->p1.y > 0)               g->p1.y -= move;
    if (IsKeyDown(KEY_S) && g->p1.y + g->p1.height < SCREEN_H) g->p1.y += move;

    UpdateBot(g, dt);

    if (g->serving) return;   

    PushTrail(g);
    g->ball.x += g->ballVel.x * dt;
    g->ball.y += g->ballVel.y * dt;
    g->ballAngle += 180.0f * dt;   

    if (g->ball.y - BALL_SIZE/2.0f <= 0) {
        g->ball.y = BALL_SIZE/2.0f;
        g->ballVel.y = fabsf(g->ballVel.y);
        SpawnParticles(g, g->ball, WHITE, 8);
    }
    if (g->ball.y + BALL_SIZE/2.0f >= SCREEN_H) {
        g->ball.y = SCREEN_H - BALL_SIZE/2.0f;
        g->ballVel.y = -fabsf(g->ballVel.y);
        SpawnParticles(g, g->ball, WHITE, 8);
    }

    Rectangle ballRect = { g->ball.x - BALL_SIZE/2.0f, g->ball.y - BALL_SIZE/2.0f, (float)BALL_SIZE, (float)BALL_SIZE };

    if (CheckCollisionRecs(ballRect, g->p1) && g->ballVel.x < 0) {
        float rel = (g->ball.y - (g->p1.y + g->p1.height/2.0f)) / (g->p1.height/2.0f);
        float bounceAngle = rel * 60.0f * DEG2RAD;
        float spd = fminf(sqrtf(g->ballVel.x*g->ballVel.x + g->ballVel.y*g->ballVel.y) + BALL_SPEEDUP, BALL_MAX_SPEED);
        g->ballVel.x =  cosf(bounceAngle) * spd;
        g->ballVel.y =  sinf(bounceAngle) * spd;
        g->ball.x = g->p1.x + g->p1.width + BALL_SIZE/2.0f + 1;
        g->flashTimer = 0.08f;
        g->shakeTimer = 0.12f;
        SpawnParticles(g, g->ball, (Color){0,220,255,255}, 22);
    }

    if (CheckCollisionRecs(ballRect, g->p2) && g->ballVel.x > 0) {
        float rel = (g->ball.y - (g->p2.y + g->p2.height/2.0f)) / (g->p2.height/2.0f);
        float bounceAngle = rel * 60.0f * DEG2RAD;
        float spd = fminf(sqrtf(g->ballVel.x*g->ballVel.x + g->ballVel.y*g->ballVel.y) + BALL_SPEEDUP, BALL_MAX_SPEED);
        g->ballVel.x = -cosf(bounceAngle) * spd;
        g->ballVel.y =  sinf(bounceAngle) * spd;
        g->ball.x = g->p2.x - BALL_SIZE/2.0f - 1;
        g->flashTimer = 0.08f;
        g->shakeTimer = 0.12f;
        SpawnParticles(g, g->ball, (Color){255,80,120,255}, 22);
    }

    if (g->ball.x < -BALL_SIZE) {
        g->score2++;
        SpawnParticles(g, (Vector2){0, g->ball.y}, (Color){255,80,120,255}, 40);
        g->shakeTimer = 0.3f;
        if (g->score2 >= SCORE_TO_WIN) { g->state = STATE_WIN; g->winner = 2; }
        else ResetBall(g, 2);
    }
    if (g->ball.x > SCREEN_W + BALL_SIZE) {
        g->score1++;
        SpawnParticles(g, (Vector2){(float)SCREEN_W, g->ball.y}, (Color){0,220,255,255}, 40);
        g->shakeTimer = 0.3f;
        if (g->score1 >= SCORE_TO_WIN) { g->state = STATE_WIN; g->winner = 1; }
        else ResetBall(g, 1);
    }

    if (g->flashTimer > 0) g->flashTimer -= dt;
    if (g->shakeTimer > 0) g->shakeTimer -= dt;
    UpdateParticles(g, dt);
}

/* ── DRAW CORES ── */
static void DrawNeonRect(Rectangle r, Color col, int glowLayers)
{
    for (int i = glowLayers; i >= 0; i--) {
        Color gc = col;
        gc.a = (i == 0) ? 255 : (unsigned char)(40 - i*4);
        float expand = (float)i * 3.0f;
        DrawRectangleRec((Rectangle){ r.x-expand, r.y-expand, r.width+expand*2, r.height+expand*2 }, gc);
    }
}

static void DrawNeonCircle(Vector2 c, float r, Color col, int glowLayers)
{
    for (int i = glowLayers; i >= 0; i--) {
        Color gc = col;
        gc.a = (i == 0) ? 255 : (unsigned char)(35 - i*3);
        DrawCircleV(c, r + (float)i*3.0f, gc);
    }
}

static void DrawDashedLine(void)
{
    int segments = 20;
    float segH   = SCREEN_H / (float)(segments * 2 - 1);
    for (int i = 0; i < segments; i++) {
        float y = i * segH * 2;
        Color col = { 255, 255, 255, 60 };
        DrawRectangle(SCREEN_W/2 - 2, (int)y, 4, (int)segH, col);
    }
}

static void DrawScore(int score, int cx, int cy, Color col)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", score);
    int fontSize = 90;
    int tw = MeasureText(buf, fontSize);
    for (int i = 4; i >= 0; i--) {
        Color gc = col;
        gc.a = (i == 0) ? 220 : (unsigned char)(25 - i*4);
        DrawText(buf, cx - tw/2 + i, cy + i, fontSize, gc);
        DrawText(buf, cx - tw/2 - i, cy - i, fontSize, gc);
    }
    DrawText(buf, cx - tw/2, cy, fontSize, col);
}

static void DrawMenu(Game *g)
{
    DrawCircle(SCREEN_W/2, SCREEN_H/2, 220, (Color){0,180,255,18});
    DrawCircle(SCREEN_W/2, SCREEN_H/2, 150, (Color){0,180,255,14});

    const char *title = "PONG";
    int tsz = 120;
    int tw  = MeasureText(title, tsz);
    for (int i = 6; i >= 0; i--) {
        Color gc = { 0, 220, 255, (unsigned char)(20 - i*2) };
        DrawText(title, SCREEN_W/2 - tw/2 - i, 120 - i, tsz, gc);
        DrawText(title, SCREEN_W/2 - tw/2 + i, 120 + i, tsz, gc);
    }
    DrawText(title, SCREEN_W/2 - tw/2, 120, tsz, (Color){0,220,255,255});
    DrawText("A CLASSIC REIMAGINED", SCREEN_W/2 - MeasureText("A CLASSIC REIMAGINED",18)/2, 255, 18, (Color){255,255,255,120});

    DrawRectangle(SCREEN_W/2 - 300, 300, 600, 170, (Color){255,255,255,10});
    DrawRectangleLines(SCREEN_W/2 - 300, 300, 600, 170, (Color){255,255,255,40});
    DrawText("DIFFICULTY", SCREEN_W/2 - MeasureText("DIFFICULTY",22)/2, 320, 22, WHITE);

    const char *easyText   = "1  -  EASY     (slow bot)";
    const char *normalText = "2  -  NORMAL   (balanced)";
    const char *hardText   = "3  -  HARD     (fast bot)";

    Color easyCol   = (g->difficulty == DIFF_EASY)   ? (Color){0,220,255,255} : WHITE;
    Color normalCol = (g->difficulty == DIFF_NORMAL) ? (Color){0,220,255,255} : WHITE;
    Color hardCol   = (g->difficulty == DIFF_HARD)   ? (Color){0,220,255,255} : WHITE;

    DrawText(easyText,   SCREEN_W/2 - 180, 360, 18, easyCol);
    DrawText(normalText, SCREEN_W/2 - 180, 390, 18, normalCol);
    DrawText(hardText,   SCREEN_W/2 - 180, 420, 18, hardCol);

    float pulse = (sinf((float)GetTime()*3.0f)+1.0f)*0.5f;
    Color pc = { 255,255,255, (unsigned char)(100 + pulse*155) };
    DrawText("PRESS  ENTER  TO  START", SCREEN_W/2 - MeasureText("PRESS  ENTER  TO  START",22)/2, 510, 22, pc);
}

static void DrawHUD(const Game *g)
{
    DrawText("PLAYER", 55, 14, 18, (Color){0,220,255,150});
    DrawText("BOT", SCREEN_W - 55 - MeasureText("BOT",18), 14, 18, (Color){255,80,120,150});

    const char *difText = "";
    Color difColor = (Color){255,255,255,100};
    switch (g->difficulty) {
        case DIFF_EASY:   difText = "EASY";   difColor = (Color){100,255,100,180}; break;
        case DIFF_NORMAL: difText = "NORMAL"; difColor = (Color){255,255,100,180}; break;
        case DIFF_HARD:   difText = "HARD";   difColor = (Color){255,100,100,180}; break;
    }
    DrawText(difText, SCREEN_W/2 - MeasureText(difText,14)/2, 10, 14, difColor);

    DrawScore(g->score1, SCREEN_W/4,     14, (Color){0,220,255,255});
    DrawScore(g->score2, SCREEN_W*3/4,   14, (Color){255,80,120,255});

    for (int i = 0; i < SCORE_TO_WIN; i++) {
        Color fc = (i < g->score1) ? (Color){0,220,255,255} : (Color){255,255,255,40};
        DrawCircle(SCREEN_W/4 - (SCORE_TO_WIN*14)/2 + i*14 + 7, 122, 5, fc);
    }
    for (int i = 0; i < SCORE_TO_WIN; i++) {
        Color fc = (i < g->score2) ? (Color){255,80,120,255} : (Color){255,255,255,40};
        DrawCircle(SCREEN_W*3/4 - (SCORE_TO_WIN*14)/2 + i*14 + 7, 122, 5, fc);
    }

    if (g->serving) {
        int cnt = (int)(g->serveTimer) + 1;
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", cnt);
        float pulse = (sinf((float)GetTime()*6.0f)+1.0f)*0.5f;
        Color pc = {255,255,255,(unsigned char)(150+pulse*105)};
        DrawText(buf, SCREEN_W/2 - MeasureText(buf,60)/2, SCREEN_H/2 - 30, 60, pc);
    }
    DrawText("P = Pause", SCREEN_W/2 - MeasureText("P = Pause",14)/2, SCREEN_H - 22, 14, (Color){255,255,255,50});
}

static void DrawPauseOverlay(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,140});
    DrawText("PAUSED", SCREEN_W/2 - MeasureText("PAUSED",80)/2, SCREEN_H/2 - 60, 80, WHITE);
    DrawText("Press  P  to resume", SCREEN_W/2 - MeasureText("Press  P  to resume",22)/2, SCREEN_H/2 + 40, 22, (Color){255,255,255,160});
}

static void DrawWinScreen(const Game *g)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,160});
    Color wc = (g->winner == 1) ? (Color){0,220,255,255} : (Color){255,80,120,255};
    char buf[32];
    snprintf(buf, sizeof(buf), g->winner == 1 ? "YOU WIN!" : "BOT WINS!");
    int tsz = 70;
    int tw  = MeasureText(buf, tsz);
    for (int i = 5; i >= 0; i--) {
        Color gc = wc; gc.a = (unsigned char)(20 - i*3);
        DrawText(buf, SCREEN_W/2-tw/2-i, SCREEN_H/2-80-i, tsz, gc);
        DrawText(buf, SCREEN_W/2-tw/2+i, SCREEN_H/2-80+i, tsz, gc);
    }
    DrawText(buf, SCREEN_W/2-tw/2, SCREEN_H/2-80, tsz, wc);

    char sb[24];
    snprintf(sb, sizeof(sb), "%d  —  %d", g->score1, g->score2);
    DrawText(sb, SCREEN_W/2 - MeasureText(sb,42)/2, SCREEN_H/2 + 10, 42, WHITE);

    float pulse = (sinf((float)GetTime()*3.0f)+1.0f)*0.5f;
    Color pc = {255,255,255,(unsigned char)(100+pulse*155)};
    DrawText("Press  ENTER  to play again", SCREEN_W/2 - MeasureText("Press  ENTER  to play again",20)/2, SCREEN_H/2+100, 20, pc);
}

/* ── Web Assembly Frame Loop Wrapper Context ── */
void GameUpdateFrame(void)
{
    float dt = GetFrameTime();
    if (dt > 0.05f) dt = 0.05f;

    if (g.state == STATE_MENU) {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) g.difficulty = DIFF_EASY;
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) g.difficulty = DIFF_NORMAL;
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) g.difficulty = DIFF_HARD;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) InitGame(&g);
    } else {
        UpdateGame(&g, dt);
    }

    BeginTextureMode(screen);
        ClearBackground((Color){8, 8, 18, 255});

        if (g.state == STATE_MENU) {
            DrawMenu(&g);
        } else {
            for (int x = 0; x < SCREEN_W; x += 60) DrawLine(x, 0, x, SCREEN_H, (Color){255,255,255,8});
            for (int y = 0; y < SCREEN_H; y += 60) DrawLine(0, y, SCREEN_W, y, (Color){255,255,255,8});

            DrawDashedLine();
            DrawRectangleLinesEx((Rectangle){0,0,SCREEN_W,SCREEN_H}, 3, (Color){255,255,255,40});
            DrawRectangleLinesEx((Rectangle){2,2,SCREEN_W-4,SCREEN_H-4}, 1, (Color){255,255,255,15});

            if (g.flashTimer > 0) {
                float t = g.flashTimer / 0.08f;
                DrawRectangle(0,0,SCREEN_W,SCREEN_H, (Color){255,255,255,(unsigned char)(t*35)});
            }

            DrawTrail(&g);
            DrawNeonCircle(g.ball, BALL_SIZE/2.0f, (Color){0,220,255,255}, 6);
            DrawNeonRect(g.p1, (Color){0,220,255,255},  7);
            DrawNeonRect(g.p2, (Color){255,80,120,255}, 7);

            DrawCircle((int)(g.p1.x + g.p1.width/2), (int)(g.p1.y), (int)(g.p1.width/2), (Color){0,220,255,255});
            DrawCircle((int)(g.p1.x + g.p1.width/2), (int)(g.p1.y + g.p1.height), (int)(g.p1.width/2), (Color){0,220,255,255});
            DrawCircle((int)(g.p2.x + g.p2.width/2), (int)(g.p2.y), (int)(g.p2.width/2), (Color){255,80,120,255});
            DrawCircle((int)(g.p2.x + g.p2.width/2), (int)(g.p2.y + g.p2.height), (int)(g.p2.width/2), (Color){255,80,120,255});

            DrawParticles(&g);
            DrawHUD(&g);

            if (g.state == STATE_PAUSED) DrawPauseOverlay();
            if (g.state == STATE_WIN)    DrawWinScreen(&g);
        }
    EndTextureMode();

    BeginDrawing();
        ClearBackground(BLACK);
        float ox = 0, oy = 0;
        if (g.shakeTimer > 0) {
            float mag = (g.shakeTimer / 0.3f) * 7.0f;
            ox = (float)(rand() % (int)(mag*2+1)) - mag;
            oy = (float)(rand() % (int)(mag*2+1)) - mag;
        }
        DrawTextureRec(screen.texture, (Rectangle){0, 0, (float)SCREEN_W, -(float)SCREEN_H}, (Vector2){ox, oy}, WHITE);
    EndDrawing();
}

int main(void)
{
    srand((unsigned)time(NULL));
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "PONG");

    screen = LoadRenderTexture(SCREEN_W, SCREEN_H);
    g.state = STATE_MENU;
    g.difficulty = DIFF_NORMAL;  

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(GameUpdateFrame, 0, 1);
#else
    SetTargetFPS(TARGET_FPS);
    while (!WindowShouldClose()) {
        GameUpdateFrame();
    }
    UnloadRenderTexture(screen);
    CloseWindow();
#endif
    return 0;
}