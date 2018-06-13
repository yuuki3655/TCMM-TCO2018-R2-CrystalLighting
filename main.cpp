// Git Repository: https://github.com/yuuki3655/TCMM-TCO2018-R2-CrystalLighting

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef LOCAL_DEBUG_MODE
#define NDEBUG ;
#endif

using namespace std;

constexpr uint8_t GREEN = 0x3;
constexpr uint8_t VIOLET = 0x5;
constexpr uint8_t ORANGE = 0x6;

constexpr uint8_t EMPTY_CELL = 0;
constexpr uint8_t SLASH_MIRROR = (0x1 << 6);
constexpr uint8_t BACKSLASH_MIRROR = (0x2 << 6);
constexpr uint8_t OBSTACLE = (0x3 << 6);

constexpr uint8_t LANTERN_COLOR_MASK = 0x7;
constexpr uint8_t CRYSTAL_COLOR_MASK = (0x7 << 3);

constexpr int DIR_X[] = {0, 1, 0, -1};
constexpr int DIR_Y[] = {-1, 0, 1, 0};
constexpr int MIRROR_S_TO[] = {1, 0, 3, 2};
constexpr int MIRROR_B_TO[] = {3, 2, 1, 0};

// Timer implementation based on nika's submission.
// http://community.topcoder.com/longcontest/?module=ViewProblemSolution&pm=14907&rd=17153&cr=20315020&subnum=17
class Timer {
 public:
  inline void Start() { start_seconds_ = GetSeconds(); }

  inline double GetNormalizedTime() const {
    return min((GetSeconds() - start_seconds_) / TIME_LIMIT_SECONDS, 0.9999);
  }

 private:
  inline uint64_t GetTSC() const {
    uint64_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return lo + (hi << 32);
  }
  inline double GetSeconds() const { return GetTSC() / 2.79e9; }

  double start_seconds_;
  static constexpr double TIME_LIMIT_SECONDS = 10;
};

typedef pair<int, int> Pos;

struct Lantern {
  int x;
  int y;
  uint8_t color;

  inline bool operator<(const Lantern& rhs) const {
    return make_tuple(x, y, color) < make_tuple(rhs.x, rhs.y, rhs.color);
  }

  inline bool operator==(const Lantern& rhs) const {
    return make_tuple(x, y, color) == make_tuple(rhs.x, rhs.y, rhs.color);
  }
};

struct Mirror {
  int x;
  int y;
  uint8_t type;

  inline bool operator<(const Mirror& rhs) const {
    return make_tuple(x, y, type) < make_tuple(rhs.x, rhs.y, rhs.type);
  }
};

struct Board {
  int w, h;
  vector<uint8_t> cells;
  vector<uint16_t> lay;
  set<Pos> obstacles;
  set<Mirror> mirrors;
  set<Lantern> lanterns;
  int wrong_lays;
  int invalid_lays;
  int lit_crystals;
  int lit_compound_crystals;
  int lit_wrong_crystals;
  int crystals_nbit_off[4];

  inline void SetCell(int x, int y, uint8_t cell_value) {
    cells[y * w + x] = cell_value;
  }

  inline uint8_t GetCell(int x, int y) const { return cells[y * w + x]; }

  inline bool SetLay(int x, int y, int dir, uint16_t lantern_color) {
    uint16_t shifted = lantern_color << (4 * dir);
    if (lay[y * w + x] & shifted) {
      assert(GetLay(x, y, dir) == lantern_color);
      return false;
    }
    assert(GetLay(x, y, dir) == 0);
    lay[y * w + x] |= shifted;
    return true;
  }

  inline bool RemoveLay(int x, int y, int dir) {
    uint16_t shifted = 0xf << (4 * dir);
    if (!(lay[y * w + x] & shifted)) {
      return false;
    }
    lay[y * w + x] &= ~shifted;
    return true;
  }

  inline bool HasLay(int x, int y) const { return lay[y * w + x]; }

  inline uint8_t GetLay(int x, int y, int dir) const {
    return (lay[y * w + x] >> (4 * dir)) & LANTERN_COLOR_MASK;
  }

  inline uint8_t GetLitColor(int x, int y) const {
    uint16_t color = lay[y * w + x];
    return (color | (color >> 4) | (color >> 8) | (color >> 12)) & 0x7;
  }

  inline uint8_t GetLitColor(int x, int y, int exclude_dir) const {
    uint16_t color = lay[y * w + x] & ~(0xf << (4 * exclude_dir));
    return (color | (color >> 4) | (color >> 8) | (color >> 12)) & 0x7;
  }

  inline bool IsInBound(int x, int y) const {
    return !(x < 0 || x >= w || y < 0 || y >= h);
  }

  inline bool IsEmpty(int x, int y) const {
    return GetCell(x, y) == EMPTY_CELL;
  }

  inline bool IsCrystal(int x, int y) const {
    return GetCell(x, y) & CRYSTAL_COLOR_MASK;
  }

  inline bool IsSecondaryColorCrystal(int x, int y) const {
    uint8_t color = GetCrystalColor(x, y);
    return color == GREEN || color == VIOLET || color == ORANGE;
  }

  inline uint8_t GetCrystalColor(int x, int y) const {
    return (GetCell(x, y) & CRYSTAL_COLOR_MASK) >> 3;
  }

  inline bool IsLantern(int x, int y) const {
    return GetCell(x, y) & LANTERN_COLOR_MASK;
  }

  inline bool IsObstacle(int x, int y) const {
    return GetCell(x, y) == OBSTACLE;
  }

  inline bool IsSlashMirror(int x, int y) const {
    return GetCell(x, y) == SLASH_MIRROR;
  }

  inline bool IsBackslashMirror(int x, int y) const {
    return GetCell(x, y) == BACKSLASH_MIRROR;
  }

  inline void LayTrace(int x, int y, int dir, const uint8_t lantern_color) {
    assert(lantern_color);
    while (IsInBound(x, y) && SetLay(x, y, dir, lantern_color)) {
      if (IsObstacle(x, y)) {
        break;
      } else if (IsLantern(x, y)) {
        ++invalid_lays;
        break;
      } else if (IsSlashMirror(x, y)) {
        dir = MIRROR_S_TO[dir];
      } else if (IsBackslashMirror(x, y)) {
        dir = MIRROR_B_TO[dir];
      } else if (IsCrystal(x, y)) {
        uint8_t prev_lit_color = GetLitColor(x, y, /*exclude_dir=*/dir);
        uint8_t lit_color = GetLitColor(x, y);
        uint8_t crystal_color = GetCrystalColor(x, y);
        auto& crystals = IsSecondaryColorCrystal(x, y) ? lit_compound_crystals
                                                       : lit_crystals;
        if (prev_lit_color != 0) {
          if (prev_lit_color == crystal_color) {
            --crystals;
          } else {
            --lit_wrong_crystals;
          }
        }
        if (lit_color == crystal_color) {
          ++crystals;
        } else {
          ++lit_wrong_crystals;
        }

        --crystals_nbit_off[__builtin_popcount(prev_lit_color ^ crystal_color)];
        ++crystals_nbit_off[__builtin_popcount(lit_color ^ crystal_color)];

        if (!(lantern_color & crystal_color)) {
          ++wrong_lays;
        }

        break;
      }

      x += DIR_X[dir];
      y += DIR_Y[dir];
    }
  }

  inline int RevertLayTrace(int x, int y, int dir,
                            const uint8_t lantern_color) {
    assert(lantern_color);
    const int initial_x = x;
    const int initial_y = y;
    int last_entrant_dir = dir;
    while (IsInBound(x, y) && RemoveLay(x, y, dir)) {
      if (IsObstacle(x, y)) {
        break;
      } else if (IsLantern(x, y)) {
        --invalid_lays;
        break;
      } else if (IsSlashMirror(x, y)) {
        dir = MIRROR_S_TO[dir];
      } else if (IsBackslashMirror(x, y)) {
        dir = MIRROR_B_TO[dir];
      } else if (IsCrystal(x, y)) {
        uint8_t lit_color = GetLitColor(x, y);
        uint8_t prev_lit_color = lit_color | lantern_color;
        uint8_t crystal_color = GetCrystalColor(x, y);
        auto& crystals = IsSecondaryColorCrystal(x, y) ? lit_compound_crystals
                                                       : lit_crystals;
        if (prev_lit_color == crystal_color) {
          --crystals;
        } else {
          --lit_wrong_crystals;
        }
        if (lit_color == crystal_color) {
          ++crystals;
        } else if (lit_color != 0) {
          ++lit_wrong_crystals;
        }

        --crystals_nbit_off[__builtin_popcount(prev_lit_color ^ crystal_color)];
        ++crystals_nbit_off[__builtin_popcount(lit_color ^ crystal_color)];

        if (!(lantern_color & crystal_color)) {
          --wrong_lays;
        }

        break;
      }

      x += DIR_X[dir];
      y += DIR_Y[dir];
      if (x == initial_x && y == initial_y) {
        last_entrant_dir = dir;
      }
    }
    return last_entrant_dir;
  }

  inline void PutItem(int item_x, int item_y, uint8_t item) {
    assert(item == EMPTY_CELL || IsEmpty(item_x, item_y));
    if (!HasLay(item_x, item_y)) {
      SetCell(item_x, item_y, item);
      return;
    }

    uint8_t colors[4];
    for (int dir = 0; dir < 4; ++dir) {
      colors[dir] = GetLay(item_x, item_y, dir);
      if (colors[dir]) {
        int last_entrant_dir = RevertLayTrace(item_x, item_y, dir, colors[dir]);
        if (last_entrant_dir != dir) {
          colors[last_entrant_dir] = 0;
        }
      }
    }
    SetCell(item_x, item_y, item);
    for (int dir = 0; dir < 4; ++dir) {
      if (colors[dir]) {
        LayTrace(item_x, item_y, dir, colors[dir]);
      }
    }
  }

  inline void RemoveItem(int item_x, int item_y) {
    assert(!IsEmpty(item_x, item_y));
    PutItem(item_x, item_y, EMPTY_CELL);
  }

  inline void PutLantern(int lantern_x, int lantern_y, uint8_t lantern_color) {
    assert(IsEmpty(lantern_x, lantern_y));
    PutItem(lantern_x, lantern_y, lantern_color);
    for (int dir = 0; dir < 4; ++dir) {
      LayTrace(lantern_x + DIR_X[dir], lantern_y + DIR_Y[dir], dir,
               lantern_color);
    }
    lanterns.emplace(Lantern{lantern_x, lantern_y, lantern_color});
  }

  inline void RemoveLantern(int lantern_x, int lantern_y,
                            uint8_t lantern_color) {
    assert(IsLantern(lantern_x, lantern_y));
    assert(GetCell(lantern_x, lantern_y) == lantern_color);
    for (int dir = 0; dir < 4; ++dir) {
      RevertLayTrace(lantern_x + DIR_X[dir], lantern_y + DIR_Y[dir], dir,
                     lantern_color);
    }
    RemoveItem(lantern_x, lantern_y);
    lanterns.erase({lantern_x, lantern_y, lantern_color});
  }

  inline void PutObstacle(int obstacle_x, int obstacle_y) {
    assert(IsEmpty(obstacle_x, obstacle_y));
    PutItem(obstacle_x, obstacle_y, OBSTACLE);
    obstacles.emplace(Pos{obstacle_x, obstacle_y});
  }

  inline void RemoveObstacle(int obstacle_x, int obstacle_y) {
    assert(IsObstacle(obstacle_x, obstacle_y));
    assert(obstacles.find({obstacle_x, obstacle_y}) != obstacles.end());
    obstacles.erase({obstacle_x, obstacle_y});
    RemoveItem(obstacle_x, obstacle_y);
  }

  inline void PutMirror(int mirror_x, int mirror_y, uint8_t type) {
    assert(IsEmpty(mirror_x, mirror_y));
    assert(type == SLASH_MIRROR || type == BACKSLASH_MIRROR);
    PutItem(mirror_x, mirror_y, type);
    mirrors.emplace(Mirror{mirror_x, mirror_y, type});
  }

  inline void RemoveMirror(int mirror_x, int mirror_y, uint8_t type) {
    assert(IsSlashMirror(mirror_x, mirror_y) ||
           IsBackslashMirror(mirror_x, mirror_y));
    assert(GetCell(mirror_x, mirror_y) == type);
    assert(mirrors.find(Mirror{mirror_x, mirror_y, type}) != mirrors.end());
    mirrors.erase({mirror_x, mirror_y, type});
    RemoveItem(mirror_x, mirror_y);
  }

#ifdef ENABLE_INTERNAL_STATE_CHECK
  void CheckInternalStateForDebug(const string& message,
                                  const Board& initial_board) const {
    for (const auto& l : lanterns) {
      assert(GetCell(l.x, l.y) == l.color);
    }
    for (const auto& m : mirrors) {
      assert(GetCell(m.x, m.y) == m.type);
    }
    for (const auto& o : obstacles) {
      assert(GetCell(o.first, o.second) == OBSTACLE);
    }

    vector<vector<uint8_t>> local_lit_colors(w, vector<uint8_t>(h, 0));
    int local_invalid_lays = 0;
    int local_wrong_lays = 0;
    auto update_lay = [&](int x, int y, int dir, uint8_t color) {
      while (true) {
        x += DIR_X[dir];
        y += DIR_Y[dir];
        if (!IsInBound(x, y)) {
          return;
        } else if (IsObstacle(x, y)) {
          return;
        } else if (IsLantern(x, y)) {
          ++local_invalid_lays;
          return;
        } else if (IsSlashMirror(x, y)) {
          dir = MIRROR_S_TO[dir];
        } else if (IsBackslashMirror(x, y)) {
          dir = MIRROR_B_TO[dir];
        } else if (IsCrystal(x, y)) {
          local_lit_colors[x][y] |= color;
          if (!(color & GetCrystalColor(x, y))) {
            ++local_wrong_lays;
          }
          return;
        }
      }
    };
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        if (IsLantern(x, y)) {
          assert(initial_board.IsEmpty(x, y));
          assert(lanterns.find({x, y, GetCell(x, y)}) != lanterns.end());
          for (int d = 0; d < 4; ++d) {
            update_lay(x, y, d, GetCell(x, y));
          }
        } else if (IsSlashMirror(x, y)) {
          assert(initial_board.IsEmpty(x, y));
          assert(mirrors.find({x, y, SLASH_MIRROR}) != mirrors.end());
        } else if (IsBackslashMirror(x, y)) {
          assert(initial_board.IsEmpty(x, y));
          assert(mirrors.find({x, y, BACKSLASH_MIRROR}) != mirrors.end());
        } else if (IsObstacle(x, y)) {
          if (!initial_board.IsObstacle(x, y)) {
            assert(initial_board.IsEmpty(x, y));
            assert(obstacles.find({x, y}) != obstacles.end());
          }
        }
      }
    }
    int local_lit_crystals = 0;
    int local_lit_compound_crystals = 0;
    int local_lit_wrong_crystals = 0;
    int local_crystals_nbit_off[4] = {};
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        if (GetCell(x, y) & CRYSTAL_COLOR_MASK) {
          int result = local_lit_colors[x][y] ^ (GetCell(x, y) >> 3);
          if (result == 0) {
            if (IsSecondaryColorCrystal(x, y)) {
              ++local_lit_compound_crystals;
            } else {
              ++local_lit_crystals;
            }
          } else if (local_lit_colors[x][y] != 0) {
            ++local_lit_wrong_crystals;
          }
          local_crystals_nbit_off[__builtin_popcount(result)]++;
        }
      }
    }
    if (lit_crystals != local_lit_crystals) {
      cerr << message << endl;
      cerr << lit_crystals << " != " << local_lit_crystals;
    }
    assert(lit_crystals == local_lit_crystals);
    if (lit_compound_crystals != local_lit_compound_crystals) {
      cerr << message << endl;
      cerr << lit_compound_crystals << " != " << local_lit_compound_crystals;
    }
    assert(lit_compound_crystals == local_lit_compound_crystals);
    if (lit_wrong_crystals != local_lit_wrong_crystals) {
      cerr << message << endl;
      cerr << lit_wrong_crystals << " != " << local_lit_wrong_crystals << endl;
    }
    assert(lit_wrong_crystals == local_lit_wrong_crystals);

    for (int i = 0; i < 4; ++i) {
      if (crystals_nbit_off[i] != local_crystals_nbit_off[i]) {
        cerr << message << endl;
        cerr << "bit_" << i << ": " << crystals_nbit_off[i]
             << " != " << local_crystals_nbit_off[i] << endl;
        for (int j = 0; j < 4; ++j) {
          cerr << "bit_" << j << ": " << crystals_nbit_off[j] << ", "
               << local_crystals_nbit_off[j] << endl;
        }
      }
      assert(crystals_nbit_off[i] == local_crystals_nbit_off[i]);
    }

    if (wrong_lays != local_wrong_lays) {
      cerr << message << endl;
      cerr << wrong_lays << " != " << local_wrong_lays << endl;
    }
    assert(wrong_lays == local_wrong_lays);

    if (invalid_lays != local_invalid_lays) {
      cerr << message << endl;
      cerr << invalid_lays << " != " << local_invalid_lays << endl;
    }
    assert(invalid_lays == local_invalid_lays);
  }
#endif
};

struct Answer {
  int score;
  set<Pos> obstacles;
  set<Mirror> mirrors;
  set<Lantern> lanterns;
};

class Solver {
 public:
  Solver(const Timer& timer, const Board& initial_board, int cost_lantern,
         int cost_mirror, int cost_obstacle, int max_mirrors, int max_obstacles)
      : timer_(&timer),
        board_width_(initial_board.w),
        board_height_(initial_board.h),
        initial_board_(initial_board),
        cost_lantern_(cost_lantern),
        cost_mirror_(cost_mirror),
        cost_obstacle_(cost_obstacle),
        max_mirrors_(max_mirrors),
        max_obstacles_(max_obstacles),
        board_(initial_board) {}

  inline void MaybeUpdateBestAnswer() {
    int score = GetScore();
    if (score > best_answer_.score) {
      best_answer_ =
          Answer{score, board_.obstacles, board_.mirrors, board_.lanterns};
    }
  }

  inline int GetScore() const {
    if (board_.invalid_lays || board_.mirrors.size() > max_mirrors_ ||
        board_.obstacles.size() > max_obstacles_) {
      return -1;
    }
    return board_.lit_crystals * 20 + board_.lit_compound_crystals * 30 -
           board_.lit_wrong_crystals * 10 -
           board_.lanterns.size() * cost_lantern_ -
           board_.obstacles.size() * cost_obstacle_ -
           board_.mirrors.size() * cost_mirror_;
  }

  inline double GetEnergy() const {
    double exceeded_mirrors = max(0, int(board_.mirrors.size()) - max_mirrors_);
    double exceeded_obstacles =
        max(0, int(board_.obstacles.size()) - max_obstacles_);
    return -(2.0 * board_.lit_crystals + 3.0 * board_.lit_compound_crystals +
             -1.0 * board_.lit_wrong_crystals +
             -0.1 * board_.lanterns.size() * cost_lantern_ +
             -0.1 * board_.obstacles.size() * cost_obstacle_ +
             -0.1 * board_.mirrors.size() * cost_mirror_ +
             -0.1 * board_.crystals_nbit_off[1] +
             -0.3 * board_.crystals_nbit_off[2] +
             -0.6 * board_.crystals_nbit_off[3] + -0.1 * board_.wrong_lays +
             -4.0 * board_.invalid_lays +
             -4.0 * exceeded_mirrors * exceeded_mirrors +
             -4.0 * exceeded_obstacles * exceeded_obstacles);
  }

  inline double GetTemperature() const {
    return 1.0 - timer_->GetNormalizedTime();
  }

  void SimulatedAnnealing() {
    mt19937 gen;
    vector<Pos> available_positions;
    for (int y = 0; y < board_height_; ++y) {
      for (int x = 0; x < board_width_; ++x) {
        if (board_.IsEmpty(x, y)) {
          available_positions.emplace_back(Pos{x, y});
        }
      }
    }
    uniform_int_distribution<int> rand_next_pos(0,
                                                available_positions.size() - 1);
    uniform_real_distribution<double> rand_prob(0, 1);

    double energy = GetEnergy();
    double best_energy = energy;
    auto accept = [&energy, &best_energy, &rand_prob, &gen, this]() {
#ifdef ENABLE_INTERNAL_STATE_CHECK
      board_.CheckInternalStateForDebug("accept lambda", initial_board_);
#endif
      MaybeUpdateBestAnswer();
      double new_energy = GetEnergy();
      if (new_energy <= energy ||
          rand_prob(gen) < exp(-(new_energy - energy) / GetTemperature())) {
        best_energy = min(best_energy, new_energy);
        energy = new_energy;
        return true;
      }
      return false;
    };
    while (timer_->GetNormalizedTime() < 0.95) {
#ifdef LOCAL_DEBUG_MODE
      static double next_report_time = 0;
      if (next_report_time < timer_->GetNormalizedTime()) {
        cerr << "time: " << next_report_time << ", temp: " << GetTemperature()
             << ", invalid_lays: " << board_.invalid_lays
             << ", obstacles: " << board_.obstacles.size() << "/"
             << max_obstacles_ << ", mirrors: " << board_.mirrors.size() << "/"
             << max_mirrors_ << ", energy: " << energy
             << ", best_energy: " << best_energy << ", score: " << GetScore()
             << ", best_score: " << best_answer_.score << endl;
        next_report_time += 0.1;
      }
#endif

      const auto& next_pos = available_positions[rand_next_pos(gen)];
      int x = next_pos.first;
      int y = next_pos.second;
      if (board_.IsEmpty(x, y)) {
        bool create_lantern =
            !board_.HasLay(x, y) ||
            (max_mirrors_ == 0 && max_obstacles_ == 0) ||
            uniform_real_distribution<double>(0, 1.0)(gen) < 0.001;
        if (create_lantern) {
          uint8_t color = 1 << uniform_int_distribution<int>(0, 2)(gen);
          board_.PutLantern(x, y, color);
          if (!accept()) {
            board_.RemoveLantern(x, y, color);
          }
        } else {
          assert(max_mirrors_ || max_obstacles_);
          uint8_t item_type = uniform_int_distribution<int>(1, 3)(gen) << 6;
          if (max_obstacles_ == 0) {
            item_type = uniform_int_distribution<int>(1, 2)(gen) << 6;
          }
          if (max_mirrors_ == 0) {
            item_type = OBSTACLE;
          }
          if (item_type == OBSTACLE) {
            if (board_.obstacles.size() < max_obstacles_) {
              board_.PutObstacle(x, y);
              if (!accept()) {
                board_.RemoveObstacle(x, y);
              }
            }
          } else {
            if (board_.mirrors.size() < max_mirrors_) {
              board_.PutMirror(x, y, item_type);
              if (!accept()) {
                board_.RemoveMirror(x, y, item_type);
              }
            }
          }
        }
      } else {
        if (board_.IsLantern(x, y)) {
          uint8_t color = board_.GetCell(x, y);
          board_.RemoveLantern(x, y, color);
          if (!accept()) {
            board_.PutLantern(x, y, color);
          }
        } else if (board_.IsObstacle(x, y)) {
          board_.RemoveObstacle(x, y);
          if (!accept()) {
            board_.PutObstacle(x, y);
          }
        } else if (board_.IsSlashMirror(x, y) ||
                   board_.IsBackslashMirror(x, y)) {
          uint8_t type = board_.GetCell(x, y);
          board_.RemoveMirror(x, y, type);
          if (!accept()) {
            board_.PutMirror(x, y, type);
          }
        }
      }
    }
  }

  vector<string> Solve() {
#ifdef ENABLE_INTERNAL_STATE_CHECK
    board_.CheckInternalStateForDebug("initial board state", initial_board_);
#endif

    SimulatedAnnealing();

#ifdef LOCAL_DEBUG_MODE
    cerr << "Final score = " << best_answer_.score << endl;
#endif

    vector<string> ret;
    for (const auto& l : best_answer_.lanterns) {
      stringstream ss;
      ss << l.y << " " << l.x << " " << int(l.color);
      ret.push_back(ss.str());
    }
    for (const auto& o : best_answer_.obstacles) {
      stringstream ss;
      ss << o.second << " " << o.first << " X";
      ret.push_back(ss.str());
    }
    for (const auto& m : best_answer_.mirrors) {
      stringstream ss;
      ss << m.y << " " << m.x << " " << (m.type == SLASH_MIRROR ? "/" : "\\");
      ret.push_back(ss.str());
    }
    return ret;
  }

 private:
  const Timer* timer_;
  const int board_width_;
  const int board_height_;
  const Board initial_board_;
  const int cost_lantern_;
  const int cost_mirror_;
  const int cost_obstacle_;
  const int max_mirrors_;
  const int max_obstacles_;

  Board board_;
  Answer best_answer_ = {};
};

class CrystalLighting {
 public:
  vector<string> placeItems(vector<string> target_board, int cost_lantern,
                            int cost_mirror, int cost_obstacle, int max_mirrors,
                            int max_obstacles) {
    Timer timer;
    timer.Start();
    Board board = {};
    board.w = target_board[0].size();
    board.h = target_board.size();
    board.cells.resize(board.w * board.h);
    board.lay.resize(board.w * board.h);
    for (int y = 0; y < board.h; ++y) {
      for (int x = 0; x < board.w; ++x) {
        if (target_board[y][x] == '.') {
          board.SetCell(x, y, EMPTY_CELL);
        } else if (target_board[y][x] == 'X') {
          board.SetCell(x, y, OBSTACLE);
        } else {
          uint8_t color = target_board[y][x] - '0';
          board.SetCell(x, y, color << 3);
          ++board.crystals_nbit_off[__builtin_popcount(color)];
        }
      }
    }
    return Solver(timer, board, cost_lantern, cost_mirror, cost_obstacle,
                  max_mirrors, max_obstacles)
        .Solve();
  }
};

#ifdef LOCAL_ENTRY_POINT_FOR_TESTING
// -------8<------- end of solution submitted to the website -------8<-------
template <class T>
void getVector(vector<T>& v) {
  for (int i = 0; i < v.size(); ++i) cin >> v[i];
}

int main() {
  CrystalLighting cl;
  int H;
  cin >> H;
  vector<string> targetBoard(H);
  getVector(targetBoard);
  int costLantern, costMirror, costObstacle, maxMirrors, maxObstacles;
  cin >> costLantern >> costMirror >> costObstacle >> maxMirrors >>
      maxObstacles;

  vector<string> ret = cl.placeItems(targetBoard, costLantern, costMirror,
                                     costObstacle, maxMirrors, maxObstacles);
  cout << ret.size() << endl;
  for (int i = 0; i < (int)ret.size(); ++i) cout << ret[i] << endl;
  cout.flush();
}
#endif
