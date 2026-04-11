#include <pybind11/native_enum.h>
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

#define ENV_CALLBACKS(X)                    \
  X(SnakeKilled, on_kill, (int, int))       \
  X(FoodEaten, on_eat, (int, int))          \
  X(SnakeGrowth, on_growth, (int))          \
  X(SnakeShift, on_move, (int, bool))       \
  X(LosePart, on_lpart, (int))              \
  X(BoostFoodSpawn, on_bfspawn, (int, int)) \
  X(DeathFoodSpawn, on_dfspawn, (int, int, int))

#define CB_ENUM_VALUE(Event, member, args) Event,
enum class CallbackEvent { ENV_CALLBACKS(CB_ENUM_VALUE) Count };
#undef CB_ENUM_VALUE

#define CB_FIELD(Event, member, args) \
  std::function<void(env*, CB_UNPACK args)> member;
#define CB_UNPACK(...) __VA_ARGS__

struct PyEnvCallbacks {
  py::object user_data;
  ENV_CALLBACKS(CB_FIELD)
};
#undef CB_FIELD

template <auto Member, typename... Args>
void py_dispatch(env* e, Args... args) {
  py::gil_scoped_acquire gil;
  auto* cb = static_cast<PyEnvCallbacks*>(e->user_data);
  (cb->*Member)(e, args...);
}

template <auto Member, typename CFn, typename PyFn>
void register_callback(env* e, PyEnvCallbacks* cb, PyFn&& fn, CFn env::* slot,
                       CFn dispatcher) {
  if (fn) {
    cb->*Member = std::forward<PyFn>(fn);
    e->*slot = dispatcher;
  } else {
    e->*slot = nullptr;
  }
}

struct EnvDeleter {
  void operator()(env* e) const {
    py::gil_scoped_acquire gil;
    delete static_cast<PyEnvCallbacks*>(e->user_data);
    env_destroy(e);
  }
};

using env_ptr = std::unique_ptr<env, EnvDeleter>;

#define CB_NULL_INIT(Event, member, args) e->member = nullptr;

#define CB_DISPATCH_CASE(Event, member, args)                  \
  case CallbackEvent::Event: {                                 \
    auto fn_obj = item.second.cast<py::function>();            \
    std::function<void(env*, CB_UNPACK args)> fn = fn_obj;     \
    register_callback<&PyEnvCallbacks::member>(                \
        e.get(), cb, fn, &env::member,                         \
        py_dispatch<&PyEnvCallbacks::member, CB_UNPACK args>); \
  } break;

#define CB_ENUM_EXPORT(Event, member, args) .value(#Event, CallbackEvent::Event)

PYBIND11_MODULE(_core, m) {
  m.doc() = "Python bindings for the Slither.io-style simulation environment.";

  py::class_<env::env_cfg>(
      m, "Config", "Simulation configuration. Set via Simulation constructor.")
      .def_readonly("radius", &env::env_cfg::rad)
      .def_readonly("base_speed", &env::env_cfg::bs)
      .def_readonly("speed_thickness", &env::env_cfg::str)
      .def_readonly("max_speed", &env::env_cfg::ms)
      .def_readonly("turn_speed", &env::env_cfg::ts)
      .def_readonly("tail_stiffness", &env::env_cfg::tsr)
      .def_readonly("mouth_speed", &env::env_cfg::msr)
      .def_readonly("mouth_radius", &env::env_cfg::mrr)
      .def_readonly("segment_length", &env::env_cfg::sl)
      .def_readonly("max_parts", &env::env_cfg::mp)
      .def_readonly("max_snakes", &env::env_cfg::msn);

  py::native_enum<CallbackEvent>(
      m, "Event", "enum.IntEnum",
      "Events that trigger user-defined callbacks in the simulation.\n"
      "\n"
      "Attributes\n"
      "----------\n"
      "SnakeKilled\n"
      "    Fired when a snake is killed:\n"
      "    ``(sim, victim, killer)``, where ``victim``\n"
      "    is the victim snake's index, and ``killer`` is either the killer "
      "snake's index or ``-1``"
      "    if victim is killed by border.\n"
      "FoodEaten\n"
      "    Fired when a snake eats a food:\n"
      "    ``(sim, snake, food)``, where ``eater`` is\n"
      "    the eating snake's index and ``food`` is the consumed food's "
      "index.\n"
      "SnakeGrowth\n"
      "    Fired when a snake's fractional growth changes:\n"
      "    ``(sim, snake)``, where ``snake`` is the snake's index.\n"
      "SnakeShift\n"
      "    Fired when a snake's body parts shift (when snake moves "
      "``config.segment_length`` units):\n"
      "    ``(sim, snake, new_part)``, where ``snake`` is the snake's index, "
      "and ``new_part`` is whether the snake gained a new body part or not.\n"
      "LosePart\n"
      "    Fired when a snake loses a body part:\n"
      "    ``(sim, snake)``, where ``snake`` is the snake's index.\n"
      "BoostFoodSpawn\n"
      "    Fired when a food is spawned due to boost:\n"
      "    ``(sim, food, snake)``, where ``food`` is the food's index and "
      "``snake`` is the snake's index.\n"
      "DeathFoodSpawn\n"
      "    Fired when food spawns due to a snake's death:\n"
      "    ``(sim, food, num_food, snake)``, where ``food`` is the food's "
      "starting index, ``num_food`` is the total number of foods spawned due "
      "to death, and ``snake`` is the snake's index.\n"
      "\n") ENV_CALLBACKS(CB_ENUM_EXPORT)
      .finalize();

  py::class_<env, env_ptr>(m, "Simulation")
      .def(py::init([](float rad, float bs, float str, float ms, float ts,
                       float tsr, float msr, float mrr, int sl, int mp, int msn,
                       py::object user_data, py::dict callbacks) {
             auto e = env_ptr(new env());
             auto* cb = new PyEnvCallbacks();

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

             cb->user_data = user_data;
             e->user_data = cb;

             ENV_CALLBACKS(CB_NULL_INIT)

             for (auto item : callbacks) {
               CallbackEvent ev = item.first.cast<CallbackEvent>();
               switch (ev) { ENV_CALLBACKS(CB_DISPATCH_CASE) }
             }

             if (!env_init(e.get())) {
               printf(
                   "Environment initialization failed: number of maximum "
                   "parts exceeds %d.\n",
                   MAX_PARTS);
               exit(-1);
             }
             return e;
           }),
           py::arg("radius") = 5000.0f, py::arg("base_speed") = 5.39f,
           py::arg("speed_thickness") = 0.4f, py::arg("max_speed") = 14.0f,
           py::arg("turn_speed") = 0.033f, py::arg("tail_stiffness") = 0.43f,
           py::arg("mouth_speed") = 0.208f, py::arg("mouth_radius") = 40.0f,
           py::arg("segment_length") = 42, py::arg("max_parts") = 450,
           py::arg("max_snakes") = 32, py::arg("user_data") = py::none(),
           py::arg("callbacks") = py::dict())

      .def_property(
          "user_data",
          [](env* e) -> py::object {
            auto* cb = static_cast<PyEnvCallbacks*>(e->user_data);
            return cb->user_data;
          },
          [](env* e, py::object obj) {
            auto* cb = static_cast<PyEnvCallbacks*>(e->user_data);
            cb->user_data = obj;
          },
          "User data.")

      .def_readonly("config", &env::cfg)

      .def("reset", &env_reset, "Reset the environment.")
      .def("tick", &env_tick, py::arg("dtms"),
           "Advance the simulation one step. `dtms` is elapsed time "
           "**normalized "
           "to " TOSTRING(MS_PER_TICK) " ms**.")
      .def("new_snake", &env_new_snake, py::arg("x"), py::arg("y"),
           py::arg("angle"),
           "Spawn a snake at (`x`, `y`) with `angle` in radians (``0`` to "
           "``2pi``). Returns `False` if the position is "
           "outside the spawn radius, occupied, or the max snake count is "
           "reached.")
      .def("new_food", &env_new_food, py::arg("x"), py::arg("y"),
           py::arg("value"),
           "Place food at (`x`, `y`) with the given value. Returns `False` if "
           "outside the safe radius.")
      .def_property_readonly(
          "num_snakes", [](env* e) { return tdarray_length(e->snake.t); },
          "Total number of snakes alive.")
      .def(
          "get_snake_head_x", [](env* e, int i) { return SPOS_X(e->snake, i); },
          py::arg("i"), "X coordinate of snake ``i``'s head.")
      .def(
          "get_snake_head_y", [](env* e, int i) { return SPOS_Y(e->snake, i); },
          py::arg("i"), "Y coordinate of snake ``i``'s head.")
      .def(
          "get_snake_part_x",
          [](env* e, int i, int j) {
            return e->snake.px[i][1 + ((e->snake.hi[i] - j) & MAX_PARTS_MASK)];
          },
          py::arg("i"), py::arg("j"),
          "X coordinate of body part ``j`` of snake ``i``.")
      .def(
          "get_snake_part_y",
          [](env* e, int i, int j) {
            return e->snake.py[i][1 + ((e->snake.hi[i] - j) & MAX_PARTS_MASK)];
          },
          py::arg("i"), py::arg("j"),
          "Y coordinate of body part ``j`` of snake ``i``.")
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
            return py::array_t<float>(tdarray_length(ptr), ptr, py::cast(e));
          },
          "Target angles of all snakes in radians (``0`` to ``2pi``) **(Writeable)**.")
      .def_property_readonly(
          "snake_boosts",
          [](env* e) {
            bool* ptr = e->snake.b;
            return py::array_t<bool>(tdarray_length(ptr), ptr, py::cast(e));
          },
          "Boost flags of all snakes **(Writeable)**.")
      .def_property_readonly(
          "snake_growths",
          [](env* e) {
            float* ptr = e->snake.g;
            return py::array_t<float>(tdarray_length(ptr), ptr);
          },
          "Fractional growth values of all snakes.")
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
          "Effective length of snake ``i`` (number of parts + fractional "
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
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
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
      .def_property_readonly(
          "food_ids",
          [](env* e) {
            int* ptr = e->food.id;
            ssize_t n = tdarray_length(ptr);
            return py::memoryview::from_buffer(
                ptr, sizeof(int), py::format_descriptor<int>::value, {n},
                {sizeof(int)}, true);
          },
          "Unique IDs of all food.")
      .def(
          "get_segment_x0",
          [](env* e, int i) {
            return e->snake.px[e->csnake.sidx[i]][e->csnake.s0[i]];
          },
          py::arg("i"), "Start X of collision segment ``i``.")
      .def(
          "get_segment_y0",
          [](env* e, int i) {
            return e->snake.py[e->csnake.sidx[i]][e->csnake.s0[i]];
          },
          py::arg("i"), "Start Y of collision segment ``i``.")
      .def(
          "get_segment_x1",
          [](env* e, int i) {
            return e->snake.px[e->csnake.sidx[i]][e->csnake.s1[i]];
          },
          py::arg("i"), "End X of collision segment ``i``.")
      .def(
          "get_segment_y1",
          [](env* e, int i) {
            return e->snake.py[e->csnake.sidx[i]][e->csnake.s1[i]];
          },
          py::arg("i"), "End Y of collision segment ``i``.")
      .def(
          "get_segment_snake_idx",
          [](env* e, int i) { return e->csnake.sidx[i]; }, py::arg("i"),
          "Index of the snake that owns segment ``i``.")

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
          py::arg("i"), "Segment indices in collision grid cell ``i``.")
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
          py::arg("i"), "Food indices in food collision grid cell ``i``.")

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