//
// Created by ancalled on 12/15/16.
//

#include "Decoder.h"
#include "Synthesizer.h"
#include <android/log.h>

#define  LOG_TAG    "WHISTLE-LIB"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


using namespace wsl;

Decoder::Decoder(uint32_t sr, uint16_t frame) :
        sampleRate(sr),
        minFreq(1000),
        maxFreq(20000),
        frameSize(frame),
        detector(sr, 512, minFreq, maxFreq) {

    transitionFrames = (uint8_t) (sampleRate * RAMP_TIME / 1000 / frameSize);
    sustainedFrames = (uint8_t) (sampleRate * TOP_TIME / 1000 / frameSize);
}

void Decoder::processFrame(int16_t *samples, uint32_t from) {
    float pitch = detector.getPitch(samples, from, frameSize, DETECTOR_THRESHOLD);
    if (pitch > 0) {
        if (state == NONE) state = TRANSITION_FREQ;

        if (candidate.freq > 0 /*state == TRANSITION_FREQ || state == SUSTAINED_FREQ*/) {

            float diff = abs((pitch - candidate.freq) / (maxFreq - minFreq));
            if (diff < FREQ_DIFF) {
                if (candidate.transFrames < transitionFrames) {
                    candidate.transFrames++;
                } else {
                    if (state != SUSTAINED_FREQ) state = SUSTAINED_FREQ;
                    candidate.sustFrames++;
                }
            } else {
//                LOGD("%.2f\t%c\t%.2f\t%d\n",
//                       candidate.freq,
//                       candidate.symbol,
//                       candidate.error,
//                       candidate.sustFrames);
                initCandidate(pitch);
            }

            candidate.freq = pitch;

            if (candidate.sustFrames == sustainedFrames) {
                message += candidate.symbol;
                initCandidate(pitch);
            }

        } else {
            initCandidate(pitch);
        }
    }

}


float Decoder::abs(float val) {
    return val > 0 ? val : -val;
}

Decoder::SymbMatch Decoder::match(float pitch) {
    const sound_symbol &first = SYMBOLS[0];
    if (first.freq - 40 < pitch && pitch < first.freq) {
        float er = abs(first.freq - pitch) / 100;
        return {first.symbol, er};
    }

    for (int i = 0; i < SYMBS - 1; i++) {
        sound_symbol s1 = SYMBOLS[i];
        sound_symbol s2 = SYMBOLS[i + 1];
        if (s1.freq < pitch && pitch <= s2.freq) {
            sound_symbol found = s2.freq - pitch > pitch - s1.freq ? s1 : s2;
            float er = abs(found.freq - pitch) / (s2.freq - s1.freq);
            return {found.symbol, er};
        }
    }

    const sound_symbol &last = SYMBOLS[SYMBS - 1];
    if (last.freq < pitch && pitch < last.freq + 300) {
        float er = abs(last.freq - pitch) / 500;
        return {last.symbol, er};
    }

    return {0, 0};
}

const std::string &Decoder::getMessage() const {
    return message;
}

void Decoder::clearState() {
    message.clear();
    candidate.freq = 0;
    candidate.sustFrames = 0;
    candidate.symbol = '\0';
    candidate.error = 0;
}

void Decoder::initCandidate(float pitch) {
    candidate.freq = pitch;
    candidate.sustFrames = 1;
    candidate.transFrames = 1;
    SymbMatch m = match(pitch);
    if (m.symbol != '\0') {
        candidate.symbol = m.symbol;
        candidate.error = m.error;
    }
}