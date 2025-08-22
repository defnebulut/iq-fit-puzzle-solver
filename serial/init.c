#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "init.h"


Orient orient[NUM_PIECES][MAX_ORIENTS];
int orient_cnt[NUM_PIECES] = {0};
const char piece_sym[NUM_PIECES] =
        {'Y','O','R','L','P','U','B','C','A','X','D','G'};

static int same_shape(const int a[4][4], const int b[4][4])
{
    for (int i=0;i<4;i++) for (int j=0;j<4;j++)
        if (a[i][j]!=b[i][j]) return 0;
    return 1;
}

static void normalise(int dst[4][4], const int src[4][4])
{
    int min_r=4, min_c=4;
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        if(src[r][c]) { if(r<min_r) min_r=r; if(c<min_c) min_c=c; }
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        dst[r][c] = ( (r+min_r<4 && c+min_c<4) ? src[r+min_r][c+min_c] : 0 );
}

static void rot90(int dst[4][4], const int src[4][4])
{
    int tmp[4][4] = {0};
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        if(src[r][c]) tmp[c][3-r]=1;
    normalise(dst,tmp);
}

static void flip(int dst[4][4], const int src[4][4])
{
    int tmp[4][4] = {0};
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        if(src[r][c]) tmp[r][3-c]=1;
    normalise(dst,tmp);
}

static void add_orient(int id, const int shape[4][4])
{
    for(int k=0;k<orient_cnt[id];k++)
        if(same_shape(orient[id][k].shape,shape)) return;

    Orient *o = &orient[id][orient_cnt[id]++];
    memcpy(o->shape,shape,sizeof(int)*16);

    int max_r=-1,max_c=-1;
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        if(shape[r][c]) { if(r>max_r) max_r=r; if(c>max_c) max_c=c; }
    o->h = max_r+1;
    o->w = max_c+1;
}

static void gen_orients(int id, const int base[4][4])
{
    int current_shape[4][4];
    int flipped_shape[4][4];
    normalise(current_shape, base);

    const int symmetry_breaking_piece_id = 3;

    for (int i = 0; i < 4; i++) {
        add_orient(id, current_shape);

        if (id != symmetry_breaking_piece_id) {
            flip(flipped_shape, current_shape);
            add_orient(id, flipped_shape);
        }

        int next_shape[4][4];
        rot90(next_shape, current_shape);
        memcpy(current_shape, next_shape, sizeof(int) * 16);
    }
}


Placement *place = NULL;
int place_cnt  = 0;
int p_first[NUM_PIECES];
int p_count[NUM_PIECES];

static inline int cell(int r,int c){ return r*BOARD_W+c; }

static void add_place(uint8_t pid,uint64_t mask)
{
    place = realloc(place,(place_cnt+1)*sizeof *place);
    place[place_cnt].piece = pid;
    place[place_cnt].mask  = mask;
    ++place_cnt;
}

static void gen_placements(void)
{
    const int rotational_symmetry_piece_id = 9; 

    for(int p=0;p<NUM_PIECES;p++){
        p_first[p] = place_cnt;

        for(int o=0;o<orient_cnt[p];o++){
            Orient *or = &orient[p][o];
            for(int r=0;r<=BOARD_H-or->h;r++)
            for(int c=0;c<=BOARD_W-or->w;c++){

                if (p == rotational_symmetry_piece_id) {
                    int r_new = BOARD_H - r - or->h;
                    int c_new = BOARD_W - c - or->w;
                    if (r_new < r || (r_new == r && c_new < c)) {
                        continue;
                    }
                }

                uint64_t m = 0ULL;
                for(int i=0;i<or->h;i++)
                for(int j=0;j<or->w;j++)
                    if(or->shape[i][j])
                        m |= 1ULL << cell(r+i,c+j);

                add_place(p,m);
            }
        }
        p_count[p] = place_cnt - p_first[p];
    }
}

void init_all(void)
{
    const int y[4][4] = {{1,1,1,1},{0,1,0,0},{0,0,0,0},{0,0,0,0}};
    const int o[4][4] = {{0,0,1,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}};
    const int r[4][4] = {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}};
    const int l[4][4] = {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}};
    const int p[4][4] = {{0,1,0,0},{1,1,0,0},{1,0,0,0},{1,0,0,0}};
    const int u[4][4] = {{1,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}};
    const int b[4][4] = {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}};
    const int c[4][4] = {{1,1,1,0},{0,0,1,0},{0,0,1,0},{0,0,0,0}};
    const int a[4][4] = {{1,1,0,0},{0,1,0,0},{0,0,0,0},{0,0,0,0}};
    const int x[4][4] = {{1,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}};
    const int d[4][4] = {{1,1,1,0},{0,1,0,0},{0,0,0,0},{0,0,0,0}};
    const int g[4][4] = {{1,1,1,0},{1,0,1,0},{0,0,0,0},{0,0,0,0}};
    const int (*bases[NUM_PIECES])[4] = {y,o,r,l,p,u,b,c,a,x,d,g};

    for(int i=0;i<NUM_PIECES;i++)
        gen_orients(i, bases[i]);

    gen_placements();
}