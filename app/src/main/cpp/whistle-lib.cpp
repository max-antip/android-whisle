#include <jni.h>
#include <stdint.h>
#include "Base32.h"
#include "Crc.h"


#define  LOG_TAG    "WHISTLE-LIB"

#define DEFAULT_ENCODE_SAMPLE_RATE 44100


extern "C" JNIEXPORT  jshortArray JNICALL
Java_viaphone_com_whistle_MainActivity_slPlay(JNIEnv
                                              *env, jobject instance,
                                              jstring mess_) {


    uint32_t sampleRate = DEFAULT_ENCODE_SAMPLE_RATE;

    const char *mes = env->GetStringUTFChars(mess_, JNI_FALSE);
    struct Base32 base32;
    char *body = base32.toWhistle(mes, strlen(mes));
    size_t size = strlen(body) + 4;
    char message[size];
    char *tail = generateCrc(body);
    strcpy(message, "hj");
    strcat(message, body);
    strcat(message, tail);

    Synthesizer synth(sampleRate);
    uint32_t expSize = synth.expectedSize(size);
    int16_t samples[expSize];
    uint32_t gensize = synth.generate(samples, expSize, message, 10);
    jbyteArray barr = env->NewByteArray(gensize);
    jshortArray arr = env->NewShortArray(gensize);
    env->SetShortArrayRegion(arr, 0, gensize, samples);
    return arr;

}

void Play(JNIEnv *env, jobject instance, jstring mess_) {

//    return play(samples, gensize, sampleRate);

}


extern "C" JNIEXPORT jstring JNICALL
Java_viaphone_com_whistle_MainActivity_test(JNIEnv *env, jobject instance) {
    return env->NewStringUTF("Hello From Jni");
}