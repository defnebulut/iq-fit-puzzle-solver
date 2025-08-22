#ifndef INIT_H
#define INIT_H

#include <stdint.h>

#define BOARD_W 11
#define BOARD_H 5
#define BOARD_CELLS (BOARD_W * BOARD_H)
#define NUM_PIECES 12
#define MAX_ORIENTS 8 

typedef struct {
    int shape[4][4];
    int w, h;
} Orient;

extern Orient orient[NUM_PIECES][MAX_ORIENTS];
extern int orient_cnt[NUM_PIECES];

extern const char piece_sym[NUM_PIECES];

typedef struct {
    uint64_t mask;
    uint8_t  piece;
} Placement;

extern Placement *place;
extern int place_cnt;
extern int p_first[NUM_PIECES];
extern int p_count[NUM_PIECES];

void init_all(void);

#endif
