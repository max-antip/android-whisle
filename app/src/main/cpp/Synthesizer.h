//
// Created by ancalled on 12/9/16.
//

#ifndef AUDIOPROTOCOL_SYNTHESIZER_H
#define AUDIOPROTOCOL_SYNTHESIZER_H


#include <cstdint>
#define SYMBS 32

#define RAMP_TIME 12.00
#define TOP_TIME 75.36
# define PI		3.14159265358979323846

namespace wsl {
    struct sound_symbol {
        char symbol;
        float freq;
        uint64_t phaseStep;
    };

    const sound_symbol SYMBOLS[SYMBS] = {
            {'0', 1760.00,  0xE6AFCUL},
            {'1', 1864.66,  0xF4679UL},
            {'2', 1975.53,  0x102EFCUL},
            {'3', 2093.00,  0x112556UL},
            {'4', 2217.46,  0x122A59UL},
            {'5', 2349.32,  0x133EE1UL},
            {'6', 2489.02,  0x1463DAUL},
            {'7', 2637.02,  0x159A3BUL},
            {'8', 2793.83,  0x16E316UL},
            {'9', 2959.96,  0x183F7CUL},
            {'a', 3135.96,  0x19B095UL},
            {'b', 3322.44,  0x1B37A9UL},
            {'c', 3520.00,  0x1CD5F9UL},
            {'d', 3729.31,  0x1E8CEEUL},
            {'e', 3951.07,  0x205DFEUL},
            {'f', 4186.01,  0x224AB3UL},
            {'g', 4434.92,  0x2454B3UL},
            {'h', 4698.64,  0x267DC3UL},
            {'i', 4978.03,  0x28C7AFUL},
            {'j', 5274.04,  0x2B3476UL},
            {'k', 5587.65,  0x2DC626UL},
            {'l', 5919.91,  0x307EF3UL},
            {'m', 6271.93,  0x336130UL},
            {'n', 6644.88,  0x366F52UL},
            {'o', 7040.00,  0x39ABF3UL},
            {'p', 7458.62,  0x3D19DCUL},
            {'q', 7902.13,  0x40BBF7UL},
            {'r', 8372.02,  0x449566UL},
            {'s', 8869.84,  0x48A967UL},
            {'t', 9397.27,  0x4CFB80UL},
            {'u', 9956.06,  0x518F5FUL},
            {'v', 10548.08, 0x5668EDUL}
    };

//    const sound_symbol SYMBOLS[SYMBS] = {
//            {'0', 1462, 0x2765A5UL},
//            {'1', 1583, 0x2AA85DUL},
//            {'2', 1727, 0x2E89C1UL},
//            {'3', 1900, 0x333333UL},
//            {'4', 2111, 0x38E2C9UL},
//            {'5', 2375, 0x400000UL},
//            {'6', 2714, 0x492299UL},
//            {'7', 3167, 0x5557A2UL},
//            {'8', 3800, 0x666666UL},
//            {'9', 4750, 0x800000UL}
//    };
}

class Synthesizer {

public:


    Synthesizer(uint32_t sampleRate);

    uint32_t generate(int8_t *samples, uint32_t size, const char *mes);

    wsl::sound_symbol findSymbol(char ch);

    uint32_t expectedSize(size_t symbols);

private:
    uint32_t sampleRate;
    int frame;
    int argument;
    uint16_t rampSamples;
    uint16_t topSamples;
public:
};


#endif //AUDIOPROTOCOL_SYNTHESIZER_H
