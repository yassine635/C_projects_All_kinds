/*
    SPH + Spatial Hash Grid – Full raylib Implementation
    =====================================================
    Compile with:
        gcc -o sph_sim main.c -lraylib -lm -lGL -lpthread -ldl -lrt -lX11
    (adjust for your platform)
*/

#include <raylib.h>
#include <math.h>
#include <stdio.h>

// ---------- Constants ----------
#define SCREEN_WIDTH  1000
#define SCREEN_HEIGHT 800
#define SMOOTHING_RADIUS 20.0f
#define RENDER_RADIUS   7.0f   // draw size (could be different)
#define GAS_CONSTANT    20.0f
#define REST_DENSITY    1000.0f
#define VISCOSITY       0.1f
#define GRAVITY         600.0f   // downward (pixels/s²)

#define MAX_PARTICLES   1500
#define CELL_SIZE       SMOOTHING_RADIUS
#define GRID_WIDTH      ((int)(SCREEN_WIDTH  / CELL_SIZE) + 2)
#define GRID_HEIGHT     ((int)(SCREEN_HEIGHT / CELL_SIZE) + 2)
#define MAX_CELL_PARTICLES 128

// ---------- Types ----------
typedef struct {
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float density;
    float pressure;
    float mass;
} Particle;

typedef struct {
    int count;
    int particles[MAX_CELL_PARTICLES];
} Cell;

// ---------- Global Grid ----------
Cell grid[GRID_WIDTH][GRID_HEIGHT];

// ---------- Kernel Functions ----------
float poly6(float r, float h) {
    if (r >= h || r < 0.0f) return 0.0f;
    float h2 = h * h;
    float r2 = r * r;
    float diff = h2 - r2;
    return (315.0f / (64.0f * PI * h2 * h2 * h2 * h * h)) * diff * diff * diff; 
    // 315/(64*pi*h^9) * (h² - r²)^3
}

float spiky_grad(float r, float h) {
    if (r >= h || r < 0.0f) return 0.0f;
    float diff = h - r;
    return -(45.0f / (PI * h * h * h * h * h * h)) * diff * diff;
    // -45/(pi*h^6) * (h - r)^2
}

float viscosity_lap(float r, float h) {
    if (r >= h || r < 0.0f) return 0.0f;
    return (45.0f / (PI * h * h * h * h * h * h)) * (h - r);
    // 45/(pi*h^6) * (h - r)
}

// ---------- Grid Building ----------
void BuildGrid(Particle *p, int count) {
    // clear grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[x][y].count = 0;
        }
    }

    // insert particles
    for (int i = 0; i < count; i++) {
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        if (cx < 0 || cy < 0 || cx >= GRID_WIDTH || cy >= GRID_HEIGHT)
            continue;

        Cell *cell = &grid[cx][cy];
        if (cell->count < MAX_CELL_PARTICLES) {
            cell->particles[cell->count++] = i;
        }
    }
}

// ---------- Density & Pressure ----------
void compute_density_pressure(Particle *p, int count) {
    BuildGrid(p, count);   // grid now reflects current positions

    for (int i = 0; i < count; i++) {
        float rho = 0.0f;

        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        // check 3x3 neighbourhood
        for (int oy = -1; oy <= 1; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                int nx = cx + ox;
                int ny = cy + oy;
                if (nx < 0 || ny < 0 || nx >= GRID_WIDTH || ny >= GRID_HEIGHT)
                    continue;

                Cell *cell = &grid[nx][ny];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->particles[k];
                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float r = sqrtf(dx*dx + dy*dy);

                    rho += p[j].mass * poly6(r, SMOOTHING_RADIUS);
                }
            }
        }

        if (rho < 0.001f) rho = 0.001f;

        p[i].density = rho;
        p[i].pressure = GAS_CONSTANT * (rho - REST_DENSITY);
    }
}

// ---------- Forces ----------
void compute_forces(Particle *p, int count) {
    // grid must have been built already (by compute_density_pressure)
    for (int i = 0; i < count; i++) {
        Vector2 pressure_force = {0.0f, 0.0f};
        Vector2 viscosity_force = {0.0f, 0.0f};

        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        for (int oy = -1; oy <= 1; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                int nx = cx + ox;
                int ny = cy + oy;
                if (nx < 0 || ny < 0 || nx >= GRID_WIDTH || ny >= GRID_HEIGHT)
                    continue;

                Cell *cell = &grid[nx][ny];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->particles[k];
                    if (i == j) continue;

                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float r = sqrtf(dx*dx + dy*dy);

                    if (r > 0.001f && r < SMOOTHING_RADIUS) {
                        float nxv = dx / r;
                        float nyv = dy / r;

                        // Pressure
                        float avg_p = (p[i].pressure + p[j].pressure) * 0.5f;
                        float grad = spiky_grad(r, SMOOTHING_RADIUS);
                        float scale_p = -p[j].mass * avg_p / p[j].density * grad;
                        pressure_force.x += scale_p * nxv;
                        pressure_force.y += scale_p * nyv;

                        // Viscosity
                        float lap = viscosity_lap(r, SMOOTHING_RADIUS);
                        float scale_v = VISCOSITY * p[j].mass / p[j].density * lap;
                        viscosity_force.x += scale_v * (p[j].velocity.x - p[i].velocity.x);
                        viscosity_force.y += scale_v * (p[j].velocity.y - p[i].velocity.y);
                    }
                }
            }
        }

        p[i].force.x = pressure_force.x + viscosity_force.x;
        p[i].force.y = pressure_force.y + viscosity_force.y + GRAVITY * p[i].density;
    }
}

// ---------- Integration + Wall Collisions ----------
void integrate(Particle *p, int count, float dt) {
    for (int i = 0; i < count; i++) {
        // symplectic Euler
        p[i].velocity.x += dt * p[i].force.x / p[i].density;
        p[i].velocity.y += dt * p[i].force.y / p[i].density;

        p[i].position.x += dt * p[i].velocity.x;
        p[i].position.y += dt * p[i].velocity.y;

        // wall collisions (bounce)
        if (p[i].position.x < RENDER_RADIUS) {
            p[i].position.x = RENDER_RADIUS;
            p[i].velocity.x *= -0.5f;   // restitution
        }
        if (p[i].position.x > SCREEN_WIDTH - RENDER_RADIUS) {
            p[i].position.x = SCREEN_WIDTH - RENDER_RADIUS;
            p[i].velocity.x *= -0.5f;
        }
        if (p[i].position.y < RENDER_RADIUS) {
            p[i].position.y = RENDER_RADIUS;
            p[i].velocity.y *= -0.5f;
        }
        if (p[i].position.y > SCREEN_HEIGHT - RENDER_RADIUS) {
            p[i].position.y = SCREEN_HEIGHT - RENDER_RADIUS;
            p[i].velocity.y *= -0.5f;
        }
    }
}

// ---------- Particle–Particle Collision (only after rebuilding grid) ----------
void ResolveParticleCollisions(Particle *p, int count) {
    float min_dist = RENDER_RADIUS * 2.0f;   // particles should not overlap

    // grid must be up‑to‑date with current positions (call BuildGrid before)
    for (int i = 0; i < count; i++) {
        int cx = (int)(p[i].position.x / CELL_SIZE);
        int cy = (int)(p[i].position.y / CELL_SIZE);

        for (int oy = -1; oy <= 1; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                int nx = cx + ox;
                int ny = cy + oy;
                if (nx < 0 || ny < 0 || nx >= GRID_WIDTH || ny >= GRID_HEIGHT)
                    continue;

                Cell *cell = &grid[nx][ny];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->particles[k];
                    if (j <= i) continue;

                    float dx = p[j].position.x - p[i].position.x;
                    float dy = p[j].position.y - p[i].position.y;
                    float dist = sqrtf(dx*dx + dy*dy);

                    if (dist < min_dist && dist > 0.001f) {
                        float overlap = min_dist - dist;
                        float nxv = dx / dist;
                        float nyv = dy / dist;

                        p[i].position.x -= nxv * overlap * 0.5f;
                        p[i].position.y -= nyv * overlap * 0.5f;
                        p[j].position.x += nxv * overlap * 0.5f;
                        p[j].position.y += nyv * overlap * 0.5f;
                    }
                }
            }
        }
    }
}

// ---------- Drawing ----------
void draw_particles(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        DrawCircle(p[i].position.x, p[i].position.y, RENDER_RADIUS, BLUE);
    }
}

// ---------- Main Program ----------
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SPH with Spatial Hash Grid");
    SetTargetFPS(60);

    // create particles (e.g., a block of water)
    Particle particles[MAX_PARTICLES];
    int particle_count = 0;

    float mass = 1.0f;
    float spacing = RENDER_RADIUS * 2.2f;
    int cols = (int)(SCREEN_WIDTH / spacing) - 2;
    int rows = (int)(SCREEN_HEIGHT / spacing) - 6;

    for (int row = 0; row < rows && particle_count < MAX_PARTICLES; row++) {
        for (int col = 0; col < cols && particle_count < MAX_PARTICLES; col++) {
            particles[particle_count].position.x = SCREEN_WIDTH * 0.2f + col * spacing;
            particles[particle_count].position.y = SCREEN_HEIGHT * 0.3f + row * spacing;
            particles[particle_count].velocity = (Vector2){0, 0};
            particles[particle_count].force = (Vector2){0, 0};
            particles[particle_count].density = 0.0f;
            particles[particle_count].pressure = 0.0f;
            particles[particle_count].mass = mass;
            particle_count++;
        }
    }

    float dt = 0.016f;

    while (!WindowShouldClose()) {
        // 1. compute density/pressure (builds grid internally)
        compute_density_pressure(particles, particle_count);

        // 2. compute forces (uses existing grid)
        compute_forces(particles, particle_count);

        // 3. integrate positions (handles walls)
        integrate(particles, particle_count, dt);

        // 4. rebuild grid after position change
        BuildGrid(particles, particle_count);

        // 5. resolve particle-particle overlaps
        ResolveParticleCollisions(particles, particle_count);

        // 6. render
        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_particles(particles, particle_count);

        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}