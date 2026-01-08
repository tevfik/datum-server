#include "../framework/eif_test_runner.h"
#include "eif_el.h"
#include "eif_el_rl.h"
#include <string.h>
#include <math.h>

// Forward declarations
bool test_ewc_penalty_gradient_update();
bool test_online_learning();
bool test_fewshot_learning();
bool test_el_utils();

static uint8_t pool_buffer[65536];
static eif_memory_pool_t pool;

void setup_el_coverage() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// =============================================================================
// Helper Functions
// =============================================================================

// Dummy gradient function for Federated Learning
// Computes grad = inputs (ignoring weights/targets for simplicity)
void dummy_fed_gradient(const float32_t* w, const float32_t* x, const float32_t* y,
                        float32_t* grad, int n, int in_d, int out_d) {
    // Assume flat weights for simplicity in this dummy
    // Just set gradient to 0.1 for all weights to verify update
    int num_weights = in_d * out_d; // Approximation
    if (num_weights <= 0) num_weights = 10; // Fallback
    
    for (int i = 0; i < num_weights; i++) {
        grad[i] = 0.1f;
    }
}

// Dummy gradient function for EWC
// Computes grad = 1.0
void dummy_ewc_gradient(const float32_t* w, const float32_t* x, float32_t* grad, int n) {
    for (int i = 0; i < n; i++) {
        grad[i] = 1.0f;
    }
}

// =============================================================================
// DQN Coverage Tests
// =============================================================================

bool test_dqn_full_coverage() {
    setup_el_coverage();
    eif_rl_dqn_agent_t agent;
    uint32_t state_dim = 2;
    uint32_t action_space = 2;
    uint32_t memory_capacity = 100;
    
    // 1. Initialization
    eif_status_t status = eif_rl_dqn_init(&agent, state_dim, action_space, memory_capacity, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(0, agent.current_size);
    TEST_ASSERT_EQUAL_INT(0, agent.buffer_idx);
    
    // 2. Build Model
    // This helper builds a simple MLP: Input(2) -> [Dense(16) -> ReLU] -> Dense(2) -> Output(2)
    status = eif_rl_dqn_build_simple_model(&agent, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    // Verify neural context is initialized
    TEST_ASSERT_NOT_NULL(agent.model.nodes);
    
    float32_t state[2] = {0.5f, -0.5f};
    
    // 3. Action Selection (Greedy)
    // Epsilon = 0.0f means purely greedy (argmax Q)
    // This runs inference. Since weights are random small values, result is deterministic for current weights.
    int action_greedy = eif_rl_dqn_select_action(&agent, state, 0.0f);
    TEST_ASSERT_TRUE(action_greedy >= 0 && action_greedy < (int)action_space);
    
    // 4. Action Selection (Random)
    // Epsilon = 1.1f means purely random
    int action_random = eif_rl_dqn_select_action(&agent, state, 1.1f);
    TEST_ASSERT_TRUE(action_random >= 0 && action_random < (int)action_space);

    // 5. Store Experience
    // We need enough samples to train. Let's fill 10 samples.
    float32_t next_state[2] = {0.6f, -0.6f};
    for(int i=0; i<10; i++) {
        eif_rl_dqn_store(&agent, state, 0, 1.0f, next_state, 0);
    }
    TEST_ASSERT_EQUAL_INT(10, agent.current_size);
    
    // Store one "done" state
    eif_rl_dqn_store(&agent, state, 1, -1.0f, next_state, 1);
    TEST_ASSERT_EQUAL_INT(11, agent.current_size);
    
    // 6. Train
    // Batch size = 5.
    // This triggers: Sampling -> Target Calcs -> Forward -> Backward -> Update
    float32_t loss = eif_rl_dqn_train(&agent, 5);
    // Loss should be calculated (>= 0). It will likely be small but non-zero unless weights are perfect.
    TEST_ASSERT_TRUE(loss >= 0.0f);
    
    // Train with insufficient batch size (should return 0)
    float32_t loss_empty = eif_rl_dqn_train(&agent, 100); 
    TEST_ASSERT_EQUAL_FLOAT(0.0f, loss_empty, 0.001f);

    return true;
}

// =============================================================================
// Q-Learning Coverage Tests
// =============================================================================

bool test_q_learning_coverage() {
    setup_el_coverage();
    eif_rl_q_table_t agent;
    int num_states = 5;
    int num_actions = 3;
    
    // 1. Init
    eif_status_t status = eif_rl_q_init(&agent, num_states, num_actions, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_NOT_NULL(agent.q_table);
    TEST_ASSERT_EQUAL_INT(num_states, agent.num_states); // Accessing private fields if visible, or trusting structure
    // Struct is defined in eif_el_rl.h likely
    
    // Invalid Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_init(NULL, 1, 1, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_init(&agent, 0, 1, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_init(&agent, 1, 0, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_init(&agent, 1, 1, NULL));

    // 2. Select Action (Greedy - Initial is all 0)
    int action = eif_rl_q_select_action(&agent, 0, 0.0f);
    TEST_ASSERT_TRUE(action >= 0 && action < num_actions);
    
    // 3. Select Action (Random)
    action = eif_rl_q_select_action(&agent, 0, 1.1f);
    TEST_ASSERT_TRUE(action >= 0 && action < num_actions);
    
    // Invalid Select
    TEST_ASSERT_EQUAL_INT(-1, eif_rl_q_select_action(NULL, 0, 0.0f));
    TEST_ASSERT_EQUAL_INT(-1, eif_rl_q_select_action(&agent, -1, 0.0f));
    TEST_ASSERT_EQUAL_INT(-1, eif_rl_q_select_action(&agent, 100, 0.0f));

    // 4. Update
    // s=0, a=1, r=10, next_s=1, alpha=0.1, gamma=0.9
    // Q(0,1) = 0 + 0.1 * (10 + 0.9*maxQ(1) - 0) = 0.1 * 10 = 1.0 (since maxQ(1)=0)
    status = eif_rl_q_update(&agent, 0, 1, 10.0f, 1, 0.1f, 0.9f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    // Verify manually
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, agent.q_table[0 * num_actions + 1]);
    
    // Update again with gamma
    // s=1, a=0, r=2, next_s=2
    // Q(1,0) = 0 + 0.1 * (2 + 0) = 0.2
    eif_rl_q_update(&agent, 1, 0, 2.0f, 2, 0.1f, 0.9f);
    
    // Update s=0 again, now next_s=1 has non-zero value
    // Q(0,1) was 1.0. 
    // Target = r=10 + 0.9 * maxQ(1). maxQ(1) is 0.2 (at action 0).
    // Target = 10 + 0.18 = 10.18.
    // Q(0,1)new = 1.0 + 0.1 * (10.18 - 1.0) = 1.0 + 0.1 * 9.18 = 1.918
    status = eif_rl_q_update(&agent, 0, 1, 10.0f, 1, 0.1f, 0.9f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.918f, agent.q_table[0 * num_actions + 1]);
    
    // Invalid Update
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_update(NULL, 0, 0, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_update(&agent, -1, 0, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_update(&agent, 0, -1, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_q_update(&agent, 0, 0, 0, 100, 0, 0)); // next_state invalid

    return true;
}

// =============================================================================
// Bandit Coverage Tests
// =============================================================================

bool test_bandit_coverage() {
    setup_el_coverage();
    eif_rl_bandit_t agent;
    int num_arms = 4;
    
    // 1. Init
    eif_status_t status = eif_rl_bandit_init(&agent, num_arms, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_init(NULL, 1, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_init(&agent, 0, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_init(&agent, 1, NULL));
    
    // 2. Select (Greedy, Random)
    int arm = eif_rl_bandit_select(&agent, 0.0f);
    TEST_ASSERT_TRUE(arm >= 0 && arm < num_arms);
    
    arm = eif_rl_bandit_select(&agent, 1.1f);
    TEST_ASSERT_TRUE(arm >= 0 && arm < num_arms);
    
    // Invalid Select
    TEST_ASSERT_EQUAL_INT(-1, eif_rl_bandit_select(NULL, 0.0f));
    
    // 3. Update
    // arm 1, reward 10
    // Q_1 = 0 + (10 - 0)/1 = 10
    status = eif_rl_bandit_update(&agent, 1, 10.0f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, agent.values[1]);
    TEST_ASSERT_EQUAL_INT(1, agent.counts[1]);
    
    // arm 1, reward 0
    // Q_2 = 10 + (0 - 10)/2 = 5
    status = eif_rl_bandit_update(&agent, 1, 0.0f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, agent.values[1]);
    TEST_ASSERT_EQUAL_INT(2, agent.counts[1]);
    
    // Invalid Update
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_update(NULL, 0, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_update(&agent, -1, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rl_bandit_update(&agent, 100, 0));
    
    return true;
}


// =============================================================================
// Federated Learning Tests
// =============================================================================

bool test_federated_init() {
    setup_el_coverage();
    eif_federated_client_t client;
    
    // Valid init
    eif_status_t status = eif_federated_init(&client, 10, 0.01f, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(10, client.num_weights);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, client.learning_rate);
    TEST_ASSERT_NOT_NULL(client.weights);
    TEST_ASSERT_NOT_NULL(client.gradients);
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_federated_init(NULL, 10, 0.01f, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_federated_init(&client, 0, 0.01f, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_federated_init(&client, 10, 0.01f, NULL));
    
    return true;
}

bool test_federated_workflow() {
    setup_el_coverage();
    eif_federated_client_t client;
    eif_federated_init(&client, 5, 0.1f, &pool);
    
    // Set weights
    float32_t initial_weights[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    eif_status_t status = eif_federated_set_weights(&client, initial_weights);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, client.weights[0]);
    
    // Train batch
    float32_t inputs[] = {1.0f};
    float32_t targets[] = {1.0f};
    // We use 5 weights, so let's say input_dim=5, output_dim=1
    status = eif_federated_train_batch(&client, inputs, targets, 1, 5, 1, dummy_fed_gradient);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check update
    // Gradient was 0.1. LR is 0.1. Update = 0.1 * 0.1 = 0.01.
    // New weight = 1.0 - 0.01 = 0.99.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.99f, client.weights[0]);
    TEST_ASSERT_TRUE(client.has_update);
    
    // Get update (gradients)
    float32_t delta[5];
    status = eif_federated_get_update(&client, delta);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, delta[0]); // Gradient accumulated
    
    // Apply update (from server)
    float32_t server_delta[] = {-0.5f, -0.5f, -0.5f, -0.5f, -0.5f};
    status = eif_federated_apply_update(&client, server_delta);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Weight -= LR * delta
    // 0.99 - (0.1 * -0.5) = 0.99 + 0.05 = 1.04
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.04f, client.weights[0]);
    TEST_ASSERT(!client.has_update);
    TEST_ASSERT_EQUAL_INT(1, client.round_id);
    
    return true;
}

bool test_federated_aggregate() {
    float32_t global[2] = {10.0f, 10.0f};
    float32_t c1_delta[] = {1.0f, 1.0f};
    float32_t c2_delta[] = {2.0f, 2.0f};
    const float32_t* deltas[] = {c1_delta, c2_delta};
    int num_samples[] = {100, 100}; // Equal weight
    
    eif_status_t status = eif_federated_aggregate(global, deltas, num_samples, 2, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Avg delta = (1*100 + 2*100)/200 = 1.5
    // Global -= Avg delta => 10 - 1.5 = 8.5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.5f, global[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.5f, global[1]);
    
    // Edge case: 0 samples
    int zero_samples[] = {0, 0};
    status = eif_federated_aggregate(global, deltas, zero_samples, 2, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.5f, global[0]); // Unchanged
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_federated_aggregate(NULL, deltas, num_samples, 2, 2));
    
    return true;
}

// =============================================================================
// EWC Tests
// =============================================================================

bool test_ewc_init() {
    setup_el_coverage();
    eif_ewc_t ewc;
    
    eif_status_t status = eif_ewc_init(&ewc, 10, 100.0f, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(10, ewc.num_weights);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ewc.lambda);
    TEST_ASSERT_NOT_NULL(ewc.weights);
    TEST_ASSERT_NOT_NULL(ewc.fisher);
    TEST_ASSERT_NOT_NULL(ewc.star_weights);
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_ewc_init(NULL, 10, 1.0f, &pool));
    
    return true;
}

bool test_ewc_workflow() {
    setup_el_coverage();
    eif_ewc_t ewc;
    eif_ewc_init(&ewc, 2, 1.0f, &pool);
    
    // Set weights
    float32_t weights[] = {1.0f, 2.0f};
    eif_ewc_set_weights(&ewc, weights);
    
    // Compute Fisher
    float32_t data[] = {0.0f}; // Dummy data
    eif_status_t status = eif_ewc_compute_fisher(&ewc, data, 1, 1, dummy_ewc_gradient);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Dummy gradient returns 1.0. Fisher = grad^2 = 1.0.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ewc.fisher[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ewc.fisher[1]);
    
    // Consolidate
    status = eif_ewc_consolidate(&ewc);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_TRUE(ewc.has_prior_task);
    TEST_ASSERT_EQUAL_INT(1, ewc.num_tasks);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ewc.star_weights[0]);
    
    return true;
}



bool test_ewc_penalty_gradient_update() {
    setup_el_coverage();
    eif_ewc_t ewc;
    eif_ewc_init(&ewc, 2, 1.0f, &pool);
    
    // Setup prior task
    float32_t weights[] = {1.0f, 2.0f};
    eif_ewc_set_weights(&ewc, weights);
    
    // Fisher = [1, 1]
    float32_t data[] = {0.0f};
    eif_ewc_compute_fisher(&ewc, data, 1, 1, dummy_ewc_gradient);
    
    eif_ewc_consolidate(&ewc);
    
    // Move weights away from star weights
    // Star = [1, 2]. New = [2, 3]. Diff = [1, 1].
    float32_t new_weights[] = {2.0f, 3.0f};
    eif_ewc_set_weights(&ewc, new_weights);
    
    // Penalty = 0.5 * lambda * sum(F * diff^2)
    // = 0.5 * 1.0 * (1*1^2 + 1*1^2) = 0.5 * 2 = 1.0
    float32_t penalty = eif_ewc_penalty(&ewc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, penalty);
    
    // Gradient = lambda * F * diff
    // = 1.0 * 1 * 1 = 1.0
    float32_t grad[2];
    eif_status_t status = eif_ewc_gradient(&ewc, grad);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, grad[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, grad[1]);
    
    // Update
    // Task grad = [0.5, 0.5]
    // Total grad = Task + EWC = [1.5, 1.5]
    // New weight = Old - LR * Total = 2.0 - 0.1 * 1.5 = 1.85
    float32_t task_grad[] = {0.5f, 0.5f};
    status = eif_ewc_update(&ewc, task_grad, 0.1f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.85f, ewc.weights[0]);
    
    // Edge case: No prior task
    eif_ewc_t ewc2;
    eif_ewc_init(&ewc2, 2, 1.0f, &pool);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eif_ewc_penalty(&ewc2));
    eif_ewc_gradient(&ewc2, grad);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, grad[0]);
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_ewc_gradient(NULL, grad));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_ewc_gradient(&ewc, NULL));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_ewc_update(NULL, task_grad, 0.1f));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_ewc_update(&ewc, NULL, 0.1f));
    
    return true;
}

// =============================================================================
// Online Learning Tests
// =============================================================================

bool test_online_learning() {
    setup_el_coverage();
    eif_online_learner_t learner;
    
    // Init
    eif_status_t status = eif_online_init(&learner, 2, 0.1f, 5, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(2, learner.num_weights);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, learner.learning_rate);
    TEST_ASSERT_EQUAL_INT(5, learner.window_size);
    
    // Set weights
    float32_t weights[] = {1.0f, 1.0f};
    status = eif_online_set_weights(&learner, weights);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Predict
    float32_t input[] = {2.0f, 3.0f};
    float32_t pred = eif_online_predict(&learner, input, 2);
    // 1*2 + 1*3 = 5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, pred);
    
    // Update (AdaGrad)
    // Grad = [1, 1]. Loss = 0.5.
    float32_t grad[] = {1.0f, 1.0f};
    status = eif_online_update(&learner, grad, 0.5f);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // GradSqSum += 1^2 = 1.
    // Adaptive LR = 0.1 / (sqrt(1) + eps) approx 0.1.
    // Weight -= 0.1 * 1 = 0.9.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, learner.weights[0]);
    
    // Drift detection
    // Error buffer: [0.5, 0, 0, 0, 0]. Avg = 0.5/1 = 0.5.
    // Threshold is 0.3.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, eif_online_error_rate(&learner));
    TEST_ASSERT(eif_online_drift_detected(&learner));
    
    // Reset drift
    eif_online_reset_drift(&learner);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eif_online_error_rate(&learner));
    TEST_ASSERT(!eif_online_drift_detected(&learner));
    // GradSqSum should be reset too
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, learner.grad_squared_sum[0]);
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_online_init(NULL, 2, 0.1f, 5, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_online_set_weights(NULL, weights));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_online_update(NULL, grad, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eif_online_predict(NULL, input, 2));
    TEST_ASSERT(!eif_online_drift_detected(NULL));
    eif_online_reset_drift(NULL); // Should not crash
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eif_online_error_rate(NULL));
    
    return true;
}

// =============================================================================
// Few-Shot Learning Tests
// =============================================================================

bool test_fewshot_learning() {
    setup_el_coverage();
    eif_fewshot_t fs;
    
    // Init (Default Euclidean)
    eif_status_t status = eif_fewshot_init(&fs, 5, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(EIF_DISTANCE_EUCLIDEAN, fs.distance_type); // Default
    TEST_ASSERT_EQUAL_INT(5, fs.max_classes);
    TEST_ASSERT_EQUAL_INT(2, fs.embed_dim);
    TEST_ASSERT_EQUAL_INT(0, fs.num_classes);
    
    // Test update_prototype wrapper
    float32_t ex1[] = {0.0f, 0.0f};
    status = eif_fewshot_update_prototype(&fs, ex1, 0);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Add examples for Class 0 directly: [0, 0] (already added) and [2, 2] -> Mean [1, 1]
    float32_t ex2[] = {2.0f, 2.0f};
    status = eif_fewshot_add_example(&fs, ex2, 0);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(1, fs.num_classes);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, fs.prototypes[0].embedding[0]);
    
    // Add examples for Class 1: [4, 4]
    float32_t ex3[] = {4.0f, 4.0f};
    status = eif_fewshot_add_example(&fs, ex3, 1);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(2, fs.num_classes);
    
    // Classify [0.5, 0.5] -> Should be Class 0 (Euclidean)
    float32_t query1[] = {0.5f, 0.5f};
    float32_t dist;
    int cls = eif_fewshot_classify(&fs, query1, &dist);
    TEST_ASSERT_EQUAL_INT(0, cls);
    
    // Classify [3.5, 3.5] -> Should be Class 1
    float32_t query2[] = {3.5f, 3.5f};
    cls = eif_fewshot_classify(&fs, query2, &dist);
    TEST_ASSERT_EQUAL_INT(1, cls);
    
    // Predict Proba (Euclidean based softmax)
    float32_t probs[5];
    status = eif_fewshot_predict_proba(&fs, query1, probs);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_TRUE(probs[0] > probs[1]);
    
    // --- Test Cosine Distance Path ---
    eif_fewshot_t fs_cos;
    status = eif_fewshot_init(&fs_cos, 5, 2, &pool);
    fs_cos.distance_type = EIF_DISTANCE_COSINE; // Manually set
    
    // Class 0: vector [1, 0]
    float32_t c0[] = {1.0f, 0.0f};
    eif_fewshot_add_example(&fs_cos, c0, 0);
    
    // Class 1: vector [0, 1]
    float32_t c1[] = {0.0f, 1.0f};
    eif_fewshot_add_example(&fs_cos, c1, 1);
    
    // Query [0.9, 0.1] -> Closer to [1, 0] (Class 0)
    float32_t q_cos[] = {0.9f, 0.1f};
    cls = eif_fewshot_classify(&fs_cos, q_cos, &dist);
    TEST_ASSERT_EQUAL_INT(0, cls); // Cosine sim is higher for Class 0
    
    // Query [0.1, 0.9] -> Closer to [0, 1] (Class 1)
    float32_t q_cos2[] = {0.1f, 0.9f};
    cls = eif_fewshot_classify(&fs_cos, q_cos2, &dist);
    TEST_ASSERT_EQUAL_INT(1, cls);

    // Reset
    eif_fewshot_reset(&fs);
    TEST_ASSERT_EQUAL_INT(0, eif_fewshot_num_classes(&fs));
    
    // Invalid args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_fewshot_init(NULL, 5, 2, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_fewshot_add_example(NULL, ex1, 0));
    TEST_ASSERT_EQUAL_INT(-1, eif_fewshot_classify(NULL, query1, &dist));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_fewshot_predict_proba(NULL, query1, probs));
    eif_fewshot_reset(NULL);
    TEST_ASSERT_EQUAL_INT(0, eif_fewshot_num_classes(NULL));
    
    return true;
}

bool test_el_utils() {
    // Test Euclidean Distance
    float32_t a[] = {0.0f, 0.0f};
    float32_t b[] = {3.0f, 4.0f};
    float32_t dist = eif_euclidean_distance(a, b, 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, dist);
    
    float32_t c[] = {1.0f, 1.0f};
    dist = eif_euclidean_distance(c, c, 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dist);

    // Test Cosine Similarity
    float32_t v1[] = {1.0f, 0.0f};
    float32_t v2[] = {0.0f, 1.0f};
    float32_t sim = eif_cosine_similarity(v1, v2, 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim);
    
    float32_t v3[] = {1.0f, 0.0f};
    sim = eif_cosine_similarity(v1, v3, 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, sim);
    
    float32_t v4[] = {-1.0f, 0.0f};
    sim = eif_cosine_similarity(v1, v4, 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, sim);

    // Test Linear Gradient
    // 1 sample, 2 inputs, 1 output
    // y = w1*x1 + w2*x2
    // Let w = [0.5, 0.5], x = [1.0, 2.0], y_target = [2.0]
    // y_pred = 0.5*1 + 0.5*2 = 1.5
    // error = 1.5 - 2.0 = -0.5
    // grad_w1 = x1 * error = 1.0 * -0.5 = -0.5
    // grad_w2 = x2 * error = 2.0 * -0.5 = -1.0
    
    float32_t weights[] = {0.5f, 0.5f};
    float32_t inputs[] = {1.0f, 2.0f};
    float32_t targets[] = {2.0f};
    float32_t gradient[2];
    
    eif_linear_gradient(weights, inputs, targets, gradient, 1, 2, 1);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, gradient[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, gradient[1]);

    return true;
}

BEGIN_TEST_SUITE(run_el_coverage_tests)
    RUN_TEST(test_dqn_full_coverage);
    RUN_TEST(test_q_learning_coverage);
    RUN_TEST(test_bandit_coverage);
    RUN_TEST(test_federated_init);
    RUN_TEST(test_federated_workflow);
    RUN_TEST(test_federated_aggregate);
    RUN_TEST(test_ewc_init);
    RUN_TEST(test_ewc_workflow);
    RUN_TEST(test_ewc_penalty_gradient_update);
    RUN_TEST(test_online_learning);
    RUN_TEST(test_fewshot_learning);
    RUN_TEST(test_el_utils);
END_TEST_SUITE()
