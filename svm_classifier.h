
#pragma once

#include <Arduino.h>
#include <math.h>
#include "svm_model.h"

// Number of support vectors belonging to each class
static const int CLASS_SUPPORT[N_CLASSES] = {
    108, 344, 243
};

// Starting index of each class in support_vectors
static const int CLASS_START[N_CLASSES] = {
    0, 108, 452
};

// Returns:
// 0 = alarm
// 1 = background
// 2 = impact

int predictSoundClass(const float* rawFeatures) {

    float scaledFeatures[N_FEATURES];

    // Step 1: StandardScaler normalization
    for (int f = 0; f < N_FEATURES; f++) {
        scaledFeatures[f] =
            (rawFeatures[f] - scaler_mean[f]) /
            scaler_scale[f];
    }

    // Step 2: Calculate RBF kernel values
    float kernelValues[N_SUPPORT];

    for (int s = 0; s < N_SUPPORT; s++) {

        float squaredDistance = 0.0f;

        for (int f = 0; f < N_FEATURES; f++) {

            float difference =
                scaledFeatures[f] -
                support_vectors[s * N_FEATURES + f];

            squaredDistance += difference * difference;
        }

        kernelValues[s] =
            expf(-SVM_GAMMA * squaredDistance);
    }

    // Step 3: One-vs-One classification
    int votes[N_CLASSES] = {0};

    int pairIndex = 0;

    for (int i = 0; i < N_CLASSES; i++) {

        for (int j = i + 1; j < N_CLASSES; j++) {

            float decision = intercept[pairIndex];

            // Support vectors belonging to class i
            for (int s = CLASS_START[i];
                 s < CLASS_START[i] + CLASS_SUPPORT[i];
                 s++) {

                decision +=
                    dual_coef[(j - 1) * N_SUPPORT + s] *
                    kernelValues[s];
            }

            // Support vectors belonging to class j
            for (int s = CLASS_START[j];
                 s < CLASS_START[j] + CLASS_SUPPORT[j];
                 s++) {

                decision +=
                    dual_coef[i * N_SUPPORT + s] *
                    kernelValues[s];
            }

            if (decision > 0.0f) {
                votes[i]++;
            } else {
                votes[j]++;
            }

            pairIndex++;
        }
    }

    // Step 4: Select class with maximum votes
    int bestClass = 0;

    for (int i = 1; i < N_CLASSES; i++) {

        if (votes[i] > votes[bestClass]) {
            bestClass = i;
        }
    }

    return bestClass;
}
