#ifndef EDGE_INTELLIGENCE_FRAMEWORK_H
#define EDGE_INTELLIGENCE_FRAMEWORK_H

/**
 * @file EdgeIntelligenceFramework.h
 * @brief Master header for Arduino compatibility.
 *
 * In Arduino, users typically include <LibraryName.h>.
 * This file facilitates that usage pattern.
 */

// Core Framework
#include "../core/include/eif_core.h"
#include "../core/include/eif_utils.h"

// DSP Modules
#include "../dsp/include/eif_dsp.h"
#include "../dsp/include/eif_opus.h"

// Model Runtime
#include "../ml/include/eif_model.h"

#endif // EDGE_INTELLIGENCE_FRAMEWORK_H
