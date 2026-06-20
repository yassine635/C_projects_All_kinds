#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include <stdbool.h>

#define RENDER_RADIUS     10

const int Width = 2000;
const int Height = 1000;
const int FPS = 30;
const int maxparticles = 1000;

// SPH Fluid Constants
const float G = 0.0f;               // Gravity
const float SmoothingRadius = 15.0f;  // How far particles look for neighbors (h)
const float TargetDensity = 4.0f;     // Rest density of the fluid
const float PressureMultiplier = 4.0f; // How hard particles push apart
const float ViscosityMultiplier = 0.8f; // Liquid thickness/sticky feel

typedef struct Particle
{
    Vector2 position;
    Vector2 velocity;
    float size;
    float density;
    float pressure;
} Particle;

void Initparticles(Particle *particles, int count)
{
    for (int i = 0; i < count; i++)
    {
        // Spawn them closer to the center top so they fall in a clump
        particles[i].position = (Vector2){GetRandomValue(300, 700), GetRandomValue(50, 400)};
        particles[i].velocity = (Vector2){0, 0};
        particles[i].size = 10.0f;
        particles[i].density = 0.0f;
        particles[i].pressure = 0.0f;
    }
}

void Drawparticles(Particle *particles, int count)
{
    for (int i = 0; i < count; i++)
    {
        // Color shifts to blue based on density/pressure for a water effect
        Color fluidColor = (Color){20, 100, 250, 220}; 
        DrawCircleV(particles[i].position, particles[i].size, fluidColor);
    }
}

// Simple kernel function to calculate density based on distance
float DensityKernel(float dst, float radius)
{
    if (dst < radius)
    {
        float scale = 15.0f / (PI * pow(radius, 6));
        float v = radius - dst;
        return v * v * v * scale;
    }
    return 0.0f;
}

// Kernel derivative for calculating pressure direction and magnitude
float PressureKernelDerivative(float dst, float radius)
{
    if (dst < radius)
    {
        float scale = 45.0f / (PI * pow(radius, 6));
        float v = radius - dst;
        return -v * v * scale;
    }
    return 0.0f;
}

void UpdateFluid(Particle *particles, int count)
{
    float dt = GetFrameTime();

    // STEP 1: Calculate Densities and Convert to Pressures
    for (int i = 0; i < count; i++)
    {
        float density = 0.0f;
        for (int j = 0; j < count; j++)
        {
            float dstX = particles[j].position.x - particles[i].position.x;
            float dstY = particles[j].position.y - particles[i].position.y;
            float dst = sqrtf(dstX * dstX + dstY * dstY);

            density += DensityKernel(dst, SmoothingRadius);
        }
        particles[i].density = density;
        
        // Ideal Gas State Equation for Fluid Pressure
        particles[i].pressure = (density - TargetDensity) * PressureMultiplier;
    }

    // STEP 2: Calculate Forces (Pressure & Viscosity) and Apply Motion
    for (int i = 0; i < count; i++)
    {
        Vector2 pressureForce = {0.0f, 0.0f};
        Vector2 viscosityForce = {0.0f, 0.0f};

        for (int j = 0; j < count; j++)
        {
            if (i == j) continue;

            float dstX = particles[j].position.x - particles[i].position.x;
            float dstY = particles[j].position.y - particles[i].position.y;
            float dst = sqrtf(dstX * dstX + dstY * dstY);

            if (dst < SmoothingRadius && dst > 0.0f)
            {
                // Vector pointing away from neighbor
                Vector2 dir = {dstX / dst, dstY / dst};
                
                // Pressure Force
                float slope = PressureKernelDerivative(dst, SmoothingRadius);
                float sharedPressure = (particles[i].pressure + particles[j].pressure) / 2.0f;
                // Force works from high pressure to low pressure
                pressureForce.x += dir.x * sharedPressure * slope / particles[j].density;
                pressureForce.y += dir.y * sharedPressure * slope / particles[j].density;

                // Viscosity Force (evens out velocities between close neighbors)
                float r = SmoothingRadius;
                float viscosityWeight = (r - dst) / r; // Simple linear falloff
                viscosityForce.x += (particles[j].velocity.x - particles[i].velocity.x) * viscosityWeight;
                viscosityForce.y += (particles[j].velocity.y - particles[i].velocity.y) * viscosityWeight;
            }
        }

        // Apply external forces (Gravity) + SPH forces
        particles[i].velocity.y += G * dt; // Gravity
        
        // Convert forces to acceleration changes
        // (Dividing by density keeps highly compressed areas stable)
        if (particles[i].density > 0) 
        {
            particles[i].velocity.x += (pressureForce.x / particles[i].density) * dt;
            particles[i].velocity.y += (pressureForce.y / particles[i].density) * dt;
        }

        // Apply Viscosity
        particles[i].velocity.x += viscosityForce.x * ViscosityMultiplier * dt;
        particles[i].velocity.y += viscosityForce.y * ViscosityMultiplier * dt;

        // Update Position
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;

        // STEP 3: Boundary Collisions (Left, Right, Bottom, Top)
        float damping = -0.3f; // Dampen speed on impact
        
        // Bottom Boundary
        if (particles[i].position.y > Height - particles[i].size)
        {
            particles[i].position.y = Height - particles[i].size;
            particles[i].velocity.y *= damping;
        }
        // Top Boundary
        if (particles[i].position.y < particles[i].size)
        {
            particles[i].position.y = particles[i].size;
            particles[i].velocity.y *= damping;
        }
        // Left Boundary
        if (particles[i].position.x < particles[i].size)
        {
            particles[i].position.x = particles[i].size;
            particles[i].velocity.x *= damping;
        }
        // Right Boundary
        if (particles[i].position.x > Width - particles[i].size)
        {
            particles[i].position.x = Width - particles[i].size;
            particles[i].velocity.x *= damping;
        }
    }
}



void ResolveParticleCollisions(Particle *p, int count) {
    float min_dist = RENDER_RADIUS * 2.0f; // Minimal distance before overlapping
    
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            float dx = p[j].position.x - p[i].position.x;
            float dy = p[j].position.y - p[i].position.y;
            float dist = sqrtf(dx * dx + dy * dy);

            // If particles are overlapping
            if (dist < min_dist) {
                // If they are exactly on top of each other, create a tiny default offset
                if (dist < 0.001f) {
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

int main()
{
    Particle particles[maxparticles];
    Initparticles(particles, maxparticles);
    
    InitWindow(Width, Height, "2D SPH Fluid Simulation");
    SetTargetFPS(FPS);
    
    while (!WindowShouldClose())
    {
        // Update Physics
        UpdateFluid(particles, maxparticles);

        // Render Frame
        BeginDrawing();
        ClearBackground(WHITE);
        ResolveParticleCollisions(particles, maxparticles);
        Drawparticles(particles, maxparticles);
        DrawFPS(10, 10);
        
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}