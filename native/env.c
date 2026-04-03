#include "env.h"

#include <math.h>

#include "util/tdarray.h"

static inline uint32_t pcg32_random_r(pcg32_random_t* rng) {
  uint64_t oldstate = rng->state;
  rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static inline float pcg32_float(pcg32_random_t* rng) {
  return (pcg32_random_r(rng) >> 8) * (1.0f / 16777216.0f);
}

void pcg32_seed(pcg32_random_t* rng, uint64_t seed) {
  rng->state = 0U;
  rng->inc = (seed << 1u) | 1u;
  pcg32_random_r(rng);
  rng->state += seed;
  pcg32_random_r(rng);
}

static inline int fast_mini(int a, int b) { return (a < b) ? a : b; }
static inline int fast_maxi(int a, int b) { return (a > b) ? a : b; }
static inline float fast_minf(float a, float b) { return (a < b) ? a : b; }
static inline float fast_maxf(float a, float b) { return (a > b) ? a : b; }
static inline float fast_modf(float x, float y) { return x - (int)(x / y) * y; }
static inline float fast_sinf(struct env_dat* dat, float r) {
  return dat->sin[(int)(r * NPI2) & NUM_ANGLES_MASK];
}
static inline float fast_cosf(struct env_dat* dat, float r) {
  return dat->cos[(int)(r * NPI2) & NUM_ANGLES_MASK];
}
static inline int fast_floorf(float x) {
  int i = (int)x;
  return i - (x < (float)i);
}
static inline bool point_segment2(float px, float py, float x1, float y1,
                                  float x2, float y2, float r2) {
  float vx = x2 - x1;
  float vy = y2 - y1;
  float wx = px - x1;
  float wy = py - y1;
  float sl2 = vx * vx + vy * vy;
  float t = 0.f;
  if (sl2 > 0.f) t = (wx * vx + wy * vy) / sl2;
  t = fast_maxf(0.f, fast_minf(1.f, t));
  wx -= t * vx;
  wy -= t * vy;
  return wx * wx + wy * wy <= r2;
}

static inline void recalc_snake_factors(struct env* e, int i) {
  e->snake.t[i] = fast_minf(MAX_THICKNESS, 1 + (e->snake.np[i] - 2) / 106.0f);
  float tr = (7 - e->snake.t[i]) / 6.0f;
  e->snake.ts[i] = e->cfg.ts * (0.13f + 0.87f * tr * tr);
  e->snake.bs[i] = e->cfg.bs + e->cfg.str * e->snake.t[i];
  e->snake.mr[i] = e->cfg.mrr * powf(e->snake.t[i], 0.65f);
  e->snake.mr2[i] = e->snake.mr[i] * e->snake.mr[i];
  e->snake.btr[i] = 1.0f + 0.305f * (e->snake.t[i] - 1);
  e->snake.radx2[i] = e->snake.t[i] * THICKNESS_RATIO;
  e->snake.rad[i] = e->snake.radx2[i] * 0.5f;
  e->snake.rad2[i] = e->snake.rad[i] * e->snake.rad[i];
}

bool env_init(env* e) {
  if (e->cfg.mp > MAX_PARTS) return false;

  pcg32_seed(&e->dat.rng, 0);

  for (int i = 0; i < NUM_ANGLES; i++) {
    e->dat.sin[i] = sinf(i * INPI2);
    e->dat.cos[i] = cosf(i * INPI2);
  }

  for (int i = 0; i < e->cfg.mp; i++)
    e->dat.pgr[i] = powf(1 - i / (float)e->cfg.mp, 2.25f);
  e->dat.pgr[e->cfg.mp] = e->dat.pgr[e->cfg.mp - 1];

  e->dat.sl2 = e->cfg.sl * e->cfg.sl;

  int mt = MAX_THICKNESS * THICKNESS_RATIO;
  e->csnake.csz = (mt + e->cfg.sl) + COLLISION_MARGIN;
  e->csnake.gsz = (int)ceilf((e->cfg.rad * 2) / e->csnake.csz);
  e->csnake.icsz = 1.0f / e->csnake.csz;
  int cells = e->csnake.gsz * e->csnake.gsz;
  e->csnake.cgrd = tdarray_create(int*, cells);

  for (int i = 0; i < cells; i++) {
    int* cell = tdarray_create(int, 8);
    tdarray_push(&e->csnake.cgrd, &cell);
  }

  int mr = (int)ceilf(e->cfg.mrr * powf(MAX_THICKNESS, 0.65f));
  e->cfood.csz = mr + COLLISION_MARGIN;
  e->cfood.gsz = (int)ceilf((e->cfg.rad * 2) / e->cfood.csz);
  e->cfood.icsz = 1.0f / e->cfood.csz;
  cells = e->cfood.gsz * e->cfood.gsz;
  e->cfood.cgrd = tdarray_create(int*, cells);

  for (int i = 0; i < cells; i++) {
    int* cell = tdarray_create(int, 8);
    tdarray_push(&e->cfood.cgrd, &cell);
  }

  e->dat.srad = e->cfg.rad * (1 - BORDER_MARGIN);
  e->dat.srad2 = e->dat.srad * e->dat.srad;
  e->dat.sprad = e->dat.srad * (1 - SPAWN_MARGIN);
  e->dat.sprad2 = e->dat.sprad * e->dat.sprad;

  e->snake.sin = tdarray_create(float, 8);
  e->snake.cos = tdarray_create(float, 8);
  e->snake.ang = tdarray_create(float, 8);
  e->snake.tang = tdarray_create(float, 8);
  e->snake.np = tdarray_create(int, 8);
  e->snake.hi = tdarray_create(int, 8);
  e->snake.bv = tdarray_create(float, 8);
  e->snake.b = tdarray_create(int, 8);
  e->snake.g = tdarray_create(float, 8);
  e->snake.dir = tdarray_create(int, 8);
  e->snake.state = tdarray_create(int, 8);
  e->snake.kc = tdarray_create(int, 8);
  e->snake.ldtm = tdarray_create(double, 8);
  e->snake.t = tdarray_create(float, 8);
  e->snake.ts = tdarray_create(float, 8);
  e->snake.bs = tdarray_create(float, 8);
  e->snake.mr = tdarray_create(float, 8);
  e->snake.mr2 = tdarray_create(float, 8);
  e->snake.btr = tdarray_create(float, 8);
  e->snake.radx2 = tdarray_create(float, 8);
  e->snake.rad = tdarray_create(float, 8);
  e->snake.rad2 = tdarray_create(float, 8);
  e->snake.sp = tdarray_create(float, 8);
  e->snake.px = tdarray_create(body_parts, 8);
  e->snake.py = tdarray_create(body_parts, 8);
  e->snake.id = tdarray_create(int, 8);

  e->dat.ids = tdarray_create(int, 8);

  e->food.x = tdarray_create(float, 8);
  e->food.y = tdarray_create(float, 8);
  e->food.v = tdarray_create(float, 8);
  e->food.ci = tdarray_create(int, 8);

  e->csnake.sidx = tdarray_create(int, 8);
  e->csnake.s0 = tdarray_create(int, 8);
  e->csnake.s1 = tdarray_create(int, 8);
  e->csnake.dead = tdarray_create(int, 8);

  env_reset(e);

  return true;
}

void env_destroy(env* e) {
  tdarray_destroy(e->csnake.sidx);
  tdarray_destroy(e->csnake.s0);
  tdarray_destroy(e->csnake.s1);
  tdarray_destroy(e->csnake.dead);

  tdarray_destroy(e->food.x);
  tdarray_destroy(e->food.y);
  tdarray_destroy(e->food.v);
  tdarray_destroy(e->food.ci);

  tdarray_destroy(e->snake.sin);
  tdarray_destroy(e->snake.cos);
  tdarray_destroy(e->snake.ang);
  tdarray_destroy(e->snake.tang);
  tdarray_destroy(e->snake.np);
  tdarray_destroy(e->snake.hi);
  tdarray_destroy(e->snake.bv);
  tdarray_destroy(e->snake.b);
  tdarray_destroy(e->snake.g);
  tdarray_destroy(e->snake.dir);
  tdarray_destroy(e->snake.state);
  tdarray_destroy(e->snake.kc);
  tdarray_destroy(e->snake.ldtm);
  tdarray_destroy(e->snake.t);
  tdarray_destroy(e->snake.ts);
  tdarray_destroy(e->snake.bs);
  tdarray_destroy(e->snake.mr);
  tdarray_destroy(e->snake.mr2);
  tdarray_destroy(e->snake.btr);
  tdarray_destroy(e->snake.radx2);
  tdarray_destroy(e->snake.rad);
  tdarray_destroy(e->snake.rad2);
  tdarray_destroy(e->snake.sp);
  tdarray_destroy(e->snake.px);
  tdarray_destroy(e->snake.py);
  tdarray_destroy(e->snake.id);

  tdarray_destroy(e->dat.ids);

  int cells = e->csnake.gsz * e->csnake.gsz;

  for (int i = 0; i < cells; i++) {
    tdarray_destroy(e->csnake.cgrd[i]);
  }

  tdarray_destroy(e->csnake.cgrd);

  cells = e->cfood.gsz * e->cfood.gsz;

  for (int i = 0; i < cells; i++) {
    tdarray_destroy(e->cfood.cgrd[i]);
  }

  tdarray_destroy(e->cfood.cgrd);
}

static inline void env_tick_movement(env* e, float dtms) {
  for (int i = tdarray_length(e->snake.t) - 1; i >= 0; i--) {
    if (e->snake.state[i] == 0) {
      float* px = e->snake.px[i] + 1;
      float* py = e->snake.py[i] + 1;

      if (e->snake.b[i]) {
        if (e->dat.ctm - e->snake.ldtm[i] > DRAIN_RATE) {
          e->snake.ldtm[i] = e->dat.ctm;
          if (e->snake.np[i] > 2 || e->snake.g[i] >= 0.14f) {
            int ldx = (e->snake.hi[i] - (e->snake.np[i] - 1)) & MAX_PARTS_MASK;
            env_new_food(e, px[ldx], py[ldx], 4);
          }
          int tl = (int)(e->snake.np[i] + e->snake.g[i]);
          e->snake.g[i] -= e->dat.pgr[tl] * DRAIN_RATIO;
        }
      }

      if (e->snake.g[i] <= 0) {
        if (e->snake.np[i] == 2) {
          e->snake.b[i] = 0;
          e->snake.g[i] = 0;
        } else {
          e->snake.g[i] += 0.99999f;
          e->snake.np[i]--;
          recalc_snake_factors(e, i);
        }
      }

      float dt = -0.02f + (0.015 + 0.02f) * e->snake.b[i];
      e->snake.bv[i] += dt * dtms / e->snake.btr[i];

      if (e->snake.bv[i] > 1.0f)
        e->snake.bv[i] = 1.0f;
      else if (e->snake.bv[i] < 0.0f)
        e->snake.bv[i] = 0.0f;

      e->snake.sp[i] =
          e->snake.bs[i] + (e->cfg.ms - e->snake.bs[i]) * e->snake.bv[i];

      float sp = e->snake.sp[i] * dtms * 0.25f;
      if (sp >= e->cfg.sl) sp = e->cfg.sl;

      float ts = e->snake.ts[i] * dtms;
      float dta = e->snake.tang[i] - e->snake.ang[i];

      if (dta > PI) dta -= PI2;
      if (dta < -PI) dta += PI2;

      if (dta > ts) {
        e->snake.ang[i] = fast_modf(e->snake.ang[i] + ts, PI2);
        e->snake.dir[i] = 2;
      } else if (dta < -ts) {
        e->snake.ang[i] = fast_modf(e->snake.ang[i] - ts + PI2, PI2);
        e->snake.dir[i] = 1;
      } else {
        e->snake.ang[i] = e->snake.tang[i];
        e->snake.dir[i] = 0;
      }

      e->snake.sin[i] = fast_sinf(&e->dat, e->snake.ang[i]);
      e->snake.cos[i] = fast_cosf(&e->dat, e->snake.ang[i]);

      SPOS_X(e->snake, i) += sp * e->snake.cos[i];
      SPOS_Y(e->snake, i) += sp * e->snake.sin[i];

      float phx = px[e->snake.hi[i]];
      float phy = py[e->snake.hi[i]];

      float dx = SPOS_X(e->snake, i) - phx;
      float dy = SPOS_Y(e->snake, i) - phy;

      float d2 = dx * dx + dy * dy;

      if (d2 >= e->dat.sl2) {
        e->snake.hi[i] = (e->snake.hi[i] + 1) & MAX_PARTS_MASK;

        px[e->snake.hi[i]] = SPOS_X(e->snake, i);
        py[e->snake.hi[i]] = SPOS_Y(e->snake, i);

        if (e->snake.g[i] >= 1 && e->snake.np[i] < e->cfg.mp) {
          e->snake.g[i] -= 1;
          e->snake.np[i]++;

          recalc_snake_factors(e, i);
        }

        if (e->snake.np[i] > 3) {
          int pidx = (e->snake.hi[i] - 2) & MAX_PARTS_MASK;
          int n = 0;
          float mv = 0;

          for (int j = 3; j < e->snake.np[i]; j++) {
            int idx = (e->snake.hi[i] - j) & MAX_PARTS_MASK;
            n++;
            if (n < 5) mv = e->cfg.tsr * n / 4.0f;

            px[idx] += (px[pidx] - px[idx]) * mv;
            py[idx] += (py[pidx] - py[idx]) * mv;

            pidx = idx;
          }
        }
      }
    }
  }
}

static inline void env_grid_insert_segment(struct env* e, int i, int s0,
                                           int s1) {
  float rad = e->snake.rad[i];

  float px0 = e->snake.px[i][s0];
  float py0 = e->snake.py[i][s0];
  float px1 = e->snake.px[i][s1];
  float py1 = e->snake.py[i][s1];

  float min_x = fast_minf(px0, px1) - rad;
  float max_x = fast_maxf(px0, px1) + rad;
  float min_y = fast_minf(py0, py1) - rad;
  float max_y = fast_maxf(py0, py1) + rad;

  int min_cx = fast_floorf(min_x * e->csnake.icsz);
  int max_cx = fast_floorf(max_x * e->csnake.icsz);
  int min_cy = fast_floorf(min_y * e->csnake.icsz);
  int max_cy = fast_floorf(max_y * e->csnake.icsz);

  min_cx = fast_maxi(min_cx, 0);
  min_cy = fast_maxi(min_cy, 0);
  max_cx = fast_mini(max_cx, e->csnake.gsz - 1);
  max_cy = fast_mini(max_cy, e->csnake.gsz - 1);

  for (int cy = min_cy; cy <= max_cy; cy++) {
    for (int cx = min_cx; cx <= max_cx; cx++) {
      int cell = cy * e->csnake.gsz + cx;
      int s = tdarray_length(e->csnake.s0);

      tdarray_push(&e->csnake.sidx, &i);
      tdarray_push(&e->csnake.s0, &s0);
      tdarray_push(&e->csnake.s1, &s1);

      tdarray_push(&e->csnake.cgrd[cell], &s);
    }
  }
}

static inline void env_grid_insert_food(struct env* e, int i) {
  int gx = e->food.x[i] * e->cfood.icsz;
  int gy = e->food.y[i] * e->cfood.icsz;
  int h = gy * e->cfood.gsz + gx;
  int ci = tdarray_length(e->cfood.cgrd[h]);
  tdarray_push(&e->food.ci, &ci);
  tdarray_push(&e->cfood.cgrd[h], &i);
}

static inline void env_remove_food(env* e, int i) {
  int last = tdarray_length(e->food.x) - 1;

  int gx = e->food.x[i] * e->cfood.icsz;
  int gy = e->food.y[i] * e->cfood.icsz;
  int h = gy * e->cfood.gsz + gx;

  int ci = e->food.ci[i];
  int* cell = e->cfood.cgrd[h];
  int n = tdarray_length(cell);

  int moved = cell[n - 1];
  cell[ci] = moved;
  e->food.ci[moved] = ci;
  tdarray_pop(e->cfood.cgrd[h]);

  if (i != last) {
    e->food.x[i] = e->food.x[last];
    e->food.y[i] = e->food.y[last];
    e->food.v[i] = e->food.v[last];
    e->food.ci[i] = e->food.ci[last];

    int lgx = e->food.x[i] * e->cfood.icsz;
    int lgy = e->food.y[i] * e->cfood.icsz;
    int lh = lgy * e->cfood.gsz + lgx;

    int lci = e->food.ci[i];
    e->cfood.cgrd[lh][lci] = i;
  }

  tdarray_pop(e->food.x);
  tdarray_pop(e->food.y);
  tdarray_pop(e->food.v);
  tdarray_pop(e->food.ci);
}

static inline void env_remove_snake(env* e, int i) {
  tdarray_push(&e->dat.ids, &e->snake.id[i]);
  int last = tdarray_length(e->snake.t) - 1;

  if (i != last) {
    e->snake.t[i] = e->snake.t[last];
    e->snake.g[i] = e->snake.g[last];
    e->snake.sp[i] = e->snake.sp[last];
    e->snake.bs[i] = e->snake.bs[last];
    e->snake.ts[i] = e->snake.ts[last];
    e->snake.btr[i] = e->snake.btr[last];
    e->snake.bv[i] = e->snake.bv[last];
    e->snake.mr[i] = e->snake.mr[last];
    e->snake.mr2[i] = e->snake.mr2[last];
    e->snake.ang[i] = e->snake.ang[last];
    e->snake.tang[i] = e->snake.tang[last];
    e->snake.sin[i] = e->snake.sin[last];
    e->snake.cos[i] = e->snake.cos[last];
    e->snake.rad[i] = e->snake.rad[last];
    e->snake.radx2[i] = e->snake.radx2[last];
    e->snake.rad2[i] = e->snake.rad2[last];
    e->snake.ldtm[i] = e->snake.ldtm[last];

    SPOS_X(e->snake, i) = SPOS_X(e->snake, last);
    SPOS_Y(e->snake, i) = SPOS_Y(e->snake, last);

    float* pxi = e->snake.px[i] + 1;
    float* pyi = e->snake.py[i] + 1;

    float* pxl = e->snake.px[last] + 1;
    float* pyl = e->snake.py[last] + 1;

    for (int j = 0; j < e->snake.np[last]; j++) {
      int idx = (e->snake.hi[last] - j) & MAX_PARTS_MASK;
      pxi[idx] = pxl[idx];
      pyi[idx] = pyl[idx];
    }

    e->snake.b[i] = e->snake.b[last];
    e->snake.hi[i] = e->snake.hi[last];
    e->snake.np[i] = e->snake.np[last];
    e->snake.dir[i] = e->snake.dir[last];
    e->snake.state[i] = e->snake.state[last];
    e->snake.kc[i] = e->snake.kc[last];
    e->snake.id[i] = e->snake.id[last];
  }

  tdarray_pop(e->snake.t);
  tdarray_pop(e->snake.g);
  tdarray_pop(e->snake.sp);
  tdarray_pop(e->snake.bs);
  tdarray_pop(e->snake.ts);
  tdarray_pop(e->snake.btr);
  tdarray_pop(e->snake.bv);
  tdarray_pop(e->snake.mr);
  tdarray_pop(e->snake.mr2);
  tdarray_pop(e->snake.ang);
  tdarray_pop(e->snake.tang);
  tdarray_pop(e->snake.sin);
  tdarray_pop(e->snake.cos);
  tdarray_pop(e->snake.rad);
  tdarray_pop(e->snake.radx2);
  tdarray_pop(e->snake.rad2);
  tdarray_pop(e->snake.ldtm);
  tdarray_pop(e->snake.px);
  tdarray_pop(e->snake.py);
  tdarray_pop(e->snake.b);
  tdarray_pop(e->snake.hi);
  tdarray_pop(e->snake.np);
  tdarray_pop(e->snake.dir);
  tdarray_pop(e->snake.state);
  tdarray_pop(e->snake.kc);
  tdarray_pop(e->snake.id);
}

static inline void env_grid_insert_snake(env* e, int i) {
  int s0 = 0;
  for (int j = 0; j < e->snake.np[i]; j++) {
    int s1 = ((e->snake.hi[i] - j) & MAX_PARTS_MASK) + 1;
    env_grid_insert_segment(e, i, s0, s1);
    s0 = s1;
  }
}

static inline void env_build_csnake_grid(env* e) {
  tdarray_clear(e->csnake.sidx);
  tdarray_clear(e->csnake.s0);
  tdarray_clear(e->csnake.s1);

  int cells = e->csnake.gsz * e->csnake.gsz;
  for (int i = 0; i < cells; i++) tdarray_clear(e->csnake.cgrd[i]);

  for (int i = tdarray_length(e->snake.t) - 1; i >= 0; i--)
    if (e->snake.state[i] == 0) env_grid_insert_snake(e, i);
}

static inline void env_food_collision(env* e) {
  for (int i = tdarray_length(e->snake.t) - 1; i >= 0; i--) {
    if (e->snake.state[i] == 0) {
      float mx = SPOS_X(e->snake, i) + e->snake.cos[i] *
                                           (0.36 * e->snake.radx2[i] + 31) *
                                           e->cfg.msr * e->snake.sp[i];
      float my = SPOS_Y(e->snake, i) + e->snake.sin[i] *
                                           (0.36 * e->snake.radx2[i] + 31) *
                                           e->cfg.msr * e->snake.sp[i];

      int gx = mx * e->cfood.icsz;
      int gy = my * e->cfood.icsz;

      gx = fast_maxi(1, fast_mini(gx, e->cfood.gsz - 2));
      gy = fast_maxi(1, fast_mini(gy, e->cfood.gsz - 2));

      for (int cy = -1; cy <= 1; cy++) {
        for (int cx = -1; cx <= 1; cx++) {
          int nx = gx + cx;
          int ny = gy + cy;
          int h = ny * e->cfood.gsz + nx;

          for (int j = tdarray_length(e->cfood.cgrd[h]) - 1; j >= 0; j--) {
            int f = e->cfood.cgrd[h][j];
            float dx = e->food.x[f] - mx;
            float dy = e->food.y[f] - my;

            if (dx * dx + dy * dy <= e->snake.mr2[i]) {
              int tl = (int)(e->snake.np[i] + e->snake.g[i]);
              e->snake.g[i] += e->dat.pgr[tl] * e->food.v[f] * e->food.v[f] *
                               FOOD_VALUE_RATIO;
              env_remove_food(e, f);
            }
          }
        }
      }
    }
  }
}

static inline void env_snake_border_collision(env* e) {
  for (int i = tdarray_length(e->snake.t) - 1; i >= 0; i--) {
    float hx = SPOS_X(e->snake, i) + e->snake.cos[i] * e->snake.rad[i];
    float hy = SPOS_Y(e->snake, i) + e->snake.sin[i] * e->snake.rad[i];

    float cx = e->cfg.rad;
    float cy = e->cfg.rad;

    float dx = hx - cx;
    float dy = hy - cy;

    if (dx * dx + dy * dy >= e->dat.srad2) {
      tdarray_push(&e->csnake.dead, &i);
      e->snake.state[i] = 2;
    }
  }
}

static inline void env_snake_snake_collision(env* e) {
  for (int i = tdarray_length(e->snake.t) - 1; i >= 0; i--) {
    if (e->snake.state[i] == 0) {
      float hx = SPOS_X(e->snake, i) + e->snake.cos[i] * e->snake.rad[i];
      float hy = SPOS_Y(e->snake, i) + e->snake.sin[i] * e->snake.rad[i];

      int gx = hx * e->csnake.icsz;
      int gy = hy * e->csnake.icsz;
      int cell = gy * e->csnake.gsz + gx;

      for (int j = tdarray_length(e->csnake.cgrd[cell]) - 1; j >= 0; j--) {
        int s = e->csnake.cgrd[cell][j];
        int sidx = e->csnake.sidx[s];

        if (sidx != i && e->snake.state[sidx] == 0) {
          int s0 = e->csnake.s0[s];
          int s1 = e->csnake.s1[s];
          float s0x = e->snake.px[sidx][s0];
          float s0y = e->snake.py[sidx][s0];
          float s1x = e->snake.px[sidx][s1];
          float s1y = e->snake.py[sidx][s1];

          if (point_segment2(hx, hy, s0x, s0y, s1x, s1y, e->snake.rad2[sidx])) {
            tdarray_push(&e->csnake.dead, &i);
            e->snake.state[i] = 1;
            e->snake.kc[sidx]++;

            float lpx = SPOS_X(e->snake, i);
            float lpy = SPOS_Y(e->snake, i);

            float* px = e->snake.px[i] + 1;
            float* py = e->snake.py[i] + 1;

            for (int k = 0; k < e->snake.np[i]; k++) {
              int idx = (e->snake.hi[i] - k) & MAX_PARTS_MASK;
              float tpx = px[idx];
              float tpy = py[idx];

              for (int l = 0; l < 2; l++) {
                float fx = lpx + (tpx - lpx) * l / 2.0f;
                float fy = lpy + (tpy - lpy) * l / 2.0f;
                fx += e->snake.radx2[i] * (pcg32_float(&e->dat.rng) - 0.5);
                fy += e->snake.radx2[i] * (pcg32_float(&e->dat.rng) - 0.5);
                env_new_food(e, fx, fy, 14.2f);
              }
              lpx = tpx;
              lpy = tpy;
            }

            goto EARLY_EXIT;
          }
        }
      }
    EARLY_EXIT:;
    }
  }
}

static inline void env_remove_dead_snakes(env* e) {
  int num_dead = tdarray_length(e->csnake.dead);
  for (int d = 0; d < num_dead; d++) {
    int i = e->csnake.dead[d];
    env_remove_snake(e, i);
    int moved = tdarray_length(e->snake.t);
    if (moved != i) {
      for (int k = d + 1; k < num_dead; k++) {
        if (e->csnake.dead[k] == moved) {
          e->csnake.dead[k] = i;
          break;
        }
      }
    }
  }
  tdarray_clear(e->csnake.dead);
}

static inline void env_tick_collision(env* e) {
  env_remove_dead_snakes(e);
  env_snake_border_collision(e);
  env_build_csnake_grid(e);
  env_snake_snake_collision(e);
  env_food_collision(e);
}

void env_reset(env* e) {
  int cells = e->csnake.gsz * e->csnake.gsz;
  for (int i = 0; i < cells; i++) tdarray_clear(e->csnake.cgrd[i]);

  cells = e->cfood.gsz * e->cfood.gsz;
  for (int i = 0; i < cells; i++) tdarray_clear(e->cfood.cgrd[i]);

  tdarray_clear(e->snake.sin);
  tdarray_clear(e->snake.cos);
  tdarray_clear(e->snake.ang);
  tdarray_clear(e->snake.tang);
  tdarray_clear(e->snake.np);
  tdarray_clear(e->snake.hi);
  tdarray_clear(e->snake.bv);
  tdarray_clear(e->snake.b);
  tdarray_clear(e->snake.g);
  tdarray_clear(e->snake.dir);
  tdarray_clear(e->snake.state);
  tdarray_clear(e->snake.kc);
  tdarray_clear(e->snake.ldtm);
  tdarray_clear(e->snake.t);
  tdarray_clear(e->snake.ts);
  tdarray_clear(e->snake.bs);
  tdarray_clear(e->snake.mr);
  tdarray_clear(e->snake.mr2);
  tdarray_clear(e->snake.btr);
  tdarray_clear(e->snake.radx2);
  tdarray_clear(e->snake.rad);
  tdarray_clear(e->snake.rad2);
  tdarray_clear(e->snake.sp);
  tdarray_clear(e->snake.px);
  tdarray_clear(e->snake.py);
  tdarray_clear(e->snake.id);

  tdarray_clear(e->food.x);
  tdarray_clear(e->food.y);
  tdarray_clear(e->food.v);
  tdarray_clear(e->food.ci);
  tdarray_clear(e->dat.ids);
  tdarray_clear(e->csnake.dead);

  e->dat.cid = 0;
  e->dat.ctm = 0;
}

void env_tick(env* e, float dtms) {
  env_tick_movement(e, dtms);
  env_tick_collision(e);
  e->dat.ctm += dtms * MS_PER_TICK;
}

bool env_new_snake(env* e, float x, float y, float ang) {
  int i = tdarray_length(e->snake.t) - _tdarray_length(e->csnake.dead);
  if (i == e->cfg.msn) return false;

  float cx = e->cfg.rad;
  float cy = e->cfg.rad;
  float dx = x - cx;
  float dy = y - cy;

  if ((dx * dx + dy * dy) >= e->dat.sprad2) {
    return false;
  }

  float s = fast_sinf(&e->dat, ang);
  float c = fast_cosf(&e->dat, ang);
  int gx = x * e->csnake.icsz;
  int gy = y * e->csnake.icsz;
  int cell = gy * e->csnake.gsz + gx;

  if (tdarray_length(e->csnake.cgrd[cell])) {
    return false;
  }

  tdarray_push(&e->snake.sin, &s);
  tdarray_push(&e->snake.cos, &c);
  tdarray_push(&e->snake.ang, &ang);
  tdarray_push(&e->snake.tang, &ang);
  tdarray_push(&e->snake.np, ((int[]){2}));
  tdarray_push(&e->snake.hi, ((int[]){e->snake.np[i] - 1}));
  tdarray_push(&e->snake.bv, ((float[]){0}));
  tdarray_push(&e->snake.b, ((int[]){0}));
  tdarray_push(&e->snake.g, ((float[]){0}));
  tdarray_push(&e->snake.dir, ((int[]){0}));
  tdarray_push(&e->snake.state, ((int[]){0}));
  tdarray_push(&e->snake.kc, ((int[]){0}));
  tdarray_push(&e->snake.ldtm, ((double[]){0}));
  tdarray_push(&e->snake.t, ((float[]){0}));
  tdarray_push(&e->snake.ts, ((float[]){0}));
  tdarray_push(&e->snake.bs, ((float[]){0}));
  tdarray_push(&e->snake.mr, ((float[]){0}));
  tdarray_push(&e->snake.mr2, ((float[]){0}));
  tdarray_push(&e->snake.btr, ((float[]){0}));
  tdarray_push(&e->snake.radx2, ((float[]){0}));
  tdarray_push(&e->snake.rad, ((float[]){0}));
  tdarray_push(&e->snake.rad2, ((float[]){0}));
  tdarray_push(&e->snake.sp, ((float[]){0}));
  tdarray_push(&e->snake.px, ((body_parts[]){0}));
  tdarray_push(&e->snake.py, ((body_parts[]){0}));

  int nids = tdarray_length(e->dat.ids);
  if (nids) {
    tdarray_push(&e->snake.id, &e->dat.ids[nids - 1]);
    tdarray_pop(e->dat.ids);
  } else {
    int id = e->dat.cid++;
    tdarray_push(&e->snake.id, &id);
  }

  recalc_snake_factors(e, i);
  e->snake.sp[i] = e->snake.bs[i];

  SPOS_X(e->snake, i) = x;
  SPOS_Y(e->snake, i) = y;

  float* px = e->snake.px[i] + 1;
  float* py = e->snake.py[i] + 1;

  float mv = 0;
  int n = 0;

  float safe = e->cfg.rad * (1 - (BORDER_MARGIN + 0.005));

  for (int j = 0; j < e->snake.np[i]; j++) {
    if (j > 2) {
      n++;
      if (n < 5) mv = e->cfg.tsr * n / 4.0f;
    }

    x -= e->cfg.sl * e->snake.cos[i] * (1 - mv);
    y -= e->cfg.sl * e->snake.sin[i] * (1 - mv);

    float dx = x - cx;
    float dy = y - cy;
    float dist2 = dx * dx + dy * dy;

    if (dist2 > safe * safe) {
      float scale = safe / sqrtf(dist2);
      x = cx + dx * scale;
      y = cy + dy * scale;
    }

    int idx = e->snake.hi[i] - j;

    px[idx] = x;
    py[idx] = y;
  }

  env_grid_insert_snake(e, i);
  return true;
}

bool env_new_food(env* e, float x, float y, float v) {
  float dx = e->cfg.rad - x;
  float dy = e->cfg.rad - y;
  float d2 = dx * dx + dy * dy;
  if (d2 >= e->dat.srad2) return false;

  int i = tdarray_length(e->food.x);

  tdarray_push(&e->food.x, &x);
  tdarray_push(&e->food.y, &y);
  tdarray_push(&e->food.v, &v);

  env_grid_insert_food(e, i);

  return true;
}