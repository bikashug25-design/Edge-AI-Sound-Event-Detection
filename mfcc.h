
#pragma once

#include <Arduino.h>

// ========================================
// AUDIO CONFIGURATION
// ========================================

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_WINDOW_SIZE 16000

// ========================================
// MFCC CONFIGURATION
// ========================================

#define MFCC_FFT_SIZE 512
#define MFCC_HOP_LENGTH 256

#define MFCC_NUM_MELS 40
#define MFCC_NUM_COEFFICIENTS 20

#define MFCC_NUM_FRAMES 63

// ========================================
// FEATURE CONFIGURATION
// ========================================

// 20 MFCC mean values
// 20 MFCC standard deviation values

#define MFCC_NUM_FEATURES 40

// ========================================
// MFCC FEATURE EXTRACTION FUNCTION
// ========================================

bool extractMFCCFeatures(
    const float* audio,
    float* features
);
