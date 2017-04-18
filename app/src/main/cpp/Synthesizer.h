//
// Created by ancalled on 12/9/16.
//

#ifndef AUDIOPROTOCOL_SYNTHESIZER_H
#define AUDIOPROTOCOL_SYNTHESIZER_H


#include <cstdint>

#define SYMBS 32

#define RAMP_TIME 12.00
#define TOP_TIME 75.36
#define PI        3.14159265358979323846

namespace wsl {
    struct sound_symbol {
        char symbol;
        float freq;
    };

    const sound_symbol SYMBOLS[SYMBS] = {
            {'0', 1760.00},
            {'1', 1864.66},
            {'2', 1975.53},
            {'3', 2093.00},
            {'4', 2217.46},
            {'5', 2349.32},
            {'6', 2489.02},
            {'7', 2637.02},
            {'8', 2793.83},
            {'9', 2959.96},
            {'a', 3135.96},
            {'b', 3322.44},
            {'c', 3520.00},
            {'d', 3729.31},
            {'e', 3951.07},
            {'f', 4186.01},
            {'g', 4434.92},
            {'h', 4698.64},
            {'i', 4978.03},
            {'j', 5274.04},
            {'k', 5587.65},
            {'l', 5919.91},
            {'m', 6271.93},
            {'n', 6644.88},
            {'o', 7040.00},
            {'p', 7458.62},
            {'q', 7902.13},
            {'r', 8372.02},
            {'s', 8869.84},
            {'t', 9397.27},
            {'u', 9956.06},
            {'v', 10548.0}
    };

    static uint8_t toNum(char c) {
        if (c < '0') {
            return 128;
        } else if (c <= '9') {
            return c - 48;
        } else if (c < 'a') {
            return 128;
        } else if (c <= 'v') {
            return c + 10 - 97;
        } else {
            return 128;
        }
    }

    static char toChar(uint8_t n) {
        if (n < 0) {
            return '\0';
        } else if (n < 10) {
            return 48 + n;
        } else if (n < 32) {
            return 97 + (n - 10);
        } else {
            return '\0';
        }
    }


}

class Synthesizer {

public:

    Synthesizer(uint32_t sampleRate);

    uint32_t generate(int16_t *samples, uint32_t size, const char *mes, double volume = 1.0);

    wsl::sound_symbol findSymbol(char ch);

    uint32_t expectedSize(size_t symbols);

private:
    const uint32_t sampleRate;
    const uint16_t rampSamples;
    const uint16_t topSamples;
};


#endif //AUDIOPROTOCOL_SYNTHESIZER_H
