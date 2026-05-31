#include <raylib.h>
#include <iostream>

using namespace std; // Added this line


#define height 800
#define width 800
#define rec_height 60
#define rec_widht 20





int main()
{
    int circle_x=width/2;
    int circle_y=height/2;
    int r=20;
    
    int  p1x=10;
    int  p1y=height/2-rec_height/2;
    int  p2x=width-(rec_widht+10);
    int  p2y=height/2-rec_height/2; 
    cout << "starting game" << endl;
    InitWindow(width,height,"pong game");
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        // event
        
        

        // up date



        // draing
        BeginDrawing();
        DrawCircle(circle_x,circle_y,r,WHITE);
        DrawRectangle(p1x,p1y,rec_widht,rec_height,WHITE);
        DrawRectangle(p2x,p2y,rec_widht,rec_height,WHITE);
        DrawLine(width/2,0,width/2,height,WHITE);

        EndDrawing();


    }
    


    CloseWindow();

    return 0;
}