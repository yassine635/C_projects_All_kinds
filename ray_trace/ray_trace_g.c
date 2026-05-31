#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>

#define HEIGHT 600
#define WIDTH 1200

#define COLOR_WHITE 0xffffffff
#define COLOR_BLACK 0x00000000
#define COLOR_RAY   0xf5e042

#define RAY_NUMBER 500

typedef struct {
    double x;
    double y;
    double radius;
} Cercle;

typedef struct {
    double x_start, y_start;
    double dir_x, dir_y;
} Ray;

// -------------------- DRAW PIXEL --------------------
void draw_pixel(SDL_Surface* surface, int x, int y, Uint32 color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    pixels[y * WIDTH + x] = color;
}

// -------------------- DRAW FILLED CIRCLE --------------------
void Fillcercle(SDL_Surface* surface, Cercle c, Uint32 color) {
    double r2 = c.radius * c.radius;

    for (int x = (int)(c.x - c.radius); x <= (int)(c.x + c.radius); x++) {
        for (int y = (int)(c.y - c.radius); y <= (int)(c.y + c.radius); y++) {

            double dx = x - c.x;
            double dy = y - c.y;

            if (dx*dx + dy*dy <= r2) {
                draw_pixel(surface, x, y, color);
            }
        }
    }
}

// -------------------- GENERATE RAYS --------------------
void generate_rays(Cercle circle, Ray rays[RAY_NUMBER]) {
    for (int i = 0; i < RAY_NUMBER; i++) {
        double angle = ((double)i / RAY_NUMBER) * 2 * M_PI;

        rays[i].x_start = circle.x;
        rays[i].y_start = circle.y;
        rays[i].dir_x = cos(angle);
        rays[i].dir_y = sin(angle);
    }
}

// -------------------- DRAW RAYS --------------------
void Fillrays(SDL_Surface* surface, Ray rays[RAY_NUMBER], Uint32 color, Cercle obstacle) {
    double r2 = obstacle.radius * obstacle.radius;
    Uint32* pixels = (Uint32*)surface->pixels;

    double step = 0.7;

    for (int i = 0; i < RAY_NUMBER; i++) {
        double x = rays[i].x_start;
        double y = rays[i].y_start;

        while (1) {
            x += rays[i].dir_x * step;
            y += rays[i].dir_y * step;

            int xi = (int)x;
            int yi = (int)y;

            // stop if outside screen
            if (xi < 0 || xi >= WIDTH || yi < 0 || yi >= HEIGHT)
                break;

            pixels[yi * WIDTH + xi] = color;

            // check collision with circle
            double dx = x - obstacle.x;
            double dy = y - obstacle.y;

            if (dx*dx + dy*dy <= r2)
                break;
        }
    }
}

// -------------------- MAIN --------------------
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "Optimized Ray Tracing",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        0
    );

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    Cercle light = {200, 200, 40};
    Cercle obstacle = {600, 300, 90};

    Ray rays[RAY_NUMBER];

    int running = 1;
    SDL_Event event;

    double speed = 5;

    while (running) {

        // -------- EVENTS --------
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;

            if (event.type == SDL_MOUSEMOTION && event.motion.state != 0) {
                light.x = event.motion.x;
                light.y = event.motion.y;
            }
        }

        generate_rays(light, rays);

        // -------- DRAW --------
        SDL_LockSurface(surface);

        // clear screen
        Uint32* pixels = (Uint32*)surface->pixels;
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            pixels[i] = COLOR_BLACK;

        Fillrays(surface, rays, COLOR_RAY, obstacle);
        Fillcercle(surface, obstacle, COLOR_WHITE);
        Fillcercle(surface, light, COLOR_WHITE);

        SDL_UnlockSurface(surface);

        SDL_UpdateWindowSurface(window);

        // -------- MOVE OBSTACLE --------
        obstacle.y += speed;
        if (obstacle.y - obstacle.radius < 0 || obstacle.y + obstacle.radius > HEIGHT)
            speed = -speed;

        SDL_Delay(10);
    }

    SDL_Quit();
    return 0;
}