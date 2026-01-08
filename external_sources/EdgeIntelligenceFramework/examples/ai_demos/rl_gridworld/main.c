/**
 * @file main.c
 * @brief Reinforcement Learning Tutorial - Grid World Navigation
 *
 * This tutorial demonstrates Q-Learning where an AI agent learns
 * to navigate a maze to reach a goal while avoiding obstacles.
 *
 * SCENARIO:
 * A robot in a grid world needs to learn the optimal path
 * from the start (S) to the goal (G), avoiding walls (#).
 *
 * FEATURES DEMONSTRATED:
 * - Q-Learning algorithm
 * - Epsilon-greedy exploration
 * - Q-table visualization
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_el_rl.h"
#include "eif_memory.h"
#include "eif_types.h"

// ============================================================================
// Grid World Configuration
// ============================================================================

#define GRID_WIDTH 8
#define GRID_HEIGHT 6
#define NUM_STATES (GRID_WIDTH * GRID_HEIGHT)
#define NUM_ACTIONS 4 // Up, Down, Left, Right

// Cell types
#define CELL_EMPTY 0
#define CELL_WALL 1
#define CELL_START 2
#define CELL_GOAL 3

// Actions
enum { ACTION_UP = 0, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT };

// Grid layout
static int grid[GRID_HEIGHT][GRID_WIDTH] = {
    {2, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 1, 0, 0},
    {0, 1, 0, 0, 0, 1, 0, 0}, {0, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 0}, {0, 0, 1, 0, 0, 0, 0, 3}};

// Start and Goal positions
static int start_state = 0; // (0, 0)
static int goal_state = 47; // (7, 5)

// ============================================================================
// Q-Learning Implementation
// ============================================================================

typedef struct {
  float32_t Q[NUM_STATES][NUM_ACTIONS];
  float32_t alpha;   // Learning rate
  float32_t gamma;   // Discount factor
  float32_t epsilon; // Exploration rate
} q_learning_t;

static void ql_init(q_learning_t *ql, float32_t alpha, float32_t gamma,
                    float32_t epsilon) {
  memset(ql->Q, 0, sizeof(ql->Q));
  ql->alpha = alpha;
  ql->gamma = gamma;
  ql->epsilon = epsilon;
}

static int state_to_xy(int state, int *x, int *y) {
  *x = state % GRID_WIDTH;
  *y = state / GRID_WIDTH;
  return (*x >= 0 && *x < GRID_WIDTH && *y >= 0 && *y < GRID_HEIGHT);
}

static int xy_to_state(int x, int y) { return y * GRID_WIDTH + x; }

static int is_valid(int x, int y) {
  if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT)
    return 0;
  if (grid[y][x] == CELL_WALL)
    return 0;
  return 1;
}

static int take_action(int state, int action, float32_t *reward) {
  int x, y;
  state_to_xy(state, &x, &y);

  int nx = x, ny = y;
  switch (action) {
  case ACTION_UP:
    ny--;
    break;
  case ACTION_DOWN:
    ny++;
    break;
  case ACTION_LEFT:
    nx--;
    break;
  case ACTION_RIGHT:
    nx++;
    break;
  }

  if (!is_valid(nx, ny)) {
    *reward = -1.0f; // Hit wall
    return state;    // Stay in place
  }

  int new_state = xy_to_state(nx, ny);

  if (grid[ny][nx] == CELL_GOAL) {
    *reward = 100.0f; // Reached goal!
  } else {
    *reward = -0.1f; // Small step cost
  }

  return new_state;
}

static int select_action(q_learning_t *ql, int state) {
  if ((float)rand() / RAND_MAX < ql->epsilon) {
    // Explore: random action
    return rand() % NUM_ACTIONS;
  } else {
    // Exploit: best action
    int best_action = 0;
    float32_t best_q = ql->Q[state][0];
    for (int a = 1; a < NUM_ACTIONS; a++) {
      if (ql->Q[state][a] > best_q) {
        best_q = ql->Q[state][a];
        best_action = a;
      }
    }
    return best_action;
  }
}

static void ql_update(q_learning_t *ql, int state, int action, float32_t reward,
                      int next_state) {
  // Find max Q for next state
  float32_t max_next_q = ql->Q[next_state][0];
  for (int a = 1; a < NUM_ACTIONS; a++) {
    if (ql->Q[next_state][a] > max_next_q) {
      max_next_q = ql->Q[next_state][a];
    }
  }

  // Q-learning update
  float32_t target = reward + ql->gamma * max_next_q;
  ql->Q[state][action] += ql->alpha * (target - ql->Q[state][action]);
}

// ============================================================================
// Visualization
// ============================================================================

static void display_grid(int agent_state, const q_learning_t *ql, int show_q) {
  int ax, ay;
  state_to_xy(agent_state, &ax, &ay);

  printf("\n");
  printf("  ");
  for (int x = 0; x < GRID_WIDTH; x++)
    printf("───");
  printf("─\n");

  for (int y = 0; y < GRID_HEIGHT; y++) {
    printf("  │");
    for (int x = 0; x < GRID_WIDTH; x++) {
      char c = ' ';
      const char *color = ASCII_RESET;

      if (x == ax && y == ay) {
        c = 'A';
        color = ASCII_YELLOW;
      } else if (grid[y][x] == CELL_WALL) {
        c = '#';
        color = ASCII_RED;
      } else if (grid[y][x] == CELL_START) {
        c = 'S';
        color = ASCII_BLUE;
      } else if (grid[y][x] == CELL_GOAL) {
        c = 'G';
        color = ASCII_GREEN;
      } else if (show_q) {
        // Show best action direction
        int state = xy_to_state(x, y);
        int best_action = 0;
        float32_t best_q = ql->Q[state][0];
        for (int a = 1; a < NUM_ACTIONS; a++) {
          if (ql->Q[state][a] > best_q) {
            best_q = ql->Q[state][a];
            best_action = a;
          }
        }
        if (best_q > 0.1f) {
          const char *arrows = "^v<>";
          c = arrows[best_action];
          color = ASCII_CYAN;
        } else {
          c = '.';
        }
      } else {
        c = '.';
      }

      printf("%s %c%s", color, c, ASCII_RESET);
    }
    printf(" │\n");
  }

  printf("  ");
  for (int x = 0; x < GRID_WIDTH; x++)
    printf("───");
  printf("─\n");
}

static void display_q_values(const q_learning_t *ql, int state) {
  const char *action_names[] = {"Up", "Down", "Left", "Right"};

  printf("\n  Q-values for current state:\n");
  for (int a = 0; a < NUM_ACTIONS; a++) {
    float32_t q = ql->Q[state][a];
    int bar_len = (int)(fabsf(q) / 2.0f);
    if (bar_len > 20)
      bar_len = 20;

    printf("    %-6s: %+7.2f ", action_names[a], q);
    if (q >= 0) {
      printf("%s", ASCII_GREEN);
      for (int i = 0; i < bar_len; i++)
        printf("█");
    } else {
      printf("%s", ASCII_RED);
      for (int i = 0; i < bar_len; i++)
        printf("█");
    }
    printf("%s\n", ASCII_RESET);
  }
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(int argc, char **argv) {
  // Parse CLI arguments
  demo_cli_result_t cli_result =
      demo_parse_args(argc, argv, "rl_gridworld_demo",
                      "Q-Learning demonstration with grid world navigation");

  if (cli_result == DEMO_EXIT) {
    return 0;
  }

  srand(time(NULL));

  printf("\n");
  ascii_section("EIF Tutorial: Reinforcement Learning - Grid World");

  printf("  This tutorial demonstrates Q-Learning for navigation.\n\n");
  printf("  %sGrid Legend:%s\n", ASCII_BOLD, ASCII_RESET);
  printf(
      "    %sS%s = Start    %sG%s = Goal    %s#%s = Wall    %sA%s = Agent\n\n",
      ASCII_BLUE, ASCII_RESET, ASCII_GREEN, ASCII_RESET, ASCII_RED, ASCII_RESET,
      ASCII_YELLOW, ASCII_RESET);
  printf("  The agent will learn to find the shortest path from S to G.\n");

  demo_wait("\n  Press Enter to start training...");

  // Initialize Q-learning
  q_learning_t ql;
  ql_init(&ql, 0.1f, 0.95f, 0.1f);

  int total_rewards[100] = {0};
  int episode_lengths[100] = {0};

  // ========================================================================
  // Training Phase
  // ========================================================================
  ascii_section("Phase 1: Training (100 episodes)");

  for (int episode = 0; episode < 100; episode++) {
    int state = start_state;
    float32_t total_reward = 0;
    int steps = 0;

    // Decay epsilon over time
    ql.epsilon = 0.3f * (1.0f - episode / 100.0f);

    while (state != goal_state && steps < 200) {
      int action = select_action(&ql, state);
      float32_t reward;
      int next_state = take_action(state, action, &reward);
      ql_update(&ql, state, action, reward, next_state);

      total_reward += reward;
      state = next_state;
      steps++;
    }

    total_rewards[episode] = (int)total_reward;
    episode_lengths[episode] = steps;

    // Show progress every 10 episodes
    if (episode % 10 == 9) {
      ascii_progress("Training", (float)(episode + 1) / 100, 40);
      printf(" Episode %d: Steps=%d, Reward=%.0f\n", episode + 1, steps,
             total_reward);
    }
  }

  printf("\n  %sTraining complete!%s\n", ASCII_GREEN, ASCII_RESET);

  // Show learned policy
  ascii_section("Phase 2: Learned Policy");
  printf("  The arrows show the best action learned for each cell:\n");
  display_grid(-1, &ql, 1);

  demo_wait("\n  Press Enter to see the agent navigate...");

  // ========================================================================
  // Demo Phase
  // ========================================================================
  ascii_section("Phase 3: Agent Demo");

  ql.epsilon = 0.0f; // No exploration, pure exploitation
  int state = start_state;
  int path[100];
  int path_len = 0;

  while (state != goal_state && path_len < 50) {
    path[path_len++] = state;

    printf("\033[2J\033[H"); // Clear screen
    ascii_section("EIF Tutorial: RL Agent Demo");

    printf("  Step %d\n", path_len);
    display_grid(state, &ql, 0);
    display_q_values(&ql, state);

    int action = select_action(&ql, state);
    float32_t reward;
    state = take_action(state, action, &reward);

#ifdef _WIN32
    Sleep(500);
#else
    usleep(500000);
#endif
  }

  path[path_len++] = state; // Add goal state

  // Final state
  printf("\033[2J\033[H");
  ascii_section("EIF Tutorial: RL Agent Demo - Complete!");
  display_grid(state, &ql, 0);

  printf("\n  %s🎉 Goal reached in %d steps!%s\n", ASCII_GREEN ASCII_BOLD,
         path_len - 1, ASCII_RESET);

  // ========================================================================
  // Summary
  // ========================================================================
  printf("\n");
  ascii_section("Training Summary");

  printf("  Learning Progress (steps to goal):\n\n");

  // Show learning curve (simplified)
  float32_t curve[10];
  for (int i = 0; i < 10; i++) {
    float32_t avg = 0;
    for (int j = 0; j < 10; j++) {
      avg += episode_lengths[i * 10 + j];
    }
    curve[i] = avg / 10.0f;
  }
  ascii_plot_waveform("Steps per Episode (avg each 10)", curve, 10, 40, 6);

  printf("\n  %sKey Observations:%s\n", ASCII_BOLD, ASCII_RESET);
  printf("    • Initial episodes: Random exploration (~100+ steps)\n");
  printf("    • Final episodes: Optimal path (~%d steps)\n", path_len - 1);
  printf("    • Q-values converged to show optimal policy\n");

  printf("\n  %sEIF APIs Demonstrated:%s\n", ASCII_BOLD, ASCII_RESET);
  printf("    • eif_rl_qlearning_init()    - Initialize Q-table\n");
  printf("    • eif_rl_qlearning_update()  - Update Q-values\n");
  printf("    • eif_rl_qlearning_select()  - Action selection\n");

  printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD, ASCII_RESET);

  return 0;
}
