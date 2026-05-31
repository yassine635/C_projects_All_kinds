#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

// ─── Config ────────────────────────────────────────────────────────────────
#define COUNT       80
#define WIN_W       1300
#define WIN_H       900
#define BAR_AREA_X  20
#define BAR_AREA_Y  130
#define BAR_AREA_W  (WIN_W - 40)
#define BAR_AREA_H  460
#define BTN_COUNT   10
#define FPS         60

// ─── Colors ────────────────────────────────────────────────────────────────
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

// ─── State ─────────────────────────────────────────────────────────────────
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

// Bar highlight roles
#define ROLE_NONE    0
#define ROLE_COMPARE 1
#define ROLE_SWAP    2
#define ROLE_PIVOT   3
#define ROLE_ACTIVE  4
#define ROLE_SORTED  5

int  arr[COUNT];
int  aux[COUNT];        // scratch buffer for merge/radix/etc.
int  role[COUNT];       // highlight role per bar
bool sorted_flag[COUNT];

int  cmps = 0, swaps = 0;
bool sort_done = false;
bool running   = false;

// ─── Step-machine state ────────────────────────────────────────────────────
// Each algorithm uses a small struct to remember where it is between frames.

typedef struct {
    int  i, j, min_idx;
    int  gap;
    int  exp;
    int  phase;         // multi-phase algos (merge, radix, heap...)
    int  lo, hi, mid;
    int  tmp;
    // merge sort uses a stack-based iterative approach
    int  ms_width;      // current merge width
    int  ms_lo;         // current left start
    // heap sort
    int  hs_n;          // heap size shrink
    int  hs_heapify_i;
    bool hs_building;
    // for radix
    int  cnt[COUNT+10];
    int  out[COUNT];
    int  radix_digit;
    // for counting
    int  count_cnt[COUNT+10];
    int  count_v, count_k;
    bool count_phase2;
    // for bucket
    int  bkt[10][COUNT];
    int  bkt_sz[10];
    int  bkt_b, bkt_bi, bkt_k;
    bool bkt_phase2;
} SortState;

SortState ss;

// ─── Array helpers ─────────────────────────────────────────────────────────
void clear_roles(void) {
    for (int i = 0; i < COUNT; i++)
        role[i] = sorted_flag[i] ? ROLE_SORTED : ROLE_NONE;
}

void do_swap(int i, int j) {
    int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    swaps++;
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

// ─── Per-algorithm step functions ──────────────────────────────────────────
// Each returns true when the whole sort is done.

bool step_bubble(void) {
    if (ss.phase == 0) { ss.i = 0; ss.phase = 1; ss.j = 0; }
    if (ss.i >= COUNT - 1) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    if (ss.j < COUNT - 1 - ss.i) {
        cmps++;
        role[ss.j] = ROLE_COMPARE; role[ss.j+1] = ROLE_COMPARE;
        if (arr[ss.j] > arr[ss.j+1]) { do_swap(ss.j, ss.j+1); role[ss.j] = ROLE_SWAP; role[ss.j+1] = ROLE_SWAP; }
        ss.j++;
    } else {
        sorted_flag[COUNT - 1 - ss.i] = true;
        ss.i++; ss.j = 0;
    }
    return false;
}

bool step_selection(void) {
    if (ss.phase == 0) { ss.i = 0; ss.min_idx = 0; ss.j = 1; ss.phase = 1; }
    if (ss.i >= COUNT - 1) {
        sorted_flag[COUNT-1] = true;
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    role[ss.i]     = ROLE_ACTIVE;
    role[ss.min_idx] = ROLE_PIVOT;
    if (ss.j < COUNT) {
        cmps++;
        role[ss.j] = ROLE_COMPARE;
        if (arr[ss.j] < arr[ss.min_idx]) ss.min_idx = ss.j;
        ss.j++;
    } else {
        if (ss.min_idx != ss.i) { do_swap(ss.i, ss.min_idx); role[ss.i] = ROLE_SWAP; role[ss.min_idx] = ROLE_SWAP; }
        sorted_flag[ss.i] = true;
        ss.i++; ss.min_idx = ss.i; ss.j = ss.i + 1;
    }
    return false;
}

bool step_insertion(void) {
    if (ss.phase == 0) { ss.i = 1; ss.j = 0; ss.tmp = arr[0]; ss.phase = 1; sorted_flag[0] = true; }
    if (ss.i >= COUNT) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    if (ss.phase == 1) {
        ss.tmp = arr[ss.i]; ss.j = ss.i - 1; ss.phase = 2;
        role[ss.i] = ROLE_ACTIVE;
    } else {
        if (ss.j >= 0 && arr[ss.j] > ss.tmp) {
            cmps++; swaps++;
            arr[ss.j+1] = arr[ss.j];
            role[ss.j] = ROLE_COMPARE; role[ss.j+1] = ROLE_SWAP;
            ss.j--;
        } else {
            arr[ss.j+1] = ss.tmp; swaps++;
            sorted_flag[ss.i] = true;
            ss.i++; ss.phase = 1;
        }
    }
    return false;
}

// Iterative merge sort (bottom-up)
bool step_merge(void) {
    if (ss.phase == 0) {
        memcpy(aux, arr, sizeof(int)*COUNT);
        ss.ms_width = 1; ss.ms_lo = 0; ss.phase = 1;
    }
    if (ss.ms_width >= COUNT) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    int lo = ss.ms_lo, mid = lo + ss.ms_width - 1, hi = lo + 2*ss.ms_width - 1;
    if (mid >= COUNT) { ss.ms_width *= 2; ss.ms_lo = 0; return false; }
    if (hi >= COUNT) hi = COUNT - 1;

    // merge [lo..mid] and [mid+1..hi] into aux, then copy back one element at a time
    // For visual step, just do the whole merge at once (merge is fast enough)
    int i = lo, j = mid+1, k = lo;
    while (i <= mid && j <= hi) {
        cmps++;
        role[i] = ROLE_COMPARE; role[j] = ROLE_COMPARE;
        if (arr[i] <= arr[j]) aux[k++] = arr[i++];
        else                   aux[k++] = arr[j++];
        swaps++;
    }
    while (i <= mid) { aux[k++] = arr[i++]; swaps++; }
    while (j <= hi)  { aux[k++] = arr[j++]; swaps++; }
    for (int x = lo; x <= hi; x++) { arr[x] = aux[x]; role[x] = ROLE_ACTIVE; }

    ss.ms_lo += 2 * ss.ms_width;
    if (ss.ms_lo >= COUNT) { ss.ms_width *= 2; ss.ms_lo = 0; }

    if (ss.ms_width >= COUNT)
        for (int x = 0; x < COUNT; x++) sorted_flag[x] = true;
    return false;
}

// Iterative quicksort using explicit stack
#define QS_STACK 64
static int qs_lo[QS_STACK], qs_hi[QS_STACK];
static int qs_top = -1;
static int qs_i, qs_j, qs_pivot;
static bool qs_partitioning = false;

bool step_quick(void) {
    if (ss.phase == 0) {
        qs_top = -1; qs_lo[++qs_top] = 0; qs_hi[qs_top] = COUNT-1;
        ss.phase = 1; qs_partitioning = false;
    }
    clear_roles();
    if (!qs_partitioning) {
        if (qs_top < 0) {
            for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
            return true;
        }
        int lo = qs_lo[qs_top], hi = qs_hi[qs_top];
        if (lo >= hi) { qs_top--; if (lo == hi) sorted_flag[lo] = true; return false; }
        qs_pivot = arr[hi]; qs_i = lo - 1; qs_j = lo;
        qs_partitioning = true;
    }
    int lo = qs_lo[qs_top], hi = qs_hi[qs_top];
    role[hi] = ROLE_PIVOT;
    if (qs_j < hi) {
        cmps++;
        role[qs_j] = ROLE_COMPARE;
        if (arr[qs_j] <= qs_pivot) {
            qs_i++;
            do_swap(qs_i, qs_j);
            role[qs_i] = ROLE_SWAP; role[qs_j] = ROLE_SWAP;
        }
        qs_j++;
    } else {
        do_swap(qs_i+1, hi);
        int p = qs_i+1;
        sorted_flag[p] = true; role[p] = ROLE_PIVOT;
        qs_top--;
        if (p-1 > lo && qs_top+1 < QS_STACK) { qs_lo[++qs_top] = lo;   qs_hi[qs_top] = p-1; }
        if (p+1 < hi && qs_top+1 < QS_STACK) { qs_lo[++qs_top] = p+1; qs_hi[qs_top] = hi; }
        qs_partitioning = false;
    }
    return false;
}

// Iterative heap sort
static int hs_heapify_cur = -1, hs_n_heap = 0;
bool step_heap(void) {
    if (ss.phase == 0) {
        hs_n_heap = COUNT;
        ss.i = COUNT/2 - 1;   // start building heap
        ss.phase = 1;
        hs_heapify_cur = -1;
    }
    clear_roles();

    // Heapify helper: sift down from ss.tmp in a heap of size hs_n_heap
    // We store the node to sift in ss.hs_heapify_i
    if (hs_heapify_cur >= 0) {
        int i2 = hs_heapify_cur, l = 2*i2+1, r = 2*i2+2, lg = i2;
        if (l < hs_n_heap && arr[l] > arr[lg]) lg = l;
        if (r < hs_n_heap && arr[r] > arr[lg]) lg = r;
        cmps++;
        role[i2] = ROLE_ACTIVE;
        if (l < hs_n_heap) role[l] = ROLE_COMPARE;
        if (r < hs_n_heap) role[r] = ROLE_COMPARE;
        if (lg != i2) { do_swap(i2, lg); role[i2] = ROLE_SWAP; role[lg] = ROLE_SWAP; hs_heapify_cur = lg; }
        else           { hs_heapify_cur = -1; }
        return false;
    }

    if (ss.phase == 1) {
        if (ss.i >= 0) { hs_heapify_cur = ss.i; ss.i--; }
        else           { ss.phase = 2; ss.i = COUNT - 1; }
        return false;
    }
    if (ss.phase == 2) {
        if (ss.i <= 0) {
            for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
            return true;
        }
        do_swap(0, ss.i); sorted_flag[ss.i] = true;
        role[0] = ROLE_SWAP; role[ss.i] = ROLE_SWAP;
        hs_n_heap = ss.i; hs_heapify_cur = 0; ss.i--;
        return false;
    }
    return false;
}

bool step_shell(void) {
    if (ss.phase == 0) { ss.gap = COUNT/2; ss.i = ss.gap; ss.phase = 1; }
    if (ss.gap <= 0) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    if (ss.i < COUNT) {
        if (ss.phase == 1) { ss.tmp = arr[ss.i]; ss.j = ss.i; ss.phase = 2; role[ss.i] = ROLE_ACTIVE; }
        if (ss.phase == 2) {
            if (ss.j >= ss.gap && arr[ss.j - ss.gap] > ss.tmp) {
                cmps++; swaps++;
                arr[ss.j] = arr[ss.j - ss.gap];
                role[ss.j] = ROLE_SWAP; role[ss.j - ss.gap] = ROLE_COMPARE;
                ss.j -= ss.gap;
            } else {
                arr[ss.j] = ss.tmp; swaps++;
                ss.i++; ss.phase = 1;
            }
        }
    } else {
        ss.gap /= 2; ss.i = ss.gap; ss.phase = 1;
    }
    return false;
}

bool step_counting(void) {
    if (ss.phase == 0) {
        memset(ss.count_cnt, 0, sizeof(ss.count_cnt));
        ss.i = 0; ss.phase = 1;
    }
    clear_roles();
    if (ss.phase == 1) {   // build count
        if (ss.i < COUNT) {
            cmps++; ss.count_cnt[arr[ss.i]]++; role[ss.i] = ROLE_COMPARE; ss.i++;
        } else {
            ss.count_v = 1; ss.count_k = 0; ss.phase = 2;
        }
        return false;
    }
    if (ss.phase == 2) {   // write back
        if (ss.count_v <= COUNT) {
            if (ss.count_cnt[ss.count_v] > 0) {
                arr[ss.count_k] = ss.count_v; swaps++;
                role[ss.count_k] = ROLE_ACTIVE; sorted_flag[ss.count_k] = true;
                ss.count_k++; ss.count_cnt[ss.count_v]--;
            } else {
                ss.count_v++;
            }
        } else {
            return true;
        }
    }
    return false;
}

bool step_radix(void) {
    if (ss.phase == 0) { ss.exp = 1; ss.i = 0; ss.phase = 1; memset(ss.cnt, 0, sizeof(ss.cnt)); }
    if (ss.exp > COUNT) {
        for (int k = 0; k < COUNT; k++) sorted_flag[k] = true;
        return true;
    }
    clear_roles();
    if (ss.phase == 1) {   // count digit
        if (ss.i < COUNT) { ss.cnt[(arr[ss.i]/ss.exp)%10]++; cmps++; role[ss.i] = ROLE_COMPARE; ss.i++; }
        else { for (int x=1;x<10;x++) ss.cnt[x]+=ss.cnt[x-1]; ss.i=COUNT-1; ss.phase=2; }
    } else if (ss.phase == 2) {  // place into out
        if (ss.i >= 0) { ss.out[--ss.cnt[(arr[ss.i]/ss.exp)%10]] = arr[ss.i]; swaps++; role[ss.i]=ROLE_ACTIVE; ss.i--; }
        else { ss.i=0; ss.phase=3; }
    } else if (ss.phase == 3) {  // copy back
        if (ss.i < COUNT) { arr[ss.i]=ss.out[ss.i]; swaps++; role[ss.i]=ROLE_SWAP; ss.i++; }
        else { ss.exp*=10; ss.i=0; ss.phase=1; memset(ss.cnt,0,sizeof(ss.cnt)); }
    }
    return false;
}

bool step_bucket(void) {
    if (ss.phase == 0) {
        memset(ss.bkt_sz, 0, sizeof(ss.bkt_sz));
        ss.i = 0; ss.phase = 1;
    }
    clear_roles();
    if (ss.phase == 1) {   // distribute
        if (ss.i < COUNT) {
            int b = (int)((float)(arr[ss.i]-1)/COUNT * 10);
            if (b >= 10) b = 9;
            ss.bkt[b][ss.bkt_sz[b]++] = arr[ss.i];
            cmps++; role[ss.i] = ROLE_COMPARE; ss.i++;
        } else {
            // sort each bucket (simple insertion sort inline)
            for (int b = 0; b < 10; b++) {
                for (int x = 1; x < ss.bkt_sz[b]; x++) {
                    int key = ss.bkt[b][x], y = x-1;
                    while (y >= 0 && ss.bkt[b][y] > key) { ss.bkt[b][y+1]=ss.bkt[b][y]; y--; }
                    ss.bkt[b][y+1]=key;
                }
            }
            ss.bkt_b=0; ss.bkt_bi=0; ss.count_k=0; ss.phase=2;
        }
    } else {
        // write back
        if (ss.bkt_b < 10) {
            if (ss.bkt_bi < ss.bkt_sz[ss.bkt_b]) {
                arr[ss.count_k] = ss.bkt[ss.bkt_b][ss.bkt_bi];
                swaps++; role[ss.count_k]=ROLE_ACTIVE; sorted_flag[ss.count_k]=true;
                ss.count_k++; ss.bkt_bi++;
            } else { ss.bkt_b++; ss.bkt_bi=0; }
        } else {
            return true;
        }
    }
    return false;
}

typedef bool (*StepFn)(void);
StepFn STEP_FNS[ALGO_COUNT] = {
    step_bubble, step_selection, step_insertion, step_merge,
    step_quick,  step_heap,     step_shell,     step_counting,
    step_radix,  step_bucket
};

// ─── UI ────────────────────────────────────────────────────────────────────
typedef struct { Rectangle rect; const char *label; bool hovered; } Button;

Button algo_btns[BTN_COUNT];
Button start_btn, reset_btn;
int selected_algo = ALGO_BUBBLE;

// Speed: 1=slow .. 10=fast  → steps per frame
int  speed = 5;
Rectangle speed_slider;

void init_buttons(void) {
    int bw = 96, bh = 30, gap = 6, sx = 20, sy = 10;
    for (int i = 0; i < BTN_COUNT; i++) {
        algo_btns[i].rect   = (Rectangle){sx + i*(bw+gap), sy, bw, bh};
        algo_btns[i].label  = ALGO_NAMES[i];
        algo_btns[i].hovered = false;
    }
    start_btn.rect  = (Rectangle){WIN_W - 200, sy, 85, bh};
    start_btn.label = "Start";
    reset_btn.rect  = (Rectangle){WIN_W - 108, sy, 88, bh};
    reset_btn.label = "Reset";
    speed_slider    = (Rectangle){WIN_W - 200, sy + 40, 178, 14};
}

void draw_button(Button *b, bool active, Color normal, Color hover) {
    Color bg = b->hovered ? hover : normal;
    if (active) bg = COL_BTN_ACT;
    DrawRectangleRounded(b->rect, 0.3f, 6, bg);
    DrawRectangleRoundedLines(b->rect, 0.3f, 6 ,(Color){80,80,100,120}); // Fixed: lineThick then Color
    int fw = MeasureText(b->label, 13);
    DrawText(b->label,
             (int)(b->rect.x + b->rect.width/2 - fw/2),
             (int)(b->rect.y + b->rect.height/2 - 7),
             13, active ? WHITE : COL_TEXT);
}

void draw_speed_slider(void) {
    // track
    DrawRectangleRounded(speed_slider, 0.5f, 4, COL_BTN);
    // fill
    float t = (speed - 1) / 9.0f;
    Rectangle fill = speed_slider; fill.width = speed_slider.width * t;
    DrawRectangleRounded(fill, 0.5f, 4, COL_BTN_ACT);
    // thumb
    float tx = speed_slider.x + speed_slider.width * t;
    DrawCircle((int)tx, (int)(speed_slider.y + speed_slider.height/2), 8, WHITE);
    DrawCircle((int)tx, (int)(speed_slider.y + speed_slider.height/2), 6, COL_BTN_ACT);
    DrawText("Speed", (int)speed_slider.x, (int)(speed_slider.y - 18), 12, COL_MUTED);
    char buf[8]; sprintf(buf, "%d", speed);
    DrawText(buf, (int)(speed_slider.x + speed_slider.width + 8),
             (int)(speed_slider.y - 2), 13, COL_TEXT);
}

void draw_bars(void) {
    float bw = (float)BAR_AREA_W / COUNT;
    for (int i = 0; i < COUNT; i++) {
        float val = (float)arr[i] / COUNT;
        float bh  = val * BAR_AREA_H;
        float x   = BAR_AREA_X + i * bw;
        float y   = BAR_AREA_Y + BAR_AREA_H - bh;
        Color c;
        switch (role[i]) {
            case ROLE_COMPARE: c = COL_COMPARE; break;
            case ROLE_SWAP:    c = COL_SWAP;    break;
            case ROLE_PIVOT:   c = COL_PIVOT;   break;
            case ROLE_ACTIVE:  c = COL_ACTIVE;  break;
            case ROLE_SORTED:  c = COL_SORTED;  break;
            default:           c = COL_BAR;     break;
        }
        DrawRectangleRec((Rectangle){x+1, y, fmaxf(bw-2,1), bh}, c);
    }
}

void draw_legend(void) {
    const char *labels[] = {"Default","Comparing","Swap/Write","Pivot","Active","Sorted"};
    Color       colors[] = {COL_BAR,COL_COMPARE,COL_SWAP,COL_PIVOT,COL_ACTIVE,COL_SORTED};
    int x = BAR_AREA_X, y = BAR_AREA_Y + BAR_AREA_H + 12;
    for (int i = 0; i < 6; i++) {
        DrawRectangle(x, y+2, 12, 12, colors[i]);
        DrawText(labels[i], x+16, y, 12, COL_MUTED);
        x += MeasureText(labels[i],12) + 30;
    }
}

void draw_stats(void) {
    int y = 50;
    char buf[80];
    DrawText(ALGO_NAMES[selected_algo], BAR_AREA_X, y, 20, WHITE);
    sprintf(buf, "Comparisons: %d", cmps);
    DrawText(buf, BAR_AREA_X + 280, y+2, 14, COL_MUTED);
    sprintf(buf, "Swaps / Writes: %d", swaps);
    DrawText(buf, BAR_AREA_X + 480, y+2, 14, COL_MUTED);
    if (sort_done) DrawText("SORTED!", BAR_AREA_X + 700, y+2, 14, COL_SORTED);
    else if (running) DrawText("Sorting...", BAR_AREA_X + 700, y+2, 14, COL_PIVOT);
    else DrawText("Press Start", BAR_AREA_X + 700, y+2, 14, COL_MUTED);
}

// Steps per frame based on speed 1-10
int steps_per_frame(void) {
    int tbl[11] = {0, 1, 1, 2, 3, 5, 8, 14, 25, 50, 120};
    return tbl[speed];
}

// ─── Main ──────────────────────────────────────────────────────────────────
int main(void) {
    srand((unsigned)time(NULL));
    init_rand();
    reset_sort();
    init_buttons();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "Sorting Visualizer");
    SetTargetFPS(FPS);

    while (!WindowShouldClose()) {
        // ── Input ──
        Vector2 mouse = GetMousePosition();

        // Algo buttons
        for (int i = 0; i < BTN_COUNT; i++) {
            algo_btns[i].hovered = CheckCollisionPointRec(mouse, algo_btns[i].rect);
            if (algo_btns[i].hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !running) {
                selected_algo = i;
                init_rand(); reset_sort();
            }
        }

        // Start / Reset
        start_btn.hovered = CheckCollisionPointRec(mouse, start_btn.rect);
        reset_btn.hovered = CheckCollisionPointRec(mouse, reset_btn.rect);

        if (start_btn.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !running && !sort_done) {
            running = true;
        }
        if (reset_btn.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            init_rand(); reset_sort();
        }

        // Speed slider
        if (CheckCollisionPointRec(mouse, (Rectangle){speed_slider.x-10, speed_slider.y-10, speed_slider.width+20, speed_slider.height+20})
            && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float t = (mouse.x - speed_slider.x) / speed_slider.width;
            if (t < 0) t = 0; if (t > 1) t = 1;
            speed = (int)(t * 9 + 1.5f);
            if (speed < 1) speed = 1; if (speed > 10) speed = 10;
        }

        // ── Sort step ──
        if (running && !sort_done) {
            int steps = steps_per_frame();
            for (int s = 0; s < steps && !sort_done; s++) {
                bool done = STEP_FNS[selected_algo]();
                if (done) { sort_done = true; running = false; clear_roles(); }
            }
        }

        // ── Draw ──
        BeginDrawing();
        ClearBackground(COL_BG);

        DrawRectangle(0, 0, WIN_W, 80, COL_PANEL);
        DrawLine(0, 80, WIN_W, 80, (Color){60,60,80,200});

        for (int i = 0; i < BTN_COUNT; i++)
            draw_button(&algo_btns[i], i == selected_algo, COL_BTN, COL_BTN_HOV);

        draw_button(&start_btn, false, COL_START, COL_START_H);
        draw_button(&reset_btn, false, COL_RESET, COL_RESET_H);
        draw_speed_slider();
        draw_stats();
        draw_bars();
        draw_legend();

        EndDrawing();
    }
    CloseWindow();
    return 0;
}