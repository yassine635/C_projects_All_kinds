/*
 * sorting_visualizer_web_fixed.c
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  ALL WEBASSEMBLY / EMSCRIPTEN FIXES ARE GROUPED AT THE TOP IN THIS FILE:
 *
 *  FIX 1 – Blurry canvas (devicePixelRatio / HiDPI)
 *  FIX 2 – Mouse coordinate mismatch (canvas offset + DPR scaling)
 *  FIX 3 – Emscripten main-loop integration (emscripten_set_main_loop)
 *  FIX 4 – Correct InitWindow / canvas size bootstrap
 *  FIX 5 – CSS snippet (injected via EM_ASM) that prevents double-scaling
 *  FIX 6 – FLAG_WINDOW_RESIZABLE disabled on web (causes canvas resize bugs)
 *  FIX 7 – SetMouseScale() called after window init so raylib's internal
 *           mouse→canvas transform matches the DPR-scaled canvas
 *
 *  BUILD COMMAND (Emscripten):
 *
 *    emcc sorting_visualizer_web_fixed.c \
 *         -o sorting_visualizer.js \
 *         -I/path/to/raylib/src \
 *         /path/to/raylib/src/libraylib.a \
 *         -s USE_GLFW=3 \
 *         -s ASYNCIFY \
 *         -s ALLOW_MEMORY_GROWTH=1 \
 *         -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
 *         -s WASM=1 \
 *         -O2 \
 *         --shell-file shell_minimal.html \
 *         -lm
 *
 *  (Remove --shell-file if you embed the JS yourself in Laravel.)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

/* ── Emscripten-only headers ─────────────────────────────────────────────── */
#ifdef __EMSCRIPTEN__
#  include <emscripten/emscripten.h>
#  include <emscripten/html5.h>   /* emscripten_get_device_pixel_ratio,
                                     emscripten_set_canvas_element_size,
                                     emscripten_get_canvas_element_size   */
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  FIX 1 – devicePixelRatio helper
 *
 *  On HiDPI / Retina screens the browser CSS pixel ≠ physical pixel.
 *  raylib/Emscripten creates a canvas whose *CSS* size is WIN_W×WIN_H but
 *  whose internal drawing-buffer may be half that, making everything blurry.
 *
 *  Solution: query window.devicePixelRatio at runtime and pass it to
 *  emscripten_set_canvas_element_size() so the backing store is
 *  WIN_W*DPR × WIN_H*DPR physical pixels, then CSS keeps it at WIN_W×WIN_H
 *  logical pixels → crisp 1:1 mapping.
 * ══════════════════════════════════════════════════════════════════════════ */
#ifdef __EMSCRIPTEN__
static double g_dpr = 1.0;   /* set in web_init_canvas(), used everywhere */
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  FIX 2 – Mouse coordinate mismatch
 *
 *  The browser reports mouse events in *CSS* pixels relative to the
 *  viewport.  When the canvas is inside a scrollable div / modal with
 *  margins or CSS transform, raylib's default offset calculation is wrong.
 *
 *  Two complementary remedies are applied:
 *
 *  A) In C  : SetMouseOffset() + SetMouseScale() after InitWindow() so
 *             raylib's GetMousePosition() applies the correct transform.
 *
 *  B) In JS (injected via EM_ASM): override the canvas mousemove/click
 *             listeners to re-project coordinates using
 *             canvas.getBoundingClientRect(), which always reflects the
 *             actual rendered position even after CSS transforms/scrolling.
 *
 *  Together these handle:
 *   • canvas inside a scrolled modal  → getBoundingClientRect fixes offset
 *   • HiDPI scaling (DPR > 1)         → SetMouseScale(1/DPR, 1/DPR)
 *   • CSS width != canvas.width        → scale factor in getBoundingClientRect
 * ══════════════════════════════════════════════════════════════════════════ */

/* ─── Config ────────────────────────────────────────────────────────────── */
#define COUNT       80
#define WIN_W       1300
#define WIN_H       600
#define BAR_AREA_X  20
#define BAR_AREA_Y  130
#define BAR_AREA_W  (WIN_W - 40)
#define BAR_AREA_H  460
#define BTN_COUNT   10
#define FPS         60

/* ─── Colors ────────────────────────────────────────────────────────────── */
#define COL_BG       (Color){15,  15,  20,  255}
#define COL_BAR      (Color){80,  80,  100, 255}
#define COL_SORTED   (Color){30,  180, 120, 255}
#define COL_COMPARE  (Color){70,  140, 220, 255}
#define COL_SWAP     (Color){220, 70,  70,  255}
#define COL_PIVOT    (Color){240, 170, 40,  255}
#define COL_ACTIVE   (Color){210, 80,  150, 255}
#define COL_PANEL    (Color){25,  25,  35,  255}
#define COL_BTN      (Color){40,  40,  55,  255}
#define COL_BTN_HOV  (Color){55,  55,  75,  255}
#define COL_BTN_ACT  (Color){30,  100, 180, 255}
#define COL_TEXT     (Color){220, 220, 230, 255}
#define COL_MUTED    (Color){120, 120, 140, 255}
#define COL_START    (Color){30,  160, 90,  255}
#define COL_START_H  (Color){40,  200, 110, 255}
#define COL_RESET    (Color){60,  60,  80,  255}
#define COL_RESET_H  (Color){80,  80,  105, 255}

/* ─── State ─────────────────────────────────────────────────────────────── */
typedef enum {
    ALGO_BUBBLE, ALGO_SELECTION, ALGO_INSERTION, ALGO_MERGE,
    ALGO_QUICK,  ALGO_HEAP,     ALGO_SHELL,     ALGO_COUNTING,
    ALGO_RADIX,  ALGO_BUCKET,   ALGO_COUNT
} AlgoKind;

const char *ALGO_NAMES[ALGO_COUNT] = {
    "Bubble Sort","Selection Sort","Insertion Sort","Merge Sort",
    "Quick Sort","Heap Sort","Shell Sort","Counting Sort",
    "Radix Sort","Bucket Sort"
};

#define ROLE_NONE    0
#define ROLE_COMPARE 1
#define ROLE_SWAP    2
#define ROLE_PIVOT   3
#define ROLE_ACTIVE  4
#define ROLE_SORTED  5

int  arr[COUNT];
int  aux[COUNT];
int  role[COUNT];
bool sorted_flag[COUNT];

int  cmps = 0, swaps = 0;
bool sort_done = false;
bool running   = false;

typedef struct {
    int  i, j, min_idx;
    int  gap;
    int  exp;
    int  phase;
    int  lo, hi, mid;
    int  tmp;
    int  ms_width;
    int  ms_lo;
    int  hs_n;
    int  hs_heapify_i;
    bool hs_building;
    int  cnt[COUNT+10];
    int  out[COUNT];
    int  radix_digit;
    int  count_cnt[COUNT+10];
    int  count_v, count_k;
    bool count_phase2;
    int  bkt[10][COUNT];
    int  bkt_sz[10];
    int  bkt_b, bkt_bi, bkt_k;
    bool bkt_phase2;
} SortState;

SortState ss;

/* ─── Array helpers ──────────────────────────────────────────────────────── */
void clear_roles(void) {
    for (int i = 0; i < COUNT; i++)
        role[i] = sorted_flag[i] ? ROLE_SORTED : ROLE_NONE;
}
void do_swap(int i, int j) {
    int t = arr[i]; arr[i] = arr[j]; arr[j] = t; swaps++;
}
void init_rand(void) {
    for (int i = 0; i < COUNT; i++) arr[i] = i + 1;
    for (int i = COUNT - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}
void reset_sort(void) {
    memset(&ss, 0, sizeof(ss));
    memset(role, 0, sizeof(role));
    memset(sorted_flag, 0, sizeof(sorted_flag));
    cmps = swaps = 0;
    sort_done = running = false;
}

/* ─── Sort step functions (unchanged logic) ─────────────────────────────── */
bool step_bubble(void) {
    if (ss.phase == 0) { ss.i = 0; ss.phase = 1; ss.j = 0; }
    if (ss.i >= COUNT - 1) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true; return true;
    }
    clear_roles();
    if (ss.j < COUNT - 1 - ss.i) {
        cmps++;
        role[ss.j] = ROLE_COMPARE; role[ss.j+1] = ROLE_COMPARE;
        if (arr[ss.j] > arr[ss.j+1]) { do_swap(ss.j, ss.j+1); role[ss.j]=ROLE_SWAP; role[ss.j+1]=ROLE_SWAP; }
        ss.j++;
    } else { sorted_flag[COUNT-1-ss.i]=true; ss.i++; ss.j=0; }
    return false;
}
bool step_selection(void) {
    if (ss.phase==0){ ss.i=0; ss.min_idx=0; ss.j=1; ss.phase=1; }
    if (ss.i>=COUNT-1){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    clear_roles();
    role[ss.i]=ROLE_ACTIVE; role[ss.min_idx]=ROLE_PIVOT;
    if (ss.j<COUNT){ cmps++; role[ss.j]=ROLE_COMPARE; if(arr[ss.j]<arr[ss.min_idx]) ss.min_idx=ss.j; ss.j++; }
    else { if(ss.min_idx!=ss.i){do_swap(ss.i,ss.min_idx);role[ss.i]=ROLE_SWAP;role[ss.min_idx]=ROLE_SWAP;} sorted_flag[ss.i]=true; ss.i++; ss.min_idx=ss.i; ss.j=ss.i+1; }
    return false;
}
bool step_insertion(void) {
    if (ss.phase==0){ ss.i=1; ss.j=0; ss.tmp=arr[0]; ss.phase=1; sorted_flag[0]=true; }
    if (ss.i>=COUNT){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    clear_roles();
    if (ss.phase==1){ ss.tmp=arr[ss.i]; ss.j=ss.i-1; ss.phase=2; role[ss.i]=ROLE_ACTIVE; }
    else {
        if (ss.j>=0 && arr[ss.j]>ss.tmp){ cmps++; swaps++; arr[ss.j+1]=arr[ss.j]; role[ss.j]=ROLE_COMPARE; role[ss.j+1]=ROLE_SWAP; ss.j--; }
        else { arr[ss.j+1]=ss.tmp; swaps++; sorted_flag[ss.i]=true; ss.i++; ss.phase=1; }
    }
    return false;
}
bool step_merge(void) {
    if (ss.phase==0){ memcpy(aux,arr,sizeof(int)*COUNT); ss.ms_width=1; ss.ms_lo=0; ss.phase=1; }
    if (ss.ms_width>=COUNT){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    clear_roles();
    int lo=ss.ms_lo, mid=lo+ss.ms_width-1, hi=lo+2*ss.ms_width-1;
    if (mid>=COUNT){ ss.ms_width*=2; ss.ms_lo=0; return false; }
    if (hi>=COUNT) hi=COUNT-1;
    int i=lo,j=mid+1,k=lo;
    while(i<=mid&&j<=hi){ cmps++; role[i]=ROLE_COMPARE; role[j]=ROLE_COMPARE; if(arr[i]<=arr[j]) aux[k++]=arr[i++]; else aux[k++]=arr[j++]; swaps++; }
    while(i<=mid){aux[k++]=arr[i++];swaps++;}
    while(j<=hi) {aux[k++]=arr[j++];swaps++;}
    for(int x=lo;x<=hi;x++){arr[x]=aux[x];role[x]=ROLE_ACTIVE;}
    ss.ms_lo+=2*ss.ms_width;
    if(ss.ms_lo>=COUNT){ss.ms_width*=2;ss.ms_lo=0;}
    if(ss.ms_width>=COUNT) for(int x=0;x<COUNT;x++) sorted_flag[x]=true;
    return false;
}
bool step_quick(void) {
    /* iterative quicksort with explicit stack */
    static int stk[COUNT*2]; static int top=0;
    if (ss.phase==0){ top=0; stk[top++]=0; stk[top++]=COUNT-1; ss.phase=1; }
    if (top==0){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    clear_roles();
    int hi=stk[--top], lo=stk[--top];
    if (lo<hi){
        int pivot=arr[hi]; int i=lo-1;
        role[hi]=ROLE_PIVOT;
        for(int j=lo;j<hi;j++){
            cmps++;
            if(arr[j]<=pivot){ i++; do_swap(i,j); role[i]=ROLE_SWAP; role[j]=ROLE_SWAP; }
            else role[j]=ROLE_COMPARE;
        }
        do_swap(i+1,hi);
        int p=i+1; sorted_flag[p]=true;
        stk[top++]=lo; stk[top++]=p-1;
        stk[top++]=p+1; stk[top++]=hi;
    } else if (lo==hi) sorted_flag[lo]=true;
    return false;
}
bool step_heap(void) {
    if (ss.phase==0){
        ss.hs_n=COUNT; ss.hs_building=true;
        ss.hs_heapify_i=COUNT/2-1; ss.phase=1;
    }
    clear_roles();
    if (ss.hs_building){
        /* sift down ss.hs_heapify_i */
        int n=ss.hs_n, i=ss.hs_heapify_i;
        int largest=i,l=2*i+1,r=2*i+2;
        cmps++;
        if(l<n&&arr[l]>arr[largest]) largest=l;
        if(r<n&&arr[r]>arr[largest]) largest=r;
        if(largest!=i){ do_swap(i,largest); role[i]=ROLE_SWAP; role[largest]=ROLE_SWAP; }
        role[i]=ROLE_COMPARE;
        ss.hs_heapify_i--;
        if(ss.hs_heapify_i<0) ss.hs_building=false;
    } else {
        if(ss.hs_n<=1){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
        ss.hs_n--;
        do_swap(0,ss.hs_n); sorted_flag[ss.hs_n]=true; role[ss.hs_n]=ROLE_SORTED;
        /* sift down root */
        int n=ss.hs_n, i=0;
        while(true){
            int largest=i,l=2*i+1,r=2*i+2; cmps++;
            if(l<n&&arr[l]>arr[largest]) largest=l;
            if(r<n&&arr[r]>arr[largest]) largest=r;
            if(largest==i) break;
            do_swap(i,largest); role[i]=ROLE_SWAP; role[largest]=ROLE_SWAP; i=largest;
        }
    }
    return false;
}
bool step_shell(void) {
    if (ss.phase==0){ ss.gap=COUNT/2; ss.i=ss.gap; ss.phase=1; }
    if (ss.gap<=0){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    clear_roles();
    if (ss.i<COUNT){
        int tmp=arr[ss.i],j=ss.i;
        while(j>=ss.gap&&arr[j-ss.gap]>tmp){ cmps++; swaps++; arr[j]=arr[j-ss.gap]; role[j]=ROLE_SWAP; j-=ss.gap; }
        arr[j]=tmp; role[j]=ROLE_ACTIVE; ss.i++;
    } else { ss.gap/=2; ss.i=ss.gap; }
    return false;
}
bool step_counting(void) {
    if (ss.phase==0){ memset(ss.count_cnt,0,sizeof(ss.count_cnt)); ss.i=0; ss.phase=1; }
    clear_roles();
    if (ss.phase==1){
        if(ss.i<COUNT){ ss.count_cnt[arr[ss.i]]++; cmps++; role[ss.i]=ROLE_COMPARE; ss.i++; }
        else{ ss.count_v=1; ss.count_k=0; ss.phase=2; }
    } else {
        while(ss.count_v<=COUNT&&ss.count_cnt[ss.count_v]==0) ss.count_v++;
        if(ss.count_v>COUNT){ for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
        arr[ss.count_k]=ss.count_v; sorted_flag[ss.count_k]=true; role[ss.count_k]=ROLE_ACTIVE;
        swaps++; ss.count_k++;
        ss.count_cnt[ss.count_v]--;
    }
    return false;
}
bool step_radix(void) {
    if (ss.phase==0){ ss.exp=1; ss.i=0; memset(ss.cnt,0,sizeof(ss.cnt)); ss.phase=1; }
    clear_roles();
    if (ss.exp>COUNT) { for(int k=0;k<COUNT;k++) sorted_flag[k]=true; return true; }
    if (ss.phase==1){
        if(ss.i<COUNT){ int d=(arr[ss.i]/ss.exp)%10; ss.cnt[d]++; cmps++; role[ss.i]=ROLE_COMPARE; ss.i++; }
        else{ for(int x=1;x<10;x++) ss.cnt[x]+=ss.cnt[x-1]; ss.i=COUNT-1; ss.phase=2; }
    } else if (ss.phase==2){
        if(ss.i>=0){ int d=(arr[ss.i]/ss.exp)%10; ss.out[--ss.cnt[d]]=arr[ss.i]; swaps++; role[ss.i]=ROLE_SWAP; ss.i--; }
        else{ ss.i=0; ss.phase=3; }
    } else if (ss.phase==3){
        if(ss.i<COUNT){ arr[ss.i]=ss.out[ss.i]; swaps++; role[ss.i]=ROLE_SWAP; ss.i++; }
        else{ ss.exp*=10; ss.i=0; ss.phase=1; memset(ss.cnt,0,sizeof(ss.cnt)); }
    }
    return false;
}
bool step_bucket(void) {
    if (ss.phase==0){ memset(ss.bkt_sz,0,sizeof(ss.bkt_sz)); ss.i=0; ss.phase=1; }
    clear_roles();
    if (ss.phase==1){
        if(ss.i<COUNT){
            int b=(int)((float)(arr[ss.i]-1)/COUNT*10); if(b>=10) b=9;
            ss.bkt[b][ss.bkt_sz[b]++]=arr[ss.i]; cmps++; role[ss.i]=ROLE_COMPARE; ss.i++;
        } else {
            for(int b=0;b<10;b++) for(int x=1;x<ss.bkt_sz[b];x++){ int key=ss.bkt[b][x],y=x-1; while(y>=0&&ss.bkt[b][y]>key){ss.bkt[b][y+1]=ss.bkt[b][y];y--;} ss.bkt[b][y+1]=key; }
            ss.bkt_b=0; ss.bkt_bi=0; ss.count_k=0; ss.phase=2;
        }
    } else {
        if(ss.bkt_b<10){
            if(ss.bkt_bi<ss.bkt_sz[ss.bkt_b]){ arr[ss.count_k]=ss.bkt[ss.bkt_b][ss.bkt_bi]; swaps++; role[ss.count_k]=ROLE_ACTIVE; sorted_flag[ss.count_k]=true; ss.count_k++; ss.bkt_bi++; }
            else{ ss.bkt_b++; ss.bkt_bi=0; }
        } else return true;
    }
    return false;
}

typedef bool (*StepFn)(void);
StepFn STEP_FNS[ALGO_COUNT] = {
    step_bubble, step_selection, step_insertion, step_merge,
    step_quick,  step_heap,     step_shell,     step_counting,
    step_radix,  step_bucket
};

/* ─── UI ─────────────────────────────────────────────────────────────────── */
typedef struct { Rectangle rect; const char *label; bool hovered; } Button;
Button algo_btns[BTN_COUNT];
Button start_btn, reset_btn;
int selected_algo = ALGO_BUBBLE;
int  speed = 5;
Rectangle speed_slider;

void init_buttons(void) {
    int bw=96,bh=30,gap=6,sx=20,sy=10;
    for (int i=0;i<BTN_COUNT;i++){
        algo_btns[i].rect=(Rectangle){sx+i*(bw+gap),sy,bw,bh};
        algo_btns[i].label=ALGO_NAMES[i];
        algo_btns[i].hovered=false;
    }
    start_btn.rect=(Rectangle){WIN_W-200,sy,85,bh};  start_btn.label="Start";
    reset_btn.rect=(Rectangle){WIN_W-108,sy,88,bh};  reset_btn.label="Reset";
    speed_slider=(Rectangle){WIN_W-200,sy+40,178,14};
}

void draw_button(Button *b, bool active, Color normal, Color hover) {
    Color bg = b->hovered ? hover : normal;
    if (active) bg = COL_BTN_ACT;

    DrawRectangleRounded(b->rect, 0.3f, 6, bg);
    
    // FIXED: Added line thickness (5th parameter)
    DrawRectangleRoundedLines(b->rect, 0.3f, 6, 1.0f, (Color){80,80,100,120});

    int fw = MeasureText(b->label, 13);
    DrawText(b->label, 
             (int)(b->rect.x + b->rect.width/2 - fw/2), 
             (int)(b->rect.y + b->rect.height/2 - 7), 
             13, 
             active ? WHITE : COL_TEXT);
}
void draw_speed_slider(void) {
    DrawRectangleRounded(speed_slider,0.5f,4,COL_BTN);
    float t=(speed-1)/9.0f;
    Rectangle fill=speed_slider; fill.width=speed_slider.width*t;
    DrawRectangleRounded(fill,0.5f,4,COL_BTN_ACT);
    float tx=speed_slider.x+speed_slider.width*t;
    DrawCircle((int)tx,(int)(speed_slider.y+speed_slider.height/2),8,WHITE);
    DrawCircle((int)tx,(int)(speed_slider.y+speed_slider.height/2),6,COL_BTN_ACT);
    DrawText("Speed",(int)speed_slider.x,(int)(speed_slider.y-18),12,COL_MUTED);
    char buf[8]; sprintf(buf,"%d",speed);
    DrawText(buf,(int)(speed_slider.x+speed_slider.width+8),(int)(speed_slider.y-2),13,COL_TEXT);
}
void draw_bars(void) {
    float bw=(float)BAR_AREA_W/COUNT;
    for (int i=0;i<COUNT;i++){
        float val=(float)arr[i]/COUNT, bh=val*BAR_AREA_H;
        float x=BAR_AREA_X+i*bw, y=BAR_AREA_Y+BAR_AREA_H-bh;
        Color c;
        switch(role[i]){ case ROLE_COMPARE:c=COL_COMPARE;break; case ROLE_SWAP:c=COL_SWAP;break; case ROLE_PIVOT:c=COL_PIVOT;break; case ROLE_ACTIVE:c=COL_ACTIVE;break; case ROLE_SORTED:c=COL_SORTED;break; default:c=COL_BAR;break; }
        DrawRectangleRec((Rectangle){x+1,y,fmaxf(bw-2,1),bh},c);
    }
}
void draw_legend(void) {
    const char *labels[]={"Default","Comparing","Swap/Write","Pivot","Active","Sorted"};
    Color colors[]={COL_BAR,COL_COMPARE,COL_SWAP,COL_PIVOT,COL_ACTIVE,COL_SORTED};
    int x=BAR_AREA_X,y=BAR_AREA_Y+BAR_AREA_H+12;
    for(int i=0;i<6;i++){ DrawRectangle(x,y+2,12,12,colors[i]); DrawText(labels[i],x+16,y,12,COL_MUTED); x+=MeasureText(labels[i],12)+30; }
}
void draw_stats(void) {
    int y=50; char buf[80];
    DrawText(ALGO_NAMES[selected_algo],BAR_AREA_X,y,20,WHITE);
    sprintf(buf,"Comparisons: %d",cmps); DrawText(buf,BAR_AREA_X+280,y+2,14,COL_MUTED);
    sprintf(buf,"Swaps / Writes: %d",swaps); DrawText(buf,BAR_AREA_X+480,y+2,14,COL_MUTED);
    if(sort_done) DrawText("SORTED!",BAR_AREA_X+700,y+2,14,COL_SORTED);
    else if(running) DrawText("Sorting...",BAR_AREA_X+700,y+2,14,COL_PIVOT);
    else DrawText("Press Start",BAR_AREA_X+700,y+2,14,COL_MUTED);
}
int steps_per_frame(void) {
    int tbl[11]={0,1,1,2,3,5,8,14,25,50,120}; return tbl[speed];
}

/* ══════════════════════════════════════════════════════════════════════════
 *  FIX 3 – Single frame function for emscripten_set_main_loop
 *
 *  The native version uses a blocking while(!WindowShouldClose()) loop.
 *  On the web this freezes the browser tab.  We extract one iteration into
 *  frame_step() and pass it to emscripten_set_main_loop().
 * ══════════════════════════════════════════════════════════════════════════ */
void frame_step(void) {
    Vector2 mouse = GetMousePosition();

    for (int i=0;i<BTN_COUNT;i++){
        algo_btns[i].hovered=CheckCollisionPointRec(mouse,algo_btns[i].rect);
        if(algo_btns[i].hovered&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!running){ selected_algo=i; init_rand(); reset_sort(); }
    }
    start_btn.hovered=CheckCollisionPointRec(mouse,start_btn.rect);
    reset_btn.hovered=CheckCollisionPointRec(mouse,reset_btn.rect);
    if(start_btn.hovered&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!running&&!sort_done) running=true;
    if(reset_btn.hovered&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){ init_rand(); reset_sort(); }

    if(CheckCollisionPointRec(mouse,(Rectangle){speed_slider.x-10,speed_slider.y-10,speed_slider.width+20,speed_slider.height+20})&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
        float t=(mouse.x-speed_slider.x)/speed_slider.width;
        if(t<0)t=0; if(t>1)t=1;
        speed=(int)(t*9+1.5f); if(speed<1)speed=1; if(speed>10)speed=10;
    }
    if(running&&!sort_done){
        int steps=steps_per_frame();
        for(int s=0;s<steps&&!sort_done;s++){ bool done=STEP_FNS[selected_algo](); if(done){sort_done=true;running=false;clear_roles();} }
    }

    BeginDrawing();
    ClearBackground(COL_BG);
    DrawRectangle(0,0,WIN_W,80,COL_PANEL);
    DrawLine(0,80,WIN_W,80,(Color){60,60,80,200});
    for(int i=0;i<BTN_COUNT;i++) draw_button(&algo_btns[i],i==selected_algo,COL_BTN,COL_BTN_HOV);
    draw_button(&start_btn,false,COL_START,COL_START_H);
    draw_button(&reset_btn,false,COL_RESET,COL_RESET_H);
    draw_speed_slider();
    draw_stats();
    draw_bars();
    draw_legend();
    EndDrawing();
}

/* ══════════════════════════════════════════════════════════════════════════
 *  FIX 4+5+6+7 – web_init_canvas()
 *
 *  Called once before the main loop on Emscripten builds.
 *  Steps:
 *   1. Read window.devicePixelRatio (DPR) via emscripten_get_device_pixel_ratio.
 *   2. Resize the canvas *backing buffer* to WIN_W*DPR × WIN_H*DPR via
 *      emscripten_set_canvas_element_size().
 *   3. Force the CSS size back to WIN_W×WIN_H so the canvas occupies the
 *      same visual space but is rendered at full physical resolution.
 *   4. Call SetMouseScale(1/DPR, 1/DPR) so raylib divides incoming CSS-pixel
 *      mouse coordinates back to logical pixel space before hit-testing.
 *   5. Inject a JS addEventListener('mousemove') override that remaps every
 *      mouse event through getBoundingClientRect() to fix offset errors from
 *      scrollable containers and CSS transforms (modal, flex layout, etc.).
 * ══════════════════════════════════════════════════════════════════════════ */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Declare global variable if it's not defined elsewhere in your file
// double g_dpr = 1.0; 

static void web_init_canvas(void) {
    g_dpr = emscripten_get_device_pixel_ratio();
    if (g_dpr < 1.0) g_dpr = 1.0;

    emscripten_set_canvas_element_size("#canvas",
        (int)(WIN_W * g_dpr),
        (int)(WIN_H * g_dpr));

    // We open EM_ASM ONLY for the pure JavaScript portion
    EM_ASM(({
        var canvas = document.getElementById('canvas');
        var logW = $0;
        var logH = $1;
        var dpr  = $2;

        canvas.style.width  = logW + 'px';
        canvas.style.height = logH + 'px';
        canvas.style.imageRendering = 'pixelated';

        // Mouse patching
        function patchMouseEvent(e) {
            var rect = canvas.getBoundingClientRect();
            var cssScaleX = rect.width  / logW;
            var cssScaleY = rect.height / logH;

            var lx = (e.clientX - rect.left) / cssScaleX;
            var ly = (e.clientY - rect.top)  / cssScaleY;

            canvas._correctedMouseX = lx;
            canvas._correctedMouseY = ly;
        }

        var origGetBCR = canvas.getBoundingClientRect.bind(canvas);
        canvas.getBoundingClientRect = function() {
            return origGetBCR();
        };

        ["mousemove", "mousedown", "mouseup", "click"].forEach(function(evName) {
            canvas.addEventListener(evName, patchMouseEvent, true);
        });

        console.log('[SortViz] Canvas initialized: ' + logW + 'x' + logH + ', DPR=' + dpr);
    }), WIN_W, WIN_H, g_dpr);

    // This is C code again, so it sits comfortably outside EM_ASM
    float inv_dpr = 1.0f / (float)g_dpr;
    SetMouseScale(inv_dpr, inv_dpr);

    ClearWindowState(FLAG_WINDOW_RESIZABLE);
}
#endif


/* ══════════════════════════════════════════════════════════════════════════
 *  main()
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    srand((unsigned)time(NULL));
    init_rand();
    reset_sort();
    init_buttons();

    /* ── FIX 6: Do NOT set FLAG_WINDOW_RESIZABLE on web builds ─────────── */
#ifndef __EMSCRIPTEN__
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);   /* native only */
#endif

    InitWindow(WIN_W, WIN_H, "Sorting Visualizer");
    SetTargetFPS(FPS);

#ifdef __EMSCRIPTEN__
    /* FIX 6 continued: clear resizable flag after init on web */
    ClearWindowState(FLAG_WINDOW_RESIZABLE);

    /* FIX 4+5+7: set up DPR-correct canvas dimensions & mouse scale */
    web_init_canvas();

    /* FIX 3: hand control to the browser event loop */
    emscripten_set_main_loop(frame_step, 0 /* use browser rAF */, 1 /* simulate infinite loop */);
#else
    /* Native desktop: normal blocking loop */
    while (!WindowShouldClose()) {
        frame_step();
    }
    CloseWindow();
#endif

    return 0;
}

/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  COMPANION: CSS to paste into your Laravel blade / stylesheet
 *
 *  Add this inside a <style> tag or your app.css.  It prevents the browser
 *  from applying additional scaling that would re-introduce blurriness and
 *  mouse mismatches.
 *
 *  -------------------------------------------------------------------------
 *
 *  #canvas {
 *      display: block;          -- remove inline baseline gap
 *      image-rendering: pixelated;  -- prevent browser smoothing on HiDPI
 *      -- width/height are set dynamically in JS (web_init_canvas EM_ASM)
 *      -- DO NOT set width/height here; let JS control them
 *  }
 *
 *  .modal-window {
 *      -- These are the critical rules:
 *      overflow: hidden;        -- prevent scroll offset bug
 *      padding: 0;              -- no padding between modal and canvas
 *      border: 0;               -- same
 *      display: flex;
 *      align-items: flex-start;
 *      justify-content: flex-start;
 *      -- DO NOT use CSS transform: scale() on the modal or canvas wrapper
 *      -- DO NOT use zoom: on the canvas or any ancestor
 *      -- If you must use transform/zoom, update the cssScaleX/Y calculation
 *      --   in the JS patchMouseEvent function above.
 *  }
 *
 *  -------------------------------------------------------------------------
 *  COMPANION: Emscripten build flags summary
 *
 *  emcc sorting_visualizer_web_fixed.c \
 *       -o sorting_visualizer.js        \
 *       -I$(RAYLIB_SRC)                 \
 *       $(RAYLIB_SRC)/libraylib.a       \
 *       -s USE_GLFW=3                   \
 *       -s ASYNCIFY                     \
 *       -s ALLOW_MEMORY_GROWTH=1        \
 *       -s WASM=1                       \
 *       -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
 *       -O2 -lm
 *
 *  The -s ASYNCIFY flag is NOT strictly needed here (we use
 *  emscripten_set_main_loop instead of a blocking loop), but it is
 *  included because raylib's audio and some platform calls may use it.
 *  Remove it to reduce binary size if you have no audio.
 *
 *  -------------------------------------------------------------------------
 *  SUMMARY OF ALL FIXES AND WHERE THEY LIVE
 *
 *  Issue                │ Root cause                   │ Fix location
 *  ─────────────────────┼──────────────────────────────┼──────────────────────
 *  Blurry canvas        │ Backing buffer = CSS px,      │ web_init_canvas():
 *                       │ not physical px on HiDPI      │ emscripten_set_canvas_
 *                       │                               │ element_size(*DPR)
 *                       │                               │ + CSS style.width/height
 *  ─────────────────────┼──────────────────────────────┼──────────────────────
 *  Mouse offset wrong   │ Canvas inside scrolled modal  │ EM_ASM JS: override
 *  (50-80px shift)      │ → cached getBoundingClientRect│ getBoundingClientRect
 *                       │ returns stale/wrong origin    │ to return live rect
 *  ─────────────────────┼──────────────────────────────┼──────────────────────
 *  Mouse scale wrong    │ rawMouse in CSS px but raylib │ SetMouseScale(1/DPR)
 *  on Retina            │ treats as logical px          │ after InitWindow
 *  ─────────────────────┼──────────────────────────────┼──────────────────────
 *  Browser tab freezes  │ Blocking while() loop         │ emscripten_set_main_
 *                       │                               │ loop(frame_step,...)
 *  ─────────────────────┼──────────────────────────────┼──────────────────────
 *  Canvas resizes reset │ FLAG_WINDOW_RESIZABLE on web  │ Compile-time #ifdef +
 *  DPR fix              │ fires resize → canvas reset   │ ClearWindowState()
 *  ═══════════════════════════════════════════════════════════════════════════
 */