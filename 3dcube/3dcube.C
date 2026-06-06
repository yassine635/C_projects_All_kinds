#include <stdio.h>
#include <raylib.h>

#define width 800 
#define height 600

int main(){


    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type
    Vector3 origin={0,0,0};
    InitWindow(width,height,"3d_cube");
    SetTargetFPS(60);
    
    while(!WindowShouldClose()){
    UpdateCamera(&camera, CAMERA_FREE);
    if (IsKeyPressed(KEY_Z)) camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    BeginDrawing();
    ClearBackground(RAYWHITE);
        BeginMode3D(camera);    
        
        
                
                DrawCube(origin,1,1,1,RED);
                DrawCubeWires(origin, 1.0f, 1.0f, 1.0f, MAROON);
                DrawGrid(10,1.0f);
        
        
        EndMode3D();  
    EndDrawing();  
    }
    
    CloseWindow();

    return 0;
}