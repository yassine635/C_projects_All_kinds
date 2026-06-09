#include <stdio.h>
#include <raylib.h>
#include <stdbool.h>
#include <math.h>

#define w 1000
#define h 800
#define wr 50
#define hr 50
const Color rect_color=RED;
const int speed=10; 
static int lastDirection = 0;
int key;

typedef struct {
	float x,y,r;
	Color color;
} Crcele;

void movemnt(float* posx, float* posy) {
    // Get the latest key press (only one per frame, consumes it)
    int pressed = GetKeyPressed();
    if (pressed != 0) {
        // Store the newly pressed direction key
        lastDirection = pressed;
    }

    // Move according to the last stored direction
    if (lastDirection == KEY_RIGHT) {
        *posx += speed;
    }
    else if (lastDirection == KEY_LEFT) {
        *posx -= speed;
    }
    else if (lastDirection == KEY_DOWN) {
        *posy += speed;
    }
    else if (lastDirection == KEY_UP) {
        *posy -= speed;
    }
}






void chek_colision(float* posx,float* posy , int mdoe){
	switch (mdoe)
	{
	case 1:
		{
			if( *posx+wr >= w ){
				*posx=w-wr;
			}
			if( *posx <= 0 ){
				*posx=0;
			}
			if( *posy+hr >= h ){
				*posy=h-hr;
			}
			if( *posy <= 0 ){
				*posy=0;
			}
		}
		break;
	case 2:
		{
			int textWidth = MeasureText("game over", 20);
			int posX = (GetScreenWidth() / 2) - (textWidth / 2);
			int posY = GetScreenHeight() / 2; // Rough vertical positioning
			if( *posx+wr >= w ){
				DrawText("game over", posX, posY, 20, LIGHTGRAY);
			}
			if( *posx <= 0 ){
				DrawText("game over", posX, posY, 20, LIGHTGRAY);
			}
			if( *posy+hr >= h ){
				DrawText("game over", posX, posY, 20, LIGHTGRAY);
			}
			if( *posy <= 0 ){
				DrawText("game over", posX, posY, 20, LIGHTGRAY);
			}
		}
		break;
	
	
	}

	
}






int main(){
const int cellSize=40;
	int score=1;

	int posx=(w/2)-wr;
	int posy=(h/2)-hr;
	Crcele cer={GetRandomValue(10,w-10),GetRandomValue(10,h-10),20,GREEN};
	Rectangle rect={posx,posy,wr,hr};

	int mode=2;

	
	InitWindow(w,h,"snake game");
	SetTargetFPS(60);
	while (!WindowShouldClose())
	{
		

		

		


		BeginDrawing();
		ClearBackground(BLACK);
			chek_colision(&rect.x,&rect.y,mode);
			movemnt(&rect.x,&rect.y);
			if(CheckCollisionCircleRec((Vector2){cer.x,cer.y},cer.r,rect)){
				DrawCircle(cer.x,cer.y,cer.r,BLACK);
				cer=(Crcele){GetRandomValue(10,w-10),GetRandomValue(10,h-10),20,GREEN};
				
			}
			DrawRectangle(rect.x,rect.y,rect.width,rect.height,rect_color);
			DrawCircle(cer.x,cer.y,cer.r,cer.color);
			
			
		
		EndDrawing();
	}
	 CloseWindow();

	return 0;

}
