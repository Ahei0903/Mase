#include "MazeController.h"
#include "MazeModel.h"
#include "MazeView.h"
#include "MazeNode.h"

#include <chrono>
#include <random>
#include <algorithm>
#include <stack>
#include <queue>
#include <array>
#include <utility>
#include <memory>
#include <iostream>

MazeModel::MazeModel(uint32_t height, uint32_t width)
    : maze{ height, std::vector<MazeElement>{ width, MazeElement::GROUND } } {}

void MazeModel::setController(MazeController *controller_ptr)
{
  this->controller_ptr = controller_ptr;
}

void MazeModel::emptyMap()
{
  for (auto &row : maze)
    std::fill(row.begin(), row.end(), MazeElement::GROUND);
}

void MazeModel::resetMaze()
{
  for (int32_t y{}; y < MAZE_HEIGHT; ++y) {
    for (int32_t x{}; x < MAZE_WIDTH; ++x) {
      if (y == 0 || y == MAZE_HEIGHT - 1 || x == 0 || x == MAZE_WIDTH - 1)    // 上牆或下牆
        maze[y][x] = MazeElement::WALL;
      else if (x % 2 == 1 && y % 2 == 1)    // xy 都為奇數的點當作GROUND
        maze[y][x] = MazeElement::GROUND;
      else
        maze[y][x] = MazeElement::WALL;    // xy 其他的點當牆做切割點的動作
    }
  }

  controller_ptr->setFrameMaze(maze);
}

void MazeModel::resetWallAroundMaze()
{
  for (int32_t y = 0; y < MAZE_HEIGHT; ++y) {
    for (int32_t x = 0; x < MAZE_WIDTH; ++x) {
      if (x == 0 || x == MAZE_WIDTH - 1 || y == 0 || y == MAZE_HEIGHT - 1)
        maze[y][x] = MazeElement::WALL;    // Wall
      else
        maze[y][x] = MazeElement::GROUND;    // Ground
    }
  }
}

/* --------------------maze generation methods -------------------- */

void MazeModel::generateMazePrim()
{
  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());    // 產生亂數
  std::array<int32_t, 4> direction_order{ 0, 1, 2, 3 };
  std::vector<MazeNode> explored_cache;
  std::vector<MazeNode> candidate_list;    // 待找的牆的列表

  {
    MazeNode seed_node;
    setBeginPoint(seed_node);
    explored_cache.emplace_back(seed_node);

    std::shuffle(direction_order.begin(), direction_order.end(), gen);
    for (const int32_t index : direction_order) {
      const auto [dir_y, dir_x] = dir_vec[index];
      if (inMaze(seed_node, dir_y, dir_x))
        candidate_list.emplace_back(MazeNode{ seed_node.y + dir_y, seed_node.x + dir_x, maze[seed_node.y + dir_y][seed_node.x + dir_x] });    // 將起點四周在迷宮內的牆加入 candidate_list 列表中
    }
  }

  while (!candidate_list.empty()) {
    std::uniform_int_distribution<> wall_dis(0, candidate_list.size() - 1);
    int32_t random_index = wall_dis(gen);
    MazeNode current_node = candidate_list[random_index];    // pick one point out
    MazeElement up_element{ MazeElement::INVALID }, down_element{ MazeElement::INVALID }, left_element{ MazeElement::INVALID }, right_element{ MazeElement::INVALID };    // 目前這個牆的上下左右結點
    // 如果抽到的那格確定是牆再去判斷，有時候會有一個牆重複被加到清單裡的情形
    if (current_node.element == MazeElement::WALL) {
      if (inMaze(current_node, -1, 0))
        up_element = maze[current_node.y - 1][current_node.x];
      if (inMaze(current_node, 1, 0))
        down_element = maze[current_node.y + 1][current_node.x];
      if (inMaze(current_node, 0, -1))
        left_element = maze[current_node.y][current_node.x - 1];
      if (inMaze(current_node, 0, 1))
        right_element = maze[current_node.y][current_node.x + 1];

      // 如果左右都探索過了，或上下都探索過了，就把這個牆留著，並且加到確定是牆壁的 vector 裡
      if ((up_element == MazeElement::EXPLORED && down_element == MazeElement::EXPLORED) || (left_element == MazeElement::EXPLORED && right_element == MazeElement::EXPLORED)) {
        candidate_list.erase(candidate_list.begin() + random_index);    // 如果「上下都走過」或「左右都走過」，那麼就把這個牆留著
      }
      else {
        // 不然就把牆打通
        current_node.element = MazeElement::EXPLORED;
        maze[current_node.y][current_node.x] = MazeElement::EXPLORED;
        explored_cache.emplace_back(current_node);
        candidate_list.erase(candidate_list.begin() + random_index);

        controller_ptr->enFramequeue(current_node);

        if (up_element == MazeElement::EXPLORED && down_element == MazeElement::GROUND)    // 上面探索過，下面還沒
          ++current_node.y;    // 將目前的節點改成牆壁 "下面" 那個節點
        else if (up_element == MazeElement::GROUND && down_element == MazeElement::EXPLORED)    // 下面探索過，上面還沒
          --current_node.y;    // 將目前的節點改成牆壁 "上面" 那個節點
        else if (left_element == MazeElement::EXPLORED && right_element == MazeElement::GROUND)    // 左邊探索過，右邊還沒
          ++current_node.x;    // 將目前的節點改成牆壁 "右邊" 那個節點
        else if (left_element == MazeElement::GROUND && right_element == MazeElement::EXPLORED)    // 右邊探索過，左邊還沒
          --current_node.x;    // 將目前的節點改成牆壁 "左邊" 那個節點

        current_node.element = MazeElement::EXPLORED;
        maze[current_node.y][current_node.x] = MazeElement::EXPLORED;    // 將現在的節點(牆壁上下左右其中一個，看哪個方向符合條件) 改為 EXPLORED
        explored_cache.emplace_back(current_node);    // 將這個節點的座標記起來，等等要改回 GROUND
        std::shuffle(direction_order.begin(), direction_order.end(), gen);
        for (const int32_t index : direction_order) {    //(新的點的)上下左右遍歷
          const auto [dir_y, dir_x] = dir_vec[index];
          if (inMaze(current_node, dir_y, dir_x)) {    // 如果上(下左右)的牆在迷宮內
            if (maze[current_node.y + dir_y][current_node.x + dir_x] == MazeElement::WALL)    // 而且如果這個節點是牆
              candidate_list.emplace_back(MazeNode{ current_node.y + dir_y, current_node.x + dir_x, maze[current_node.y + dir_y][current_node.x + dir_x] });    // 就將這個節點加入wall列表中
          }
        }

        controller_ptr->enFramequeue(current_node);
      }
    }
  }

  controller_ptr->setModelComplete();
}    // end generateMazePrim()

void MazeModel::generateMazeRecursionBacktracker()
{
  struct TraceNode {
    MazeNode node;
    int8_t index = 0;
    std::array<uint8_t, 4> direction_order = { 0, 1, 2, 3 };
  };

  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::stack<TraceNode> explored_cache;    // 之後要改回道路的座標清單
  std::stack<TraceNode> candidate_list;

  {
    TraceNode seed_node;
    std::shuffle(seed_node.direction_order.begin(), seed_node.direction_order.end(), gen);
    setBeginPoint(seed_node.node);
    candidate_list.push(seed_node);
    explored_cache.push(seed_node);
  }

  while (!candidate_list.empty()) {
    TraceNode &current_node = candidate_list.top();
    if (current_node.index == 4) {
      candidate_list.pop();
      continue;
    }

    int8_t dir = current_node.direction_order[current_node.index++];
    const auto [dir_y, dir_x] = dir_vec[dir];

    if (!inMaze(current_node.node, 2 * dir_y, 2 * dir_x))
      continue;

    if (maze[current_node.node.y + 2 * dir_y][current_node.node.x + 2 * dir_x] == MazeElement::GROUND) {
      TraceNode target_node{ { current_node.node.y + 2 * dir_y, current_node.node.x + 2 * dir_x, MazeElement::GROUND }, 0, { 0, 1, 2, 3 } };
      std::shuffle(target_node.direction_order.begin(), target_node.direction_order.end(), gen);

      current_node.node.element = MazeElement::EXPLORED;
      maze[current_node.node.y + dir_y][current_node.node.x + dir_x] = MazeElement::EXPLORED;
      controller_ptr->enFramequeue(MazeNode{ current_node.node.y + dir_y, current_node.node.x + dir_x, MazeElement::EXPLORED });
      explored_cache.emplace(TraceNode{ { current_node.node.y + dir_y, current_node.node.x + dir_x, MazeElement::EXPLORED }, 0, { 0, 1, 2, 3 } });

      target_node.node.element = MazeElement::EXPLORED;
      maze[target_node.node.y][target_node.node.x] = MazeElement::EXPLORED;
      controller_ptr->enFramequeue(target_node.node);
      explored_cache.push(target_node);

      candidate_list.push(target_node);
    }
  }

  controller_ptr->setModelComplete();
}    // end generateMazeRecursionBacktracker()

void MazeModel::generateMazeRecursionDivision(const int32_t uy, const int32_t lx, const int32_t dy, const int32_t rx)
{
  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());    // 產生亂數
  int32_t width = rx - lx + 1, height = dy - uy + 1;
  if (width < 2 && height < 2) return;
  if (!inMaze(MazeNode{ uy, lx, MazeElement::INVALID }, height - 1, width - 1))
    return;

  bool is_horizontal = (width <= height) ? true : false;
  int32_t wall_index;
  if (is_horizontal && height - 2 > 0) {
    std::uniform_int_distribution<> h_dis(uy + 1, uy + height - 2);
    wall_index = h_dis(gen);
    for (int32_t i = lx; i <= rx; ++i) maze[wall_index][i] = MazeElement::WALL;    // 將這段距離都設圍牆壁

    generateMazeRecursionDivision(uy, lx, wall_index - 1, rx);    // 上面
    generateMazeRecursionDivision(wall_index + 1, lx, dy, rx);    // 下面
  }
  else if (!is_horizontal && width - 2 > 0) {
    std::uniform_int_distribution<> w_dis(lx + 1, lx + width - 2);
    wall_index = w_dis(gen);
    for (int32_t i = uy; i <= dy; ++i) maze[i][wall_index] = MazeElement::WALL;    // 將這段距離都設圍牆壁

    generateMazeRecursionDivision(uy, lx, dy, wall_index - 1);    // 左邊
    generateMazeRecursionDivision(uy, wall_index + 1, dy, rx);    // 右邊
  }
  else
    return;

  int32_t path_index;
  if (is_horizontal) {
    while (true) {
      std::uniform_int_distribution<> w_dis(lx, lx + width - 1);
      path_index = w_dis(gen);
      // if (maze[wall_index - 1][path_index] + maze[wall_index + 1][path_index] + maze[wall_index][path_index - 1] + maze[wall_index][path_index + 1] <= 2 * MazeElement::WALL) break;
    }
    maze[wall_index][path_index] = MazeElement::GROUND;
  }
  else {
    while (true) {
      std::uniform_int_distribution<> h_dis(uy, uy + height - 1);
      path_index = h_dis(gen);
      // if (maze[path_index - 1][wall_index] + maze[path_index + 1][wall_index] + maze[path_index][wall_index - 1] + maze[path_index][wall_index + 1] <= 2 * MazeElement::WALL) break;
    }
    maze[path_index][wall_index] = MazeElement::GROUND;
  }
}    // end generateMazeRecursionDivision()

/* --------------------maze solving methods -------------------- */

bool MazeModel::solveMazeDFS(const int32_t y, const int32_t x)
{
  maze[1][0] = MazeElement::BEGIN;    // 起點
  maze[y][x] = MazeElement::EXPLORED;    // 探索過的點

  if (y == END_Y && x == END_X) {    // 如果到終點了就回傳True
    maze[y][x] = MazeElement::END;    // 終點

    return true;
  }
  for (const auto &[dir_y, dir_x] : dir_vec) {    // 上下左右
    const int32_t temp_y = y + dir_y, temp_x = x + dir_x;
    if (is_in_maze(temp_y, temp_x)) {    // 如果這個節點在迷宮內
      if (maze[temp_y][temp_x] == MazeElement::GROUND)    // 而且如果這個節點還沒被探索過
        if (solveMazeDFS(temp_y, temp_x))    // 就繼續遞迴，如果已經找到目標就會回傳 true ，所以這裡放在 if 裡面
          return true;
    }
  }
  return false;
}    // end solveMazeDFS()

void MazeModel::solveMazeBFS()
{
  std::queue<std::pair<int32_t, int32_t>> result;    // 存節點的 qeque
  result.push(std::make_pair(BEGIN_Y, BEGIN_X));    // 將一開始的節點加入 qeque
  maze[BEGIN_Y][BEGIN_X] = MazeElement::BEGIN;    // 起點


  while (!result.empty()) {
    const auto [temp_y, temp_x]{ result.front() };    // 目前的節點
    result.pop();    // 將目前的節點拿出來

    for (const auto &dir : dir_vec) {    // 遍歷上下左右
      const int32_t y = temp_y + dir.first, x = temp_x + dir.second;    // 上下左右的節點

      if (is_in_maze(y, x)) {    // 如果這個節點在迷宮內
        if (maze[y][x] == MazeElement::GROUND) {    // 而且如果這個節點還沒被探索過，也不是牆壁
          maze[y][x] = MazeElement::EXPLORED;    // 那就探索他，改 EXPLORED

          if (y == END_Y && x == END_X) {    // 找到終點就return
            maze[y][x] = MazeElement::END;    // 終點

            return;
          }
          else
            result.push(std::make_pair(y, x));    // 沒找到節點就加入節點
        }
      }
    }
  }    // end while
}    // end solveMazeBFS()

void MazeModel::solveMazeUCS(const MazeAction actions)
{
  struct Node {
    int32_t __Weight;    // 權重 (Cost Function)
    int32_t y;    // y座標
    int32_t x;    // x座標
    Node(int32_t weight, int32_t y, int32_t x) : __Weight(weight), y(y), x(x) {}
    bool operator>(const Node &other) const { return __Weight > other.__Weight; }    // priority比大小只看權重
    bool operator<(const Node &other) const { return __Weight < other.__Weight; }    // priority比大小只看權重
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> result;    // 待走的結點，greater代表小的會在前面，由小排到大
  int32_t weight{};    // 用來計算的權重

  switch (actions) {    // 起點
  case MazeAction::S_UCS_MANHATTAN:
    weight = abs(END_X - BEGIN_X) + abs(END_Y - BEGIN_Y);    // 權重為曼哈頓距離
    break;
  case MazeAction::S_UCS_TWO_NORM:
    weight = pow_two_norm(BEGIN_Y, BEGIN_X);    // 權重為 two_norm
    break;
  case MazeAction::S_UCS_INTERVAL:
    constexpr int32_t interval_y = MAZE_HEIGHT / 10, interval_x = MAZE_WIDTH / 10;    // 分 10 個區間
    weight = (static_cast<int32_t>(BEGIN_Y / interval_y) < static_cast<int32_t>(BEGIN_X / interval_x)) ? (10 - static_cast<int32_t>(BEGIN_Y / interval_y)) : (10 - static_cast<int32_t>(BEGIN_X / interval_x));    // 權重以區間計算，兩個相除是看它在第幾個區間，然後用總區間數減掉，代表它的基礎權重，再乘以1000
    break;
  }

  result.push(Node(weight, BEGIN_Y, BEGIN_Y));    // 將起點加進去

  while (true) {
    if (result.empty())
      return;    // 沒找到目標

    const auto temp = result.top();    // 目前最優先的結點
    result.pop();    // 取出結點判斷

    if (temp.y == END_Y && temp.x == END_X) {
      maze[temp.y][temp.x] = MazeElement::END;    // 終點

      return;    // 如果取出的點是終點就return
    }
    else if (maze[temp.y][temp.x] == MazeElement::GROUND) {
      if (temp.y == BEGIN_Y && temp.x == BEGIN_X)
        maze[temp.y][temp.x] = MazeElement::BEGIN;    // 起點
      else {
        maze[temp.y][temp.x] = MazeElement::EXPLORED;    // 探索過的點要改EXPLORED
      }

      for (const auto &dir : dir_vec) {
        const int32_t y = temp.y + dir.first, x = temp.x + dir.second;

        if (is_in_maze(y, x)) {
          if (maze[y][x] == MazeElement::GROUND) {    // 如果這個結點還沒走過，就把他加到待走的結點裡
            switch (actions) {
            case MazeAction::S_UCS_MANHATTAN:
              weight = abs(END_X - x) + abs(END_Y - y);    // 權重為曼哈頓距離
              break;
            case MazeAction::S_UCS_TWO_NORM:
              weight = pow_two_norm(y, x);    // 權重為 Two_Norm
              break;
            case MazeAction::S_UCS_INTERVAL:
              constexpr int32_t interval_y = MAZE_HEIGHT / 10, interval_x = MAZE_WIDTH / 10;    // 分 10 個區間
              weight = (static_cast<int32_t>(y / interval_y) < static_cast<int32_t>(x / interval_x)) ? (10 - static_cast<int32_t>(y / interval_y)) : (10 - static_cast<int32_t>(x / interval_x));    // 權重為區間
              break;
            }
            result.push(Node(temp.__Weight + weight, y, x));    // 加入節點
          }
        }
      }    // end for
    }
  }    // end while
}    // end solveMazeUCS()

void MazeModel::solveMazeGreedy()
{
  struct Node {
    int32_t __Weight;    // 權重為 Two_Norm 平方 (Heuristic function)
    int32_t y;    // y座標
    int32_t x;    // x座標
    Node(int32_t weight, int32_t y, int32_t x) : __Weight(weight), y(y), x(x) {}
    bool operator>(const Node &other) const { return __Weight > other.__Weight; }    // priority比大小只看權重
    bool operator<(const Node &other) const { return __Weight < other.__Weight; }    // priority比大小只看權重
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> result;    // 待走的結點，greater代表小的會在前面，由小排到大
  result.push(Node(pow_two_norm(BEGIN_Y, BEGIN_X), BEGIN_Y, BEGIN_X));    // 將起點加進去

  while (true) {
    if (result.empty())
      return;    // 沒找到目標
    const auto temp = result.top();    // 目前最優先的結點
    result.pop();    // 取出結點判斷

    if (temp.y == END_Y && temp.x == END_X) {
      maze[temp.y][temp.x] = MazeElement::END;    // 終點

      return;    // 如果取出的點是終點就return
    }
    else if (maze[temp.y][temp.x] == MazeElement::GROUND) {
      if (temp.y == BEGIN_Y && temp.x == BEGIN_X)
        maze[temp.y][temp.x] = MazeElement::BEGIN;    // 起點
      else {
        maze[temp.y][temp.x] = MazeElement::EXPLORED;    // 探索過的點要改EXPLORED
      }

      for (const auto &dir : dir_vec) {
        const int32_t y = temp.y + dir.first, x = temp.x + dir.second;

        if (is_in_maze(y, x)) {
          if (maze[y][x] == MazeElement::GROUND)    // 如果這個結點還沒走過，就把他加到待走的結點裡
            result.push(Node(pow_two_norm(y, x), y, x));
        }
      }
    }
  }    // end while
}    // end solveMazeGreedy()

void MazeModel::solveMazeAStar(const MazeAction actions)
{
  enum class Types : int32_t {
    Normal = 0,    // Cost Function 為 50
    Interval = 1,    // Cost Function 以區間來計算，每一個區間 Cost 差10，距離終點越遠 Cost 越大
  };

  struct Node {
    int32_t __Cost;    // Cost Function 有兩種，以區間計算，每個區間 Cost 差10
    int32_t __Weight;    // 權重以區間(Cost Function) + Two_Norm 平方(Heuristic Function) 計算，每個區間 Cost 差1000
    int32_t y;    // y座標
    int32_t x;    // x座標
    Node(int32_t cost, int32_t weight, int32_t y, int32_t x) : __Cost(cost), __Weight(weight), y(y), x(x) {}
    bool operator>(const Node &other) const { return __Weight > other.__Weight; }    // priority比大小只看權重
    bool operator<(const Node &other) const { return __Weight < other.__Weight; }    // priority比大小只看權重
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> result;    // 待走的結點，greater代表小的會在前面，由小排到大
  constexpr int32_t interval_y = MAZE_HEIGHT / 10, interval_x = MAZE_WIDTH / 10;    // 分 10 個區間
  int32_t cost{}, weight{};

  if (actions == MazeAction::S_ASTAR_INTERVAL) {
    cost = 50;
    weight = cost + abs(END_X - BEGIN_X) + abs(END_Y - BEGIN_Y);
  }
  else if (actions == MazeAction::S_ASTAR_INTERVAL) {
    cost = (static_cast<int32_t>(BEGIN_Y / interval_y) < static_cast<int32_t>(BEGIN_X / interval_x)) ? (10 - static_cast<int32_t>(BEGIN_Y / interval_y)) * 8 : (10 - static_cast<int32_t>(BEGIN_X / interval_x)) * 8;    // Cost 以區間計算，兩個相除是看它在第幾個區間，然後用總區間數減掉，代表它的基礎 Cost，再乘以8
    weight = cost + pow_two_norm(BEGIN_Y, BEGIN_X);    // 權重以區間(Cost) + Two_Norm 計算
  }
  result.push(Node(cost, weight, BEGIN_Y, BEGIN_X));    // 將起點加進去

  while (true) {
    if (result.empty())
      return;    // 沒找到目標
    const auto temp = result.top();    // 目前最優先的結點
    result.pop();    // 取出結點

    if (temp.y == END_Y && temp.x == END_X) {
      maze[temp.y][temp.x] = MazeElement::END;    // 終點

      return;    // 如果取出的點是終點就return
    }
    else if (maze[temp.y][temp.x] == MazeElement::GROUND) {
      if (temp.y == BEGIN_Y && temp.x == BEGIN_X)
        maze[temp.y][temp.x] = MazeElement::BEGIN;    // 起點
      else {
        maze[temp.y][temp.x] = MazeElement::EXPLORED;    // 探索過的點要改EXPLORED
      }

      for (const auto &dir : dir_vec) {
        const int32_t y = temp.y + dir.first, x = temp.x + dir.second;

        if (is_in_maze(y, x)) {
          if (maze[y][x] == MazeElement::GROUND) {    // 如果這個結點還沒走過，就把他加到待走的結點裡
            if (actions == MazeAction::S_ASTAR_INTERVAL) {
              cost = 50;    // cost function設為常數 50
              weight = cost + abs(END_X - x) + abs(END_Y - y);    // heuristic function 設為曼哈頓距離
            }
            else if (actions == MazeAction::S_ASTAR_INTERVAL) {
              cost = (static_cast<int32_t>(y / interval_y) < static_cast<int32_t>(x / interval_x)) ? temp.__Cost + (10 - static_cast<int32_t>(y / interval_y)) * 8 : temp.__Cost + (10 - static_cast<int32_t>(x / interval_x)) * 8;    // Cost 以區間計算，兩個相除是看它在第幾個區間，然後用總區間數減掉，代表它的基礎 Cost，再乘以8
              weight = cost + pow_two_norm(y, x);    // heuristic function 設為 two_norm 平方
            }
            result.push(Node(cost, weight, y, x));
          }
        }
      }
    }
  }    // end while
}    // end solveMazeAStar()

/* -------------------- private utility function --------------------   */

void MazeModel::setFlag()
{
  maze[BEGIN_Y][BEGIN_X] = MazeElement::BEGIN;
  maze[END_Y][END_X] = MazeElement::END;
  controller_ptr->enFramequeue(MazeNode{ BEGIN_Y, BEGIN_X, MazeElement::BEGIN });
  controller_ptr->enFramequeue(MazeNode{ END_Y, END_X, MazeElement::END });
  controller_ptr->enFramequeue(MazeNode{ -1, -1, MazeElement::INVALID });
}

bool MazeModel::inMaze(const MazeNode &node, const int32_t delta_y, const int32_t delta_x)
{
  return (node.y + delta_y < MAZE_HEIGHT - 1) && (node.x + delta_x < MAZE_WIDTH - 1) && (node.y + delta_y > 0) && (node.x + delta_x > 0);    // 下牆、右牆、上牆、左牆
}

/**
 * @brief generate a random begin point for maze generation algorithm
 *
 * @param seed_y
 * @param seed_x
 */
void MazeModel::setBeginPoint(MazeNode &node)
{
  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<> y_dis(0, (MAZE_HEIGHT - 3) / 2);
  std::uniform_int_distribution<> x_dis(0, (MAZE_WIDTH - 3) / 2);

  node.y = 2 * y_dis(gen) + 1;
  node.x = 2 * x_dis(gen) + 1;
  node.element = MazeElement::EXPLORED;
  maze[node.y][node.x] = MazeElement::EXPLORED;    // Set the randomly chosen point as the generation start point

  controller_ptr->enFramequeue(node);
}    // end setBeginPoint

bool MazeModel::is_in_maze(const int32_t y, const int32_t x)
{
  return (y < MAZE_HEIGHT) && (x < MAZE_WIDTH) && (y >= 0) && (x >= 0);
}

int32_t MazeModel::pow_two_norm(const int32_t y, const int32_t x)
{
  return pow((END_Y - y), 2) + pow((END_X - x), 2);
}
