#include "eif_ts.h"
#include <math.h>
#include <string.h>

eif_status_t eif_ts_hw_init(eif_ts_hw_t* model, int season_length, eif_ts_hw_type_t type, eif_memory_pool_t* pool) {
    if (!model || season_length <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->season_length = season_length;
    model->type = type;
    
    model->seasonals = eif_memory_alloc(pool, season_length * sizeof(float32_t), 4);
    if (!model->seasonals) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Defaults
    model->alpha = 0.3f;
    model->beta = 0.1f;
    model->gamma = 0.1f;
    
    model->initialized = false;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ts_hw_update(eif_ts_hw_t* model, float32_t input) {
    if (!model) return EIF_STATUS_INVALID_ARGUMENT;
    
    if (!model->initialized) {
        // Initialization logic (needs at least one season usually)
        // Simple init:
        model->level = input;
        model->trend = 0.0f; // Need more points for trend
        for(int i=0; i<model->season_length; i++) model->seasonals[i] = (model->type == EIF_TS_HW_MULTIPLICATIVE) ? 1.0f : 0.0f;
        model->initialized = true;
        return EIF_STATUS_OK;
    }
    
    // Holt-Winters Update
    // L_t = alpha * (Y_t - S_{t-s}) + (1-alpha) * (L_{t-1} + T_{t-1})
    // T_t = beta * (L_t - L_{t-1}) + (1-beta) * T_{t-1}
    // S_t = gamma * (Y_t - L_t) + (1-gamma) * S_{t-s}
    
    // We need to track index in seasonals circular buffer?
    // Or we assume update is called sequentially and we rotate seasonals?
    // Let's rotate seasonals: S[0] is always S_{t-s}. S[last] is S_t.
    // Actually, S_{t-s} is the seasonal component from 'season_length' steps ago.
    // If we have a buffer of length 'season_length', then S[0] is exactly that.
    // After update, we update S[0] and move it to end?
    // No, seasonal index depends on time t mod s.
    // We need to track time index or just rotate.
    // Let's rotate: S[0] is the seasonal factor for CURRENT step.
    // After update, we put new S at end (for next cycle) and shift?
    // No, S[0] is used for current, then updated, then becomes S for next cycle (which is season_length steps away).
    // Wait, S is periodic. S[t] corresponds to S[t-s].
    // So we just update S[current_season_idx].
    // We need to track current_season_idx.
    
    // Let's add season_idx to struct?
    // I forgot to add it in header.
    // I will assume season_idx = 0 and rotate buffer.
    
    float32_t s_last = model->seasonals[0];
    float32_t l_prev = model->level;
    float32_t t_prev = model->trend;
    
    float32_t l_curr, t_curr, s_curr;
    
    if (model->type == EIF_TS_HW_ADDITIVE) {
        l_curr = model->alpha * (input - s_last) + (1.0f - model->alpha) * (l_prev + t_prev);
        t_curr = model->beta * (l_curr - l_prev) + (1.0f - model->beta) * t_prev;
        s_curr = model->gamma * (input - l_curr) + (1.0f - model->gamma) * s_last;
    } else {
        // Multiplicative
        l_curr = model->alpha * (input / s_last) + (1.0f - model->alpha) * (l_prev + t_prev);
        t_curr = model->beta * (l_curr - l_prev) + (1.0f - model->beta) * t_prev;
        s_curr = model->gamma * (input / l_curr) + (1.0f - model->gamma) * s_last;
    }
    
    model->level = l_curr;
    model->trend = t_curr;
    
    // Update seasonal buffer (rotate)
    // Shift left, put new s at end?
    // No, S[0] was for this step. We need it again in 'season_length' steps.
    // So we put it at the end of the queue.
    memmove(model->seasonals, &model->seasonals[1], (model->season_length - 1) * sizeof(float32_t));
    model->seasonals[model->season_length - 1] = s_curr;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ts_hw_forecast(const eif_ts_hw_t* model, int steps, float32_t* forecast) {
    if (!model || !forecast || steps <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int h = 1; h <= steps; h++) {
        int s_idx = (model->season_length - 1 + h) % model->season_length; 
        // Wait, seasonals buffer is rotated.
        // seasonals[0] is for next step (t+1)? 
        // In update(), we used seasonals[0] for current step t.
        // Then we rotated. So seasonals[0] is now for t+1.
        // seasonals[1] is for t+2.
        // So for step h (1-based), we use seasonals[h-1].
        // But we need to handle h > season_length (wrap around).
        
        float32_t s_comp = model->seasonals[(h - 1) % model->season_length];
        
        if (model->type == EIF_TS_HW_ADDITIVE) {
            forecast[h-1] = model->level + h * model->trend + s_comp;
        } else {
            forecast[h-1] = (model->level + h * model->trend) * s_comp;
        }
    }
    
    return EIF_STATUS_OK;
}
