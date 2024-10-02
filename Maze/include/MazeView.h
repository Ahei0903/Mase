#ifndef MAZEVIEW_H
#define MAZEVIEW_H

/**
 * @file MazeView.h
 * @author Mes (mes900903@gmail.com)
 * @brief Display the maze
 * @version 0.1
 * @date 2024-09-22
 */

#include "MazeController.h"
#include "MazeNode.h"
#include "ThreadSafeQueue.h"
#include "imgui_impl_glfw.h"

class MazeController;
struct MazeNode;

class MazeView {
public:
  MazeView(uint32_t height, uint32_t width);
  void setController(MazeController *controller_ptr);

  void render(GLFWwindow *);
  void renderGUI();
  void setFrameMaze(const std::vector<std::vector<MazeElement>> &maze);
  void enFramequeue(const MazeNode &node);

private:
  std::vector<std::vector<MazeElement>> render_maze;
  MazeController *controller_ptr;
  ThreadSafeQueue<MazeNode> MazeDiffQueue;
  MazeNode update_node;
  bool stop_flag;
  std::mutex maze_mutex;

private:
  void deFramequeue();
  void renderMaze();
};

#endif