/*
# Author: Shijie Ma, Wina Prasetyo
# Date:   16/10/2025
# File:   C file
# Description:  Two-player random solvable maze for UCFK4 (5x7 LED matrix)
*/

/* Course-provided headers (already in ence260-ucfk4 tree) */
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "system.h"
#include "pacer.h"
#include "tinygl.h"
#include "font5x7_1.h"
#include "navswitch.h"
#include "ir_uart.h"

/* ---- Board / grid variables ---- */
#define W 5                 /* Maze width */
#define H 7                 /* Maze height */
          
static uint8_t START_X, START_Y;    /* Player starting position */
static uint8_t GOAL_X, GOAL_Y;      /* Goal position */

/* ---- Maze representation -----------------------------------
 * MAZE[x] uses the low 7 bits: bit y==1 means "wall at (x,y)".
 * Therefore: 1 = wall, 0 = free.
 * ------------------------------------------------------------ */
static uint8_t MAZE[W];

/* ----- Maze helper functions ----- */

/* ----- maze_is_wall() ----- 
 * Check if a cell is a wall
 * ------------------------------------------------------------- */
static inline bool maze_is_wall(uint8_t x, uint8_t y) {
    if (x >= W || y >= H) return true;      /* out of bounds = wall */
    return (MAZE[x] >> y) & 1u;
}

/* ----- maze_set_wall() ----- 
 * Set a cell as a wall
 * ------------------------------------------------------------- */
static inline void maze_set_wall(uint8_t x, uint8_t y) {
    if (x < W && y < H) MAZE[x] |= (uint8_t)(1u << y);
}

/* ----- maze_set_free() ----- 
 * Set a cell as free space
 * ------------------------------------------------------------- */
static inline void maze_set_free(uint8_t x, uint8_t y) {
    if (x < W && y < H) MAZE[x] &= (uint8_t)~(1u << y);
}

/* ----- maze_fill_all_walls() ----- 
 * Fill entire grid with walls (all 7 bits set in each column)
 * ------------------------------------------------------------- */
static void maze_fill_all_walls (void)
{
    for (uint8_t x = 0; x < W; x++) {
        MAZE[x] = 0x7F; /* 0b1111111 -> all rows are walls */
    }
}

/* ----- urand_u8() ----- 
 * Simple uniform RNG helper: return [0, m-1]
 * ------------------------------------------------------------- */
static inline uint8_t urand_u8(uint8_t m) {
    return (uint8_t)(rand() % m);
}

/* ----- pick_random_edge() ----- 
 * Pick a random edge for start/goal 
 * ------------------------------------------------------------- */
static void pick_random_edge(uint8_t* x, uint8_t* y, uint8_t exclude_edge) {
    uint8_t edge;
    do { edge = urand_u8(4); } while (edge == exclude_edge); /* avoid same edge as start */

    switch (edge) {
        case 0: *x = 1 + urand_u8(W - 2); *y = 0; break;       /* Top edge */
        case 1: *x = 1 + urand_u8(W - 2); *y = H - 1; break;   /* Bottom edge */
        case 2: *x = 0; *y = 1 + urand_u8(H - 2); break;       /* Left edge */
        case 3: *x = W - 1; *y = 1 + urand_u8(H - 2); break;   /* Right edge */
    }
}

/* ----- Maze generation functions  -----
 * Strategy:
 *   - Keep borders as walls by only carving inside 1..W-2 / 1..H-2.
 *   - Carve a path from START to GOAL using a biased random walk:
 *       prefer steps that reduce |dx| + |dy| to the goal (but not always).
 *   - Optionally open extra cells (density knob) to vary difficulty.
 * ------------------------------------------------------------- */

/* ----- carve_path_biased() -----
 * Carve a guaranteed path from (sx,sy) to (gx,gy) inside the interior.
 * ------------------------------------------------------------- */
static void carve_path_biased(uint8_t sx, uint8_t sy, uint8_t gx, uint8_t gy) {
    uint8_t x = sx, y = sy;
    maze_set_free(x, y); /* clear starting point */

    /* Directions: 0=E, 1=W, 2=S, 3=N */
    const int8_t dx[4] = { +1, -1, 0, 0 };
    const int8_t dy[4] = { 0, 0, +1, -1 };

    /* Guard to avoid pathological loops on tiny grids */
    for (uint16_t guard = 0; (x != gx || y != gy) && guard < 400; guard++) {
        /* Randomize evaluation order */
        uint8_t order[4] = {0, 1, 2, 3};
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t j = urand_u8(4);
            uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
        }

        /* Bias: pick the direction that reduces Manhattan distance most,
           but only ~2/3 of the time so we keep variety. */
        if (urand_u8(10) < 7) {
            uint8_t best_i = 0; int best_d = 999;
            for (uint8_t k = 0; k < 4; k++) {
                int nx = (int)x + dx[order[k]];
                int ny = (int)y + dy[order[k]];
                if (nx <= 0 || nx >= W-1 || ny <= 0 || ny >= H-1) continue;
                int d = abs((int)gx - nx) + abs((int)gy - ny);
                if (d < best_d) { best_d = d; best_i = k; }
            }
            /* Move best to front */
            uint8_t tmp = order[0]; order[0] = order[best_i]; order[best_i] = tmp;
        }

        /* Take the first feasible direction in 'order' and carve */
        bool moved = false;
        for (uint8_t k = 0; k < 4; k++) {
            int nx = (int)x + dx[order[k]];
            int ny = (int)y + dy[order[k]];
            if (nx <= 0 || nx >= W-1 || ny <= 0 || ny >= H-1) continue;
            x = (uint8_t)nx; y = (uint8_t)ny;
            maze_set_free(x, y);
            moved = true;
            break;
        }

        /* If we somehow didn't move (rare on 5x7), nudge toward goal axis-wise */
        if (!moved) {
            if (x < gx && x+1 < W-1) x++;
            else if (x > gx && x-1 > 0) x--;
            else if (y < gy && y+1 < H-1) y++;
            else if (y > gy && y-1 > 0) y--;
            maze_set_free(x, y);
        }
    }
    maze_set_free(gx, gy); /* ensure the goal cell is free */
}

/* ----- open_random_interior() -----
 * Open up some additional interior cells to tune difficulty.
 * density: 0..100 (higher -> more walls -> harder).
 * Only flip cells that are still walls, never cover the carved path.
 * ------------------------------------------------------------- */
static void open_random_interior(uint8_t density) {
    for (uint8_t x = 1; x < W-1; x++)
        for (uint8_t y = 1; y < H-1; y++)
            if (maze_is_wall(x, y) && urand_u8(100) > density)
                maze_set_free(x, y);
}

/* ----- generate_maze() -----
 * For maze generator
 * Public: build a fresh random, solvable maze.
 * ------------------------------------------------------------- */
static void generate_maze(uint16_t seed) {
    srand(seed);
    maze_fill_all_walls();     /* fill maze with walls */

    /* Pick start and goal positions */
    uint8_t start_edge = urand_u8(4);
    switch (start_edge) {
        case 0: START_X = 1 + urand_u8(W-2); START_Y = 0; break;
        case 1: START_X = 1 + urand_u8(W-2); START_Y = H-1; break;
        case 2: START_X = 0; START_Y = 1 + urand_u8(H-2); break;
        case 3: START_X = W-1; START_Y = 1 + urand_u8(H-2); break;
    }

    pick_random_edge(&GOAL_X, &GOAL_Y, start_edge);

    /* Adjust interior coordinates so path carving doesn't hit walls */
    uint8_t sx_in = START_X, sy_in = START_Y;
    uint8_t gx_in = GOAL_X, gy_in = GOAL_Y;
    if (START_Y == 0) sy_in++;
    else if (START_Y == H-1) sy_in--;
    if (START_X == 0) sx_in++;
    else if (START_X == W-1) sx_in--;
    if (GOAL_Y == 0) gy_in++;
    else if (GOAL_Y == H-1) gy_in--;
    if (GOAL_X == 0) gx_in++;
    else if (GOAL_X == W-1) gx_in--;

    carve_path_biased(sx_in, sy_in, gx_in, gy_in);   /* create path */
    open_random_interior(65);                        /* random gaps */
    maze_set_free(START_X, START_Y);                 /* clear start */
    maze_set_free(GOAL_X, GOAL_Y);                   /* clear goal */
}

/* ----- draw_frame() -----
 * For rendering
 * We redraw the entire 5x7 frame each iteration.
 * On such a tiny grid this is simpler and perfectly fine.
 * drawing the maze, player position, and blinking for the player.
 * ------------------------------------------------------------- */
static void draw_frame(uint8_t px, uint8_t py, bool blink_on) {
    tinygl_clear();
    for (uint8_t x = 0; x < W; x++)
        for (uint8_t y = 0; y < H; y++)
            if (maze_is_wall(x, y)) tinygl_draw_point(tinygl_point(x,y),1);
    if (blink_on) tinygl_draw_point(tinygl_point(px, py), 1);
}

/* ----- ir_check() -----
 * Check IR receiver and update game flags
 * ready_other: set true if opponent pressed ready
 * lost: set true if opponent won
 * other_seed: recieve and reconstruct seed from IR in two 8-bits part
 * ------------------------------------------------------------- */
static void ir_check(bool *ready_other, bool *lost, uint16_t *other_seed) {
    while (ir_uart_read_ready_p()) {
        uint8_t msg = ir_uart_getc();
        if (msg == 'R') {
            *ready_other = true;
            uint8_t high = ir_uart_getc();             /* Read high byte of seed */
            uint8_t low = ir_uart_getc();              /* Read low byte of seed */
            *other_seed = ((uint16_t)high << 8) | low; /* Combine two bytes into one 16-bit value to form full seed */
        } else if (msg == 'W') {
            *lost = true;
        }
    }
}

/* ----- Game states ----- */
typedef enum {
    STATE_PRESS_PLAY,      /* Waiting for player to press */
    STATE_GENERATE_GAME,   /* Generate maze once both ready */
    STATE_COUNTDOWN,       /* Show 3-2-1 countdown */
    STATE_PLAYING,         /* Player moves in maze */
    STATE_RESULT           /* Show W/L and reset */
} game_state_t;

/* ----- Main Function ----- */
int main(void) {
    /* Hardware / framwork init */
    system_init();
    navswitch_init();
    ir_uart_init();

    static const uint16_t PACER_RATE = 300; /* 300 Hz is smooth on 5x7 display */
    pacer_init(PACER_RATE);

    /* Setting the tinygl init, speed, and font */
    tinygl_init(PACER_RATE);
    tinygl_font_set(&font5x7_1);
    tinygl_text_speed_set(12);

    /* Game variables */
    game_state_t state = STATE_PRESS_PLAY;
    bool ready_other = false;  /* Did opponent press ready? */
    bool you_sent = false;     /* Did you send ready? */
    bool won = false;
    bool lost = false;
    uint8_t px = START_X, py = START_Y;
    uint16_t blink_counter = 0;
    bool blink_on = true;

    uint16_t my_seed = 0;
    uint16_t other_seed = 0;
    uint16_t shared_seed = 0;

    /* Phase timers (in pacer ticks) */
    uint16_t ticks = 0;
    const uint16_t RESET_T = PACER_RATE * 2; /* Resetting timer ~2 s  */

    static const uint16_t BLINK_PERIOD = 150;   /* Player blink period */

    while (1) {
        pacer_wait();
        navswitch_update();

        /* Update the blink state */
        blink_counter++;
        if (blink_counter >= BLINK_PERIOD) blink_counter = 0;
        blink_on = (blink_counter < BLINK_PERIOD / 2);


        switch(state) {
            case STATE_PRESS_PLAY:
                /* Show "P" until player presses */
                if(you_sent == false) {
                    tinygl_text("P");
                }

                /* IR receive at all times  */
                ir_check(&ready_other, &lost, &other_seed);

                /* IR transmit that you're ready once pushed */
                if (navswitch_push_event_p(NAVSWITCH_PUSH)) {
                    ir_uart_putc('R');
                
                    my_seed = (TCNT1 ^ (TCNT1 << 8)) & 0xFFFF; /* Generate a random 16-bit seed using the current Timer1 value */
                    /* Sending the seed via IR in two 8-bit parts */
                    ir_uart_putc(my_seed >> 8);                /* Send a high byte of seed */
                    ir_uart_putc(my_seed & 0xFF);              /* Send a low byte of seed */

                    you_sent = true;

                    /* If an opponent hasn't been detected */
                    if (ready_other == false) {
                        tinygl_clear();
                        tinygl_text("Waiting");
                        /* Check the opponent again */
                        ir_check(&ready_other, &lost, &other_seed);
                    }
                }

                /* only when both ready do you move on */
                if (you_sent && ready_other) {
                    shared_seed = my_seed ^ other_seed;   /* seed from both player using XOR to get shared, symmetric seed */
                    state = STATE_GENERATE_GAME;
                }
                
                break;

            case STATE_GENERATE_GAME:
                /* Generate the maze for the game */
                generate_maze(shared_seed);
                px = START_X; py = START_Y;
                won = lost = false;
                state = STATE_COUNTDOWN;
                tinygl_text("3");  /* Start countdown */
                break;

            case STATE_COUNTDOWN:
                /* changing the screen for the countdown and move onto playing state once done */
                for (uint8_t i=3;i>0;i--) {
                    char buf[2]; buf[0]='0'+i; buf[1]='\0';
                    tinygl_text(buf);
                    for (uint16_t j=0;j<800;j++){ pacer_wait(); tinygl_update(); }
                }
                state = STATE_PLAYING;
                break;

            case STATE_PLAYING:
                /* check if other board is done already */
                ir_check(&ready_other, &lost, &other_seed);
                
                if (!won && !lost) {
                    /* the movement of the blinking player if not lost yet */
                    if (navswitch_push_event_p(NAVSWITCH_NORTH) && !maze_is_wall(px, py-1)) py--;
                    if (navswitch_push_event_p(NAVSWITCH_SOUTH) && !maze_is_wall(px, py+1)) py++;
                    if (navswitch_push_event_p(NAVSWITCH_WEST) && !maze_is_wall(px-1, py)) px--;
                    if (navswitch_push_event_p(NAVSWITCH_EAST) && !maze_is_wall(px+1, py)) px++;

                    /* win if it reaches goal */
                    if (px == GOAL_X && py == GOAL_Y) {
                        won = true;
                        ir_uart_putc('W');
                    }
                    draw_frame(px, py, blink_on); /* drawing the movement and blinking */
                } else {
                    /* if lose, display result and move to the next state */
                    state = STATE_RESULT;
                    ticks = 0;
                    tinygl_clear();

                    if (won) {
                        tinygl_text("W");
                    }
                    else if (lost) {
                        tinygl_text("L");
                    }
                }
                break;

            case STATE_RESULT:
                /* Wait before resetting everything and going back to first playing state */
                if (++ticks >= RESET_T) {
                    ready_other = false;
                    you_sent = false;                    
                    tinygl_text("P");
                    state = STATE_PRESS_PLAY;
                }
                break;
        }

        tinygl_update(); /* must be called every cycle to refresh display */
    }
}
