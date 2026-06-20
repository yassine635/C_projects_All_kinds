#include <raylib.h>

#define w 800
#define h 900    



int main(void)
{
    

    InitWindow(w, h, "tetress");

    SetTargetFPS(60);
    

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawText("Congrats! Your environment is working.", 190, 200, 40,BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
