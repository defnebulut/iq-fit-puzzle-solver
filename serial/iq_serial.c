#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include "init.h"

static uint64_t neighbor_masks[BOARD_CELLS];
static uint64_t FULL_MASK;

static int *placements_by_cell[BOARD_CELLS];
static int placements_by_cell_cnt[BOARD_CELLS];

static uint8_t  *idx_pid;

static inline int orphan_1x1(uint64_t occ)
{
    for (int p = 0; p < BOARD_CELLS; ++p)
        if (!(occ & (1ULL<<p)) &&
            (neighbor_masks[p] & occ) == neighbor_masks[p])
            return 1;
    return 0;
}
#define SHOULD_PRUNE(m)  orphan_1x1(m)

static FILE     *fp_out;
static uint64_t  sol_written = 0;

static void dump_solution(char board[])
{
    ++sol_written;
    fprintf(fp_out,"Solution %" PRIu64 ":\n", sol_written);
    for (int r = 0; r < BOARD_H; ++r) {
        for (int c = 0; c < BOARD_W; ++c)
            fputc(board[r*BOARD_W+c], fp_out), fputc(' ', fp_out);
        fputc('\n', fp_out);
    }
    fputs("==========\n", fp_out);
}
static void emit(uint64_t occ_piece[NUM_PIECES])
{
    char B[BOARD_CELLS];
    char B_h_mir[BOARD_CELLS];
    char B_v_mir[BOARD_CELLS];
    char B_180_rot[BOARD_CELLS];

    memset(B, '.', BOARD_CELLS);
    for (int pid = 0; pid < NUM_PIECES; ++pid) {
        uint64_t m = occ_piece[pid];
        while (m) {
            int b = __builtin_ctzll(m);
            B[b] = piece_sym[pid];
            m &= m - 1;
        }
    }

    for (int r = 0; r < BOARD_H; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            B_h_mir[r * BOARD_W + c] = B[r * BOARD_W + (BOARD_W - 1 - c)];
            B_v_mir[r * BOARD_W + c] = B[(BOARD_H - 1 - r) * BOARD_W + c];
            B_180_rot[r * BOARD_W + c] = B[(BOARD_H - 1 - r) * BOARD_W + (BOARD_W - 1 - c)];
        }
    }

    dump_solution(B);

    if (memcmp(B, B_h_mir, BOARD_CELLS) != 0) {
        dump_solution(B_h_mir);
    }

    if (memcmp(B, B_v_mir, BOARD_CELLS) != 0 && memcmp(B_h_mir, B_v_mir, BOARD_CELLS) != 0) {
        dump_solution(B_v_mir);
    }

    if (memcmp(B, B_180_rot, BOARD_CELLS) != 0 && memcmp(B_h_mir, B_180_rot, BOARD_CELLS) != 0 &&
        memcmp(B_v_mir, B_180_rot, BOARD_CELLS) != 0) {
        dump_solution(B_180_rot);
    }
}

static void dfs(uint64_t occ, uint32_t used_mask,
                uint64_t occ_piece[NUM_PIECES], int depth)
{
    if (SHOULD_PRUNE(occ)) return;

    if (used_mask == (1u<<NUM_PIECES)-1) {
        emit(occ_piece);
        return;
    }

    int first = __builtin_ctzll(~occ & FULL_MASK);

    int *lst = placements_by_cell[first];
    int  cnt = placements_by_cell_cnt[first];

    for (int k=0;k<cnt;++k) {
        int idx  = lst[k];
        int pid  = idx_pid[idx];

        if (used_mask & (1u<<pid)) continue;

        uint64_t pmask = place[idx].mask;
        if (pmask & occ) continue;

        occ_piece[pid] = pmask;
        dfs(occ|pmask, used_mask|(1u<<pid), occ_piece, depth+1);
        occ_piece[pid] = 0;
    }
}
static void build_tables(void)
{
    FULL_MASK = (BOARD_CELLS==64)?~0ULL:((1ULL<<BOARD_CELLS)-1);

    for (int r=0;r<BOARD_H;++r)
        for (int c=0;c<BOARD_W;++c) {
            int bit = r*BOARD_W + c;
            uint64_t N = 0;
            if (r)               N |= 1ULL<<(bit-BOARD_W);
            if (r+1<BOARD_H)     N |= 1ULL<<(bit+BOARD_W);
            if (c)               N |= 1ULL<<(bit-1);
            if (c+1<BOARD_W)     N |= 1ULL<<(bit+1);
            neighbor_masks[bit]=N;
        }

    idx_pid = malloc(place_cnt);
    for (int pid=0; pid<NUM_PIECES; ++pid)
        for (int i=p_first[pid]; i<p_first[pid]+p_count[pid]; ++i)
            idx_pid[i]=pid;

    memset(placements_by_cell_cnt,0,sizeof(placements_by_cell_cnt));
    for (int idx=0; idx<place_cnt; ++idx) {
        uint64_t m = place[idx].mask;
        while (m){ int b=__builtin_ctzll(m);
                   ++placements_by_cell_cnt[b]; m&=m-1; }
    }
    for (int b=0;b<BOARD_CELLS;++b)
        placements_by_cell[b]=malloc(placements_by_cell_cnt[b]*sizeof(int)),
        placements_by_cell_cnt[b]=0;

    for (int idx=0; idx<place_cnt; ++idx) {
        uint64_t m = place[idx].mask;
        while (m){ int b=__builtin_ctzll(m);
                   placements_by_cell[b][ placements_by_cell_cnt[b]++ ] = idx;
                   m&=m-1; }
    }
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);

    init_all();
    build_tables();

    fp_out = fopen("solutions.txt","w");
    if(!fp_out){ perror("solutions.txt"); return 1; }

    struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);

    uint64_t occ_piece[NUM_PIECES]={0};
    dfs(0ULL,0,occ_piece,0);

    clock_gettime(CLOCK_MONOTONIC,&t1);
    double sec = (t1.tv_sec - t0.tv_sec) +
                 (t1.tv_nsec - t0.tv_nsec)/1e9;

    printf("\n=== RESULTS ===\n"
           "Total solutions written: %" PRIu64 "\n"
           "Elapsed: %.2f s\n", sol_written, sec);

    fclose(fp_out);
    return 0;
}
