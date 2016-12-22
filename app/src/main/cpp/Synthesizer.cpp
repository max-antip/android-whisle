//
// Created by ancalled on 12/9/16.
//

#include <cstddef>
#include <cstring>
#include <cmath>
#include "Synthesizer.h"

using namespace wsl;

Synthesizer::Synthesizer(uint32_t sampleRate) : sampleRate(sampleRate) {
    frame = 0;
    argument = 0;
    rampSamples = (uint16_t) ((sampleRate * RAMP_TIME) / 1000);
    topSamples = (uint16_t) ((sampleRate * TOP_TIME) / 1000);
}


uint32_t Synthesizer::generate(int8_t *samples, uint32_t size, const char *mes) {
    uint32_t proc = 0;
    size_t len = strlen(mes);
    float freq = 0, prevFreq = 0;
    const uint8_t halfampl = 1 << 5;
    const uint8_t amplStep = (uint8_t) (rampSamples / halfampl);

    for (int i = 0; i < len; i++) {
        char ch = mes[i];
        sound_symbol ss = findSymbol(ch);
        if (ss.symbol != '\0') {
            if (!freq) {
                freq = ss.freq;
            }
            if (!prevFreq) {
                prevFreq = freq;
            }
            float freqGradient = (ss.freq - prevFreq) / rampSamples;
            uint8_t amplitude = halfampl;

            for (int j = 0; j < topSamples + rampSamples; j++) {
                double arg = (2 * PI * (argument++) * freq) / sampleRate;
                double sn = sin(arg);
                double smpl = amplitude * sn;
                int8_t bt = (int8_t) round(smpl);
                samples[proc++] = bt;
                if (j < rampSamples) {
                    freq += freqGradient;
                    if (!(j % amplStep) && !(amplitude & 0x80)) {
                        amplitude++;
                    }
                }

                if (proc >= size - 1) return proc;
            }

            frame++;
            prevFreq = ss.freq;
        }
    }
    return proc;
}


sound_symbol Synthesizer::findSymbol(char ch) {
    for (int i = 0; i < SYMBS; i++) {
        sound_symbol s = SYMBOLS[i];
        if (s.symbol == ch) {
            return s;
        }
    }
    return {'\0', -1, 0};
}

uint32_t Synthesizer::expectedSize(size_t symbols) {
    return symbols * (rampSamples + topSamples);
}
