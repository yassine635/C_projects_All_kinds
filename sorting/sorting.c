#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define COUNT 100
#define HEIGHT 600
#define WIDTH 900
int num[COUNT];


typedef struct{
    bool swaped;
    int i,j;
    bool sort_done;
} SwapetStatus ;

void swap(int i,int j){
    int temp=num[i] ;
    num[i]=num[j];
    num[j]=temp;
}

void drw_bars(SwapetStatus sort_s){
    
   for (int i=0 ; i < COUNT ; i++){
        int value= num[i];
        int bat_height=value*HEIGHT*0.75/COUNT;
        Color color = WHITE;
        if( sort_s.swaped && (i == sort_s.i || i == sort_s.j))
        {
            color=RED;
        }
        DrawRectangle(i*(WIDTH/COUNT),(HEIGHT*0.75)-(bat_height),WIDTH/COUNT-2,bat_height,color);
    }
}

void init_rand(){
    for(int i=0 ; i < COUNT ;++i ){
        num[i]=i;
    }
    for(int i = COUNT-1 ; i >= 0 ; i--){
        int j = rand() % (i+1);
        swap(i,j);

    }
}

SwapetStatus sort_step(){
    
    static SwapetStatus stuts={false,0,0,false};
    static bool any_swap=false;
    static int i = 0;
    stuts.i=i;
    stuts.j=i+1;
    
    if( i < COUNT ){
        int current_value=num[i];
        int next_value=num[i+1];
        if(current_value > next_value){
            swap(i,i+1);
            stuts.swaped=true;
            any_swap=true;
        }
        else{
            stuts.swaped=false;
        }
        i++;
    }
    else{
        if(!any_swap){
            stuts.sort_done=true;
        }
        i=0;
        any_swap=false;
    }
    return stuts;
}


int main() {
    init_rand();

    SwapetStatus sort_s=sort_step();

    InitWindow(WIDTH, HEIGHT, "sorting");
    //SetTargetFPS(COUNT);
    while (!WindowShouldClose()) {
        if(sort_s.sort_done != true)
        {
            sort_s=sort_step();
        }
        BeginDrawing();
        ClearBackground(BLACK);
        
        drw_bars(sort_s);
        EndDrawing();
    } 

    CloseWindow();
}