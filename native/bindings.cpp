#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

extern "C" {
#include <stdio.h>

#include "env.h"
#include "util/tdarray.h"
}

namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
  m.doc() =
      "pyslither: Python bindings for the Slither.io-style simulation "
      "environment.";

  py::class_<env::env_cfg>(
      m, "Config", "Simulation configuration. Set via Simulation constructor.")
      .def_readonly("radius", &env::env_cfg::rad)
      .def_readonly("base_speed", &env::env_cfg::bs)
      .def_readonly("speed_thickness_ratio", &env::env_cfg::str)
      .def_readonly("max_speed", &env::env_cfg::ms)
      .def_readonly("turn_speed", &env::env_cfg::ts)
      .def_readonly("tail_stiffness_ratio", &env::env_cfg::tsr)
      .def_readonly("mouth_speed_ratio", &env::env_cfg::msr)
      .def_readonly("mouth_radius_ratio", &env::env_cfg::mrr)
      .def_readonly("segment_length", &env::env_cfg::sl)
      .def_readonly("max_parts", &env::env_cfg::mp)
      .def_readonly("max_snakes", &env::env_cfg::msn);

  py::class_<env>(m, "Simulation")
      .def(py::init([](float rad, float bs, float str, float ms, float ts,
                       float tsr, float msr, float mrr, int sl, int mp,
                       int msn) {
             env* e = new env();

             e->cfg.rad = rad;
             e->cfg.bs = bs;
             e->cfg.str = str;
             e->cfg.ms = ms;
             e->cfg.ts = ts;
             e->cfg.tsr = tsr;
             e->cfg.msr = msr;
             e->cfg.mrr = mrr;
             e->cfg.sl = sl;
             e->cfg.mp = mp;
             e->cfg.msn = msn;

             if (!env_init(e)) {
               printf(
                   "Environment initialization failed: number of maximum parts "
                   "exceeds %d.\n",
                   MAX_PARTS);
               exit(-1);
             }

             return e;
           }),
           py::arg("radius") = 5000.0f, py::arg("base_speed") = 5.39f,
           py::arg("speed_thickness_ratio") = 0.4f,
           py::arg("max_speed") = 14.0f, py::arg("turn_speed") = 0.033f,
           py::arg("tail_stiffness_ratio") = 0.43f,
           py::arg("mouth_speed_ratio") = 0.208f,
           py::arg("mouth_radius_ratio") = 40.0f,
           py::arg("segment_length") = 42, py::arg("max_parts") = 450,
           py::arg("max_snakes") = 32)

      .def_readonly("config", &env::cfg)

      .def("reset", &env_reset, "Reset the environment.")
      .def("tick", &env_tick, py::arg("dtms"),
           "Advance the simulation one step. `dtms` is elapsed time "
           "**normalized "
           "to " TOSTRING(MS_PER_TICK) " ms**.")
      .def("new_snake", &env_new_snake, py::arg("x"), py::arg("y"),
           py::arg("angle"),
           "Spawn a snake at (`x`, `y`) with `angle` in radians (`0` to "
           "`2pi`). Returns `False` if the position is "
           "outside the spawn radius, occupied, or the max snake count is "
           "reached.")
      .def("new_food", &env_new_food, py::arg("x"), py::arg("y"),
           py::arg("value"),
           "Place food at (`x`, `y`) with the given value. Returns `False` if "
           "outside the safe radius.")

      .def(
          "set_snake_target_angle",
          [](env* e, float ang, int i) { e->snake.tang[i] = ang; },
          py::arg("angle"), py::arg("i"),
          "Set the desired heading for snake `i` in radians (`0` to `2pi`).")
      .def(
          "set_snake_boost", [](env* e, bool b, int i) { e->snake.b[i] = b; },
          py::arg("boost"), py::arg("i"),
          "Enable or disable boosting for snake `i`.")

      .def_property_readonly(
          "num_snakes",
          [](env* e) {
            return tdarray_length(e->snake.t) - _tdarray_length(e->csnake.dead);
          },
          "Total number of snakes alive.")
      .def(
          "get_snake_head_x", [](env* e, int i) { return SPOS_X(e->snake, i); },
          py::arg("i"), "X coordinate of snake `i`'s head.")
      .def(
          "get_snake_head_y", [](env* e, int i) { return SPOS_Y(e->snake, i); },
          py::arg("i"), "Y coordinate of snake `i`'s head.")
      .def(
          "get_snake_part_x",
          [](env* e, int i, int j) {
            return e->snake.px[i][1 + ((e->snake.hi[i] - j) & MAX_PARTS_MASK)];
          },
          py::arg("i"), py::arg("j"),
          "X coordinate of body part `j` of snake `i`.")
      .def(
          "get_snake_part_y",
          [](env* e, int i, int j) {
            return e->snake.py[i][1 + ((e->snake.hi[i] - j) & MAX_PARTS_MASK)];
          },
          py::arg("i"), py::arg("j"),
          "Y coordinate of body part `j` of snake `i`.")
      .def_property_readonly(
          "snake_angles",
          [](env* e) {
            float* ptr = e->snake.ang;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Current angles (radians) of all snakes.")
      .def_property_readonly(
          "snake_target_angles",
          [](env* e) {
            float* ptr = e->snake.tang;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Target angles of all snakes.")
      .def_property_readonly(
          "snake_speeds",
          [](env* e) {
            float* ptr = e->snake.sp;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Speeds of all snakes.")
      .def(
          "get_snake_length",
          [](env* e, int i) { return e->snake.np[i] + e->snake.g[i]; },
          py::arg("i"),
          "Effective length of snake `i` (number of parts + fractional "
          "growth).")
      .def_property_readonly(
          "snake_ids",
          [](env* e) {
            int* ptr = e->snake.id;
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          "Unique IDs of all snakes.")
      .def_property_readonly(
          "snake_kill_counts",
          [](env* e) {
            int* ptr = e->snake.kc;
            return py::array_t<int>(tdarray_length(ptr), ptr);
          },
          "Kill counts of all snakes.")
      .def_property_readonly(
          "snake_part_counts",
          [](env* e) {
            int* ptr = e->snake.np;
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          "Part counts of all snakes.")
      .def_property_readonly(
          "snake_dead_flags",
          [](env* e) {
            int* ptr = e->snake.dead;
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          "Dead flags of all snakes (`1` = died this tick).")
      .def_property_readonly(
          "snake_radii",
          [](env* e) {
            float* ptr = e->snake.rad;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Collision radii of all snakes.")

      .def_property_readonly(
          "num_food", [](env* e) { return tdarray_length(e->food.x); },
          "Number of foods currently in the world.")
      .def_property_readonly(
          "food_xs",
          [](env* e) {
            float* ptr = e->food.x;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "X coordinates of all foods.")

      .def_property_readonly(
          "food_ys",
          [](env* e) {
            float* ptr = e->food.y;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Y coordinates of all foods.")
      .def_property_readonly(
          "food_values",
          [](env* e) {
            float* ptr = e->food.v;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Values of all foods.")

      .def(
          "get_segment_x0",
          [](env* e, int i) {
            return e->snake.px[e->csnake.sidx[i]][e->csnake.s0[i]];
          },
          py::arg("i"), "Start X of collision segment `i`.")
      .def(
          "get_segment_y0",
          [](env* e, int i) {
            return e->snake.py[e->csnake.sidx[i]][e->csnake.s0[i]];
          },
          py::arg("i"), "Start Y of collision segment `i`.")
      .def(
          "get_segment_x1",
          [](env* e, int i) {
            return e->snake.px[e->csnake.sidx[i]][e->csnake.s1[i]];
          },
          py::arg("i"), "End X of collision segment `i`.")
      .def(
          "get_segment_y1",
          [](env* e, int i) {
            return e->snake.py[e->csnake.sidx[i]][e->csnake.s1[i]];
          },
          py::arg("i"), "End Y of collision segment `i`.")
      .def(
          "get_segment_snake_idx",
          [](env* e, int i) { return e->csnake.sidx[i]; }, py::arg("i"),
          "Index of the snake that owns segment `i`.")

      .def(
          "get_segment_cell",
          [](env* e, float x, float y) {
            int cell = (int)(y * e->csnake.icsz) * e->csnake.gsz +
                       (int)(x * e->csnake.icsz);
            int* ptr = e->csnake.cgrd[cell];
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          py::arg("x"), py::arg("y"),
          "Segment indices in the collision grid cell that contains world "
          "position (`x`, `y`).")
      .def(
          "get_segment_cell",
          [](env* e, int i) {
            int* ptr = e->csnake.cgrd[i];
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          py::arg("i"), "Segment indices in collision grid cell `i`.")
      .def(
          "get_food_cell",
          [](env* e, float x, float y) {
            int cell = (int)(y * e->cfood.icsz) * e->cfood.gsz +
                       (int)(x * e->cfood.icsz);
            int* ptr = e->cfood.cgrd[cell];
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          py::arg("x"), py::arg("y"),
          "Food indices in the food collision grid cell that contains world "
          "position (`x`, `y`).")
      .def(
          "get_food_cell",
          [](env* e, int i) {
            int* ptr = e->cfood.cgrd[i];
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          py::arg("i"), "Food indices in food collision grid cell `i`.")

      .def_property_readonly("segment_grid_size",
                             [](env* e) { return e->csnake.gsz; })
      .def_property_readonly("food_grid_size",
                             [](env* e) { return e->cfood.gsz; })
      .def_property_readonly("segment_cell_size",
                             [](env* e) { return e->csnake.csz; })
      .def_property_readonly("food_cell_size",
                             [](env* e) { return e->cfood.csz; })
      .def_property_readonly("inv_segment_cell_size",
                             [](env* e) { return e->csnake.icsz; })
      .def_property_readonly("inv_food_cell_size",
                             [](env* e) { return e->cfood.icsz; })
      .def_property_readonly("safe_radius", [](env* e) { return e->dat.srad; })
      .def_property_readonly("spawn_radius",
                             [](env* e) { return e->dat.sprad; });

  m.attr("MS_PER_TICK") = MS_PER_TICK;
}