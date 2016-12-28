//
// Created by ancalled on 12/15/16.
//

#ifndef WHISTLE_DECODER_H
#define WHISTLE_DECODER_H


#include <cstdint>
#include <vector>
#include <string>
#include "PitchDetector.h"

//#define DETECTOR_THRESHOLD 0.15
#define DETECTOR_THRESHOLD 0.60
#define FREQ_DIFF 0.001

class Decoder {

public:
    Decoder(uint32_t sr, uint16_t frame);

    void processFrame(int16_t *samples, uint32_t from = 0);

    const std::string &getMessage() const;

    void clearState();

private:
    struct Candidate {
        float freq;
        int transFrames;
        int sustFrames;
        char symbol;
        float error;
    };

    struct SymbMatch {
        char symbol;
        float error;
    };

    enum State {
        NONE = 1,
        TRANSITION_FREQ = 2,
        SUSTAINED_FREQ = 3,
        FREQ_ERROR = 4,
    };

    const uint32_t sampleRate;
    const float minFreq;
    const float maxFreq;
    uint16_t frameSize;
    uint8_t transitionFrames;
    uint8_t sustainedFrames;
    PitchDetector detector;
    State state = NONE;
    Candidate candidate = {0, 0, '\0', 0};


    std::string message;

    SymbMatch match(float pitch);

    float abs(float val);

    void initCandidate(float pitch);
};


#endif //WHISTLE_DECODER_H
