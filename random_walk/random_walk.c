#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define WIDTH       900
#define HEIGHT      600
#define SCALE       10
#define AGENT_SIZE  2
#define TARGET_FPS  60
#define FRAME_DELAY (1000 / TARGET_FPS)

/* ── Types ────────────────────────────────────────────────────────────── */

typedef struct { int vx, vy; } Velocity;

typedef struct { float r, g, b; } RGB;

typedef struct {
    int x, y;
    RGB rgb;
} Agent;

/* ── Random direction ─────────────────────────────────────────────────── */

static Velocity get_rand_velocity(void) {
    static const Velocity dirs[4] = {
        {0, -1}, {0, 1}, {-1, 0}, {1, 0}
    };
    return dirs[rand() % 4];
}

/* ── HSL → RGB conversion ─────────────────────────────────────────────── */

static float hue2rgb(float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
    if (t < 1.f / 2.f) return q;
    if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
    return p;
}

static RGB hsl2rgb(float h, float s, float l) {
    RGB result;
    if (s == 0.f) {
        result.r = result.g = result.b = l * 255.f;
    } else {
        float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
        float p = 2.f * l - q;
        result.r = hue2rgb(p, q, h + 1.f / 3.f) * 255.f;
        result.g = hue2rgb(p, q, h)              * 255.f;
        result.b = hue2rgb(p, q, h - 1.f / 3.f) * 255.f;
    }
    return result;
}

/* ── Agent initialisation ─────────────────────────────────────────────── */

static void create_agents(Agent *agents, int num_agents) {
    for (int i = 0; i < num_agents; i++) {
        float h = (float)rand() / (float)RAND_MAX;
        agents[i] = (Agent){
            .x   = WIDTH  / 2,
            .y   = HEIGHT / 2,
            .rgb = hsl2rgb(h, 1.f, 0.5f)
        };
    }
}

/* ── Move one agent and draw its trail ────────────────────────────────── */

static void move_agent(SDL_Surface *surface, Agent *agent) {
    Velocity v = get_rand_velocity();

    for (int i = 0; i < SCALE; i++) {
        agent->x += v.vx;
        agent->y += v.vy;

        /* Bounce / clamp at window edges */
        if (agent->x < 0)             { agent->x = 0;            v.vx = -v.vx; }
        if (agent->x >= WIDTH  - AGENT_SIZE) { agent->x = WIDTH  - AGENT_SIZE - 1; v.vx = -v.vx; }
        if (agent->y < 0)             { agent->y = 0;            v.vy = -v.vy; }
        if (agent->y >= HEIGHT - AGENT_SIZE) { agent->y = HEIGHT - AGENT_SIZE - 1; v.vy = -v.vy; }

        SDL_Rect rect = { agent->x, agent->y, AGENT_SIZE, AGENT_SIZE };
        Uint32 color  = SDL_MapRGB(surface->format,
                                   (Uint8)agent->rgb.r,
                                   (Uint8)agent->rgb.g,
                                   (Uint8)agent->rgb.b);
        SDL_FillRect(surface, &rect, color);
    }
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {

    int num_agents;
    if (argc == 1) {
        num_agents = 5;
    } else if (argc == 2) {
        num_agents = atoi(argv[1]);
        if (num_agents <= 0) {
            fprintf(stderr, "Number of agents must be a positive integer.\n");
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Usage: %s [num_agents]\n", argv[0]);
        return EXIT_FAILURE;
    }

    srand((unsigned)time(NULL));

    /* ── SDL init ── */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {          /* BUG FIX: was missing */
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Random Walk",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, 0);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Surface *surface = SDL_GetWindowSurface(window);

    /* Clear to black on startup */
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0));

    Agent *agents = calloc((size_t)num_agents, sizeof(Agent));
    if (!agents) {
        fprintf(stderr, "Out of memory.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    create_agents(agents, num_agents);

    /* ── Main loop ── */
    bool running = true;
    SDL_Event event;

    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        /* Events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        /* Update all agents */
        for (int i = 0; i < num_agents; i++) {
            move_agent(surface, &agents[i]);
        }

        /* Single present per frame — not once per agent */
        SDL_UpdateWindowSurface(window);

        /* Frame-rate cap */
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - elapsed);
        }
    }

    /* ── Cleanup ── */
    free(agents);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}