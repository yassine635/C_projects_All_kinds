#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include <stdbool.h>

// ===== SPH Constants =====
#define SMOOTHING_RADIUS   30.0f    // h
#define REST_DENSITY       5.0f    // ρ₀
#define GAS_CONSTANT       20.0f    // stiffness
#define VISCOSITY          0.1f     // viscosity
#define PARTICLE_MASS      1.0f
#define GRAVITY            600.0f   // pixels/s²
#define DAMPING            0.5f    // wall bounce (fraction of velocity retained)
#define DT                 0.05f    // timestep
#define RENDER_RADIUS      10       // visual radius
#define CEVULE_BALL        100      // particles per spawn
#define SCREEN_WIDTH       1000
#define SCREEN_HEIGHT      800
#define FPS                30
#define MAX_PARTICLES      10000

// ===== Globals =====
int particle_count = 0;    // number of active particles

// ===== Particle Structure =====
typedef struct Particle {
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float   density;
    float   pressure;
    int     size;
    float   mass;
} Particle;

// ===== Kernel Functions (static inline for speed) =====
static inline float poly6(float r, float h) {
    if (r <= 0.0f || r >= h) return 0.0f;
    float x = h*h - r*r;
    return (315.0f / (64.0f * PI * powf(h, 9))) * x*x*x;
}

static inline float spiky_grad(float r, float h) {
    if (r <= 0.0f || r >= h) return 0.0f;
    float x = h - r;
    return -(45.0f / (PI * powf(h, 6))) * x*x;
}

static inline float viscosity_lap(float r, float h) {
    if (r <= 0.0f || r >= h) return 0.0f;
    return (45.0f / (PI * powf(h, 6))) * (h - r);
}

// ===== SPH Steps =====
void compute_density_pressure(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        float rho = 0.0f;
        for (int j = 0; j < count; j++) {
            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float r = sqrtf(dx*dx + dy*dy);
            rho += p[j].mass * poly6(r, SMOOTHING_RADIUS);
        }
        // Clamp to avoid division by zero
        if (rho < 0.001f) rho = 0.001f;
        p[i].density = rho;
        p[i].pressure = GAS_CONSTANT * (rho - REST_DENSITY);
    }
}

void compute_forces(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        Vector2 f_pressure  = {0.0f, 0.0f};
        Vector2 f_viscosity = {0.0f, 0.0f};

        for (int j = 0; j < count; j++) {
            if (i == j) continue;

            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float r = sqrtf(dx*dx + dy*dy);

            if (r < SMOOTHING_RADIUS && r > 0.001f) {
                // Normal from i to j
                float nx = dx / r;
                float ny = dy / r;

                // Pressure force (symmetric)
                float avg_p = (p[i].pressure + p[j].pressure) * 0.5f;
                float grad = spiky_grad(r, SMOOTHING_RADIUS);
                float p_scale = -p[j].mass * avg_p / p[j].density * grad;
                f_pressure.x += p_scale * nx;
                f_pressure.y += p_scale * ny;

                // Viscosity force
                float lap = viscosity_lap(r, SMOOTHING_RADIUS);
                float v_scale = VISCOSITY * p[j].mass / p[j].density * lap;
                f_viscosity.x += v_scale * (p[j].velocity.x - p[i].velocity.x);
                f_viscosity.y += v_scale * (p[j].velocity.y - p[i].velocity.y);
            }
        }

        // Gravity (force per unit mass, scaled by density to match SPH equations)
        p[i].force.x = f_pressure.x + f_viscosity.x;
        p[i].force.y = f_pressure.y + f_viscosity.y + GRAVITY * p[i].density;
    }
}

void integrate(Particle *p, int count, int width, int height) {
    for (int i = 0; i < count; i++) {
        // Update velocity (a = F / ρ)
        p[i].velocity.x += DT * p[i].force.x / p[i].density;
        p[i].velocity.y += DT * p[i].force.y / p[i].density;

        // Update position
        p[i].position.x += DT * p[i].velocity.x;
        p[i].position.y += DT * p[i].velocity.y;

        // Wall collisions (with damping)
        float r = RENDER_RADIUS;
        if (p[i].position.x - r < 0.0f) {
            p[i].position.x = r;
            p[i].velocity.x *= -DAMPING;
        } else if (p[i].position.x + r > width) {
            p[i].position.x = width - r;
            p[i].velocity.x *= -DAMPING;
        }
        if (p[i].position.y - r < 0.0f) {
            p[i].position.y = r;
            p[i].velocity.y *= -DAMPING;
        } else if (p[i].position.y + r > height) {
            p[i].position.y = height - r;
            p[i].velocity.y *= -DAMPING;
        }
    }
}

// ===== Drawing =====
void draw_particles(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        float speed = sqrtf(p[i].velocity.x*p[i].velocity.x + p[i].velocity.y*p[i].velocity.y);
        float t = fminf(speed / 300.0f, 1.0f);
        Color col = ColorLerp(BLUE, RED, t);
        DrawCircleV(p[i].position, RENDER_RADIUS, col);
    }
}

// ===== Particle Management =====
void Initparticles(Particle *particles, int count) {
    int cols = 20;
    float spacing = RENDER_RADIUS * 2.2f;
    for (int i = 0; i < count; i++) {
        int x_idx = i % cols;
        int y_idx = i / cols;
        particles[i].position = (Vector2){ 400 + x_idx * spacing, 100 + y_idx * spacing };
        particles[i].velocity = (Vector2){0, 0};
        particles[i].force    = (Vector2){0, 0};
        particles[i].mass     = PARTICLE_MASS;
        particles[i].size     = RENDER_RADIUS;
    }
    particle_count = count;
}

void SpawnCircle(Particle *particles, int *count, Vector2 center) {
    int num = CEVULE_BALL;
    int start = *count;
    int new_total = start + num;
    if (new_total > MAX_PARTICLES) {
        new_total = MAX_PARTICLES;
        num = MAX_PARTICLES - start;
    }
    if (num <= 0) return;

    float radius = 60.0f;
    for (int i = 0; i < num; i++) {
        int idx = start + i;
        float angle = GetRandomValue(0, 628) / 100.0f;    // 0..2π
        float dist = sqrtf((float)GetRandomValue(0, 10000) / 10000.0f) * radius;
        particles[idx].position = (Vector2){
            center.x + cosf(angle) * dist,
            center.y + sinf(angle) * dist
        };
        particles[idx].velocity = (Vector2){0, 0};
        particles[idx].force    = (Vector2){0, 0};
        particles[idx].mass     = PARTICLE_MASS;
        particles[idx].size     = RENDER_RADIUS;
    }
    *count = new_total;
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

// ===== Main =====
int main() {
    Particle particles[MAX_PARTICLES];
    Initparticles(particles, 200);   // start with 200 particles (adjust as desired)

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SPH Fluid Simulation");
    SetTargetFPS(FPS);

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            Vector2 mouse = GetMousePosition();
            SpawnCircle(particles, &particle_count, mouse);
        }

        BeginDrawing();
        ClearBackground(WHITE);

        compute_density_pressure(particles, particle_count);
        compute_forces(particles, particle_count);
        integrate(particles, particle_count, SCREEN_WIDTH, SCREEN_HEIGHT);
        ResolveParticleCollisions(particles, particle_count);
        draw_particles(particles, particle_count);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}