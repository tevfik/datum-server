#include "eif_ts.h"
#include <string.h>
#include <math.h>

eif_status_t eif_ts_arima_init(eif_ts_arima_t* model, int p, int d, int q, eif_memory_pool_t* pool) {
    if (!model || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->p = p;
    model->d = d;
    model->q = q;
    
    if (p > 0) {
        model->ar_coeffs = eif_memory_alloc(pool, p * sizeof(float32_t), 4);
        model->history = eif_memory_alloc(pool, p * sizeof(float32_t), 4);
        if (!model->ar_coeffs || !model->history) return EIF_STATUS_OUT_OF_MEMORY;
        memset(model->ar_coeffs, 0, p * sizeof(float32_t));
        memset(model->history, 0, p * sizeof(float32_t));
    }
    
    if (q > 0) {
        model->ma_coeffs = eif_memory_alloc(pool, q * sizeof(float32_t), 4);
        model->errors = eif_memory_alloc(pool, q * sizeof(float32_t), 4);
        if (!model->ma_coeffs || !model->errors) return EIF_STATUS_OUT_OF_MEMORY;
        memset(model->ma_coeffs, 0, q * sizeof(float32_t));
        memset(model->errors, 0, q * sizeof(float32_t));
    }
    
    if (d > 0) {
        model->diff_history = eif_memory_alloc(pool, d * sizeof(float32_t), 4);
        if (!model->diff_history) return EIF_STATUS_OUT_OF_MEMORY;
        memset(model->diff_history, 0, d * sizeof(float32_t));
    }
    
    model->history_idx = 0;
    model->error_idx = 0;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ts_arima_predict(eif_ts_arima_t* model, float32_t input, float32_t* prediction) {
    if (!model || !prediction) return EIF_STATUS_INVALID_ARGUMENT;
    
    // 1. Differencing (Integration) handling
    // We need to reconstruct the non-differenced value from the prediction of the differenced series.
    // But usually we predict the next value given history.
    // If d=1, we model diff(t) = y(t) - y(t-1).
    // We predict diff(t+1). Then y(t+1) = y(t) + diff(t+1).
    
    // For simplicity, let's assume we are predicting the next value in the *differenced* domain first,
    // then integrating back.
    
    // But 'input' is the actual observed value at time t.
    // We update state with 'input', and predict t+1.
    
    float32_t current_diff = input;
    
    // Apply differencing to input to get stationary input
    if (model->d > 0) {
        // Simple d=1 support for now. For d>1 need loop.
        // diff(t) = input - last_input
        float32_t last = model->diff_history[0];
        current_diff = input - last;
        model->diff_history[0] = input; // Update for next time
    }
    
    // 2. Update State with current_diff (observed)
    
    // Update AR history
    if (model->p > 0) {
        model->history[model->history_idx] = current_diff;
        model->history_idx = (model->history_idx + 1) % model->p;
    }
    
    // Update MA errors
    // We need the error of the prediction we *would have made* for this step.
    // But we didn't calculate it (or we did but didn't store it).
    // Let's calculate "retroactive prediction" for error?
    // Or just use 0 for now to keep it simple as before.
    if (model->q > 0) {
        float32_t error = 0.0f; 
        model->errors[model->error_idx] = error;
        model->error_idx = (model->error_idx + 1) % model->q;
    }

    // 3. ARMA Prediction for NEXT step (t+1)
    // pred = sum(ar * hist) + sum(ma * err)
    float32_t pred_diff = 0.0f;
    
    // AR
    for (int i = 0; i < model->p; i++) {
        // history[history_idx-1] is the most recent (current_diff).
        int idx = (model->history_idx - 1 - i + model->p) % model->p;
        pred_diff += model->ar_coeffs[i] * model->history[idx];
    }
    
    // MA
    for (int i = 0; i < model->q; i++) {
        int idx = (model->error_idx - 1 - i + model->q) % model->q;
        pred_diff += model->ma_coeffs[i] * model->errors[idx];
    }
    
    // 4. Integrate back
    float32_t final_pred = pred_diff;
    if (model->d > 0) {
        final_pred += input; // y(t+1) = y(t) + diff(t+1)
    }
    
    *prediction = final_pred;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ts_arima_fit(eif_ts_arima_t* model, const float32_t* data, int length) {
    // Yule-Walker with Levinson-Durbin for AR(p)
    // Ignores MA and d for now (assumes data is stationary)
    if (!model || !data || length <= model->p) return EIF_STATUS_INVALID_ARGUMENT;
    
    if (model->p > 0) {
        int p = model->p;
        
        // Compute mean
        float32_t mean = 0.0f;
        for (int i = 0; i < length; i++) {
            mean += data[i];
        }
        mean /= length;
        
        // Autocorrelation (centered)
        float32_t r[32];  // Max AR order 32
        if (p > 31) p = 31;
        
        for (int k = 0; k <= p; k++) {
            float32_t sum = 0.0f;
            for (int i = 0; i < length - k; i++) {
                sum += (data[i] - mean) * (data[i + k] - mean);
            }
            r[k] = sum / length;
        }
        
        // Levinson-Durbin recursion
        float32_t a[32] = {0};  // AR coefficients
        float32_t a_prev[32] = {0};
        float32_t e = r[0];
        
        for (int m = 1; m <= p; m++) {
            // Compute reflection coefficient
            float32_t lambda = r[m];
            for (int j = 1; j < m; j++) {
                lambda -= a_prev[j] * r[m - j];
            }
            
            if (fabsf(e) < 1e-10f) {
                // Prevent division by zero
                break;
            }
            
            float32_t k_m = lambda / e;
            
            // Update coefficients
            a[m] = k_m;
            for (int j = 1; j < m; j++) {
                a[j] = a_prev[j] - k_m * a_prev[m - j];
            }
            
            // Update prediction error
            e = e * (1.0f - k_m * k_m);
            
            // Copy to a_prev for next iteration
            for (int j = 1; j <= m; j++) {
                a_prev[j] = a[j];
            }
        }
        
        // Store coefficients (Levinson uses 1-indexed, we use 0-indexed)
        for (int i = 0; i < model->p; i++) {
            model->ar_coeffs[i] = a[i + 1];
        }
        
        // Initialize history with last p values
        for (int i = 0; i < model->p && i < length; i++) {
            model->history[i] = data[length - model->p + i] - mean;
        }
        model->history_idx = 0;
    }
    
    // For MA coefficients, use simple initialization
    if (model->q > 0) {
        // Simple approach: set MA coefficients to small values
        for (int i = 0; i < model->q; i++) {
            model->ma_coeffs[i] = 0.1f / (i + 1);
        }
    }
    
    return EIF_STATUS_OK;
}
