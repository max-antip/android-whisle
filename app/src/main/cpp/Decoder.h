//
// Created by ancalled on 12/15/16.
//

#ifndef WHISTLE_DECODER_H
#define WHISTLE_DECODER_H


#include <cstdint>
#include <vector>
#include <string>
#include "PitchDetector.h"

class Decoder {

public:
    Decoder(uint32_t sr, uint16_t frame);

    void processFrame(int16_t *samples, uint32_t from);

    const std::string &getMessage() const;

private:
    struct Candidate {
        float freq;
        int frames;
        char symbol;
        float error;
    };

    struct SymbMatch {
        char symbol;
        float error;
    };

    const uint32_t sampleRate;
    const float minFreq;
    const float maxFreq;
    uint16_t frameSize;
    uint8_t framesToDetect;
    PitchDetector detector;
    Candidate candidate = {0, 0, '\0', 0};


    std::string message;

    SymbMatch match(float pitch);

    float abs(float val);
};


#endif //WHISTLE_DECODER_H
