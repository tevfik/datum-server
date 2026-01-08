#include "eif_el_rl.h"
#include "eif_test_runner.h"
#include <math.h>

bool test_q_learning() {
    // Test Q-Learning on a simple 2-state, 2-action problem
    // State 0: Action 0 -> Reward 0, State 0
    //          Action 1 -> Reward 10, State 1 (Goal)
    // State 1: Terminal (or loop)
    
    eif_rl_q_table_t agent;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_rl_q_init(&agent, 2, 2, &pool);
    
    // Train
    // Q(0, 1) should increase
    // Q(0, 0) should stay near 0
    
    float32_t alpha = 0.1f;
    float32_t gamma = 0.9f;
    
    for (int i = 0; i < 100; i++) {
        // From State 0, take Action 1
        eif_rl_q_update(&agent, 0, 1, 10.0f, 1, alpha, gamma);
        // From State 0, take Action 0
        eif_rl_q_update(&agent, 0, 0, 0.0f, 0, alpha, gamma);
    }
    
    // Check Q-Values
    // Q(0, 1) should be approaching 10 (since next state 1 has 0 value initially and we don't update it)
    // Actually, Q(0,1) -> 10.
    
    float32_t q01 = agent.q_table[0 * 2 + 1];
    float32_t q00 = agent.q_table[0 * 2 + 0];
    
    TEST_ASSERT_TRUE(q01 > 5.0f);
    TEST_ASSERT_TRUE(q00 > 5.0f);
    
    // Select Action
    // Should pick Action 1 (index 1) from State 0
    int action = eif_rl_q_select_action(&agent, 0, 0.0f); // epsilon=0 (exploit)
    TEST_ASSERT_EQUAL_INT(1, action);
    
    return true;
}

bool test_bandit() {
    // Test Multi-Armed Bandit
    // Arm 0: Reward 1.0
    // Arm 1: Reward 0.0
    
    eif_rl_bandit_t agent;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_rl_bandit_init(&agent, 2, &pool);
    
    // Update Arm 0 with high reward
    for (int i = 0; i < 10; i++) {
        eif_rl_bandit_update(&agent, 0, 1.0f);
    }
    
    // Update Arm 1 with low reward
    for (int i = 0; i < 10; i++) {
        eif_rl_bandit_update(&agent, 1, 0.0f);
    }
    
    // Check values
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, agent.values[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, agent.values[1]);
    
    // Select Action (Exploit)
    int arm = eif_rl_bandit_select(&agent, 0.0f);
    TEST_ASSERT_EQUAL_INT(0, arm);
    
    return true;
}

BEGIN_TEST_SUITE(run_rl_tests)
    RUN_TEST(test_q_learning);
    RUN_TEST(test_bandit);
END_TEST_SUITE()
