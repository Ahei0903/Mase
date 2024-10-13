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

#include <iostream>

MazeModel::MazeModel(uint32_t height, uint32_t width)
    : maze{ height, std::vector<MazeElement>{ width, MazeElement::GROUND } } {}

void MazeModel::setController(MazeController *controller_ptr)
{
  this->controller_ptr__ = controller_ptr;
}

void MazeModel::emptyMap()
{
  for (auto &row : maze)
    std::fill(row.begin(), row.end(), MazeElement::GROUND);

  controller_ptr__->enFramequeue(maze);
  setFlag__();
}

void MazeModel::cleanExplorer()
{
  for (auto &row : maze) {
    std::replace(row.begin(), row.end(), MazeElement::EXPLORED, MazeElement::GROUND);
    std::replace(row.begin(), row.end(), MazeElement::ANSWER, MazeElement::GROUND);
  }

  controller_ptr__->enFramequeue(maze);
  setFlag__();
}

void MazeModel::initMaze__()
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

  controller_ptr__->enFramequeue(maze);
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

  controller_ptr__->enFramequeue(maze);
}

/* --------------------maze generation methods -------------------- */

void MazeModel::generateMazePrim()
{
  initMaze__();

  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());    // 產生亂數
  std::array<int32_t, 4> direction_order{ 0, 1, 2, 3 };
  std::vector<MazeNode> candidate_list;    // 待找的牆的列表

  {
    MazeNode seed_node;
    setBeginPoint__(seed_node);

    std::shuffle(direction_order.begin(), direction_order.end(), gen);
    for (const int32_t index : direction_order) {
      const auto [dir_y, dir_x] = dir_vec[index];
      if (inWall__(seed_node.y + dir_y, seed_node.x + dir_x))
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
      if (inWall__(current_node.y - 1, current_node.x))
        up_element = maze[current_node.y - 1][current_node.x];
      if (inWall__(current_node.y + 1, current_node.x))
        down_element = maze[current_node.y + 1][current_node.x];
      if (inWall__(current_node.y, current_node.x - 1))
        left_element = maze[current_node.y][current_node.x - 1];
      if (inWall__(current_node.y, current_node.x + 1))
        right_element = maze[current_node.y][current_node.x + 1];

      // 如果左右都探索過了，或上下都探索過了，就把這個牆留著，並且加到確定是牆壁的 vector 裡
      if ((up_element == MazeElement::EXPLORED && down_element == MazeElement::EXPLORED) || (left_element == MazeElement::EXPLORED && right_element == MazeElement::EXPLORED)) {
        candidate_list.erase(candidate_list.begin() + random_index);    // 如果「上下都走過」或「左右都走過」，那麼就把這個牆留著
      }
      else {
        // 不然就把牆打通
        current_node.element = MazeElement::EXPLORED;
        maze[current_node.y][current_node.x] = MazeElement::EXPLORED;
        candidate_list.erase(candidate_list.begin() + random_index);

        controller_ptr__->enFramequeue(maze);

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
        std::shuffle(direction_order.begin(), direction_order.end(), gen);
        for (const int32_t index : direction_order) {    //(新的點的)上下左右遍歷
          const auto [dir_y, dir_x] = dir_vec[index];
          if (inWall__(current_node.y + dir_y, current_node.x + dir_x)) {    // 如果上(下左右)的牆在迷宮內
            if (maze[current_node.y + dir_y][current_node.x + dir_x] == MazeElement::WALL)    // 而且如果這個節點是牆
              candidate_list.emplace_back(MazeNode{ current_node.y + dir_y, current_node.x + dir_x, maze[current_node.y + dir_y][current_node.x + dir_x] });    // 就將這個節點加入wall列表中
          }
        }

        controller_ptr__->enFramequeue(maze);
      }
    }
  }

  setFlag__();
  cleanExplorer();
  controller_ptr__->setModelComplete();
}    // end generateMazePrim()

void MazeModel::generateMazeRecursionBacktracker()
{
  initMaze__();

  struct TraceNode {
    MazeNode node;
    int8_t index = 0;
    std::array<uint8_t, 4> direction_order = { 0, 1, 2, 3 };
  };

  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::stack<TraceNode> candidate_list;

  {
    TraceNode seed_node;
    std::shuffle(seed_node.direction_order.begin(), seed_node.direction_order.end(), gen);
    setBeginPoint__(seed_node.node);
    candidate_list.push(seed_node);
  }

  while (!candidate_list.empty()) {
    TraceNode &current_node = candidate_list.top();
    if (current_node.index == 4) {
      candidate_list.pop();
      continue;
    }

    int8_t dir = current_node.direction_order[current_node.index++];
    const auto [dir_y, dir_x] = dir_vec[dir];

    if (!inWall__(current_node.node.y + 2 * dir_y, current_node.node.x + 2 * dir_x))
      continue;

    if (maze[current_node.node.y + 2 * dir_y][current_node.node.x + 2 * dir_x] == MazeElement::GROUND) {
      TraceNode target_node{ { current_node.node.y + 2 * dir_y, current_node.node.x + 2 * dir_x, MazeElement::GROUND }, 0, { 0, 1, 2, 3 } };
      std::shuffle(target_node.direction_order.begin(), target_node.direction_order.end(), gen);

      current_node.node.element = MazeElement::EXPLORED;
      maze[current_node.node.y + dir_y][current_node.node.x + dir_x] = MazeElement::EXPLORED;
      controller_ptr__->enFramequeue(maze);

      target_node.node.element = MazeElement::EXPLORED;
      maze[target_node.node.y][target_node.node.x] = MazeElement::EXPLORED;
      controller_ptr__->enFramequeue(maze);

      candidate_list.push(target_node);
    }
  }

  setFlag__();
  cleanExplorer();
  controller_ptr__->setModelComplete();
}    // end generateMazeRecursionBacktracker()

void MazeModel::generateMazeRecursionDivision(const int32_t uy, const int32_t lx, const int32_t dy, const int32_t rx, bool is_first_call)
{
  if (is_first_call)
    resetWallAroundMaze();

  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());    // 產生亂數
  int32_t width = rx - lx + 1, height = dy - uy + 1;
  if (width < 3 || height < 3) return;

  const bool cut_horizontal = (height > width);
  int32_t wall_index;
  if (cut_horizontal) {
    std::uniform_int_distribution<> h_dis(1, (height - 1) / 2);
    wall_index = uy + 2 * h_dis(gen) - 1;

    for (int32_t i = lx; i <= rx; ++i) {
      maze[wall_index][i] = MazeElement::WALL;    // 將這段距離都設圍牆壁
      controller_ptr__->enFramequeue(maze);
    }

    std::uniform_int_distribution<> w_dis(1, (width - 1) / 2);
    int32_t break_point = lx + 2 * w_dis(gen);
    maze[wall_index][break_point] = MazeElement::GROUND;
    controller_ptr__->enFramequeue(maze);

    generateMazeRecursionDivision(uy, lx, wall_index - 1, rx);    // 上面
    generateMazeRecursionDivision(wall_index + 1, lx, dy, rx);    // 下面
  }
  else {
    std::uniform_int_distribution<> w_dis(1, (width - 1) / 2);
    wall_index = lx + 2 * w_dis(gen) - 1;

    for (int32_t i = uy; i <= dy; ++i) {
      maze[i][wall_index] = MazeElement::WALL;    // 將這段距離都設圍牆壁
      controller_ptr__->enFramequeue(maze);
    }

    std::uniform_int_distribution<> h_dis(1, (height - 1) / 2);
    int32_t break_point = uy + 2 * h_dis(gen);
    maze[break_point][wall_index] = MazeElement::GROUND;
    controller_ptr__->enFramequeue(maze);

    generateMazeRecursionDivision(uy, lx, dy, wall_index - 1);    // 左邊
    generateMazeRecursionDivision(uy, wall_index + 1, dy, rx);    // 右邊
  }

  if (is_first_call) {
    setFlag__();
    cleanExplorer();
    controller_ptr__->setModelComplete();
  }
}    // end generateMazeRecursionDivision()

/* --------------------maze solving methods -------------------- */

bool MazeModel::solveMazeDFS(const int32_t y, const int32_t x, bool is_first_call)
{
  if (maze[y][x] == MazeElement::END)    // 如果到終點了就回傳True
    return true;

  if (is_first_call) {
    cleanExplorer();
  }
  else {
    maze[y][x] = MazeElement::EXPLORED;    // 探索過的點
    controller_ptr__->enFramequeue(maze);
  }

  for (const auto &[dir_y, dir_x] : dir_vec) {    // 上下左右
    const int32_t target_y = y + dir_y;
    const int32_t target_x = x + dir_x;

    if (!inMaze__(target_y, target_x))
      continue;

    if (maze[target_y][target_x] != MazeElement::GROUND && maze[target_y][target_x] != MazeElement::END)    // 而且如果這個節點還沒被探索過
      continue;

    if (solveMazeDFS(target_y, target_x)) {    // 就繼續遞迴，如果已經找到目標就會回傳 true ，所以這裡放在 if 裡面
      if (maze[target_y][target_x] != MazeElement::END) {
        maze[target_y][target_x] = MazeElement::ANSWER;
        controller_ptr__->enFramequeue(maze);
      }

      if (is_first_call)
        controller_ptr__->setModelComplete();

      return true;
    }
  }

  if (is_first_call)
    controller_ptr__->setModelComplete();

  return false;
}    // end solveMazeDFS()

void MazeModel::solveMazeBFS()
{
  cleanExplorer();

  std::vector<std::vector<MazeNode>> parent(MAZE_HEIGHT, std::vector<MazeNode>(MAZE_WIDTH, { -1, -1, MazeElement::INVALID }));
  std::queue<std::pair<int32_t, int32_t>> path;    // 存節點的 qeque
  path.push(std::make_pair(BEGIN_Y, BEGIN_X));    // 將一開始的節點加入 qeque

  while (!path.empty()) {
    const auto [current_y, current_x] = std::move(path.front());    // 目前的節點
    path.pop();    // 將目前的節點拿出來

    for (const auto &dir : dir_vec) {    // 遍歷上下左右
      const int32_t target_y = current_y + dir.first;
      const int32_t target_x = current_x + dir.second;

      if (!inMaze__(target_y, target_x))
        continue;

      if (maze[target_y][target_x] == MazeElement::END) {
        int32_t ans_y = current_y;
        int32_t ans_x = current_x;

        while (ans_y != BEGIN_Y || ans_x != BEGIN_X) {
          maze[ans_y][ans_x] = MazeElement::ANSWER;
          controller_ptr__->enFramequeue(maze);
          const auto [parent_y, parent_x, _] = parent[ans_y][ans_x];
          ans_y = parent_y;
          ans_x = parent_x;
        }

        controller_ptr__->setModelComplete();
        return;
      }

      if (maze[target_y][target_x] == MazeElement::GROUND) {    // 而且如果這個節點還沒被探索過，也不是牆壁
        maze[target_y][target_x] = MazeElement::EXPLORED;    // 那就探索他，改 EXPLORED
        controller_ptr__->enFramequeue(maze);

        parent[target_y][target_x] = { current_y, current_x, MazeElement::EXPLORED };
        path.push(std::make_pair(target_y, target_x));    // 沒找到節點就加入節點
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

        if (inMaze__(y, x)) {
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

        if (inMaze__(y, x)) {
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

        if (inMaze__(y, x)) {
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

void MazeModel::setFlag__()
{
  maze[BEGIN_Y][BEGIN_X] = MazeElement::BEGIN;
  maze[END_Y][END_X] = MazeElement::END;

  controller_ptr__->enFramequeue(maze);
}

bool MazeModel::inWall__(const int32_t y, const int32_t x)
{
  return (y < MAZE_HEIGHT - 1) && (x < MAZE_WIDTH - 1) && (y > 0) && (x > 0);    // 下牆、右牆、上牆、左牆
}

bool MazeModel::inMaze__(const int32_t y, const int32_t x)
{
  return (y < MAZE_HEIGHT) && (x < MAZE_WIDTH) && (y > -1) && (x > -1);
}

/**
 * @brief generate a random begin point for maze generation algorithm
 *
 * @param seed_y
 * @param seed_x
 */
void MazeModel::setBeginPoint__(MazeNode &node)
{
  std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<> y_dis(0, (MAZE_HEIGHT - 3) / 2);
  std::uniform_int_distribution<> x_dis(0, (MAZE_WIDTH - 3) / 2);

  node.y = 2 * y_dis(gen) + 1;
  node.x = 2 * x_dis(gen) + 1;
  node.element = MazeElement::EXPLORED;
  maze[node.y][node.x] = MazeElement::EXPLORED;    // Set the randomly chosen point as the generation start point

  controller_ptr__->enFramequeue(maze);
}    // end setBeginPoint__

int32_t MazeModel::pow_two_norm(const int32_t y, const int32_t x)
{
  return pow((END_Y - y), 2) + pow((END_X - x), 2);
}
