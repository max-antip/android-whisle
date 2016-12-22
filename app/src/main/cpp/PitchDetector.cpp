//
// Created by Andrey Koltochnik on 11/12/16.
//

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include "PitchDetector.h"


PitchDetector::PitchDetector(u_int32_t sr, u_int32_t bufSize, float minFreq, float maxFreq) :
        bufferSize(bufSize), sampleRate(sr) {

    probability = 0.0;
    minLag = (uint16_t) (sr / maxFreq);
//    minLag = 1;
    maxLag = (uint16_t) (sr / minFreq);
    buf = (float *) malloc(sizeof(float) * maxLag);

}


float PitchDetector::getProbability() {
    return probability;
}

float PitchDetector::getPitch(int16_t *samples, uint32_t from, uint32_t size, float threshold) {
    int tauEstimate;
    float pitchInHertz = -1;

    difference(samples, from, size);

    cumulativeMeanNormalizedDifference();
    tauEstimate = absoluteThreshold(threshold);

    float ftau = -1;
    if (tauEstimate != -1) {
        ftau = parabolicInterpolation(tauEstimate);
        pitchInHertz = sampleRate / ftau;
    }

    return pitchInHertz;
}


void PitchDetector::difference(int16_t *samples, uint32_t from, uint32_t size) {
    int i;
    int tau;
    int16_t delta;
    for (tau = minLag; tau < maxLag; tau++) {
        int ln = size > maxLag ? maxLag : size;
        for (i = from; i < from + ln; i++) {
            delta = samples[i] - samples[i + tau];
            float sqr = delta * delta;
            if (i == from) {
                buf[tau] = sqr;
            } else {
                buf[tau] += sqr;
            }
        }
    }
}

void PitchDetector::cumulativeMeanNormalizedDifference() {
    int tau;
    buf[minLag] = 1;
    float runningSum = 0;
    for (tau = minLag + 1; tau < maxLag; tau++) {
        runningSum += buf[tau];
//        buf[tau] *= tau / runningSum;
        buf[tau] *= (tau - minLag) / runningSum;
    }
}

int PitchDetector::absoluteThreshold(float threshold) {
    int tau;
    for (tau = minLag + 2; tau < maxLag; tau++) {
        float v = buf[tau];
        if (v < threshold) {
            while (tau + 1 < maxLag && buf[tau + 1] < buf[tau]) {
                tau++;
            }

            probability = 1 - buf[tau];
            break;
        }
    }
    // if no pitch found, tau => -1
    if (tau == maxLag || buf[tau] >= threshold) {
        tau = -1;
        probability = 0;
    }
    return tau;
}


float PitchDetector::parabolicInterpolation(int tauEstimate) {
    float betterTau;
    int x0;
    int x2;

    if (tauEstimate < 1) {
        x0 = tauEstimate;
    } else {
        x0 = tauEstimate - 1;
    }
    if (tauEstimate + 1 < maxLag) {
        x2 = tauEstimate + 1;
    } else {
        x2 = tauEstimate;
    }
    if (x0 == tauEstimate) {
        if (buf[tauEstimate] <= buf[x2]) {
            betterTau = tauEstimate;
        } else {
            betterTau = x2;
        }
    } else if (x2 == tauEstimate) {
        if (buf[tauEstimate] <= buf[x0]) {
            betterTau = tauEstimate;
        } else {
            betterTau = x0;
        }
    } else {
        float s0, s1, s2;
        s0 = buf[x0];
        s1 = buf[tauEstimate];
        s2 = buf[x2];
        // fixed AUBIO implementation, thanks to Karl Helgason:
        // (2.0f * s1 - s2 - s0) was incorrectly multiplied with -1
        betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
    }
    return betterTau;
}

float *PitchDetector::getBuf() {
    return buf;
}
