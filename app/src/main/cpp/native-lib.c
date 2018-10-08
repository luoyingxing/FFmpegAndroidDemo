//
// native-lib
//
// Created by luoyingxing on 2018/10/08.
//
#ifdef __cplusplus
extern "C"{
#endif

#include <../jni.h>
#include <string.h>
#include <android/log.h>
#include "include/libavcodec/avcodec.h"
#include <math.h>

#include <include/libavutil/opt.h>
#include <include/libavutil/channel_layout.h>
#include <include/libavutil/common.h>
#include <include/libavutil/imgutils.h>
#include <include/libavutil/mathematics.h>
#include <include/libavutil/samplefmt.h>

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096


#define LOG_TAG "lib-native"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)


JNIEXPORT jstring JNICALL
Java_com_lyx_ffmpeg_MainActivity_stringFromJNI(JNIEnv *env, jobject obj) {
    char *str = "Get from .c";
    return (*env)->NewStringUTF(env, str);
}


int
decode_write_frame(const char *outfilename, AVCodecContext *avctx, AVFrame *frame, int *frame_count,
                   AVPacket *pkt, int last) {
    int len, got_frame;
    char buf[1024];

    len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
        return len;
    }
    if (got_frame) {
        printf("Saving %sframe %3d\n", last ? "last " : "", *frame_count);
        fflush(stdout);

        /* the picture is allocated by the decoder, no need to free it */
        snprintf(buf, sizeof(buf), outfilename, *frame_count);
//        pgm_save(frame->data[0], frame->linesize[0], avctx->width, avctx->height, buf);
        (*frame_count)++;
    }
    if (pkt->data) {
        pkt->size -= len;
        pkt->data += len;
    }
    return 0;
}


JNIEXPORT void JNICALL
Java_com_lyx_ffmpeg_MainActivity_play(JNIEnv *env, jobject obj) {
    LOGD("=====>> start play");
    char *outfilename = "test.h264";
    char *filename = AV_CODEC_ID_H264;

    jclass envClz = (*env)->FindClass(env, "android/os/Environment");
    jmethodID getExternalStorageDirectory = (*env)->GetStaticMethodID(env, envClz,
                                                                      "getExternalStorageDirectory",
                                                                      "()Ljava/io/File;");
    jobject jobject1 = (*env)->CallStaticObjectMethod(env, envClz, getExternalStorageDirectory);
    jmethodID getAbsolutePath = (*env)->GetMethodID(env, (*env)->FindClass(env, "java/io/File"),
                                                    "getAbsolutePath", "()Ljava/lang/String;");
    jstring jstring1 = (*env)->CallObjectMethod(env, jobject1, getAbsolutePath);
    const char *path_utf = (*env)->GetStringUTFChars(env, jstring1, NULL);
    LOGI("%s", path_utf);


    AVCodec *codec;
    AVCodecContext *c = NULL;
    int frame_count;
    FILE *f;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;

    av_init_packet(&avpkt);

/* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
    memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    printf("Decode video file %s to %s\n", filename, outfilename);

    /* find the mpeg1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return;
    }

    if (codec->capabilities & CODEC_CAP_TRUNCATED)
        c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
   MUST be initialized there because this information is not
   available in the bitstream. */

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        return;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return;
    }

    frame_count = 0;
    for (;;) {
        avpkt.size = fread(inbuf, 1, INBUF_SIZE, f);
        if (avpkt.size == 0)
            break;

        /* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
           and this is the only method to use them because you cannot
           know the compressed data size before analysing it.

           BUT some other codecs (msmpeg4, mpeg4) are inherently frame
           based, so you must call them with all the data for one
           frame exactly. You must also initialize 'width' and
           'height' before initializing them. */

        /* NOTE2: some codecs allow the raw parameters (frame size,
           sample rate) to be changed at any frame. We handle this, so
           you should also take care of it */

        /* here, we use a stream based decoder (mpeg1video), so we
           feed decoder and see if it could decode a frame */
        avpkt.data = inbuf;
        while (avpkt.size > 0)
            if (decode_write_frame(outfilename, c, frame, &frame_count, &avpkt, 0) < 0)
                return;
    }

    /* some codecs, such as MPEG, transmit the I and P frame with a
   latency of one frame. You must do the following to have a
   chance to get the last frame of the video */
    avpkt.data = NULL;
    avpkt.size = 0;
    decode_write_frame(outfilename, c, frame, &frame_count, &avpkt, 1);

    fclose(f);

    avcodec_close(c);
    av_free(c);
//    avcodec_free_frame(&frame);
    printf("\n");
}

#ifdef __cplusplus
}
#endif
