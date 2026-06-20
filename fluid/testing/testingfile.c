#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// SPH tuning constants
#define SMOOTHING_RADIUS  30.0f      // h — neighborhood size in pixels
#define REST_DENSITY      10.0f      // ρ₀ — target density
#define GAS_CONSTANT      10.0f     // k — stiffness
#define VISCOSITY         0.5f     // μ — resistance to flow
#define PARTICLE_MASS     1.0f
#define GRAVITY           400.0f     // pixels/s²
#define DAMPING           0.04f      // bounce energy loss on walls
#define DT                0.016f     // timestep
#define RENDER_RADIUS     10         // visual size

const int Width = 1000;
const int Height = 800;
const int FPS = 30;
#define maxparticles 1000

// Spatial Grid Constants for Optimization
#define CELL_SIZE       ((int)(SMOOTHING_RADIUS) + 1)
#define GRID_COLS       (1000 / CELL_SIZE + 1)
#define GRID_ROWS       (800 / CELL_SIZE + 1)
#define MAX_PER_CELL    50

typedef struct Particle {
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float   density;
    float   pressure;
    int     size;
    float   mass;
} Particle;

// Uniform grid arrays
static int grid[GRID_ROWS][GRID_COLS][MAX_PER_CELL];
static int grid_count[GRID_ROWS][GRID_COLS];

// SPH Kernel Pre-computed Scaling Coefficients (HUGE performance boost!)
static float POLY6_SCALE;
static float SPIKY_GRAD_SCALE;
static float VISC_LAP_SCALE;

void InitKernelScales() {
    POLY6_SCALE = 315.0f / (64.0f * PI * powf(SMOOTHING_RADIUS, 9));
    SPIKY_GRAD_SCALE = -45.0f / (PI * powf(SMOOTHING_RADIUS, 6));
    VISC_LAP_SCALE = 45.0f / (PI * powf(SMOOTHING_RADIUS, 6));
}

// Rebuild spatial grid bucketing
void RebuildGrid(Particle *p, int count) {
    memset(grid_count, 0, sizeof(grid_count));
    for (int i = 0; i < count; i++) {
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);
        cx = Clamp(cx, 0, GRID_COLS - 1);
        cy = Clamp(cy, 0, GRID_ROWS - 1);

        int cnt = grid_count[cy][cx];
        if (cnt < MAX_PER_CELL) {
            grid[cy][cx][cnt] = i;
            grid_count[cy][cx]++;
        }
    }
}

// Optimized Kernel inline operations
float poly6(float r_sq, float h_sq) {
    if (r_sq >= h_sq) return 0.0f;
    float x = h_sq - r_sq;
    return POLY6_SCALE * x * x * x;
}

float spiky_grad(float r, float h) {
    if (r <= 0 || r > h) return 0.0f;
    float x = h - r;
    return SPIKY_GRAD_SCALE * x * x;
}

float viscosity_lap(float r, float h) {
    if (r < 0 || r > h) return 0.0f;
    return VISC_LAP_SCALE * (h - r);
}

void compute_density_pressure(Particle *p, int count) {
    float h_sq = SMOOTHING_RADIUS * SMOOTHING_RADIUS;

    for (int i = 0; i < count; i++) {
        p[i].density = 0.0f;
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        // Scan only the 9 neighboring grid buckets
        for (int _gy = cy - 1; _gy <= cy + 1; _gy++) {
            if (_gy < 0 || _gy >= GRID_ROWS) continue;
            for (int _gx = cx - 1; _gx <= cx + 1; _gx++) {
                if (_gx < 0 || _gx >= GRID_COLS) continue;

                int cell_cnt = grid_count[_gy][_gx];
                for (int k = 0; k < cell_cnt; k++) {
                    int j = grid[_gy][_gx][k];
                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float r_sq = dx * dx + dy * dy;

                    p[i].density += p[j].mass * poly6(r_sq, h_sq);
                }
            }
        }
        p[i].pressure = GAS_CONSTANT * (p[i].density - REST_DENSITY);
    }
}

void compute_forces(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        Vector2 f_pressure  = {0, 0};
        Vector2 f_viscosity = {0, 0};
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        for (int _gy = cy - 1; _gy <= cy + 1; _gy++) {
            if (_gy < 0 || _gy >= GRID_ROWS) continue;
            for (int _gx = cx - 1; _gx <= cx + 1; _gx++) {
                if (_gx < 0 || _gx >= GRID_COLS) continue;

                int cell_cnt = grid_count[_gy][_gx];
                for (int k = 0; k < cell_cnt; k++) {
                    int j = grid[_gy][_gx][k];
                    if (i == j) continue;

                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float r_sq = dx * dx + dy * dy;

                    if (r_sq < SMOOTHING_RADIUS * SMOOTHING_RADIUS && r_sq > 1e-4f) {
                        float r = sqrtf(r_sq);
                        float nx = (p[i].position.x - p[j].position.x) / r;
                        float ny = (p[i].position.y - p[j].position.y) / r;

                        float avg_pressure = (p[i].pressure + p[j].pressure) * 0.5f;
                        float grad = spiky_grad(r, SMOOTHING_RADIUS);
                        float pressure_scale = -p[j].mass * avg_pressure / p[j].density * grad;
                        f_pressure.x += pressure_scale * nx;
                        f_pressure.y += pressure_scale * ny;

                        float lap = viscosity_lap(r, SMOOTHING_RADIUS);
                        float visc_scale = VISCOSITY * p[j].mass / p[j].density * lap;
                        f_viscosity.x += visc_scale * (p[j].velocity.x - p[i].velocity.x);
                        f_viscosity.y += visc_scale * (p[j].velocity.y - p[i].velocity.y);
                    }
                }
            }
        }
        Vector2 f_gravity = { 0, GRAVITY * p[i].density };
        p[i].force.x = f_pressure.x + f_viscosity.x + f_gravity.x;
        p[i].force.y = f_pressure.y + f_viscosity.y + f_gravity.y;
    }
}

void integrate(Particle *p, int count, int width, int height) {
    for (int i = 0; i < count; i++) {
        if (p[i].density > 1e-4f) {
            p[i].velocity.x += DT * p[i].force.x / p[i].density;
            p[i].velocity.y += DT * p[i].force.y / p[i].density;
        }
        p[i].position.x += DT * p[i].velocity.x;
        p[i].position.y += DT * p[i].velocity.y;

        if (p[i].position.x - RENDER_RADIUS < 0) {
            p[i].velocity.x *= -DAMPING;
            p[i].position.x  = RENDER_RADIUS;
        }
        if (p[i].position.x + RENDER_RADIUS > width) {
            p[i].velocity.x *= -DAMPING;
            p[i].position.x  = width - RENDER_RADIUS;
        }
        if (p[i].position.y - RENDER_RADIUS < 0) {
            p[i].velocity.y *= -DAMPING;
            p[i].position.y  = RENDER_RADIUS;
        }
        if (p[i].position.y + RENDER_RADIUS > height) {
            p[i].velocity.y *= -DAMPING;
            p[i].position.y  = height - RENDER_RADIUS;
        }
    }
}

void ResolveParticleCollisions(Particle *p, int count) {
    float min_dist = RENDER_RADIUS * 2.0f;
    float min_dist_sq = min_dist * min_dist;
    
    for (int i = 0; i < count; i++) {
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        for (int _gy = cy - 1; _gy <= cy + 1; _gy++) {
            if (_gy < 0 || _gy >= GRID_ROWS) continue;
            for (int _gx = cx - 1; _gx <= cx + 1; _gx++) {
                if (_gx < 0 || _gx >= GRID_COLS) continue;

                int cell_cnt = grid_count[_gy][_gx];
                for (int k = 0; k < cell_cnt; k++) {
                    int j = grid[_gy][_gx][k];
                    if (i >= j) continue; // Prevent processing the pair twice

                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float dist_sq = dx * dx + dy * dy;

                    if (dist_sq < min_dist_sq) {
                        float dist = sqrtf(dist_sq);
                        if (dist < 0.001f) {
                            p[j].position.x += 0.1f;
                            continue;
                        }

                        float overlap = min_dist - dist;
                        float nx = dx / dist;
                        float ny = dy / dist;

                        p[i].position.x -= nx * overlap * 0.5f;
                        p[i].position.y -= ny * overlap * 0.5f;
                        p[j].position.x += nx * overlap * 0.5f;
                        p[j].position.y += ny * overlap * 0.5f;

                        float kx = p[i].velocity.x - p[j].velocity.x;
                        float ky = p[i].velocity.y - p[j].velocity.y;
                        float p_ratio = (kx * nx + ky * ny) * 0.5f;
                        if (p_ratio > 0) {
                            p[i].velocity.x -= p_ratio * nx;
                            p[i].velocity.y -= p_ratio * ny;
                            p[j].velocity.x += p_ratio * nx;
                            p[j].velocity.y += p_ratio * ny;
                        }
                    }
                }
            }
        }
    }
}

void draw_particles(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        DrawCircleV(p[i].position, RENDER_RADIUS, BLUE);
    }
}

void Initparticles(Particle *particles, int count) {
    for (int i = 0; i < count; i++) {
        particles[i].position = (Vector2){GetRandomValue(200, Width - 200), GetRandomValue(50, 400)};
        particles[i].velocity = (Vector2){0, 0};
        particles[i].force = (Vector2){0, 0};
        particles[i].mass = PARTICLE_MASS;
    }
}

int main() {
    Particle particles[maxparticles];
    InitKernelScales();
    Initparticles(particles, maxparticles);
    
    InitWindow(Width, Height, "Optimized SPH Fluid Simulation");
    SetTargetFPS(FPS);

    while (!WindowShouldClose()) {
        // Spatial partitioning step
        RebuildGrid(particles, maxparticles);

        // Core physics calculations utilizing the spatial grid
        compute_density_pressure(particles, maxparticles);
        compute_forces(particles, maxparticles);
        integrate(particles, maxparticles, Width, Height);
        ResolveParticleCollisions(particles, maxparticles);

        BeginDrawing();
        ClearBackground(WHITE);
        draw_particles(particles, maxparticles);
        DrawFPS(14, 14);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}