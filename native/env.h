#ifndef ENV_H
#define ENV_H

#include <stdbool.h>
#include <stdint.h>

#define MS_PER_TICK 8

#define MAX_PARTS (1 << 9)  // MUST be a power of 2
#define MAX_PARTS_MASK (MAX_PARTS - 1)
#define MAX_THICKNESS 6
#define THICKNESS_RATIO 29
#define FOOD_VALUE_RATIO (5 / 1569.0f)
#define DRAIN_RATIO 0.08f
#define DRAIN_RATE 150

#define NUM_ANGLES (1 << 8)  // MUST be a power of 2
#define NUM_ANGLES_MASK (NUM_ANGLES - 1)
#define PI 3.14159265359f
#define PI2 6.28318530718f
#define NPI2 (NUM_ANGLES / PI2)
#define INPI2 (PI2 / NUM_ANGLES)
#define COLLISION_MARGIN 10

#define SPOS_X(snake, i) snake.px[i][0]
#define SPOS_Y(snake, i) snake.py[i][0]

#define BORDER_MARGIN 0.02f
#define SPAWN_MARGIN 0.1f

typedef struct {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

typedef float body_parts[1 + MAX_PARTS];

typedef struct env {
  struct env_cfg {
    float rad;
    float bs;
    float str;
    float ms;
    float ts;
    float tsr;
    float msr;
    int mrr;
    int sl;
    int mp;
    int msn;
  } cfg;

  struct env_dat {
    pcg32_random_t rng;

    float sin[NUM_ANGLES];
    float cos[NUM_ANGLES];
    float pgr[MAX_PARTS + 1];

    int sl2;

    int* sids;
    int* fids;
    int scid;
    int fcid;

    float srad;
    float srad2;
    float sprad;
    float sprad2;

    double ctm;
  } dat;

  struct env_csnake {
    int csz;
    int gsz;
    int** cgrd;
    int* sidx;
    int* s0;
    int* s1;
    int* dead;

    float icsz;
  } csnake;

  struct env_snake {
    float* t;
    float* g;
    float* sp;
    float* bs;
    float* ts;
    float* btr;
    float* bv;
    float* mr;
    float* mr2;
    float* ang;
    float* tang;
    float* sin;
    float* cos;
    float* rad;
    float* radx2;
    float* rad2;

    body_parts* px;
    body_parts* py;

    bool* b;
    bool* state;
    int* hi;
    int* np;
    int* dir;
    int* kc;
    int* id;

    double* ldtm;
  } snake;

  struct env_cfood {
    int csz;
    int** cgrd;
    int gsz;
    float icsz;
  } cfood;

  struct env_food {
    float* x;
    float* y;
    float* v;
    int* ci;
    int* id;
  } food;

  void* user_data;

  void (*on_kill)(struct env*, int, int);
  void (*on_eat)(struct env*, int, int);
  void (*on_growth)(struct env*, int);
  void (*on_lpart)(struct env*, int);
  void (*on_move)(struct env*, int, bool);
  void (*on_bfspawn)(struct env*, int, int);
  void (*on_dfspawn)(struct env*, int, int, int);

} env;

bool env_init(env* e);
void env_destroy(env* e);
void env_reset(env* e);
void env_tick(env* e, float dtms);
int env_new_snake(env* e, float x, float y, float ang);
int env_new_food(env* e, float x, float y, float v);

#endif