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


struct OpenSLConf {
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    pthread_mutex_t audioEngineLock;

    SLObjectItf engineObject;
    SLObjectItf outputMixObject;
    SLVolumeItf bqPlayerVolume;
    SLObjectItf recorderObject;
    SLRecordItf recorderItf;
    SLAndroidSimpleBufferQueueItf recBufQueue;
    SLmilliHertz bqSampleRateMillis;
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerItf;
    SLEngineItf engineItf;
    SLuint32 numBuffers;
};

struct JCallBacks {
    jobject jCallerInstance;
    jmethodID jShowSamplesMID;
    jmethodID jShowPitchesMID;
    jshortArray jSamplesArray;
};

struct WhistleContext {

    uint32_t sampleRate;
    uint16_t recBufSize;
    uint16_t decFrameSize;
    short **recBuffers;
    short *decTmpBuf;
    int tmpBufCur;
    int currBuf;

    OpenSLConf *sl;
    Decoder *decoder;
    Synthesizer *synth;
    JCallBacks *jCallBacks;
};


JavaVM *gJvm = nullptr;

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


void play(JNIEnv *env, jobject instance, jstring str_, WhistleContext *c) {

    const char *mes = env->GetStringUTFChars(str_, JNI_FALSE);
    uint32_t size = (uint32_t) (strlen(mes) * (c->sampleRate * (TOP_TIME + RAMP_TIME) / 1000)) + 1;
    int8_t samples[size];

    uint32_t gen = c->synth->generate(samples, size, mes);
    if (pthread_mutex_trylock(&(c->sl->audioEngineLock))) {
        // If we could not acquire audio engine lock, reject this request and client should re-try
//        return JNI_FALSE;
    }

    SLresult result = (*c->sl->bqPlayerBufferQueue)->Enqueue(c->sl->bqPlayerBufferQueue,
                                                             (short *) samples,
                                                             gen);

    if (SL_RESULT_SUCCESS != result) {
        pthread_mutex_unlock(&(c->sl->audioEngineLock));
//        return JNI_FALSE;
    }
    jbyteArray arr = env->NewByteArray(size);
    env->SetByteArrayRegion(arr, 0, size, samples);
//    return arr;
}


void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    WhistleContext *c = (WhistleContext *) context;
    assert(bq == c->sl->bqPlayerBufferQueue);
    assert(NULL == context);
    pthread_mutex_unlock(&(c->sl->audioEngineLock));
}


void startOrContinueRecording(WhistleContext *c) {
    SLresult result;

    OpenSLConf *sl = c->sl;

    if (pthread_mutex_trylock(&(sl->audioEngineLock))) {
        return;
    }
    // in case already recording, stop recording and clear buffer queue
    result = (*sl->recorderItf)->SetRecordState(sl->recorderItf, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    result = (*(sl->recBufQueue))->Clear(sl->recBufQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*(sl->recBufQueue))->Enqueue(sl->recBufQueue,
                                           c->recBuffers[c->currBuf],
                                           c->recBufSize * sizeof(short));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording
    result = (*sl->recorderItf)->SetRecordState(sl->recorderItf, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

}


void jSendSamplesToDraw(short *samples, WhistleContext *c) {
    JNIEnv *env = getEnv();
    env->SetShortArrayRegion(c->jCallBacks->jSamplesArray, 0, c->recBufSize, samples);
    env->CallVoidMethod(c->jCallBacks->jCallerInstance, c->jCallBacks->jShowSamplesMID,
                        c->jCallBacks->jSamplesArray);
}

void jSendPitchToDraw(float pitch, WhistleContext *c) {
    JNIEnv *env = getEnv();
    env->CallVoidMethod(c->jCallBacks->jCallerInstance, c->jCallBacks->jShowPitchesMID, pitch);
}


void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
//    assert(NULL == context);
    WhistleContext *c = (WhistleContext *) context;
    OpenSLConf *sl = c->sl;
    assert(bq == sl->recBufQueue);

    pthread_mutex_unlock(&(sl->audioEngineLock));

    int procBuf = c->currBuf;
    c->currBuf = (c->currBuf + 1) % sl->numBuffers;

    startOrContinueRecording(c);

    short *samples = c->recBuffers[procBuf];
    jSendSamplesToDraw(samples, c);

    if (c->decoder) {
        uint32_t from = 0;
        if (c->tmpBufCur > 0) {
            std::copy(samples, samples + c->decFrameSize - c->tmpBufCur,
                      c->decTmpBuf + c->tmpBufCur);
            float pitch = c->decoder->processFrame(samples, from);
            jSendPitchToDraw(pitch, c);
            from += c->decFrameSize - c->tmpBufCur;
            c->tmpBufCur = 0;
        }

        for (; ;) {
            float pitch = c->decoder->processFrame(samples, from);
            jSendPitchToDraw(pitch, c);

            string decoded = c->decoder->getMessage();
            if (decoded.size() > 0) {
//            LOGD("Decoded message: %s", decoded.c_str());
            }

            if (from + c->decFrameSize < c->recBufSize) {
                from += c->decFrameSize;
            } else {
                std::copy(samples + from, samples + c->decFrameSize, c->decTmpBuf);
                c->tmpBufCur = c->decFrameSize - from;
                break;
            }
        }
    }

//    LOGD("Read: %d at %d", gProcessed++, (int) (time(0) - recordStart));
}


extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_slInit(JNIEnv *env,
                                              jobject instance,
                                              jint jsampleRate,
                                              jint jbufSize,
                                              jstring drawSamplesMthd,
                                              jstring drawSamplesSign,
                                              jstring drawPitchesMthd,
                                              jstring drawPitchesSign) {

    WhistleContext *c;
    c = (WhistleContext *) calloc(sizeof(WhistleContext), 1);

    c->sampleRate = (uint32_t) jsampleRate;

    OpenSLConf *sl;
    sl = (OpenSLConf *) calloc(sizeof(OpenSLConf), 1);


    sl->bqSampleRateMillis = c->sampleRate * 1000;
    sl->numBuffers = 2;
    c->synth = new Synthesizer(c->sampleRate);

    uint16_t sdnSymLen = (uint16_t) (c->sampleRate * (RAMP_TIME + TOP_TIME) / 1000);
    int framesPerSound = 10;
    c->decFrameSize = (uint16_t) (sdnSymLen / framesPerSound);
    c->recBufSize = (uint16_t) jbufSize;
    c->decoder = new Decoder(c->sampleRate, c->decFrameSize);

    c->recBuffers = (short **) malloc(sl->numBuffers * sizeof(short *));
    for (int buf = 0; buf < sl->numBuffers; buf++) {
        c->recBuffers[buf] = (short *) malloc(sizeof(short) * c->recBufSize);
        for (int i = 0; i < c->recBufSize; i++) {
            c->recBuffers[buf][i] = 0;
        }
    }
    c->currBuf = 0;

    c->decTmpBuf = (short *) malloc(sizeof(short) * c->decFrameSize);
    for (int i = 0; i < c->recBufSize; i++) {
        c->decTmpBuf[i] = 0;
    }
    c->tmpBufCur = 0;


    LOGD("SL Init: sample rate: %d", c->sampleRate);
    LOGD("SL Init: frame size: %d", c->recBufSize);

    //-----------------------------------------------

    SLresult result;

    result = slCreateEngine(&(sl->engineObject), 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*(sl->engineObject))->Realize(sl->engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects
    result = (*(sl->engineObject))->GetInterface(sl->engineObject, SL_IID_ENGINE, &(sl->engineItf));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*sl->engineItf)->CreateOutputMix(sl->engineItf, &(sl->outputMixObject), 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the output mix
    result = (*sl->outputMixObject)->Realize(sl->outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;


    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   1,
                                   sl->bqSampleRateMillis,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER,
                                   SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
//    format_pcm.samplesPerSec = bqSampleRateMillis /*/ 2*/;       //sample rate in milliseconds
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, sl->outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    const SLInterfaceID apids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean apreq[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*sl->engineItf)->CreateAudioPlayer(sl->engineItf, &(sl->bqPlayerObject), &audioSrc,
                                                 &audioSnk,
                                                 sl->bqSampleRateMillis ? 2 : 3, apids, apreq);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerObject)->Realize(sl->bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerObject)->GetInterface(sl->bqPlayerObject, SL_IID_PLAY,
                                                 &(sl->bqPlayerItf));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerObject)->GetInterface(sl->bqPlayerObject, SL_IID_BUFFERQUEUE,
                                                 &(sl->bqPlayerBufferQueue));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerBufferQueue)->RegisterCallback(sl->bqPlayerBufferQueue,
                                                          bqPlayerCallback, c);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerObject)->GetInterface(sl->bqPlayerObject, SL_IID_VOLUME,
                                                 &(sl->bqPlayerVolume));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->bqPlayerItf)->SetPlayState(sl->bqPlayerItf, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;


    //-------------------------------------------

    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,
                                      NULL};
    SLDataSource recAudioSrc = {&loc_dev, NULL};

    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            (SLuint32) sl->numBuffers};
    SLuint32 channels = 1;
    SLDataFormat_PCM recFormat_pcm = {SL_DATAFORMAT_PCM,
                                      channels,
                                      sl->bqSampleRateMillis,
                                      SL_PCMSAMPLEFORMAT_FIXED_16,
                                      SL_PCMSAMPLEFORMAT_FIXED_16,
                                      SL_SPEAKER_FRONT_CENTER,
                                      SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink recAudioSnk = {&loc_bq, &recFormat_pcm};

    const SLInterfaceID recId[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean recReq[1] = {SL_BOOLEAN_TRUE};
    result = (*sl->engineItf)->CreateAudioRecorder(sl->engineItf, &(sl->recorderObject),
                                                   &recAudioSrc,
                                                   &recAudioSnk, 1, recId, recReq);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->recorderObject)->Realize(sl->recorderObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->recorderObject)->GetInterface(sl->recorderObject, SL_IID_RECORD,
                                                 &(sl->recorderItf));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->recorderObject)->GetInterface(sl->recorderObject,
                                                 SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                 &(sl->recBufQueue));
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*sl->recBufQueue)->RegisterCallback(sl->recBufQueue, bqRecorderCallback, c);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // ---------------------------

    JCallBacks *jcb = (JCallBacks *) calloc(sizeof(JCallBacks), 1);
    jcb->jCallerInstance = reinterpret_cast<jobject>(env->NewGlobalRef(instance));
    jclass cls = env->GetObjectClass(jcb->jCallerInstance);
    jcb->jShowSamplesMID = env->
            GetMethodID(cls,
                        env->GetStringUTFChars(drawSamplesMthd, NULL),
                        env->GetStringUTFChars(drawSamplesSign, NULL));
    jcb->jShowPitchesMID = env->
            GetMethodID(cls,
                        env->GetStringUTFChars(drawPitchesMthd, NULL),
                        env->GetStringUTFChars(drawPitchesSign, NULL));

    jcb->jSamplesArray = reinterpret_cast<jshortArray>(env->NewGlobalRef(
            env->NewShortArray(c->recBufSize)));

    // ----------------------------

    c->sl = sl;
    c->jCallBacks = jcb;

    startOrContinueRecording(c);
}


