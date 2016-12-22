//
// Created by ancalled on 12/20/16.
//


#include <Decoder.h>
#include "gtest/gtest.h"
#include "Synthesizer.h"
#include <cstring>

using namespace std;

#define SR_SIZE 4

static const uint32_t SAMPLE_RATES[SR_SIZE] = {
        19000,
        38000,
        44100,
        62500
};

size_t levDist(const char *s, size_t n, const char *t, size_t m) {
    ++n;
    ++m;
    size_t *d = new size_t[n * m];

    memset(d, 0, sizeof(size_t) * n * m);

    for (size_t i = 1, im = 0; i < m; ++i, ++im) {
        for (size_t j = 1, jn = 0; j < n; ++j, ++jn) {
            if (s[jn] == t[im]) {
                d[(i * n) + j] = d[((i - 1) * n) + (j - 1)];
            } else {
                d[(i * n) + j] = min(d[(i - 1) * n + j] + 1, /* A deletion. */
                                     min(d[i * n + (j - 1)] + 1, /* An insertion. */
                                         d[(i - 1) * n + (j - 1)] + 1)); /* A substitution. */
            }
        }
    }

    size_t r = d[n * m - 1];
    delete[] d;
    return r;
}

string randomMes() {
    int len = rand() % 50;
    string mes = "";
    for (int i = 0; i < len; i++) {
        char symbol = wsl::SYMBOLS[rand() % SYMBS].symbol;
        mes += symbol;
    }
    return mes;
}


TEST(WhistleTest, Generate) {
    const char *mes = "hjntdb982ilj6etj6e3l\0";
    size_t mesLen = strlen(mes);

    for (int i = 0; i < SR_SIZE; i++) {
        uint32_t sampleRate = SAMPLE_RATES[i];

        Synthesizer synth(sampleRate);

        uint32_t size = (uint32_t) (mesLen * sampleRate * (TOP_TIME + RAMP_TIME) / 1000);
        int8_t samples[size];

        uint32_t gen = synth.generate(samples, size, mes);
        EXPECT_EQ(size, gen);
    }
};


TEST(WhistleTest, CodeAndDecode) {
//    uint32_t sampleRate = 44100;
    uint32_t sampleRate = 62500;
    Synthesizer synth(sampleRate);


//    for (int i = 0; i < 100; i++) {
//        string toEncode = randomMes();
//        string toEncode = "hjntdb982ilj6etj6e3l\0";
//    string toEncode = "uf0c4lq8bg9iuiml2cftv87tevi10eonnek27nu71o9er2l2\0";
    string toEncode = "5v030l\0";


        uint32_t size = (uint32_t) (toEncode.size() * (sampleRate * (TOP_TIME + RAMP_TIME) / 1000)) + 1;
        int8_t samples[size];

        uint32_t gen = synth.generate(samples, size, toEncode.c_str());
//        printf("Generated %d samples\n", gen);

        // Decoding

        int8_t framesPerSound = 10;
        uint16_t frameSize = (uint16_t) (sampleRate * (RAMP_TIME + TOP_TIME) / 1000 / framesPerSound);

        Decoder decoder(sampleRate, frameSize);
        int frame = 0;
        uint32_t n = 0;
        int16_t buf[frameSize];

        while (n < gen) {
            uint32_t sz = n + frameSize < gen ? frameSize : gen - n;
            for (int i = 0; i < sz; i++) {
                buf[i] = (int16_t) (samples[n + i] - 127);
            }

            decoder.processFrame(buf, 0);

            n += frameSize;
            frame++;
        }

        string decoded = decoder.getMessage();
//        cout << "Decoded: " << decoded << endl;
        EXPECT_EQ(toEncode, decoded);
        size_t dist = levDist(toEncode.c_str(), toEncode.size(), decoded.c_str(), decoded.size());
        cout << "Dist: " << dist << endl;
//    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}