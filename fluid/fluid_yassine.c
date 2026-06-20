#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include <stdbool.h>

// SPH tuning constants
#define SMOOTHING_RADIUS 30.0f // h — neighborhood size in pixels
#define REST_DENSITY 10.0f     // ρ₀ — target density
#define GAS_CONSTANT 20.0f     // k — stiffness (higher = less compressible)
#define VISCOSITY 0.1f         // μ — resistance to flow
#define PARTICLE_MASS 1.0f
#define GRAVITY 600.0f   // pixels/s²
#define DAMPING 0.98f    // bounce energy loss on walls
#define DT 0.05f        // timestep (match your frame rate)
#define RENDER_RADIUS 10 // visual size — global, not per-particle
#define CEVULE_BALL 100  // number of particles to spawn

const int Width = 1000;
const int Height = 800;
const int FPS = 30;
const int maxparticles = 1000;
const float G = 900;
bool generated = false;
int particle_count = maxparticles;

typedef struct Particle
{
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float density;
    float pressure;
    int size;
    float mass;
} Particle;

#define CEVULE_BALL 100 // number of particles in the circle (you can make this radius too)

void SpawnCircle(Particle *particles, int *count, Vector2 center) {
    int num = CEVULE_BALL;
    int start_idx = *count;  // Start appending after existing particles
    int new_total = start_idx + num;

    if (new_total > maxparticles) {
        new_total = maxparticles;
        num = maxparticles - start_idx;  // Add as many as we can
    }

    if (num <= 0) return;  // Array is full

    float radius = 60.0f;
    for (int i = 0; i < num; i++) {
        int idx = start_idx + i;
        float angle = GetRandomValue(0, 628) / 100.0f;
        float dist = sqrtf((float)GetRandomValue(0, 10000) / 10000.0f) * radius;
        particles[idx].position = (Vector2){ center.x + cosf(angle) * dist, center.y + sinf(angle) * dist };
        particles[idx].velocity = (Vector2){0, 0};
        particles[idx].force = (Vector2){0, 0};
        particles[idx].mass = PARTICLE_MASS;
        particles[idx].size = RENDER_RADIUS;
    }
    *count = new_total;  // Update the count to include the new ones
}

// Poly6 — used for density
float poly6(float r, float h)
{
    if (r < 0 || r > h)
        return 0.0f;
    float x = (h * h) - (r * r);
    return (315.0f / (64.0f * PI * powf(h, 9))) * x * x * x;
}

// Spiky gradient magnitude — used for pressure force
float spiky_grad(float r, float h)
{
    if (r <= 0 || r > h)
        return 0.0f;
    float x = h - r;
    return -(45.0f / (PI * powf(h, 6))) * x * x;
}

// Viscosity laplacian — used for viscosity force
float viscosity_lap(float r, float h)
{
    if (r < 0 || r > h)
        return 0.0f;
    return (45.0f / (PI * powf(h, 6))) * (h - r);
}

void compute_density_pressure(Particle *p, int count)
{
    for (int i = 0; i < count; i++)
    {
        p[i].density = 0.0f;

        for (int j = 0; j < count; j++)
        {
            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float r = sqrtf(dx * dx + dy * dy);

            p[i].density += p[j].mass * poly6(r, SMOOTHING_RADIUS);
        }

        // Equation of state: P = k(ρ - ρ₀)
        p[i].pressure = GAS_CONSTANT * (p[i].density - REST_DENSITY);
        // After the j-loop inside compute_density_pressure:
        if (p[i].density < 0.001f)
            p[i].density = 0.001f;
    }
}

void compute_forces(Particle *p, int count)
{
    for (int i = 0; i < count; i++)
    {
        Vector2 f_pressure = {0, 0};
        Vector2 f_viscosity = {0, 0};

        for (int j = 0; j < count; j++)
        {
            if (i == j)
                continue;

            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float r = sqrtf(dx * dx + dy * dy);

            if (r < SMOOTHING_RADIUS && r > 0.001f)
            {
                // Direction from j to i (pressure pushes outward)
                float nx = (p[i].position.x - p[j].position.x) / r;
                float ny = (p[i].position.y - p[j].position.y) / r;

                // Pressure force (symmetric)
                float avg_pressure = (p[i].pressure + p[j].pressure) / 2.0f;
                float grad = spiky_grad(r, SMOOTHING_RADIUS);
                float pressure_scale = -p[j].mass * avg_pressure / p[j].density * grad;
                f_pressure.x += pressure_scale * nx;
                f_pressure.y += pressure_scale * ny;

                // Viscosity force
                float lap = viscosity_lap(r, SMOOTHING_RADIUS);
                float visc_scale = VISCOSITY * p[j].mass / p[j].density * lap;
                f_viscosity.x += visc_scale * (p[j].velocity.x - p[i].velocity.x);
                f_viscosity.y += visc_scale * (p[j].velocity.y - p[i].velocity.y);
            }
        }

        // Gravity
        Vector2 f_gravity = {0, GRAVITY * p[i].density};

        p[i].force.x = f_pressure.x + f_viscosity.x + f_gravity.x;
        p[i].force.y = f_pressure.y + f_viscosity.y + f_gravity.y;
    }
}

void integrate(Particle *p, int count, int width, int height)
{
    for (int i = 0; i < count; i++)
    {
        // a = F / ρ  (not F/m — SPH uses density here)
        p[i].velocity.x += DT * p[i].force.x / p[i].density;
        p[i].velocity.y += DT * p[i].force.y / p[i].density;
        p[i].position.x += DT * p[i].velocity.x;
        p[i].position.y += DT * p[i].velocity.y;

        // Boundary collisions
        if (p[i].position.x - RENDER_RADIUS < 0)
        {
            p[i].velocity.x *= -DAMPING;
            p[i].position.x = RENDER_RADIUS;
        }
        if (p[i].position.x + RENDER_RADIUS > width)
        {
            p[i].velocity.x *= -DAMPING;
            p[i].position.x = width - RENDER_RADIUS;
        }
        if (p[i].position.y - RENDER_RADIUS < 0)
        {
            p[i].velocity.y *= -DAMPING;
            p[i].position.y = RENDER_RADIUS;
        }
        if (p[i].position.y + RENDER_RADIUS > height)
        {
            p[i].velocity.y *= -DAMPING;
            p[i].position.y = height - RENDER_RADIUS;
        }
    }
}
void draw_particles(Particle *p, int count)
{
    for (int i = 0; i < count; i++)
    {
        // Color by speed for nice visuals
        float speed = sqrtf(p[i].velocity.x * p[i].velocity.x +
                            p[i].velocity.y * p[i].velocity.y);
        float t = fminf(speed / 300.0f, 1.0f); // normalize

        Color col = ColorLerp(BLUE, RED, t); // slow=blue, fast=red
        DrawCircleV(p[i].position, RENDER_RADIUS, col);
    }
}

// In your main loop:
// compute_density_pressure(particles, count);
// compute_forces(particles, count);
// integrate(particles, count, screenW, screenH);
// draw_particles(particles, count);

void Initparticles(Particle *particles, int count)
{
    int cols = 20; // 20x25 grid
    int rows = 25;
    float spacing = RENDER_RADIUS * 2.2f; // Slight gap to avoid initial explosion

    for (int i = 0; i < count; i++)
    {
        int x_idx = i % cols;
        int y_idx = i / cols;
        particles[i].position = (Vector2){
            400 + x_idx * spacing, // Centered, near top
            100 + y_idx * spacing};
        particles[i].velocity = (Vector2){0, 0};
        particles[i].force = (Vector2){0, 0};
        particles[i].mass = PARTICLE_MASS;
        particles[i].size = RENDER_RADIUS;
    }
}

void Drawparticles(Particle *particles, int count)
{
    for (int i = 0; i < count; i++)
    {
        DrawCircleV(particles[i].position, particles[i].size, BLACK);
    }
}

void movment(Particle *particles, int count)
{
    float dt = GetFrameTime();
    for (int i = 0; i < count; i++)
    {
        particles[i].velocity.y += G * dt;
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;

        if (particles[i].position.y > Height - particles[i].size)
        {
            particles[i].position.y = Height - particles[i].size;
            particles[i].velocity.y *= -0.8;
        }
    }
}

void ResolveParticleCollisions(Particle *p, int count)
{
    float min_dist = RENDER_RADIUS * 2.0f; // Minimal distance before overlapping

    for (int i = 0; i < count; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float dist = sqrtf(dx * dx + dy * dy);

            // If particles are overlapping
            if (dist < min_dist)
            {
                // If they are exactly on top of each other, create a tiny default offset
                if (dist < 0.001f)
                {
                    p[j].position.x += 0.1f;
                    continue;
                }

                // Calculate how much they overlap
                float overlap = min_dist - dist;

                // Normal direction vector from i to j
                float nx = dx / dist;
                float ny = dy / dist;

                // Push both particles away equally by half the overlap amount
                p[i].position.x -= nx * overlap * 0.5f;
                p[i].position.y -= ny * overlap * 0.5f;
                p[j].position.x += nx * overlap * 0.5f;
                p[j].position.y += ny * overlap * 0.5f;

                // Optional: Slightly dampen velocities along the collision vector to reduce jitter
                float kx = p[i].velocity.x - p[j].velocity.x;
                float ky = p[i].velocity.y - p[j].velocity.y;
                float p_ratio = (kx * nx + ky * ny) * 0.5f;
                if (p_ratio > 0)
                {
                    p[i].velocity.x -= p_ratio * nx;
                    p[i].velocity.y -= p_ratio * ny;
                    p[j].velocity.x += p_ratio * nx;
                    p[j].velocity.y += p_ratio * ny;
                }
            }
        }
    }
}

int main()
{

    Particle particles[maxparticles];
    Initparticles(particles, maxparticles);
    InitWindow(Width, Height, "Hello World");
    SetTargetFPS(FPS);
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            Vector2 mouse = GetMousePosition();
            int new_count = maxparticles; // or a variable 'particle_count' if you track it
            SpawnCircle(particles, &new_count, mouse);
            // If you use a dynamic count, update it; otherwise maxparticles stays 500 but only first CEVULE_BALL are used.
        }

        compute_density_pressure(particles, maxparticles);
        compute_forces(particles, maxparticles);
        integrate(particles, maxparticles, Width, Height);
        ResolveParticleCollisions(particles, maxparticles);
        draw_particles(particles, maxparticles);

        // Drawparticles(particles, maxparticles);
        // movment(particles, maxparticles);

        EndDrawing();
    }
}
