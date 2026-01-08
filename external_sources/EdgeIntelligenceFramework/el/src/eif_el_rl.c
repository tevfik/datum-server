#include "eif_el_rl.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

// Helper for random float [0, 1]
static float32_t rand_float() {
    return (float32_t)rand() / (float32_t)RAND_MAX;
}

// --- Tabular Q-Learning ---

eif_status_t eif_rl_q_init(eif_rl_q_table_t* agent, int num_states, int num_actions, eif_memory_pool_t* pool) {
    if (!agent || num_states <= 0 || num_actions <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    agent->num_states = num_states;
    agent->num_actions = num_actions;
    
    agent->q_table = (float32_t*)eif_memory_alloc(pool, num_states * num_actions * sizeof(float32_t), 4);
    if (!agent->q_table) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize to 0 (optimistic initialization could be passed as param, but 0 is standard)
    memset(agent->q_table, 0, num_states * num_actions * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_rl_q_update(eif_rl_q_table_t* agent, int state, int action, float32_t reward, int next_state, float32_t alpha, float32_t gamma) {
    if (!agent || state < 0 || state >= agent->num_states || action < 0 || action >= agent->num_actions) return EIF_STATUS_INVALID_ARGUMENT;
    if (next_state < 0 || next_state >= agent->num_states) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Max Q(next_state, a')
    float32_t max_q_next = -FLT_MAX;
    for (int a = 0; a < agent->num_actions; a++) {
        float32_t q = agent->q_table[next_state * agent->num_actions + a];
        if (q > max_q_next) max_q_next = q;
    }
    
    // Q(s,a) = Q(s,a) + alpha * (r + gamma * max_a' Q(s',a') - Q(s,a))
    int idx = state * agent->num_actions + action;
    float32_t current_q = agent->q_table[idx];
    agent->q_table[idx] = current_q + alpha * (reward + gamma * max_q_next - current_q);
    
    return EIF_STATUS_OK;
}

int eif_rl_q_select_action(const eif_rl_q_table_t* agent, int state, float32_t epsilon) {
    if (!agent || state < 0 || state >= agent->num_states) return -1;
    
    // Epsilon-greedy
    if (rand_float() < epsilon) {
        // Explore: Random action
        return rand() % agent->num_actions;
    } else {
        // Exploit: Best action
        int best_action = -1;
        float32_t max_q = -FLT_MAX;
        
        // Simple argmax. Ties broken by first occurrence (could randomize ties)
        for (int a = 0; a < agent->num_actions; a++) {
            float32_t q = agent->q_table[state * agent->num_actions + a];
            if (q > max_q) {
                max_q = q;
                best_action = a;
            }
        }
        return best_action;
    }
}

// --- Contextual Bandits ---

eif_status_t eif_rl_bandit_init(eif_rl_bandit_t* agent, int num_arms, eif_memory_pool_t* pool) {
    if (!agent || num_arms <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    agent->num_arms = num_arms;
    
    agent->values = (float32_t*)eif_memory_alloc(pool, num_arms * sizeof(float32_t), 4);
    agent->counts = (int*)eif_memory_alloc(pool, num_arms * sizeof(int), 4);
    
    if (!agent->values || !agent->counts) return EIF_STATUS_OUT_OF_MEMORY;
    
    memset(agent->values, 0, num_arms * sizeof(float32_t));
    memset(agent->counts, 0, num_arms * sizeof(int));
    
    return EIF_STATUS_OK;
}

int eif_rl_bandit_select(const eif_rl_bandit_t* agent, float32_t epsilon) {
    if (!agent) return -1;
    
    if (rand_float() < epsilon) {
        return rand() % agent->num_arms;
    } else {
        int best_arm = -1;
        float32_t max_val = -FLT_MAX;
        for (int i = 0; i < agent->num_arms; i++) {
            if (agent->values[i] > max_val) {
                max_val = agent->values[i];
                best_arm = i;
            }
        }
        return best_arm;
    }
}

eif_status_t eif_rl_bandit_update(eif_rl_bandit_t* agent, int arm, float32_t reward) {
    if (!agent || arm < 0 || arm >= agent->num_arms) return EIF_STATUS_INVALID_ARGUMENT;
    
    agent->counts[arm]++;
    float32_t n = (float32_t)agent->counts[arm];
    float32_t old_val = agent->values[arm];
    
    // Incremental mean update: Q_n+1 = Q_n + (R - Q_n) / n
    agent->values[arm] = old_val + (reward - old_val) / n;
    
    return EIF_STATUS_OK;
}
