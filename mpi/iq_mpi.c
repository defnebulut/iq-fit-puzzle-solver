#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include "init.h"

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward64)

static inline int ctzll(uint64_t x) {
    if (x == 0) return 64;
    unsigned long index;
    _BitScanForward64(&index, x);
    return (int)index;
}
#define __builtin_ctzll(x) ctzll(x)
#endif

#ifdef _WIN32
  #include <windows.h>
  #ifndef CLOCK_MONOTONIC
    #define CLOCK_MONOTONIC 0
  #endif

  static int clock_gettime(int dummy, struct timespec *ts) {
      static LARGE_INTEGER freq;
      static BOOL initialized = FALSE;
      LARGE_INTEGER count;
      if (!initialized) {
          QueryPerformanceFrequency(&freq);
          initialized = TRUE;
      }
      QueryPerformanceCounter(&count);
      ts->tv_sec  = (long)(count.QuadPart / freq.QuadPart);
      ts->tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1000000000 / freq.QuadPart);
      return 0;
  }
#endif

static uint64_t neighbor_masks[BOARD_CELLS];
static uint64_t FULL_MASK;

static int *placements_by_cell[BOARD_CELLS];
static int  placements_by_cell_cnt[BOARD_CELLS];

static uint8_t *idx_pid;

static inline int orphan_1x1(uint64_t occ)
{
    for (int p = 0; p < BOARD_CELLS; ++p) {
        if (!(occ & (1ULL << p)) &&
            (neighbor_masks[p] & occ) == neighbor_masks[p]) {
            return 1;
        }
    }
    return 0;
}
#define SHOULD_PRUNE(m)  orphan_1x1(m)

static FILE     *fp_out;
static uint64_t  sol_written = 0;

static void dump_solution(char board[])
{
    ++sol_written;
    fprintf(fp_out, "Solution %" PRIu64 ":\n", sol_written);
    for (int r = 0; r < BOARD_H; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            fputc(board[r * BOARD_W + c], fp_out);
            fputc(' ', fp_out);
        }
        fputc('\n', fp_out);
    }
    fputs("==========\n", fp_out);
}

static void emit(uint64_t occ_piece[NUM_PIECES])
{
    char B[BOARD_CELLS];

    memset(B, '.', BOARD_CELLS);
    for (int pid = 0; pid < NUM_PIECES; ++pid) {
        uint64_t m = occ_piece[pid];
        while (m) {
            int b = __builtin_ctzll(m);
            B[b] = piece_sym[pid];
            m &= m - 1;
        }
    }
    dump_solution(B);
}


static void dfs(uint64_t occ, uint32_t used_mask,
                uint64_t occ_piece[NUM_PIECES], int depth)
{
    if (SHOULD_PRUNE(occ)) return;

    if (used_mask == (1u << NUM_PIECES) - 1) {
        emit(occ_piece);
        return;
    }

    int first = __builtin_ctzll(~occ & FULL_MASK);

    int *lst = placements_by_cell[first];
    int  cnt = placements_by_cell_cnt[first];

    for (int k = 0; k < cnt; ++k) {
        int idx = lst[k];
        int pid = idx_pid[idx];
        if (used_mask & (1u << pid)) continue;

        uint64_t pmask = place[idx].mask;
        if (pmask & occ) continue;

        occ_piece[pid] = pmask;
        dfs(occ | pmask, used_mask | (1u << pid), occ_piece, depth + 1);
        occ_piece[pid] = 0;
    }
}

static void build_tables(void)
{
    FULL_MASK = (BOARD_CELLS == 64)
                ? ~0ULL
                : ((1ULL << BOARD_CELLS) - 1);

    for (int r = 0; r < BOARD_H; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            int bit = r * BOARD_W + c;
            uint64_t N = 0;
            if (r)              N |= 1ULL << (bit - BOARD_W);
            if (r + 1 < BOARD_H) N |= 1ULL << (bit + BOARD_W);
            if (c)            N |= 1ULL << (bit - 1);
            if (c + 1 < BOARD_W) N |= 1ULL << (bit + 1);
            neighbor_masks[bit] = N;
        }
    }

    idx_pid = malloc(place_cnt);
    for (int pid = 0; pid < NUM_PIECES; ++pid) {
        for (int i = p_first[pid]; i < p_first[pid] + p_count[pid]; ++i) {
            idx_pid[i] = pid;
        }
    }

    memset(placements_by_cell_cnt, 0,
           sizeof(placements_by_cell_cnt));
    for (int idx = 0; idx < place_cnt; ++idx) {
        uint64_t m = place[idx].mask;
        while (m) {
            int b = __builtin_ctzll(m);
            ++placements_by_cell_cnt[b];
            m &= m - 1;
        }
    }
    for (int b = 0; b < BOARD_CELLS; ++b) {
        placements_by_cell[b] = malloc(
            placements_by_cell_cnt[b] * sizeof(int));
        placements_by_cell_cnt[b] = 0;
    }

    for (int idx = 0; idx < place_cnt; ++idx) {
        uint64_t m = place[idx].mask;
        while (m) {
            int b = __builtin_ctzll(m);
            placements_by_cell[b][
                placements_by_cell_cnt[b]++] = idx;
            m &= m - 1;
        }
    }
}

static void write_board_to_file(FILE *f, char board[], uint64_t *counter) {
    ++(*counter);
    fprintf(f, "Solution %" PRIu64 ":\n", *counter);
    for (int r = 0; r < BOARD_H; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            fputc(board[r * BOARD_W + c], f);
            fputc(' ', f);
        }
        fputc('\n', f);
    }
    fputs("==========\n", f);
}

static void merge_generate_and_cleanup(int nprocs, double elapsed_time)
{
    FILE *out = fopen("solutions.txt", "w");
    if (!out) {
        fprintf(stderr, "[Rank 0] Unable to create solutions.txt\n");
        return;
    }

    uint64_t total_solutions_written = 0;
    char line[256];
    char fname[64];
    char current_board[BOARD_CELLS];
    int board_row = 0;

    for (int r_idx = 0; r_idx < nprocs; ++r_idx) {
        snprintf(fname, sizeof(fname), "solutions_%d.txt", r_idx);
        FILE *in = fopen(fname, "r");
        if (!in) continue;

        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, "Solution ", 9) == 0) {
                board_row = 0;
            } else if (strncmp(line, "==========", 10) == 0) {
                char B_h_mir[BOARD_CELLS], B_v_mir[BOARD_CELLS], B_180_rot[BOARD_CELLS];

                for (int r = 0; r < BOARD_H; ++r) {
                    for (int c = 0; c < BOARD_W; ++c) {
                        B_h_mir[r * BOARD_W + c] = current_board[r * BOARD_W + (BOARD_W - 1 - c)];
                        B_v_mir[r * BOARD_W + c] = current_board[(BOARD_H - 1 - r) * BOARD_W + c];
                        B_180_rot[r * BOARD_W + c] = current_board[(BOARD_H - 1 - r) * BOARD_W + (BOARD_W - 1 - c)];
                    }
                }
                
                write_board_to_file(out, current_board, &total_solutions_written);
                if (memcmp(current_board, B_h_mir, BOARD_CELLS) != 0) write_board_to_file(out, B_h_mir, &total_solutions_written);
                if (memcmp(current_board, B_v_mir, BOARD_CELLS) != 0 && memcmp(B_h_mir, B_v_mir, BOARD_CELLS) != 0) write_board_to_file(out, B_v_mir, &total_solutions_written);
                if (memcmp(current_board, B_180_rot, BOARD_CELLS) != 0 && memcmp(B_h_mir, B_180_rot, BOARD_CELLS) != 0 && memcmp(B_v_mir, B_180_rot, BOARD_CELLS) != 0) write_board_to_file(out, B_180_rot, &total_solutions_written);

            } else if (board_row < BOARD_H) {
                for (int c = 0; c < BOARD_W; ++c) {
                    current_board[board_row * BOARD_W + c] = line[c * 2];
                }
                board_row++;
            }
        }
        fclose(in);
        if (remove(fname) != 0) {
            fprintf(stderr, "[Rank 0] Warning: Could not delete %s\n", fname);
        }
    }

    fclose(out);
    
    printf("\n=== FINAL RESULTS ===\n");
    printf("Total solutions found (all symmetries): %" PRIu64 "\n", total_solutions_written);
}

int main(int argc, char **argv)
{
    int rank, nprocs;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    init_all();
    build_tables();

    char fname[64];
    snprintf(fname, sizeof(fname), "solutions_%d.txt", rank);
    fp_out = fopen(fname, "w");
    if (!fp_out) {
        fprintf(stderr, "[Rank %d] Failed to open %s\n", rank, fname);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int first = __builtin_ctzll(FULL_MASK);
    int *lst = placements_by_cell[first];
    int  cnt  = placements_by_cell_cnt[first];

    for (int k = 0; k < cnt; ++k) {
        if ((k % nprocs) != rank) continue;
        int idx = lst[k];
        int pid = idx_pid[idx];
        uint64_t pmask = place[idx].mask;

        uint64_t occ_piece[NUM_PIECES] = {0};
        occ_piece[pid] = pmask;
        dfs(pmask, (1u << pid), occ_piece, 1);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double local_elapsed = (t1.tv_sec - t0.tv_sec) +
                           (t1.tv_nsec - t0.tv_nsec) / 1e9;

    uint64_t canonical_count = 0;
    double   max_elapsed  = 0.0;
    MPI_Reduce(&sol_written, &canonical_count, 1,
               MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&local_elapsed, &max_elapsed, 1,
               MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n=== INTERIM RESULTS ===\n");
        printf("Canonical solutions found: %" PRIu64 "\n", canonical_count);
        printf("Elapsed (max over ranks): %.2f s\n", max_elapsed);
    }
    

    fclose(fp_out);
    fp_out = NULL;

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        merge_generate_and_cleanup(nprocs, max_elapsed);
    }

    free(idx_pid);
    for (int b = 0; b < BOARD_CELLS; ++b) {
        free(placements_by_cell[b]);
    }

    MPI_Finalize();
    return 0;
}
