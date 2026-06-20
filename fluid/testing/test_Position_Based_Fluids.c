
#include <raylib.h>
#include <math.h>
#include <stdio.h>

// ---------- Constants ----------
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600
#define SMOOTHING_RADIUS 20.0f
#define RENDER_RADIUS   5.0f

#define REST_DENSITY    1000.0f
#define STIFFNESS       0.5f       // constraint stiffness
#define EPSILON         0.0001f    // denominator clamp
#define VISCOSITY       0.05f      // velocity damping (0 = none, 1 = full)
#define GRAVITY         1000.0f   // downward (pixels/s²)

#define MAX_PARTICLES   1000
#define CELL_SIZE       SMOOTHING_RADIUS
#define GRID_WIDTH      ((int)(SCREEN_WIDTH  / CELL_SIZE) + 2)
#define GRID_HEIGHT     ((int)(SCREEN_HEIGHT / CELL_SIZE) + 2)
#define MAX_CELL_PARTICLES 128
#define PBF_ITERATIONS  3          // number of constraint iterations per frame

// ---------- Types ----------
typedef struct {
    Vector2 position;
    Vector2 old_position;          // previous position (for velocity update)
    Vector2 velocity;
    Vector2 predicted_position;    // after gravity and before constraints
    float density;
    float lambda;                  // Lagrange multiplier
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
}

float spiky_grad(float r, float h) {
    if (r >= h || r < 0.0f) return 0.0f;
    float diff = h - r;
    return -(45.0f / (PI * h * h * h * h * h * h)) * diff * diff;
}

// ---------- Grid Building (from a given position array) ----------
void BuildGridFromPositions(Vector2 *positions, int count) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[x][y].count = 0;
        }
    }
    for (int i = 0; i < count; i++) {
        int cx = (int)(positions[i].x / CELL_SIZE);
        int cy = (int)(positions[i].y / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= GRID_WIDTH || cy >= GRID_HEIGHT)
            continue;
        Cell *cell = &grid[cx][cy];
        if (cell->count < MAX_CELL_PARTICLES) {
            cell->particles[cell->count++] = i;
        }
    }
}

// ---------- Compute Density and Lambdas ----------
void compute_density_and_lambdas(Particle *p, int count) {
    // Build grid from predicted positions (used for neighbor search)
    Vector2 pred_pos[MAX_PARTICLES];
    for (int i = 0; i < count; i++) pred_pos[i] = p[i].predicted_position;
    BuildGridFromPositions(pred_pos, count);

    // 1. Compute densities at predicted positions
    for (int i = 0; i < count; i++) {
        float rho = 0.0f;
        int cx = (int)(pred_pos[i].x / CELL_SIZE);
        int cy = (int)(pred_pos[i].y / CELL_SIZE);
        for (int oy = -1; oy <= 1; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                int nx = cx + ox;
                int ny = cy + oy;
                if (nx < 0 || ny < 0 || nx >= GRID_WIDTH || ny >= GRID_HEIGHT)
                    continue;
                Cell *cell = &grid[nx][ny];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->particles[k];
                    float dx = pred_pos[j].x - pred_pos[i].x;
                    float dy = pred_pos[j].y - pred_pos[i].y;
                    float r = sqrtf(dx*dx + dy*dy);
                    rho += p[j].mass * poly6(r, SMOOTHING_RADIUS);
                }
            }
        }
        p[i].density = (rho < 0.001f) ? 0.001f : rho;
    }

    // 2. Compute lambdas (constraint multipliers)
    for (int i = 0; i < count; i++) {
        float C = p[i].density / REST_DENSITY - 1.0f;
        float sum_grad_sq = 0.0f;
        int cx = (int)(pred_pos[i].x / CELL_SIZE);
        int cy = (int)(pred_pos[i].y / CELL_SIZE);
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
                    float dx = pred_pos[j].x - pred_pos[i].x;
                    float dy = pred_pos[j].y - pred_pos[i].y;
                    float r = sqrtf(dx*dx + dy*dy);
                    if (r < SMOOTHING_RADIUS && r > 0.001f) {
                        float grad = spiky_grad(r, SMOOTHING_RADIUS);
                        sum_grad_sq += grad * grad;   // |∇W_ij|²
                    }
                }
            }
        }
        p[i].lambda = -STIFFNESS * C / (sum_grad_sq + EPSILON);
    }
}

// ---------- Apply Position Corrections ----------
void apply_position_corrections(Particle *p, int count) {
    Vector2 pred_pos[MAX_PARTICLES];
    for (int i = 0; i < count; i++) pred_pos[i] = p[i].predicted_position;
    BuildGridFromPositions(pred_pos, count);   // refresh grid (positions may have changed)

    Vector2 delta[MAX_PARTICLES];
    for (int i = 0; i < count; i++) {
        delta[i] = (Vector2){0.0f, 0.0f};
        int cx = (int)(pred_pos[i].x / CELL_SIZE);
        int cy = (int)(pred_pos[i].y / CELL_SIZE);
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
                    float dx = pred_pos[j].x - pred_pos[i].x;
                    float dy = pred_pos[j].y - pred_pos[i].y;
                    float r = sqrtf(dx*dx + dy*dy);
                    if (r < SMOOTHING_RADIUS && r > 0.001f) {
                        float grad = spiky_grad(r, SMOOTHING_RADIUS);
                        float nxv = dx / r;
                        float nyv = dy / r;
                        float lambda_sum = p[i].lambda + p[j].lambda;
                        delta[i].x += lambda_sum * grad * nxv;
                        delta[i].y += lambda_sum * grad * nyv;
                    }
                }
            }
        }
    }
    // Apply corrections to predicted positions
    for (int i = 0; i < count; i++) {
        p[i].predicted_position.x += delta[i].x;
        p[i].predicted_position.y += delta[i].y;
    }
}

// ---------- PBF Main Step ----------
void pbf_step(Particle *p, int count, float dt) {
    // 1. Predict positions (apply gravity)
    for (int i = 0; i < count; i++) {
        p[i].old_position = p[i].position;
        p[i].velocity.y += GRAVITY * dt;
        p[i].predicted_position.x = p[i].position.x + dt * p[i].velocity.x;
        p[i].predicted_position.y = p[i].position.y + dt * p[i].velocity.y;
    }

    // 2. Iteratively enforce density constraints
    for (int iter = 0; iter < PBF_ITERATIONS; iter++) {
        compute_density_and_lambdas(p, count);
        apply_position_corrections(p, count);
    }

    // 3. Update positions and velocities
    for (int i = 0; i < count; i++) {
        // New position is the corrected predicted position
        p[i].position = p[i].predicted_position;

        // Velocity from change in position
        p[i].velocity.x = (p[i].position.x - p[i].old_position.x) / dt;
        p[i].velocity.y = (p[i].position.y - p[i].old_position.y) / dt;

        // Apply viscosity (velocity damping)
        p[i].velocity.x *= (1.0f - VISCOSITY);
        p[i].velocity.y *= (1.0f - VISCOSITY);
    }

    // 4. Boundary collisions (walls)
    for (int i = 0; i < count; i++) {
        if (p[i].position.x < RENDER_RADIUS) {
            p[i].position.x = RENDER_RADIUS;
            p[i].velocity.x *= -0.5f;
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

    // 5. (Optional) Particle-particle collision avoidance – not strictly needed with PBF,
    // but can be added for extra stability. We'll skip it here.
}

// ---------- Drawing ----------
void draw_particles(Particle *p, int count) {
    for (int i = 0; i < count; i++) {
        DrawCircle(p[i].position.x, p[i].position.y, RENDER_RADIUS, BLUE);
    }
}

// ---------- Main ----------
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PBF + Spatial Hash Grid");
    SetTargetFPS(30);

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
            particles[particle_count].old_position = particles[particle_count].position;
            particles[particle_count].velocity = (Vector2){0, 0};
            particles[particle_count].predicted_position = particles[particle_count].position;
            particles[particle_count].density = REST_DENSITY;
            particles[particle_count].lambda = 0.0f;
            particles[particle_count].mass = mass;
            particle_count++;
        }
    }

    float dt = 1.0f / 60.0f;

    while (!WindowShouldClose()) {
        pbf_step(particles, particle_count, dt);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_particles(particles, particle_count);
        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}