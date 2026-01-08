#ifndef EIF_EL_RL_H
#define EIF_EL_RL_H


#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

// --- Tabular Q-Learning ---

typedef struct {
    int num_states;
    int num_actions;
    float32_t* q_table; // num_states * num_actions
} eif_rl_q_table_t;

eif_status_t eif_rl_q_init(eif_rl_q_table_t* agent, int num_states, int num_actions, eif_memory_pool_t* pool);
eif_status_t eif_rl_q_update(eif_rl_q_table_t* agent, int state, int action, float32_t reward, int next_state, float32_t alpha, float32_t gamma);
int eif_rl_q_select_action(const eif_rl_q_table_t* agent, int state, float32_t epsilon); // Epsilon-greedy

// --- Contextual Bandits (Multi-Armed Bandit) ---

typedef struct {
    int num_arms;
    float32_t* values; // Estimated value of each arm
    int* counts;       // Number of times each arm was pulled
} eif_rl_bandit_t;

eif_status_t eif_rl_bandit_init(eif_rl_bandit_t* agent, int num_arms, eif_memory_pool_t* pool);
int eif_rl_bandit_select(const eif_rl_bandit_t* agent, float32_t epsilon); // Epsilon-greedy
eif_status_t eif_rl_bandit_update(eif_rl_bandit_t* agent, int arm, float32_t reward);

// --- Deep Q-Network (DQN) ---

#include "eif_neural.h"

typedef struct {
    eif_model_t model;
    eif_neural_context_t ctx;
    eif_neural_train_ctx_t train_ctx;
    
    // Replay Buffer
    int buffer_size;
    int buffer_idx;
    int current_size;
    
    // Experience: s, a, r, s', done
    float32_t* state_buffer;      // [size, state_dim]
    int* action_buffer;           // [size]
    float32_t* reward_buffer;     // [size]
    float32_t* next_state_buffer; // [size, state_dim]
    int* done_buffer;             // [size]
    
    int state_dim;
    int num_actions;
    float32_t gamma;
    
} eif_rl_dqn_agent_t;

eif_status_t eif_rl_dqn_init(eif_rl_dqn_agent_t* agent, int state_dim, int num_actions, int buffer_size, eif_memory_pool_t* pool);
eif_status_t eif_rl_dqn_build_simple_model(eif_rl_dqn_agent_t* agent, eif_memory_pool_t* pool);
int eif_rl_dqn_select_action(eif_rl_dqn_agent_t* agent, const float32_t* state, float32_t epsilon);
void eif_rl_dqn_store(eif_rl_dqn_agent_t* agent, const float32_t* state, int action, float32_t reward, const float32_t* next_state, int done);
float32_t eif_rl_dqn_train(eif_rl_dqn_agent_t* agent, int batch_size);

#endif // EIF_EL_RL_H

