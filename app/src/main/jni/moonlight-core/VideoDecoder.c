//
// Created by Viktor Pih on 2020/8/6.
//

#include "VideoDecoder.h"
#include <stdlib.h>

#include <media/NdkMediaExtractor.h>

#include <sys/system_properties.h>

#include <pthread.h>

#include <android/log.h>

#   define  LOG_TAG    "VideoDecoder"
#   define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

typedef enum {
    INPUT_BUFFER_STATUS_INVALID,
    INPUT_BUFFER_STATUS_FREE,
    INPUT_BUFFER_STATUS_WORKING,
    INPUT_BUFFER_STATUS_QUEUING,
};

//
//Decoder decoder = {-1, NULL, NULL, NULL, 0, false, false, false, false};

int VIDEO_FORMAT_MASK_H264 = 0x00FF;


#define Build_VERSION_CODES_KITKAT 19
#define Build_VERSION_CODES_M 23
#define Build_VERSION_CODES_R 30

int sdk_version() {
    static char sdk_ver_str[PROP_VALUE_MAX+1];
    int sdk_ver = 0;
    if (__system_property_get("ro.build.version.sdk", sdk_ver_str)) {
        sdk_ver = atoi(sdk_ver_str);
    } else {
        // Not running on Android or SDK version is not available
        // ...
        LOGD("sdk_version fail!");
    }
    return sdk_ver;
}

// 获取空的输入缓冲区
bool getEmptyInputBuffer(VideoDecoder* videoDeoder, VideoInputBuffer* inputBuffer) {

    int bufidx = AMediaCodec_dequeueInputBuffer(videoDeoder->codec, 0);
    if (bufidx < 0) {
        return false;
    }

    size_t bufsize;
    void* buf = AMediaCodec_getInputBuffer(videoDeoder->codec, bufidx, &bufsize);
    if (buf == 0) {
        assert(!"error");
        return false;
    }

    inputBuffer->index = bufidx;
    inputBuffer->buffer = buf;
    inputBuffer->bufsize = bufsize;

    return true;
}

const int InputBufferMaxSize = 1;
pthread_mutex_t inputCache_lock = PTHREAD_MUTEX_INITIALIZER; // lock NDK API access

void makeInputBuffer(VideoDecoder* videoDeoder) {

    pthread_mutex_lock(&inputCache_lock); {

        for (int i = 0; i < InputBufferMaxSize; i++) {
            VideoInputBuffer* inputBuffer = &videoDeoder->inputBufferCache[i];
            if (inputBuffer->status == INPUT_BUFFER_STATUS_INVALID) {
                if (getEmptyInputBuffer(videoDeoder, inputBuffer)) {
                    inputBuffer->status = INPUT_BUFFER_STATUS_FREE;
                    LOGD("makeInputBuffer create inputBuffer");
                } else {
                    LOGD("makeInputBuffer fail!");
                }
            }
        }

    } pthread_mutex_unlock(&inputCache_lock);
}

void _queueInputBuffer(VideoDecoder* videoDeoder, int index) {

    VideoInputBuffer* inputBuffer = &videoDeoder->inputBufferCache[index];
    assert(inputBuffer->status == INPUT_BUFFER_STATUS_QUEUING);

    // push to codec
    AMediaCodec_queueInputBuffer(videoDeoder->codec, inputBuffer->index, 0, inputBuffer->bufsize, inputBuffer->timestampUs,
                                 inputBuffer->codecFlags);

    // mark to free
    inputBuffer->status = INPUT_BUFFER_STATUS_FREE;
    LOGD("_queueInputBuffer free %d", index);
}

void queueInputBuffer(VideoDecoder* videoDeoder) {

    pthread_mutex_lock(&inputCache_lock); {

        for (int i = 0; i < InputBufferMaxSize; i++) {
            
            VideoInputBuffer* inputBuffer = &videoDeoder->inputBufferCache[i];
            if (inputBuffer->status == INPUT_BUFFER_STATUS_QUEUING) {
                // Queue one input for each time
                LOGD("queueInputBuffer: %d", i);
                _queueInputBuffer(videoDeoder, i);
                break; // or continue
            }
        }        

    } pthread_mutex_unlock(&inputCache_lock);
}

VideoDecoder* VideoDecoder_create(JNIEnv *env, jobject surface, const char* name, const char* mimeType, int width, int height, int fps, int lowLatency) {

    int sdk_ver = sdk_version();

    // Codecs have been known to throw all sorts of crazy runtime exceptions
    // due to implementation problems
    AMediaCodec* codec = AMediaCodec_createCodecByName(name);

    AMediaFormat* videoFormat = AMediaFormat_new();
    AMediaFormat_setString(videoFormat, AMEDIAFORMAT_KEY_MIME, mimeType);

    // Avoid setting KEY_FRAME_RATE on Lollipop and earlier to reduce compatibility risk
    if (sdk_ver >= Build_VERSION_CODES_M) {
        // We use prefs.fps instead of redrawRate here because the low latency hack in Game.java
        // may leave us with an odd redrawRate value like 59 or 49 which might cause the decoder
        // to puke. To be safe, we'll use the unmodified value.
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    }

    // Adaptive playback can also be enabled by the whitelist on pre-KitKat devices
    // so we don't fill these pre-KitKat
    if (/* adaptivePlayback &&  */ sdk_ver >= Build_VERSION_CODES_KITKAT) {
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
    }

    if (sdk_ver >= Build_VERSION_CODES_R && lowLatency) {
        AMediaFormat_setInt32(videoFormat, "latency", lowLatency);
    }
    else if (sdk_ver >= Build_VERSION_CODES_M) {
//        // Set the Qualcomm vendor low latency extension if the Android R option is unavailable
//        if (MediaCodecHelper.decoderSupportsQcomVendorLowLatency(selectedDecoderName)) {
//            // MediaCodec supports vendor-defined format keys using the "vendor.<extension name>.<parameter name>" syntax.
//            // These allow access to functionality that is not exposed through documented MediaFormat.KEY_* values.
//            // https://cs.android.com/android/platform/superproject/+/master:hardware/qcom/sdm845/media/mm-video-v4l2/vidc/common/inc/vidc_vendor_extensions.h;l=67
//            //
//            // Examples of Qualcomm's vendor extensions for Snapdragon 845:
//            // https://cs.android.com/android/platform/superproject/+/master:hardware/qcom/sdm845/media/mm-video-v4l2/vidc/vdec/src/omx_vdec_extensions.hpp
//            // https://cs.android.com/android/_/android/platform/hardware/qcom/sm8150/media/+/0621ceb1c1b19564999db8293574a0e12952ff6c
//            videoFormat.setInteger("vendor.qti-ext-dec-low-latency.enable", 1);
//        }
//
//        if (MediaCodecHelper.decoderSupportsMaxOperatingRate(selectedDecoderName)) {
//            videoFormat.setInteger(MediaFormat.KEY_OPERATING_RATE, Short.MAX_VALUE);
//        }
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    media_status_t status = AMediaCodec_configure(codec, videoFormat, window, 0, 0);
    if (status != 0)
    {
        LOGD("AMediaCodec_configure() failed with error %i for format %u", (int)status, 21);
        return 0;
    }

//    videoDecoder.setVideoScalingMode(MediaCodec.VIDEO_SCALING_MODE_SCALE_TO_FIT);
//    if (USE_FRAME_RENDER_TIME && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
//        videoDecoder.setOnFrameRenderedListener(new MediaCodec.OnFrameRenderedListener() {
//            @Override
//            public void onFrameRendered(MediaCodec mediaCodec, long presentationTimeUs, long renderTimeNanos) {
//                long delta = (renderTimeNanos / 1000000L) - (presentationTimeUs / 1000);
//                if (delta >= 0 && delta < 1000) {
//                    if (USE_FRAME_RENDER_TIME) {
//                        activeWindowVideoStats.totalTimeMs += delta;
//                    }
//                }
//            }
//        }, null);
//    }
//
//    LimeLog.info("Using codec "+selectedDecoderName+" for hardware decoding "+mimeType);
//
//    // Start the decoder
//    videoDecoder.start();
//
//    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
//        legacyInputBuffers = videoDecoder.getInputBuffers();
//    }

    VideoDecoder* videoDeoder = (VideoDecoder*)malloc(sizeof(VideoDecoder));
    videoDeoder->window = window;
    videoDeoder->codec = codec;
    videoDeoder->stop = false;
    videoDeoder->stopCallback = 0;
    pthread_mutex_init(&videoDeoder->lock, 0);
    sem_init(&videoDeoder->rendering_sem, 0, 0);

    videoDeoder->inputBufferCache = malloc(sizeof(VideoInputBuffer)*InputBufferMaxSize);
    for (int i = 0; i < InputBufferMaxSize; i++) {
        VideoInputBuffer inputBuffer = {
            .index = 0,
            .buffer = 0,
            .bufsize = 0,
            .timestampUs = 0,
            .codecFlags = 0,
            .status = INPUT_BUFFER_STATUS_INVALID
        };
        
        videoDeoder->inputBufferCache[i] = inputBuffer;
    }

    return videoDeoder;
}

void releaseVideoDecoder(VideoDecoder* videoDeoder) {
    AMediaCodec_delete(videoDeoder->codec);

    pthread_mutex_destroy(&videoDeoder->lock);
    sem_destroy(&videoDeoder->rendering_sem);

    free(videoDeoder->inputBufferCache);
    free(videoDeoder);
}

void VideoDecoder_release(VideoDecoder* videoDeoder) {

    pthread_mutex_lock(&videoDeoder->lock);

    // 停止
    if (!videoDeoder->stop) {
        // release at stop
        videoDeoder->stopCallback = releaseVideoDecoder;
        VideoDecoder_stop(videoDeoder);
    } else {
        releaseVideoDecoder(videoDeoder);
    }

    LOGD("VideoDecoder_release: released!");

    pthread_mutex_unlock(&videoDeoder->lock);
}

pthread_t pid;

void* rendering_thread(VideoDecoder* videoDecoder)
{
    while(!videoDecoder->stop) {

        // Build input buffer cache
        makeInputBuffer(videoDecoder);

        // Waitting for a signal
        sem_wait(&videoDecoder->rendering_sem);
        if (videoDecoder->stop) break;

        // Queue one input for each time
        queueInputBuffer(videoDecoder);

        // Try to output a frame
        AMediaCodecBufferInfo info;
        int outIndex = AMediaCodec_dequeueOutputBuffer(videoDecoder->codec, &info, 0);
        if (outIndex >= 0) {
            AMediaCodec_releaseOutputBuffer(videoDecoder->codec, outIndex, 0);
        }
        
        LOGD("rendering_thread: Rendering ...");

//        sleep(1);
    }

    if (videoDecoder->stopCallback) {
        videoDecoder->stopCallback(videoDecoder);
    }

    LOGD("rendering_thread: Thread quited!");
}

void VideoDecoder_start(VideoDecoder* videoDeoder) {

    LOGD("VideoDecoder_start pthread_mutex_lock");
    pthread_mutex_lock(&videoDeoder->lock);

    assert(!videoDeoder->stop);

    if (AMediaCodec_start(videoDeoder->codec) != AMEDIA_OK) {
        LOGD("AMediaCodec_start: Could not start encoder.");
    }
    else {
        LOGD("AMediaCodec_start: encoder successfully started");
    }

    // 启动线程
    pthread_create(&pid, 0, rendering_thread, videoDeoder);    

    pthread_mutex_unlock(&videoDeoder->lock);

    LOGD("VideoDecoder_start pthread_mutex_unlock");
}

void VideoDecoder_stop(VideoDecoder* videoDeoder) {

    videoDeoder->stop = true;
}

int VideoDecoder_submitDecodeUnit(VideoDecoder* videoDeoder, void* decodeUnitData, int decodeUnitLength, int decodeUnitType,
                                int frameNumber, long receiveTimeMs) {

    pthread_mutex_lock(&videoDeoder->lock);

    LOGD("VideoDecoder_submitDecodeUnit: submit %p", decodeUnitData);

    int codecFlags = 0;

    // H264 SPS
//    if (((char*)decodeUnitData)[4] == 0x67) {
//
//    }

    pthread_mutex_unlock(&videoDeoder->lock);

    return 0;
}

int VideoDecoder_dequeueInputBuffer(VideoDecoder* videoDeoder) {

    int index = -1;
    // get a empty input buffer from cache
    pthread_mutex_lock(&inputCache_lock); {

        for (int i = 0; i < InputBufferMaxSize; i++) {
            VideoInputBuffer* inputBuffer = &videoDeoder->inputBufferCache[i];
            if (inputBuffer->status == INPUT_BUFFER_STATUS_FREE) {
                inputBuffer->status = INPUT_BUFFER_STATUS_WORKING;
                LOGD("VideoDecoder_getInputBuffer working %d", i);
                index = i;
                break;
            }
        }

    } pthread_mutex_unlock(&inputCache_lock);

    LOGD("VideoDecoder_getInputBuffer index %d", index);

    return index;
}

VideoInputBuffer* VideoDecoder_getInputBuffer(VideoDecoder* videoDeoder, int index) {

    VideoInputBuffer* inputBuffer;
    pthread_mutex_lock(&videoDeoder->lock); {

        pthread_mutex_lock(&inputCache_lock); {

            inputBuffer = &videoDeoder->inputBufferCache[index];
            if (inputBuffer->status != INPUT_BUFFER_STATUS_WORKING) {
                LOGD("VideoDecoder_getInputBuffer error index %d", index);
                inputBuffer = 0;
            } else {
                LOGD("VideoDecoder_getInputBuffer index %d", index);
            }
            
        } pthread_mutex_unlock(&inputCache_lock);

    } pthread_mutex_unlock(&videoDeoder->lock);
    
    return inputBuffer;
}

bool VideoDecoder_queueInputBuffer(VideoDecoder* videoDecoder, int index, long timestampUs, int codecFlags) {

    pthread_mutex_lock(&videoDecoder->lock); {

        pthread_mutex_lock(&inputCache_lock);
        {
            // add to list
            VideoInputBuffer *inputBuffer = &videoDecoder->inputBufferCache[index];
            assert(inputBuffer->status == INPUT_BUFFER_STATUS_WORKING);

            inputBuffer->timestampUs = timestampUs;
            inputBuffer->codecFlags = codecFlags;

            inputBuffer->status = INPUT_BUFFER_STATUS_QUEUING;

            // send rendering semaphore
            sem_post(&videoDecoder->rendering_sem);

            LOGD("post queueInputBuffer: %d", index);

        } pthread_mutex_unlock(&inputCache_lock);

    } pthread_mutex_unlock(&videoDecoder->lock);

    return true;
}