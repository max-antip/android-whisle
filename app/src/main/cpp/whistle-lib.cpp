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
short *nextBuffer;
unsigned nextSize;
short *resampleBuf = NULL;
SLObjectItf engineObject = NULL;
SLObjectItf outputMixObject = NULL;
SLEffectSendItf bqPlayerEffectSend;
SLVolumeItf bqPlayerVolume;
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
SLObjectItf recorderObject = NULL;
SLRecordItf recorderRecord;
SLAndroidSimpleBufferQueueItf recorderBufferQueue;


// aux effect on the output mix, used by the buffer queue player
//static const SLEnvironmentalReverbSettings reverbSettings =
//        SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;


SLmilliHertz bqPlayerSampleRate = 0;
//static jint bqPlayerBufSize = 0;
SLObjectItf bqPlayerObject = NULL;
SLPlayItf bqPlayerPlay;
SLEngineItf engineEngine;
int nextCount;

//static const uint32_t sampleRate = 88200;
const uint32_t sampleRate = 44100;
const uint16_t frameSize = (uint16_t) (sampleRate * (RAMP_TIME + TOP_TIME) / 1000 / 10);
const int bufNum = 2;
short recorderBuffers[bufNum][frameSize];
int activeRecBuf = 0;

Decoder decoder(sampleRate, frameSize);

JavaVM *gJvm = nullptr;
jobject jCallerInstance;
jmethodID jShowSamplesMID;
jmethodID jShowPitchesMID;
jshortArray jSamplesArray;


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
    auto env = getEnv();
    jSamplesArray = reinterpret_cast<jshortArray>(env->NewGlobalRef(env->NewShortArray(frameSize)));
    return JNI_VERSION_1_6;
}


extern "C" JNIEXPORT jbyteArray JNICALL
Java_viaphone_com_whistle_MainActivity_play(JNIEnv *env, jobject instance, jstring str_) {


    const char *mes = env->GetStringUTFChars(str_, JNI_FALSE);

    Synthesizer synth(sampleRate);

    uint32_t size = (uint32_t) (strlen(mes) * (sampleRate * (TOP_TIME + RAMP_TIME) / 1000)) + 1;
    int8_t samples[size];

    uint32_t gen = synth.generate(samples, size, mes);
    if (pthread_mutex_trylock(&audioEngineLock)) {
        // If we could not acquire audio engine lock, reject this request and client should re-try
        return JNI_FALSE;
    }

    SLresult result;

    nextBuffer = (short *) samples;
//    nextSize = sizeof(samples);
    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, gen);

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
    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (--nextCount > 0 && NULL != nextBuffer && 0 != nextSize) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
        }
        (void) result;
    } else {
//        releaseResampleBuf();
        pthread_mutex_unlock(&audioEngineLock);
    }
}


extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_createBufferQueueAudioPlayer(JNIEnv *env,
                                                                    jobject obj
        /*, jint sampleRate,
        jint bufSize*/) {
    SLresult result;
    bqPlayerSampleRate = sampleRate * 1000;
//    if (sampleRate >= 0 && bufSize >= 0) {
    /*
     * device native buffer size is another factor to minimize audio latency, not used in this
     * sample: we only play one giant buffer here
     */
//        bqPlayerBufSize = bufSize;
//    }

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    format_pcm.samplesPerSec = bqPlayerSampleRate;       //sample rate in mili second
//    format_pcm.bitsPerSample = 16;
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    /*
     * create audio player:
     *     fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     *     for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME,
                                  SL_IID_EFFECTSEND, /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                              SL_BOOLEAN_TRUE, /*SL_BOOLEAN_TRUE,*/ };

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                bqPlayerSampleRate ? 2 : 3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the effect send interface
    bqPlayerEffectSend = NULL;
    if (0 == bqPlayerSampleRate) {
        result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                                 &bqPlayerEffectSend);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }


#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
#endif

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}


extern "C" JNIEXPORT  void JNICALL
Java_viaphone_com_whistle_MainActivity_createEngine(JNIEnv *env, jobject obj) {

    SLresult result;

    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
//    if (SL_RESULT_SUCCESS == result) {
//        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
//                outputMixEnvironmentalReverb, &reverbSettings);
//        (void) result;
//    }

}


void startOrContinueRecording() {
    SLresult result;

    if (pthread_mutex_trylock(&audioEngineLock)) {
        return;
    }
    // in case already recording, stop recording and clear buffer queue
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // the buffer is not valid for playback yet
//    recorderSize = 0;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we would enqueue at least 2 empty b jshortArray jsa = env->NewShortArray(frameSize);uffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recorderBuffers[activeRecBuf],
                                             frameSize * sizeof(short));
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error


    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

}


void initJavaCallBacks(JNIEnv *env, jobject inst,
                       jstring drawSamplesMthd,
                       jstring drawSamplesSign,
                       jstring drawPitchesMthd,
                       jstring drawPitchesSign) {
//    if (jInstance.IsSameObject(inst)) return;
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
}


void jSendSamplesToDraw(short *samples) {
    JNIEnv *env = getEnv();
    env->SetShortArrayRegion(jSamplesArray, 0, frameSize, samples);
    env->CallVoidMethod(jCallerInstance, jShowSamplesMID, jSamplesArray);
}

void jSendPitchToDraw(float pitch) {
    JNIEnv *env = getEnv();
    env->CallVoidMethod(jCallerInstance, jShowPitchesMID, pitch);
}


void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == recorderBufferQueue);
    assert(NULL == context);
    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording

    int procBuf = activeRecBuf;
    activeRecBuf = (activeRecBuf + 1) % bufNum;

    pthread_mutex_unlock(&audioEngineLock);
    startOrContinueRecording();

//    LOGD("Got next frame, rec buf: %d", procBuf);

    jSendSamplesToDraw(recorderBuffers[procBuf]);

    float pitch = decoder.processFrame(recorderBuffers[procBuf]);
    jSendPitchToDraw(pitch);

    string decoded = decoder.getMessage();
    if (decoded.size() > 0) {
        LOGD("Decoded message: %s", decoded.c_str());
    }
}


extern "C" JNIEXPORT void JNICALL
Java_viaphone_com_whistle_MainActivity_startRecording(JNIEnv *env, jobject thiz,
                                                      jstring drawSamplesMthd,
                                                      jstring drawSamplesSign,
                                                      jstring drawPitchesMthd,
                                                      jstring drawPitchesSign) {

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Recording activated!");

    initJavaCallBacks(env, thiz,
                      drawSamplesMthd, drawSamplesSign,
                      drawPitchesMthd, drawPitchesSign
    );

    startOrContinueRecording();
}



extern "C"
JNIEXPORT jboolean JNICALL
Java_viaphone_com_whistle_MainActivity_createAudioRecorder(JNIEnv *env, jobject obj) {
    SLresult res;

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    res = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
                                               &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != res) {
        return JNI_FALSE;
    }

    // realize the audio recorder
    res = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != res) {
        return JNI_FALSE;
    }

    // get the record interface
    res = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == res);
    (void) res;

    // get the buffer queue interface
    res = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                          &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == res);
    (void) res;

    // register callback on the buffer queue
    res = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback,
                                                   NULL);
    assert(SL_RESULT_SUCCESS == res);
    (void) res;

    return JNI_TRUE;
}