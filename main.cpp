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

constexpr uint8_t BLUE = 0x1;
constexpr uint8_t YELLOW = 0x2;
constexpr uint8_t RED = 0x4;

constexpr uint8_t GREEN = 0x3;
constexpr uint8_t VIOLET = 0x5;
constexpr uint8_t ORANGE = 0x6;

constexpr uint8_t EMPTY_CELL = 0;
constexpr uint8_t SLASH_MIRROR = (0x1 << 6);
constexpr uint8_t BACKSLASH_MIRROR = (0x2 << 6);
constexpr uint8_t OBSTACLE = (0x3 << 6);

constexpr uint8_t LANTERN_COLOR_MASK = 0x7;
constexpr uint8_t CRYSTAL_COLOR_MASK = (0x7 << 3);

// Timer implementation based on nika's submission.
// http://community.topcoder.com/longcontest/?module=ViewProblemSolution&pm=14907&rd=17153&cr=20315020&subnum=17
class Timer {
 public:
  void Start() { start_seconds_ = GetSeconds(); }

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

struct LanternLookupMetadata {
  int x;
  int y;
  uint8_t color;

  bool operator<(const LanternLookupMetadata& rhs) const {
    return make_tuple(x, y, color) < make_tuple(rhs.x, rhs.y, rhs.color);
  }

  bool operator==(const LanternLookupMetadata& rhs) const {
    return make_tuple(x, y, color) == make_tuple(rhs.x, rhs.y, rhs.color);
  }
};

typedef pair<Pos, Pos> InvalidLantern;

struct MirrorMetadata {
  int x;
  int y;
  uint8_t type;

  bool operator<(const MirrorMetadata& rhs) const {
    return make_tuple(x, y, type) < make_tuple(rhs.x, rhs.y, rhs.type);
  }
};

inline uint8_t CompoundColor(const set<LanternLookupMetadata>& lanterns) {
  return accumulate(
      lanterns.begin(), lanterns.end(), 0,
      [](uint8_t c, const LanternLookupMetadata& l) { return c | l.color; });
}

inline uint8_t CompoundColor(const set<LanternLookupMetadata>& lanterns,
                             const LanternLookupMetadata& exclude) {
  return accumulate(lanterns.begin(), lanterns.end(), 0,
                    [&](uint8_t c, const LanternLookupMetadata& l) {
                      return l == exclude ? c : (c | l.color);
                    });
}

constexpr int DIR_X[] = {0, 1, 0, -1};
constexpr int DIR_Y[] = {-1, 0, 1, 0};
constexpr int MIRROR_S_TO[] = {1, 0, 3, 2};
constexpr int MIRROR_B_TO[] = {3, 2, 1, 0};

struct Board {
  int w, h;
  vector<uint8_t> cells;
  set<Pos> obstacles;
  set<MirrorMetadata> mirrors;
  set<LanternLookupMetadata> lanterns;
  array<vector<set<LanternLookupMetadata>>, 4> lantern_lookup;
  set<InvalidLantern> invalid_lanterns;
  set<Pos> lit_crystals;
  set<Pos> lit_compound_crystals;
  set<Pos> lit_wrong_crystals;

  array<set<Pos>, 3> crystals_nbit_off;

  inline void SetCell(int x, int y, uint8_t cell_value) {
    cells[y * w + x] = cell_value;
  }

  inline uint8_t GetCell(int x, int y) const { return cells[y * w + x]; }

  inline void AddLanternInfo(int x, int y, int dir, int lantern_x,
                             int lantern_y, uint8_t lantern_color) {
    lantern_lookup[dir][y * w + x].emplace(
        LanternLookupMetadata{lantern_x, lantern_y, lantern_color});
  }

  inline void RemoveLanternInfo(int x, int y, int dir, int lantern_x,
                                int lantern_y, uint8_t lantern_color) {
    lantern_lookup[dir][y * w + x].erase({lantern_x, lantern_y, lantern_color});
  }

  inline const set<LanternLookupMetadata>& GetLanternInfo(int x, int y,
                                                          int dir) const {
    return lantern_lookup[dir][y * w + x];
  }

  inline bool HasAnyLanternInfo(int x, int y) const {
    for (int i = 0; i < 4; ++i) {
      if (!GetLanternInfo(x, y, i).empty()) {
        return true;
      }
    }
    return false;
  }

  inline uint8_t GetLitColor(int x, int y) const {
    uint8_t color = 0;
    for (int dir = 0; dir < 4; ++dir) {
      int x2 = x - DIR_X[dir];
      int y2 = y - DIR_Y[dir];
      color |=
          IsInBound(x2, y2) ? CompoundColor(GetLanternInfo(x2, y2, dir)) : 0;
    }
    return color;
  }

  inline uint8_t GetLitColor(int x, int y, int exclude_x, int exclude_y,
                             uint8_t exclude_color) const {
    uint8_t color = 0;
    for (int dir = 0; dir < 4; ++dir) {
      int x2 = x - DIR_X[dir];
      int y2 = y - DIR_Y[dir];
      color |= IsInBound(x2, y2)
                   ? CompoundColor(GetLanternInfo(x2, y2, dir),
                                   {exclude_x, exclude_y, exclude_color})
                   : 0;
    }
    return color;
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

  void LayTrace(int x, int y, int dir, int lantern_x, int lantern_y,
                uint8_t lantern_color) {
    assert(IsLantern(lantern_x, lantern_y));
    AddLanternInfo(x, y, dir, lantern_x, lantern_y, lantern_color);

#ifdef LOCAL_DEBUG_MODE
    int loop_counter = 0;
#endif
    while (true) {
      x += DIR_X[dir];
      y += DIR_Y[dir];
      if (!IsInBound(x, y)) {
        break;
      } else if (IsObstacle(x, y)) {
        break;
      } else if (IsLantern(x, y)) {
        auto l1 = make_pair(x, y);
        auto l2 = make_pair(lantern_x, lantern_y);
        invalid_lanterns.emplace(InvalidLantern{min(l1, l2), max(l1, l2)});
        break;
      } else if (IsSlashMirror(x, y)) {
        dir = MIRROR_S_TO[dir];
      } else if (IsBackslashMirror(x, y)) {
        dir = MIRROR_B_TO[dir];
      } else if (IsCrystal(x, y)) {
        uint8_t lit_color = GetLitColor(x, y);
        uint8_t crystal_color = GetCrystalColor(x, y);
        if (lit_color == crystal_color) {
          if (IsSecondaryColorCrystal(x, y)) {
            lit_compound_crystals.emplace(Pos{x, y});
            lit_wrong_crystals.erase({x, y});
          } else {
            lit_crystals.emplace(Pos{x, y});
            lit_wrong_crystals.erase({x, y});
          }
        } else {
          if (IsSecondaryColorCrystal(x, y)) {
            lit_compound_crystals.erase({x, y});
            lit_wrong_crystals.emplace(Pos{x, y});
          } else {
            lit_crystals.erase({x, y});
            lit_wrong_crystals.emplace(Pos{x, y});
          }
        }

        int off_bits_count = __builtin_popcount(lit_color ^ crystal_color);
        for (auto& p_set : crystals_nbit_off) {
          p_set.erase(Pos{x, y});
        }
        if (off_bits_count != 0) {
          crystals_nbit_off[off_bits_count - 1].emplace(Pos{x, y});
        }
        break;
      }

#ifdef LOCAL_DEBUG_MODE
      assert(loop_counter++ < w * h * 2);
#endif

      assert(!(x == lantern_x && y == lantern_y));
      AddLanternInfo(x, y, dir, lantern_x, lantern_y, lantern_color);
    }
  }

  void RevertLayTrace(int x, int y, int dir, int lantern_x, int lantern_y,
                      uint8_t lantern_color) {
    assert(IsLantern(lantern_x, lantern_y));
    RemoveLanternInfo(x, y, dir, lantern_x, lantern_y, lantern_color);

#ifdef LOCAL_DEBUG_MODE
    int loop_counter = 0;
#endif
    while (true) {
      x += DIR_X[dir];
      y += DIR_Y[dir];
      if (!IsInBound(x, y)) {
        break;
      } else if (IsObstacle(x, y)) {
        break;
      } else if (IsLantern(x, y)) {
        auto l1 = make_pair(x, y);
        auto l2 = make_pair(lantern_x, lantern_y);
        invalid_lanterns.erase({min(l1, l2), max(l1, l2)});
        break;
      } else if (IsSlashMirror(x, y)) {
        dir = MIRROR_S_TO[dir];
      } else if (IsBackslashMirror(x, y)) {
        dir = MIRROR_B_TO[dir];
      } else if (IsCrystal(x, y)) {
        uint8_t lit_color =
            GetLitColor(x, y, lantern_x, lantern_y, lantern_color);
        uint8_t crystal_color = GetCrystalColor(x, y);
        if (lit_color == crystal_color) {
          if (IsSecondaryColorCrystal(x, y)) {
            lit_compound_crystals.emplace(Pos{x, y});
            lit_wrong_crystals.erase({x, y});
          } else {
            lit_crystals.emplace(Pos{x, y});
            lit_wrong_crystals.erase({x, y});
          }
        } else if (lit_color == 0) {
          if (IsSecondaryColorCrystal(x, y)) {
            lit_compound_crystals.erase({x, y});
            lit_wrong_crystals.erase({x, y});
          } else {
            lit_crystals.erase({x, y});
            lit_wrong_crystals.erase({x, y});
          }
        } else {
          if (IsSecondaryColorCrystal(x, y)) {
            lit_compound_crystals.erase({x, y});
            lit_wrong_crystals.emplace(Pos{x, y});
          } else {
            lit_crystals.erase({x, y});
            lit_wrong_crystals.emplace(Pos{x, y});
          }
        }

        int off_bits_count = __builtin_popcount(lit_color ^ crystal_color);
        for (auto& p_set : crystals_nbit_off) {
          p_set.erase(Pos{x, y});
        }
        if (off_bits_count != 0) {
          crystals_nbit_off[off_bits_count - 1].emplace(Pos{x, y});
        }

        break;
      }

#ifdef LOCAL_DEBUG_MODE
      assert(loop_counter++ < w * h * 2);
#endif

      assert(!(x == lantern_x && y == lantern_y));
      RemoveLanternInfo(x, y, dir, lantern_x, lantern_y, lantern_color);
    }
  }

  void PutItem(int item_x, int item_y, uint8_t item) {
    assert(item == EMPTY_CELL || IsEmpty(item_x, item_y));
    array<set<LanternLookupMetadata>, 4> lanterns_to_process;
    for (int i = 0; i < 4; ++i) {
      int x = item_x - DIR_X[i];
      int y = item_y - DIR_Y[i];
      if (IsInBound(x, y)) {
        lanterns_to_process[i] = GetLanternInfo(x, y, i);
      }
    }
    for (int i = 0; i < 4; ++i) {
      int x = item_x - DIR_X[i];
      int y = item_y - DIR_Y[i];
      for (const auto& l : lanterns_to_process[i]) {
        assert(GetCell(l.x, l.y) == l.color);
        RevertLayTrace(x, y, i, l.x, l.y, l.color);
      }
    }
    SetCell(item_x, item_y, item);
    set<LanternLookupMetadata> processed_lanterns;
    for (int i = 0; i < 4; ++i) {
      for (const auto& l : lanterns_to_process[i]) {
        if (processed_lanterns.insert(l).second) {
          for (int j = 0; j < 4; ++j) {
            LayTrace(l.x, l.y, j, l.x, l.y, l.color);
          }
        }
      }
    }
  }

  void RemoveItem(int item_x, int item_y) {
    assert(!IsEmpty(item_x, item_y));

    PutItem(item_x, item_y, EMPTY_CELL);
  }

  void PutLantern(int lantern_x, int lantern_y, uint8_t lantern_color) {
    assert(IsEmpty(lantern_x, lantern_y));
    PutItem(lantern_x, lantern_y, lantern_color);
    lanterns.emplace(
        LanternLookupMetadata{lantern_x, lantern_y, lantern_color});
    for (int initial_dir = 0; initial_dir < 4; ++initial_dir) {
      LayTrace(lantern_x, lantern_y, initial_dir, lantern_x, lantern_y,
               lantern_color);
    }
  }

  void RemoveLantern(int lantern_x, int lantern_y, uint8_t lantern_color) {
    assert(IsLantern(lantern_x, lantern_y));
    assert(GetCell(lantern_x, lantern_y) == lantern_color);
    for (int initial_dir = 0; initial_dir < 4; ++initial_dir) {
      RevertLayTrace(lantern_x, lantern_y, initial_dir, lantern_x, lantern_y,
                     lantern_color);
    }
    lanterns.erase({lantern_x, lantern_y, lantern_color});
    RemoveItem(lantern_x, lantern_y);
  }

  void PutObstacle(int obstacle_x, int obstacle_y) {
    assert(IsEmpty(obstacle_x, obstacle_y));
    PutItem(obstacle_x, obstacle_y, OBSTACLE);
    obstacles.emplace(Pos{obstacle_x, obstacle_y});
  }

  void RemoveObstacle(int obstacle_x, int obstacle_y) {
    assert(IsObstacle(obstacle_x, obstacle_y));
    assert(obstacles.find({obstacle_x, obstacle_y}) != obstacles.end());
    obstacles.erase({obstacle_x, obstacle_y});
    RemoveItem(obstacle_x, obstacle_y);
  }

  void PutMirror(int mirror_x, int mirror_y, uint8_t type) {
    assert(IsEmpty(mirror_x, mirror_y));
    assert(type == SLASH_MIRROR || type == BACKSLASH_MIRROR);
    PutItem(mirror_x, mirror_y, type);
    mirrors.emplace(MirrorMetadata{mirror_x, mirror_y, type});
  }

  void RemoveMirror(int mirror_x, int mirror_y, uint8_t type) {
    assert(IsSlashMirror(mirror_x, mirror_y) ||
           IsBackslashMirror(mirror_x, mirror_y));
    assert(GetCell(mirror_x, mirror_y) == type);
    assert(mirrors.find(MirrorMetadata{mirror_x, mirror_y, type}) !=
           mirrors.end());
    mirrors.erase({mirror_x, mirror_y, type});
    RemoveItem(mirror_x, mirror_y);
  }
};

struct Answer {
  int score;
  set<Pos> obstacles;
  set<MirrorMetadata> mirrors;
  set<LanternLookupMetadata> lanterns;
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
        board_(initial_board),
        best_answer_() {
    best_answer_.score = 0;
  }

  void MaybeUpdateBestAnswer() {
    int score = GetScore();
    if (score > best_answer_.score) {
      best_answer_ =
          Answer{score, board_.obstacles, board_.mirrors, board_.lanterns};
    }
  }

  int GetScore() const {
    return board_.lit_crystals.size() * 20 +
           board_.lit_compound_crystals.size() * 30 -
           board_.lit_wrong_crystals.size() * 10 -
           board_.lanterns.size() * cost_lantern_ -
           board_.obstacles.size() * cost_obstacle_ -
           board_.mirrors.size() * cost_mirror_;
  }

  double GetEnergy() const {
    return -(GetScore() * 0.1 - 1.0 * board_.crystals_nbit_off[0].size() -
             2.0 * board_.crystals_nbit_off[1].size() -
             3.0 * board_.crystals_nbit_off[2].size());
  }

  double GetTemperature() const { return 1.0 - timer_->GetNormalizedTime(); }

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
    auto should_accept = [&energy, &rand_prob, &gen, this]() {
      double new_energy = GetEnergy();
      return new_energy < energy ||
             rand_prob(gen) < exp(-(new_energy - energy) / GetTemperature());
    };
    while (timer_->GetNormalizedTime() < 0.95) {
#ifdef LOCAL_DEBUG_MODE
      static double next_report_time = 0;
      if (next_report_time < timer_->GetNormalizedTime()) {
        cerr << "time: " << next_report_time << ", temp: " << GetTemperature()
             << ", energy: " << energy << ", score: " << GetScore()
             << ", best: " << best_answer_.score << endl;
        next_report_time += 0.1;
      }
#endif

      const auto& next_pos = available_positions[rand_next_pos(gen)];
      int x = next_pos.first;
      int y = next_pos.second;
      if (board_.IsEmpty(x, y)) {
        if (board_.HasAnyLanternInfo(x, y)) {
          uint8_t item_type = uniform_int_distribution<int>(1, 3)(gen) << 6;
          if (item_type == OBSTACLE) {
            if (board_.obstacles.size() < max_obstacles_) {
              board_.PutObstacle(x, y);
              if (should_accept()) {
                energy = GetEnergy();
                MaybeUpdateBestAnswer();
              } else {
                board_.RemoveObstacle(x, y);
              }
            }
          } else {
            if (board_.mirrors.size() < max_mirrors_) {
              board_.PutMirror(x, y, item_type);
              if (board_.invalid_lanterns.empty() && should_accept()) {
                energy = GetEnergy();
                MaybeUpdateBestAnswer();
              } else {
                board_.RemoveMirror(x, y, item_type);
              }
            }
          }
        } else {
          uint8_t color = 1 << uniform_int_distribution<int>(0, 2)(gen);
          board_.PutLantern(x, y, color);
          if (board_.invalid_lanterns.empty() && should_accept()) {
            energy = GetEnergy();
            MaybeUpdateBestAnswer();
          } else {
            board_.RemoveLantern(x, y, color);
          }
        }
      } else {
        if (board_.IsLantern(x, y)) {
          uint8_t color = board_.GetCell(x, y);
          board_.RemoveLantern(x, y, color);
          if (should_accept()) {
            energy = GetEnergy();
            MaybeUpdateBestAnswer();
          } else {
            board_.PutLantern(x, y, color);
          }
        } else if (board_.IsObstacle(x, y)) {
          board_.RemoveObstacle(x, y);
          if (board_.invalid_lanterns.empty() && should_accept()) {
            energy = GetEnergy();
            MaybeUpdateBestAnswer();
          } else {
            board_.PutObstacle(x, y);
          }
        } else if (board_.IsSlashMirror(x, y) ||
                   board_.IsBackslashMirror(x, y)) {
          uint8_t type = board_.GetCell(x, y);
          board_.RemoveMirror(x, y, type);
          if (board_.invalid_lanterns.empty() && should_accept()) {
            energy = GetEnergy();
            MaybeUpdateBestAnswer();
          } else {
            board_.PutMirror(x, y, type);
          }
        }
      }
    }
  }

  vector<string> Solve() {
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
  Answer best_answer_;
};

class CrystalLighting {
 public:
  vector<string> placeItems(vector<string> target_board, int cost_lantern,
                            int cost_mirror, int cost_obstacle, int max_mirrors,
                            int max_obstacles) {
    Timer timer;
    timer.Start();
    Board board;
    board.w = target_board[0].size();
    board.h = target_board.size();
    board.cells.resize(board.w * board.h);
    for (int y = 0; y < board.h; ++y) {
      for (int x = 0; x < board.w; ++x) {
        if (target_board[y][x] == '.') {
          board.SetCell(x, y, EMPTY_CELL);
        } else if (target_board[y][x] == 'X') {
          board.SetCell(x, y, OBSTACLE);
        } else {
          uint8_t color = target_board[y][x] - '0';
          board.SetCell(x, y, color << 3);
          board.crystals_nbit_off[__builtin_popcount(color) - 1].emplace(
              Pos{x, y});
        }
      }
    }
    for (int dir = 0; dir < 4; ++dir) {
      board.lantern_lookup[dir].resize(board.w * board.h);
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
