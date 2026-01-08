# Reinforcement Learning Tutorial: Q-Learning Grid World

## Learning Objectives

- Markov Decision Process (MDP) fundamentals
- Q-Learning algorithm
- Exploration vs exploitation
- Tabular RL on embedded systems

**Level**: Intermediate  
**Time**: 45 minutes

---

## 1. RL Basics

### MDP Components

```
Agent ←→ Environment

State (s):  Where am I?
Action (a): What can I do?
Reward (r): How good was that?
Next State (s'): Where did I end up?
```

### Grid World Example

```
┌───┬───┬───┬───┐
│ S │   │   │ G │   S = Start, G = Goal (+10)
├───┼───┼───┼───┤   X = Obstacle (-5)
│   │ X │   │   │   Each step: -0.1
├───┼───┼───┼───┤
│   │   │   │   │
└───┴───┴───┴───┘
```

---

## 2. Q-Learning Algorithm

### Q-Table

```
Q[state][action] = Expected future reward

Actions: UP(0), DOWN(1), LEFT(2), RIGHT(3)
```

### Update Rule

```
Q(s,a) ← Q(s,a) + α[r + γ·max(Q(s',a')) - Q(s,a)]

α = Learning rate (0.1)
γ = Discount factor (0.99)
```

### EIF Implementation

```c
eif_qlearning_t ql;
eif_qlearning_init(&ql, 
    n_states,        // 16 for 4×4 grid
    n_actions,       // 4 directions
    0.1f,            // learning rate
    0.99f,           // discount
    0.1f,            // epsilon (exploration)
    &pool);

// Training loop
for (int ep = 0; ep < 1000; ep++) {
    int state = START_STATE;
    
    while (state != GOAL_STATE) {
        // Choose action (ε-greedy)
        int action = eif_qlearning_choose_action(&ql, state);
        
        // Take action, observe result
        int next_state, reward;
        env_step(state, action, &next_state, &reward);
        
        // Update Q-table
        eif_qlearning_update(&ql, state, action, reward, next_state);
        
        state = next_state;
    }
}
```

---

## 3. Exploration vs Exploitation

### ε-Greedy Strategy

```c
int choose_action(eif_qlearning_t* ql, int state) {
    if ((float)rand()/RAND_MAX < ql->epsilon) {
        // Explore: random action
        return rand() % ql->n_actions;
    } else {
        // Exploit: best known action
        return argmax(ql->Q[state], ql->n_actions);
    }
}
```

### Epsilon Decay

```c
ql->epsilon *= 0.995f;  // Decay each episode
if (ql->epsilon < 0.01f) ql->epsilon = 0.01f;
```

---

## 4. ESP32 Robot Example

```c
// Simple line-following robot with Q-learning
void rl_task(void* arg) {
    eif_qlearning_t ql;
    eif_qlearning_init(&ql, 8, 3, 0.1f, 0.9f, 0.2f, &pool);
    // States: 8 sensor patterns
    // Actions: LEFT, STRAIGHT, RIGHT
    
    while (1) {
        int state = read_line_sensors();  // 0-7
        int action = eif_qlearning_choose_action(&ql, state);
        
        execute_action(action);  // Motor control
        vTaskDelay(100 / portTICK_PERIOD_MS);
        
        // Calculate reward
        int new_state = read_line_sensors();
        int reward = (new_state == CENTER_STATE) ? 1 : -1;
        
        eif_qlearning_update(&ql, state, action, reward, new_state);
    }
}
```

---

## 5. Summary

### Key Concepts
- Q-table: Value of (state, action) pairs
- Bellman equation: Recursive value update
- ε-greedy: Balance exploration/exploitation

### Memory: 4×n_states×n_actions bytes (Q-table)
