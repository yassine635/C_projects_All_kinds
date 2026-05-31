#include  <stdio.h>
#include  <raylib.h>
#include  <stdbool.h>
#include <math.h>



#define HEIGHT 600
#define WIDTH 900
#define SPEED 5
int ballX=400;
int ballY=400;
int r=50;
int angle = PI;
int P1X=10;
int P1Y=400;
int P2x=790;
int p2y=400;
Color background ={20,160,133,255};

void chek_colission_balle(int bx,int by){

    if(bx+r > WIDTH){
        ballX-=SPEED;
    }
    else if (bx-r < 0)
    {
        ballX += SPEED;
    }
    if( by+r > HEIGHT)
    {
        ballY-=SPEED;
    }
    else if ( by-r  <  0 ){
        ballY += SPEED ;
    }

}



int main(){




InitWindow(WIDTH,HEIGHT,"balle");

SetTargetFPS(60);
while(!WindowShouldClose()){

    // event handiling
    // Event Handling (Separate IFs allow diagonals)
    if (IsKeyDown(KEY_RIGHT)) {
        ballX += SPEED;
    }
    if (IsKeyDown(KEY_LEFT)) {
        ballX -= SPEED;
    }
    if (IsKeyDown(KEY_UP)) {
        ballY -= SPEED;
    }
    if (IsKeyDown(KEY_DOWN)) {
        ballY += SPEED;
    }

    // updating P
    

    // drawing
    BeginDrawing();
    ClearBackground(background);
    DrawCircle(ballX,ballY,r,WHITE);
    chek_colission_balle(ballX,ballY);

    EndDrawing();

}


CloseWindow();
return 0;

}