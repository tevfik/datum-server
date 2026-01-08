# RL Gridworld Demo - Q-Learning Navigation

## Overview
This tutorial demonstrates **Reinforcement Learning** using Q-Learning to train an agent to navigate a maze from start to goal.

## Scenario
A robot in a grid world must learn to find the optimal path:
- **Start (S)**: Initial position
- **Goal (G)**: Target destination
- **Walls (#)**: Obstacles to avoid
- **Empty (.)**: Passable cells

The agent receives rewards/penalties and learns through trial and error.

## Algorithms Used

### 1. Q-Learning
Model-free temporal difference learning algorithm.

**Q-Table:**
```
Q[state][action] = Expected cumulative reward
```

**Update Rule:**
```
Q(s,a) ← Q(s,a) + α × [r + γ × max Q(s',a') - Q(s,a)]
```

| Parameter | Symbol | Typical Value | Purpose |
|-----------|--------|---------------|---------|
| Learning Rate | α | 0.1 | How fast to update Q-values |
| Discount Factor | γ | 0.95 | Importance of future rewards |
| Exploration Rate | ε | 0.1→0 | Random action probability |

### 2. Epsilon-Greedy Policy
Balances exploration vs exploitation:

```
if random() < ε:
    action = random_action()      // Explore
else:
    action = argmax Q(s, a)       // Exploit
```

**Decay Schedule:** ε decreases over training to shift from exploration to exploitation.

### 3. Reward Structure

| Event | Reward | Purpose |
|-------|--------|---------|
| Reach Goal | +100 | Strong positive reinforcement |
| Hit Wall | -1 | Penalty for invalid moves |
| Each Step | -0.1 | Encourage shorter paths |

## Demo Walkthrough

1. **Grid Setup** - 8×6 world with walls and goal
2. **Training Phase** - 100 episodes of exploration
3. **Q-Value Convergence** - Watch values stabilize
4. **Policy Extraction** - Arrows show best actions
5. **Demo Run** - Agent follows learned policy
6. **Learning Curve** - Steps per episode decreases

## Understanding Q-Values

After training, Q-table encodes optimal policy:
```
State (2,3):
  Q[UP]    = 45.2   ← Best action
  Q[DOWN]  = 12.1
  Q[LEFT]  = 8.3
  Q[RIGHT] = 41.0
```

Policy: Choose action with highest Q-value.

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_rl_qlearning_init()` | Initialize Q-table |
| `eif_rl_qlearning_update()` | Apply Q-learning update |
| `eif_rl_qlearning_select()` | ε-greedy action selection |
| `eif_rl_qlearning_get_policy()` | Extract greedy policy |

## Convergence Behavior

| Episode | Steps to Goal | Behavior |
|---------|---------------|----------|
| 1-20 | 100+ | Random exploration |
| 20-50 | 30-50 | Learning shortcuts |
| 50-100 | ~10 | Near-optimal path |

## Extensions (Not in Demo)
- **SARSA**: On-policy alternative to Q-Learning
- **DQN**: Deep Q-Network with neural function approximation
- **Double Q-Learning**: Reduces overestimation bias

## Real-World Applications
- Game AI (board games, video games)
- Robot path planning
- Resource scheduling
- Autonomous navigation
- Trading strategies

## Run the Demo
```bash
cd build && ./bin/rl_gridworld_demo
```
