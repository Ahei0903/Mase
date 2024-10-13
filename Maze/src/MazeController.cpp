#include "MazeController.h"
#include "MazeModel.h"
#include "MazeView.h"

#include <thread>
#include <iostream>

void MazeController::setModelView(MazeModel *model_ptr, MazeView *view_ptr)
{
  this->model_ptr__ = model_ptr;
  this->view_ptr__ = view_ptr;

  this->model_ptr__->setController(this);
  this->view_ptr__->setController(this);
}

void MazeController::handleInput(const MazeAction actions)
{
  if (!isModelComplete() || !isViewComplete())
    return;

  model_complete_flag__.store(false);
  view_complete_flag__.store(false);

  std::thread t1;
  switch (actions) {
  case MazeAction::G_CLEANALL:
    model_ptr__->emptyMap();
    setModelComplete();
    break;
  case MazeAction::G_CLEAN_EXPLORED:
    model_ptr__->cleanExplored();
    setModelComplete();
    break;
  case MazeAction::G_PRIM:
  case MazeAction::G_PRIM_BREAK:
    t1 = std::thread(&MazeModel::generateMazePrim, model_ptr__, actions);
    t1.detach();
    break;
  case MazeAction::G_RECURSION_BACKTRACKER:
    t1 = std::thread(&MazeModel::generateMazeRecursionBacktracker, model_ptr__);
    t1.detach();
    break;
  case MazeAction::G_RECURSION_DIVISION:
    t1 = std::thread(&MazeModel::generateMazeRecursionDivision, model_ptr__, 1, 1, MAZE_HEIGHT - 2, MAZE_WIDTH - 2, true);
    t1.detach();
    break;
  case MazeAction::S_DFS:
    t1 = std::thread(&MazeModel::solveMazeDFS, model_ptr__, BEGIN_Y, BEGIN_X, true);
    t1.detach();
    break;
  case MazeAction::S_BFS:
    t1 = std::thread(&MazeModel::solveMazeBFS, model_ptr__);
    t1.detach();
    break;
  case MazeAction::S_UCS_MANHATTAN:
  case MazeAction::S_UCS_TWO_NORM:
    t1 = std::thread(&MazeModel::solveMazeUCS, model_ptr__, actions);
    t1.detach();
    break;
  case MazeAction::S_GREEDY_MANHATTAN:
  case MazeAction::S_GREEDY_TWO_NORM:
    t1 = std::thread(&MazeModel::solveMazeGreedy, model_ptr__, actions);
    t1.detach();
    break;
  case MazeAction::S_ASTAR_MANHATTAN:
  case MazeAction::S_ASTAR_INTERVAL:
    // model_ptr__->solveMazeAStar(actions);
    setModelComplete();
    break;
  default:
    std::clog << "invalid action" << std::endl;
    break;
  }
}

void MazeController::enFramequeue(const std::vector<std::vector<MazeElement>> &maze, const MazeNode &node)
{
  view_ptr__->enFramequeue(maze, node);
}

void MazeController::setModelComplete()
{
  view_ptr__->resetUpdateNode();
  model_complete_flag__.store(true);
}
bool MazeController::isModelComplete() const
{
  return model_complete_flag__.load();
}

void MazeController::setViewComplete()
{
  view_complete_flag__.store(true);
}
bool MazeController::isViewComplete() const
{
  return view_complete_flag__.load();
}