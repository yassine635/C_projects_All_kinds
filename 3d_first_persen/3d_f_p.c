#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <string.h>


#define w 1500
#define h 1000
#define MAX_COLUMNS 20

int main(){

	Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 4.0f };    // Camera position
    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 60.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    int cameraMode = CAMERA_FIRST_PERSON;

	char m1[]="Are you sure you want to exit program? [Y/N]";
	char m2[]="Try to close the window to get confirmation message!";

	InitWindow(w,h,"3d first persen");
	SetExitKey(KEY_NULL);       // Disable KEY_ESCAPE to close window, X-button still works

    bool exitWindowRequested = false;   // Flag to request window to exit
    bool exitWindow = false;    // Flag to set window to exit
	
	SetTargetFPS(60);
	DisableCursor();  
	// Generates some random columns
		float heights[MAX_COLUMNS] = { 0 };
		Vector3 positions[MAX_COLUMNS] = { 0 };
		Color colors[MAX_COLUMNS] = { 0 };

		for (int i = 0; i < MAX_COLUMNS; i++)
		{
			heights[i] = (float)GetRandomValue(1, 12);
			positions[i] = (Vector3){ (float)GetRandomValue(-30, 30), heights[i]/2.0f, (float)GetRandomValue(-30, 30) };
			colors[i] = (Color){ GetRandomValue(20, 255), GetRandomValue(10, 55), 30, 255 };
		}
	while(!exitWindow){

		 UpdateCamera(&camera, CAMERA_FREE);
    	if (IsKeyPressed(KEY_Z)) camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };

		if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE)) exitWindowRequested = true;

        if (exitWindowRequested)
        {
            // A request for close window has been issued, we can save data before closing
            // or just show a message asking for confirmation

            if (IsKeyPressed(KEY_Y)) exitWindow = true;
            else if (IsKeyPressed(KEY_N)) exitWindowRequested = false;
        }

		if (IsKeyPressed(KEY_ONE))
        {
            cameraMode = CAMERA_FREE;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // Reset roll
        }

        if (IsKeyPressed(KEY_TWO))
        {
            cameraMode = CAMERA_FIRST_PERSON;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // Reset roll
        }

        if (IsKeyPressed(KEY_THREE))
        {
            cameraMode = CAMERA_THIRD_PERSON;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // Reset roll
        }

        if (IsKeyPressed(KEY_FOUR))
        {
            cameraMode = CAMERA_ORBITAL;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // Reset roll
        }

		BeginDrawing();

			ClearBackground(WHITE); // clering the bckground eash frame 
				BeginMode3D(camera);
					
					DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 64.0f, 64.0f }, BLACK); // Draw ground
					DrawCube((Vector3){ -32.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 64.0f, BLUE);     // Draw a blue wall
              	  	DrawCube((Vector3){ 32.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 64.0f, LIME);      // Draw a green wall
                	DrawCube((Vector3){ 0.0f, 2.5f, 32.0f }, 64.0f, 5.0f, 1.0f, GOLD);      // Draw a yellow wall
					DrawCube((Vector3){ 0.0f, 2.5f, -32.0f }, 64.0f, 5.0f, 1.0f, RED);

					/*
					DrawLine3D((Vector3){0,0,0}, (Vector3){5,0,0}, RED);   // X
					DrawLine3D((Vector3){0,0,0}, (Vector3){0,5,0}, GREEN); // Y
					DrawLine3D((Vector3){0,0,0}, (Vector3){0,0,5}, BLUE);  // Z
					//DrawGrid(10, 1.0f); // Adds reference grid
					*/
				// Draw some cubes around
                for (int i = 0; i < MAX_COLUMNS; i++)
                {
                    DrawCube(positions[i], 2.0f, heights[i], 2.0f, colors[i]);
                    DrawCubeWires(positions[i], 2.0f, heights[i], 2.0f, MAROON);
                }

                // Draw player cube
                if (cameraMode == CAMERA_THIRD_PERSON)
                {
                    DrawCube(camera.target, 0.5f, 0.5f, 0.5f, PURPLE);
                    DrawCubeWires(camera.target, 0.5f, 0.5f, 0.5f, DARKPURPLE);
                }

		
				EndMode3D();

			if (exitWindowRequested)
            {
				ClearBackground(WHITE);
                DrawRectangle(0, h/2, w, 200, BLACK);
                DrawText("Are you sure you want to exit program? [Y/N]", w/2, h/2, 30, WHITE);
            }
            else {
				int l=strlen(m2);
				printf("L: %d \n",l);
				DrawText("Try to close the window to get confirmation message!", w/4, h/2, 20, GREEN);
			}	
				// Draw info boxes
            DrawRectangle(5, 5, 330, 100, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(5, 5, 330, 100, BLUE);

            DrawText("Camera controls:", 15, 15, 10, BLACK);
            DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
            DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
            DrawText("- Camera mode keys: 1, 2, 3, 4", 15, 60, 10, BLACK);
            DrawText("- Zoom keys: num-plus, num-minus or mouse scroll", 15, 75, 10, BLACK);
            DrawText("- Camera projection key: P", 15, 90, 10, BLACK);

            DrawRectangle(600, 5, 195, 100, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(600, 5, 195, 100, BLUE);

            DrawText("Camera status:", 610, 15, 10, BLACK);
            DrawText(TextFormat("- Mode: %s", (cameraMode == CAMERA_FREE) ? "FREE" :
                                              (cameraMode == CAMERA_FIRST_PERSON) ? "FIRST_PERSON" :
                                              (cameraMode == CAMERA_THIRD_PERSON) ? "THIRD_PERSON" :
                                              (cameraMode == CAMERA_ORBITAL) ? "ORBITAL" : "CUSTOM"), 610, 30, 10, BLACK);
            DrawText(TextFormat("- Projection: %s", (camera.projection == CAMERA_PERSPECTIVE) ? "PERSPECTIVE" :
                                                    (camera.projection == CAMERA_ORTHOGRAPHIC) ? "ORTHOGRAPHIC" : "CUSTOM"), 610, 45, 10, BLACK);
            DrawText(TextFormat("- Position: (%06.3f, %06.3f, %06.3f)", camera.position.x, camera.position.y, camera.position.z), 610, 60, 10, BLACK);
            DrawText(TextFormat("- Target: (%06.3f, %06.3f, %06.3f)", camera.target.x, camera.target.y, camera.target.z), 610, 75, 10, BLACK);
            DrawText(TextFormat("- Up: (%06.3f, %06.3f, %06.3f)", camera.up.x, camera.up.y, camera.up.z), 610, 90, 10, BLACK);
		EndDrawing();
		
	}

	CloseWindow();
	return 0;

}

