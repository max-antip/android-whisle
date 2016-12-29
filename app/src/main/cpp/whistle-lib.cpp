#include <jni.h>
#include <string>
#include "Synthesizer.h"
#include "Decoder.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <assert.h>

#define  LOG_TAG    "WHISTLE-LIB"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace std;

SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;

SLObjectItf engineObject = NULL;
SLObjectItf outputMixObject = NULL;
SLEffectSendItf bqPlayerEffectSend;
SLVolumeItf bqPlayerVolume;
SLObjectItf recorderObject = NULL;
SLRecordItf recorderItf;
SLAndroidSimpleBufferQueueItf recBufQueue;
SLmilliHertz bqSampleRateMillis = 0;
SLObjectItf bqPlayerObject = NULL;
SLPlayItf bqPlayerItf;
SLEngineItf engineItf;


//short *nextBuffer;
//unsigned nextSize;
//int nextCount;

const int gBufNum = 2;

uint32_t gSampleRate;
uint16_t gFrameSize;
short **gRecBuffers;
int gCurrBuf = 0;
int gProcessed = 0;

//Decoder decoder(sampleRate, frameSize);
Decoder *gDecoder = nullptr;

JavaVM *gJvm = nullptr;
jobject jCallerInstance;
jmethodID jShowSamplesMID;
jmethodID jShowPitchesMID;
jshortArray jSamplesArray;

time_t recordStart;


JNIEnv *getEnv() {
    JNIEnv *env;
    int status = gJvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status < 0) {
        status = gJvm->AttachCurrentThread(&env, NULL);
        if (status < 0) {
            return nullptr;
        }
    }
    return env;
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *pjvm, void *reserved) {
    gJvm = pjvm;  // cache the JavaVM pointer
//    auto env = getEnv();
    return JNI_VERSION_1_6;
}


extern "C" JNIEXPORT jbyteArray JNICALL
Java_viaphone_com_whistle_MainActivity_slPlay(JNIEnv *env, jobject instance, jstring str_) {

    const char *mes = env->GetStringUTFChars(str_, JNI_FALSE);

    Synthesizer synth(gSampleRate);

    uint32_t size = (uint32_t) (strlen(mes) * (gSampleRate * (TOP_TIME + RAMP_TIME) / 1000)) + 1;
    int8_t samples[size];

    uint32_t gen = synth.generate(samples, size, mes);
    if (pthread_mutex_trylock(&audioEngineLock)) {
        // If we could not acquire audio engine lock, reject this request and client should re-try
        return JNI_FALSE;
    }

    SLresult result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, (short *) samples, gen);

    if (SL_RESULT_SUCCESS != result) {
        pthread_mutex_unlock(&audioEngineLock);
        return JNI_FALSE;
    }
    jbyteArray arr = env->NewByteArray(size);
    env->SetByteArrayRegion(arr, 0, size, samples);
    return arr;
}


void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);
    pthread_mutex_unlock(&audioEngineLock);
}


void startOrContinueRecording() {
    SLresult result;

    if (pthread_mutex_trylock(&audioEngineLock)) {
        return;
    }
    // in case already recording, stop recording and clear buffer queue
    result = (*recorderItf)->SetRecordState(recorderItf, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    result = (*recBufQueue)->Clear(recBufQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recBufQueue)->Enqueue(recBufQueue,
                                     gRecBuffers[gCurrBuf],
                                     gFrameSize * sizeof(short));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording
    result = (*recorderItf)->SetRecordState(recorderItf, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

}


void jSendSamplesToDraw(short *samples) {
    JNIEnv *env = getEnv();
    env->SetShortArrayRegion(jSamplesArray, 0, gFrameSize, samples);
    env->CallVoidMethod(jCallerInstance, jShowSamplesMID, jSamplesArray);
}

void jSendPitchToDraw(float pitch) {
    JNIEnv *env = getEnv();
    env->CallVoidMethod(jCallerInstance, jShowPitchesMID, pitch);
}


void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == recBufQueue);
    assert(NULL == context);


    pthread_mutex_unlock(&audioEngineLock);

    int procBuf = gCurrBuf;
    gCurrBuf = (gCurrBuf + 1) % gBufNum;

    startOrContinueRecording();

    short *samples = gRecBuffers[procBuf];
    jSendSamplesToDraw(samples);

    if (gDecoder) {
        float pitch = gDecoder->processFrame(samples);
        jSendPitchToDraw(pitch);

        string decoded = gDecoder->getMessage();
        if (decoded.size() > 0) {
//            LOGD("Decoded message: %s", decoded.c_str());
        }
    }

//    LOGD("Read: %d at %d", gProcessed++, (int) (time(0) - recordStart));
}


extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_slRecord(JNIEnv *env, jobject obj) {

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Recording activated!");
    startOrContinueRecording();
    recordStart = time(0);
}




extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_slInit(JNIEnv *env,
                                              jobject instance,
                                              jint jsampleRate,
                                              jint jframeSize) {

    gSampleRate = (uint32_t) jsampleRate;
    bqSampleRateMillis = gSampleRate * 1000;
//    int framesPerSound = 10;
//    gFrameSize = (uint16_t) (gSampleRate * (RAMP_TIME + TOP_TIME) / 1000 / framesPerSound);
    gFrameSize = (uint16_t) jframeSize;
    gDecoder = new Decoder(gSampleRate, gFrameSize);

    gRecBuffers = (short **) malloc(gBufNum * sizeof(short *));
    for (int buf = 0; buf < gBufNum; buf++) {
        gRecBuffers[buf] = (short *) malloc(sizeof(short) * gFrameSize);
        for (int i = 0; i < gFrameSize; i++) {
            gRecBuffers[buf][i] = 0;
        }
    }

    LOGD("SL Init: sample rate: %d", gSampleRate);
    LOGD("SL Init: frame size: %d", gFrameSize);

    //-----------------------------------------------

    SLresult result;

    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineItf)->CreateOutputMix(engineItf, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;


    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   1,
                                   bqSampleRateMillis,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER,
                                   SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    format_pcm.samplesPerSec = bqSampleRateMillis / 2;       //sample rate in milliseconds
//    format_pcm.bitsPerSample = 16;
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    const SLInterfaceID apids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean apreq[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineItf)->CreateAudioPlayer(engineItf, &bqPlayerObject, &audioSrc, &audioSnk,
                                             bqSampleRateMillis ? 2 : 3, apids, apreq);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerItf)->SetPlayState(bqPlayerItf, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;


    //-------------------------------------------

    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource recAudioSrc = {&loc_dev, NULL};

    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM recFormat_pcm = {SL_DATAFORMAT_PCM,
                                      1,
                                      bqSampleRateMillis,
                                      SL_PCMSAMPLEFORMAT_FIXED_16,
                                      SL_PCMSAMPLEFORMAT_FIXED_16,
                                      SL_SPEAKER_FRONT_CENTER,
                                      SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink recAudioSnk = {&loc_bq, &recFormat_pcm};

    const SLInterfaceID recId[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean recReq[1] = {SL_BOOLEAN_TRUE};
    result = (*engineItf)->CreateAudioRecorder(engineItf, &recorderObject, &recAudioSrc,
                                               &recAudioSnk, 1, recId, recReq);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                             &recBufQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recBufQueue)->RegisterCallback(recBufQueue, bqRecorderCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}


extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_slInitJavaCallBacks(JNIEnv *env, jobject inst,
                                                           jstring drawSamplesMthd,
                                                           jstring drawSamplesSign,
                                                           jstring drawPitchesMthd,
                                                           jstring drawPitchesSign) {
    jCallerInstance = reinterpret_cast<jobject>(env->NewGlobalRef(inst));
    jclass cls = env->GetObjectClass(jCallerInstance);
    jShowSamplesMID = env->
            GetMethodID(cls,
                        env->GetStringUTFChars(drawSamplesMthd, NULL),
                        env->GetStringUTFChars(drawSamplesSign, NULL));
    jShowPitchesMID = env->
            GetMethodID(cls,
                        env->GetStringUTFChars(drawPitchesMthd, NULL),
                        env->GetStringUTFChars(drawPitchesSign, NULL));

    jSamplesArray = reinterpret_cast<jshortArray>(env->NewGlobalRef(
            env->NewShortArray(gFrameSize)));
}


