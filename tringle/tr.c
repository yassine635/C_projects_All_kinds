#include<raylib.h>

#define width 800
#define height 600


int main(void)
{
    InitWindow(width, height, "raylib [core] example - basic window");

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(BLUE);
        DrawTriangle( (Vector2){ 400, 100 }, (Vector2){ 300, 300 }, (Vector2){ 500, 300 }, RED);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}