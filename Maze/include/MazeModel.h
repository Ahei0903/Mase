#ifndef MAZEMODEL_H
#define MAZEMODEL_H

/**
 * @file MazeModel.h
 * @author Mes (mes900903@gmail.com)
 * @brief The model of the maze, build and store the maze
 * @version 0.1
 * @date 2024-09-22
 */

#include "MazeNode.h"
#include "MazeController.h"

#include <vector>
#include <utility>
#include <cstdint>

inline constexpr int32_t MAZE_HEIGHT = 39;
inline constexpr int32_t MAZE_WIDTH = 75;
inline constexpr int32_t BEGIN_Y = 1;
inline constexpr int32_t BEGIN_X = 0;
inline constexpr int32_t END_Y = MAZE_HEIGHT - 2;
inline constexpr int32_t END_X = MAZE_WIDTH - 1;
inline constexpr int32_t GRID_SIZE = 15;
inline constexpr std::pair<int32_t, int32_t> dir_vec[4]{ { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

enum class MazeAction : int32_t {
  G_CLEANALL,
  G_CLEAN_EXPLORED,
  G_PRIM,
  G_PRIM_BREAK,
  G_RECURSION_BACKTRACKER,
  G_RECURSION_DIVISION,
  S_DFS,
  S_BFS,
  S_UCS_MANHATTAN,    // Cost Function 為 Two_Norm，所以距離終點越遠 Cost 越大
  S_UCS_TWO_NORM,    // Cost Function 為 Two_Norm，所以距離終點越遠 Cost 越大
  S_GREEDY_MANHATTAN,
  S_GREEDY_TWO_NORM,
  S_ASTAR,
  S_ASTAR_INTERVAL
};

class MazeController;

class MazeModel {
public:
  MazeModel(uint32_t height, uint32_t width);
  void setController(MazeController *controller_ptr);

  void emptyMap();
  void cleanExplored();
  void resetWallAroundMaze();

  // maze generation and solving methods
  void generateMazePrim(const MazeAction actions);
  void generateMazeRecursionBacktracker();
  void generateMazeRecursionDivision(const int32_t uy, const int32_t lx, const int32_t dy, const int32_t rx, bool is_first_call = false);

  bool solveMazeDFS(const int32_t y, const int32_t x, bool is_first_call = false);
  void solveMazeBFS();
  void solveMazeUCS(const MazeAction actions);
  void solveMazeGreedy(const MazeAction actions);
  void solveMazeAStar(const MazeAction actions);

public:
  std::vector<std::vector<MazeElement>> maze;

private:
  MazeController *controller_ptr__;

private:
  bool inWall__(const int32_t y, const int32_t x);
  bool inMaze__(const int32_t y, const int32_t x);
  void initMaze__();
  void setFlag__();
  void setBeginPoint__(MazeNode &node);

  int32_t twoNorm__(const int32_t y, const int32_t x);
};

#endif