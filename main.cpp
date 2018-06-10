#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <numeric>
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

constexpr uint8_t LANTERN_MASK = 0x7;
constexpr uint8_t CRYSTAL_MASK = (0x7 << 3);
constexpr uint8_t ITEM_MASK = (0x3 << 6);

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

typedef pair<int, int> Pos;

struct Board {
  int w, h;
  vector<uint8_t> cells;
  set<LanternLookupMetadata> lanterns;
  vector<set<LanternLookupMetadata>> lantern_lookup_h;
  vector<set<LanternLookupMetadata>> lantern_lookup_v;
  set<Pos> lit_crystals;
  set<Pos> lit_compound_crystals;
  set<Pos> lit_wrong_crystals;

  inline void SetCell(int x, int y, uint8_t cell_value) {
    cells[y * w + x] = cell_value;
  }

  inline uint8_t GetCell(int x, int y) const { return cells[y * w + x]; }

  inline void AddLanternH(int x, int y, int lantern_x, int lantern_y,
                          uint8_t lantern_color) {
    lantern_lookup_h[y * w + x].emplace(
        LanternLookupMetadata{lantern_x, lantern_y, lantern_color});
  }

  inline void RemoveLanternH(int x, int y, int lantern_x, int lantern_y,
                             uint8_t lantern_color) {
    lantern_lookup_h[y * w + x].erase({lantern_x, lantern_y, lantern_color});
  }

  inline const set<LanternLookupMetadata>& GetLanternH(int x, int y) const {
    return lantern_lookup_h[y * w + x];
  }

  inline void AddLanternV(int x, int y, int lantern_x, int lantern_y,
                          uint8_t lantern_color) {
    lantern_lookup_v[y * w + x].emplace(
        LanternLookupMetadata{lantern_x, lantern_y, lantern_color});
  }

  inline void RemoveLanternV(int x, int y, int lantern_x, int lantern_y,
                             uint8_t lantern_color) {
    lantern_lookup_v[y * w + x].erase({lantern_x, lantern_y, lantern_color});
  }

  inline const set<LanternLookupMetadata>& GetLanternV(int x, int y) const {
    return lantern_lookup_v[y * w + x];
  }

  inline uint8_t GetLitColor(int x, int y) const {
    uint8_t color = 0;
    color |= IsInBound(x + 1, y) ? CompoundColor(GetLanternH(x + 1, y)) : 0;
    color |= IsInBound(x - 1, y) ? CompoundColor(GetLanternH(x - 1, y)) : 0;
    color |= IsInBound(x, y + 1) ? CompoundColor(GetLanternV(x, y + 1)) : 0;
    color |= IsInBound(x, y - 1) ? CompoundColor(GetLanternV(x, y - 1)) : 0;
    return color;
  }

  inline uint8_t GetLitColor(int x, int y, int exclude_x, int exclude_y,
                             uint8_t exclude_color) const {
    uint8_t color = 0;
    color |= IsInBound(x + 1, y)
                 ? CompoundColor(GetLanternH(x + 1, y),
                                 {exclude_x, exclude_y, exclude_color})
                 : 0;
    color |= IsInBound(x - 1, y)
                 ? CompoundColor(GetLanternH(x - 1, y),
                                 {exclude_x, exclude_y, exclude_color})
                 : 0;
    color |= IsInBound(x, y + 1)
                 ? CompoundColor(GetLanternV(x, y + 1),
                                 {exclude_x, exclude_y, exclude_color})
                 : 0;
    color |= IsInBound(x, y - 1)
                 ? CompoundColor(GetLanternV(x, y - 1),
                                 {exclude_x, exclude_y, exclude_color})
                 : 0;
    return color;
  }

  inline bool IsInBound(int x, int y) const {
    return !(x < 0 || x >= w || y < 0 || y >= h);
  }

  inline bool IsEmpty(int x, int y) const {
    return GetCell(x, y) == EMPTY_CELL;
  }

  inline bool IsCrystal(int x, int y) const {
    return GetCell(x, y) & CRYSTAL_MASK;
  }

  inline bool IsSecondaryColorCrystal(int x, int y) const {
    uint8_t color = GetCrystalColor(x, y);
    return color == GREEN || color == VIOLET || color == ORANGE;
  }

  inline uint8_t GetCrystalColor(int x, int y) const {
    return (GetCell(x, y) & CRYSTAL_MASK) >> 3;
  }

  inline bool IsLantern(int x, int y) const {
    return GetCell(x, y) & LANTERN_MASK;
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

  bool PutLantern(int lantern_x, int lantern_y, uint8_t lantern_color) {
    assert(IsEmpty(lantern_x, lantern_y));

    lanterns.emplace(LanternLookupMetadata{lantern_x, lantern_y, lantern_color});
    SetCell(lantern_x, lantern_y, lantern_color);
    AddLanternH(lantern_x, lantern_y, lantern_x, lantern_y, lantern_color);
    AddLanternV(lantern_x, lantern_y, lantern_x, lantern_y, lantern_color);

    bool valid = true;
    for (int initial_dir = 0; initial_dir < 4; ++initial_dir) {
      int dir = initial_dir;
      int x = lantern_x;
      int y = lantern_y;
      while (true) {
        x += DIR_X[dir];
        y += DIR_Y[dir];
        if (!IsInBound(x, y)) {
          break;
        } else if (IsObstacle(x, y)) {
          break;
        } else if (IsLantern(x, y)) {
          valid = false;
          if (x == lantern_x && y == lantern_y) {
            break;
          }
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
          break;
        }

        if (dir % 2) {
          AddLanternH(x, y, lantern_x, lantern_y, lantern_color);
        } else {
          AddLanternV(x, y, lantern_x, lantern_y, lantern_color);
        }
      }
    }

    return valid;
  }

  int RemoveLantern(int lantern_x, int lantern_y, uint8_t lantern_color) {
    assert(IsLantern(lantern_x, lantern_y));
    assert(GetCell(lantern_x, lantern_y) == lantern_color);

    lanterns.erase({lantern_x, lantern_y, lantern_color});
    SetCell(lantern_x, lantern_y, EMPTY_CELL);
    RemoveLanternH(lantern_x, lantern_y, lantern_x, lantern_y, lantern_color);
    RemoveLanternV(lantern_x, lantern_y, lantern_x, lantern_y, lantern_color);

    int score_diff = 0;
    for (int initial_dir = 0; initial_dir < 4; ++initial_dir) {
      int dir = initial_dir;
      int x = lantern_x;
      int y = lantern_y;
      while (true) {
        x += DIR_X[dir];
        y += DIR_Y[dir];
        if (!IsInBound(x, y)) {
          break;
        } else if (IsObstacle(x, y)) {
          break;
        } else if (IsLantern(x, y)) {
          if (x == lantern_x && y == lantern_y) {
            break;
          }
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
          break;
        }

        if (dir % 2) {
          RemoveLanternH(x, y, lantern_x, lantern_y, lantern_color);
        } else {
          RemoveLanternV(x, y, lantern_x, lantern_y, lantern_color);
        }
      }
    }
    return score_diff;
  }
};

class Solver {
 public:
  Solver(const Board& initial_board, int cost_lantern, int cost_mirror,
         int cost_obstacle, int max_mirrors, int max_obstacles)
      : board_width_(initial_board.w),
        board_height_(initial_board.h),
        initial_board_(initial_board),
        cost_lantern_(cost_lantern),
        cost_mirror_(cost_mirror),
        cost_obstacle_(cost_obstacle),
        max_mirrors_(max_mirrors),
        max_obstacles_(max_obstacles),
        board_(initial_board) {}

  int GetScore() const {
    return board_.lit_crystals.size() * 20 +
           board_.lit_compound_crystals.size() * 30 -
           board_.lit_wrong_crystals.size() * 10 -
           board_.lanterns.size() * cost_lantern_;
  }

  vector<string> Solve() {
    auto find_best_move = [&]() {
      int best_score = 0;
      int best_x, best_y, best_color = 0;
      for (int i = 0; i < board_width_; ++i) {
        for (int j = 0; j < board_height_; ++j) {
          if (board_.IsEmpty(i, j) && board_.GetLanternH(i, j).empty() && board_.GetLanternV(i, j).empty()) {
            for (int k = 0; k < 3; ++k) {
              int prev_score = GetScore();
              bool valid = board_.PutLantern(i, j, (1 << k));
              int score = GetScore();
              if (valid && best_score < score) {
                best_score = score;
                best_x = i;
                best_y = j;
                best_color = (1 << k);
              }
              board_.RemoveLantern(i, j, (1 << k));
              assert(prev_score == GetScore());
            }
          }
        }
      }
      return make_tuple(best_score, best_x, best_y, best_color);
    };

    int score = 0;
    while (true) {
      int next_score, next_x, next_y, next_color;
      tie(next_score, next_x, next_y, next_color) = find_best_move();
      if (next_score <= score) {
        break;
      }

      bool valid = board_.PutLantern(next_x, next_y, next_color);
      assert(valid);
      assert(next_score == GetScore());

      score = next_score;

      cerr << "(" << next_x << ", " << next_y << "), col = " << next_color
           << ", score = " << score << endl;
    }

    vector<string> ret;
    for (int i = 0; i < board_width_; ++i) {
      for (int j = 0; j < board_height_; ++j) {
        if (board_.IsLantern(i, j)) {
          stringstream ss;
          ss << j << " " << i << " " << int(board_.GetCell(i, j));
          ret.push_back(ss.str());
        }
      }
    }
    return ret;
  }

 private:
  const int board_width_;
  const int board_height_;
  const Board initial_board_;
  const int cost_lantern_;
  const int cost_mirror_;
  const int cost_obstacle_;
  const int max_mirrors_;
  const int max_obstacles_;

  Board board_;
};

class CrystalLighting {
 public:
  vector<string> placeItems(vector<string> target_board, int cost_lantern,
                            int cost_mirror, int cost_obstacle, int max_mirrors,
                            int max_obstacles) {
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
          board.SetCell(x, y, (target_board[y][x] - '0') << 3);
        }
      }
    }
    board.lantern_lookup_h.resize(board.w * board.h);
    board.lantern_lookup_v.resize(board.w * board.h);
    return Solver(move(board), cost_lantern, cost_mirror, cost_obstacle,
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
