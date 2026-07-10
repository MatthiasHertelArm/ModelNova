/*
 * Common Inference Library - postprocessing (source implementation)
 *
 * Clean-room replacement for the prebuilt cil.a postprocess path for image
 * classification: numerically stable softmax (max subtraction, expf per
 * element, divide by sum - the same structure as the binary) followed by
 * argmax. Populates postprocess_data_t with the winning class index and its
 * softmax probability. The class count comes from the generated
 * model_config.h instead of the binary's runtime shape parsing.
 */

#pragma STDC FP_CONTRACT OFF

#include <math.h>
#include <string.h>
#include "cil.h"
#include "model_config.h"

status_t postprocess(void *inference_result, postprocess_data_t *result)
{
    if ((inference_result == NULL) || (result == NULL)) {
        return STATUS_INVALID_ARG;
    }

    const float *logits = (const float *)inference_result;
    const int n = MODEL_NUM_CLASSES;

    /* Numerically stable softmax */
    float max_logit = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
        }
    }

    float sum = 0.0f;
    float probs[MODEL_NUM_CLASSES];
    for (int i = 0; i < n; i++) {
        probs[i] = expf(logits[i] - max_logit);
        sum += probs[i];
    }

    int best = 0;
    float best_p = probs[0];
    for (int i = 1; i < n; i++) {
        if (probs[i] > best_p) {
            best_p = probs[i];
            best = i;
        }
    }

    result->max_confidence       = best_p / sum;
    result->detected_class_index = best;
    result->detections           = NULL;
    result->detection_count      = 0;

    return STATUS_OK;
}
