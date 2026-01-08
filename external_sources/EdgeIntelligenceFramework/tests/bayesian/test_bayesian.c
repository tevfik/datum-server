#include "../framework/eif_test_runner.h"
#include "eif_bayesian.h"
#include "eif_generic.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Particle Filter Test ---

void pf_transition(const float32_t* state, const float32_t* control, float32_t* out_state) {
    // x = x + u
    out_state[0] = state[0] + control[0];
}

float32_t pf_likelihood(const float32_t* state, const float32_t* measurement) {
    // Gaussian likelihood: exp(-0.5 * (z - x)^2 / R)
    float32_t diff = measurement[0] - state[0];
    float32_t R = 1.0f;
    return expf(-0.5f * diff * diff / R);
}

bool test_particle_filter() {
    #define NUM_PARTICLES 100
    float32_t process_noise[] = {0.1f};
    
    eif_particle_filter_t pf = {
        .num_particles = NUM_PARTICLES,
        .state_dim = 1,
        .process_noise = process_noise,
        .transition_func = pf_transition,
        .likelihood_func = pf_likelihood
    };
    
    float32_t init_state[] = {0.0f};
    float32_t init_cov[] = {0.5f};
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_pf_init(&pf, init_state, init_cov, &pool);
    
    // Predict: u = 1.0 -> x should be around 1.0
    float32_t control[] = {1.0f};
    eif_pf_predict(&pf, control);
    
    // Update: z = 1.1 -> Likelihood high for particles near 1.1
    float32_t measurement[] = {1.1f};
    eif_pf_update(&pf, measurement);
    
    float32_t est_state[1];
    eif_pf_estimate(&pf, est_state);
    
    // Estimate should be close to 1.1 (between 1.0 and 1.1 actually, biased towards measurement)
    TEST_ASSERT(est_state[0] > 0.8f && est_state[0] < 1.3f);
    
    return true;
}

// --- Complementary Filter Test ---

bool test_complementary_filter() {
    eif_complementary_filter_t cf;
    eif_complementary_filter_init(&cf, 0.98f, 0.0f);
    
    // Update 1: Gyro says +10 deg/s for 0.1s -> +1 deg. Accel says 1 deg.
    // angle = 0.98 * (0 + 10*0.1) + 0.02 * 1 = 0.98 * 1 + 0.02 = 1.0
    float32_t angle = eif_complementary_filter_update(&cf, 1.0f, 10.0f, 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, angle, 0.001f);
    
    // Update 2: Gyro says 0. Accel says 0.
    // angle = 0.98 * (1.0 + 0) + 0.02 * 0 = 0.98
    angle = eif_complementary_filter_update(&cf, 0.0f, 0.0f, 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(0.98f, angle, 0.001f);
    
    return true;
}

// --- Kalman Filter Test ---
bool test_kalman_filter() {
    // Simple 1D tracking: x = x + u, z = x
    eif_kalman_filter_t kf = {0};
    kf.n = 1; kf.m = 1; kf.p = 1;
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Allocate matrices
    kf.x = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.P = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.F = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.B = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.H = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.Q = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    kf.R = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    
    eif_status_t status = eif_kalman_init(&kf, &pool);
    TEST_ASSERT(status == EIF_STATUS_OK);
    
    // Initialize matrices
    kf.x[0] = 0.0f;
    kf.P[0] = 1.0f;
    kf.F[0] = 1.0f;
    kf.B[0] = 1.0f;
    kf.H[0] = 1.0f;
    kf.Q[0] = 0.1f;
    kf.R[0] = 0.1f;
    
    // Predict: u = 1.0 -> x_pred = 0 + 1 = 1.0
    float32_t u[] = {1.0f};
    status = eif_kalman_predict(&kf, u);
    TEST_ASSERT(status == EIF_STATUS_OK);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, kf.x[0], 0.001f);
    
    // Update: z = 1.2 -> x should be between 1.0 and 1.2
    float32_t z[] = {1.2f};
    status = eif_kalman_update(&kf, z);
    TEST_ASSERT(status == EIF_STATUS_OK);
    
    TEST_ASSERT(kf.x[0] > 1.0f && kf.x[0] < 1.2f);
    
    return true;
}

// --- Extended Kalman Filter Test ---

void ekf_f(const float32_t* x, const float32_t* u, float32_t* out) {
    // x_next = x + u + 0.1 * sin(x)
    out[0] = x[0] + u[0] + 0.1f * sinf(x[0]);
}

void ekf_F_jac(const float32_t* x, const float32_t* u, float32_t* F) {
    // d(x_next)/dx = 1 + 0.1 * cos(x)
    F[0] = 1.0f + 0.1f * cosf(x[0]);
}

void ekf_h(const float32_t* x, float32_t* out) {
    // z = sin(x)
    out[0] = sinf(x[0]);
}

void ekf_H_jac(const float32_t* x, float32_t* H) {
    // dz/dx = cos(x)
    H[0] = cosf(x[0]);
}

bool test_ekf() {
    eif_ekf_t ekf = {0};
    ekf.n = 1; ekf.p = 1;
    ekf.f_func = ekf_f;
    ekf.F_jac = ekf_F_jac;
    ekf.h_func = ekf_h;
    ekf.H_jac = ekf_H_jac;
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Allocate state matrices
    ekf.x = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    ekf.P = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    ekf.Q = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    ekf.R = (float32_t*)eif_memory_alloc(&pool, 1 * sizeof(float32_t), 4);
    
    eif_status_t status = eif_ekf_init(&ekf, &pool);
    TEST_ASSERT(status == EIF_STATUS_OK);
    
    // Init
    ekf.x[0] = 0.0f;
    ekf.P[0] = 1.0f;
    ekf.Q[0] = 0.1f;
    ekf.R[0] = 0.1f;
    
    // Predict: u = 0.5
    // x_pred = 0 + 0.5 + 0.1*sin(0) = 0.5
    float32_t u[] = {0.5f};
    status = eif_ekf_predict(&ekf, u);
    TEST_ASSERT(status == EIF_STATUS_OK);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, ekf.x[0], 0.001f);
    
    // Update: z = sin(0.6) approx 0.564
    // h(0.5) = sin(0.5) approx 0.479
    // y = 0.564 - 0.479 = 0.085
    // H = cos(0.5) approx 0.877
    // K will be positive
    // x_new = 0.5 + K * 0.085 > 0.5
    
    float32_t z[] = {sinf(0.6f)};
    status = eif_ekf_update(&ekf, z);
    TEST_ASSERT(status == EIF_STATUS_OK);
    
    TEST_ASSERT(ekf.x[0] > 0.5f);
    
    return true;
}

BEGIN_TEST_SUITE(run_bayesian_tests)
    RUN_TEST(test_particle_filter);
    RUN_TEST(test_complementary_filter);
    RUN_TEST(test_kalman_filter);
    RUN_TEST(test_ekf);
END_TEST_SUITE()
