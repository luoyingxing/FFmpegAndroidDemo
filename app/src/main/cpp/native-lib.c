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
#include <math.h>
#include <android/log.h>
#include "include/libswscale/swscale.h"
#include "include/libavcodec/avcodec.h"
#include "include/libavutil/opt.h"
#include "include/libavutil/channel_layout.h"
#include "include/libavutil/common.h"
#include "include/libavutil/imgutils.h"
#include "include/libavutil/mathematics.h"
#include "include/libavutil/samplefmt.h"

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

#define JNI_RES_CLASS "com/lyx/ffmpeg/JniReponse"

JNIEnv *jNIEnv;
jclass g_res_class = NULL;
jmethodID g_method_onResponse = NULL;

#define LOG_TAG "lib-native"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


JNIEXPORT jstring JNICALL
Java_com_lyx_ffmpeg_MainActivity_stringFromJNI(JNIEnv *env, jobject obj) {
    char *str = "Get from .c";
    return (*env)->NewStringUTF(env, str);
}

void onResponse(JNIEnv *env, jbyteArray array) {
    if (g_method_onResponse != NULL && g_res_class != NULL) {
        (*env)->CallStaticVoidMethod(env, g_res_class, g_method_onResponse, array);
    }
}

void pgm_save(const char *buf, int wrap, int xsize, int ysize, char *filename) {
//    LOGW("=====>> pgm_save   %d   %d   %d   %s   buf.size: %d", wrap, xsize, ysize, filename,
//         strlen(buf));

//    FILE *f;
//    int i;
//
//    f = fopen("/storage/emulated/0/one.jpeg", "w");
//    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
//    for (i = 0; i < ysize; i++) {
//        fwrite(buf + i * wrap, 1, xsize, f);
//    }
//    fclose(f);


    //TODO 回到图像数据
//    jbyteArray bytes = (*jNIEnv)->NewByteArray(jNIEnv, strlen(buf));
//    (*jNIEnv)->SetByteArrayRegion(jNIEnv, bytes, 0, strlen(buf), (jbyte *) buf);
//
//    onResponse(jNIEnv, bytes);


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

        pgm_save(frame->data[0], frame->linesize[0], avctx->width, avctx->height, buf);

        // ------------------------------------------------------------------------------------------------------------------------------------
        //1. 准备一个容器来装转码后的数据
//        AVFrame *dst_frame = av_frame_alloc();
//        //在解码上下文使用extradata解析出第一帧图像后，ctx的width和height,pix_format 写入了实际的视频宽高，像素格式
//        dst_frame->width = avctx->width;
//        dst_frame->height = avctx->height;
//        //2. 转码为ARGB，来给NativeWindow显示
//        dst_frame->format = AV_PIX_FMT_ARGB;
//        //3. 根据输入图像和输出图像的信息（宽、高、像素），初始化格式转换上下文
//        //应该重复使用该上下文，不要每一帧都初始化一次
//        struct SwsContext *swsCtx = sws_getContext(
//                frame->width, frame->height, (enum AVPixelFormat) frame->format,
//                frame->width, frame->height, (enum AVPixelFormat) dst_frame->format,
//                SWS_FAST_BILINEAR, NULL, NULL, NULL);
//        //4. 初始化之前准备的dst_frame的buffer
//        int buffer_size = av_image_get_buffer_size((enum AVPixelFormat) dst_frame->format,
//                                                   frame->width, frame->height, 1);
//        uint8_t *buffer = (uint8_t *) av_malloc(sizeof(uint8_t) * buffer_size);
//        //5. 绑定dst_frame和新申请的buffer
//        av_image_fill_arrays(dst_frame->data, dst_frame->linesize, buffer,
//                             (enum AVPixelFormat) dst_frame->format, dst_frame->width,
//                             dst_frame->height, 1);
//        //6. 转码
//        sws_scale(swsCtx, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height,
//                  dst_frame->data, dst_frame->linesize);
//
//        //TODO 回到图像数据
//        jbyteArray bytes = (*jNIEnv)->NewByteArray(jNIEnv, strlen(buffer));
//        (*jNIEnv)->SetByteArrayRegion(jNIEnv, bytes, 0, strlen(buffer), (jbyte *) buffer);
//
//        onResponse(jNIEnv, bytes);

        // ------------------------------------------------------------------------------------------------------------------------------------
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

    jNIEnv = env;

    jclass envClz = (*env)->FindClass(env, "android/os/Environment");
    jmethodID getExternalStorageDirectory = (*env)->GetStaticMethodID(env, envClz,
                                                                      "getExternalStorageDirectory",
                                                                      "()Ljava/io/File;");
    jobject jobject1 = (*env)->CallStaticObjectMethod(env, envClz, getExternalStorageDirectory);
    jmethodID getAbsolutePath = (*env)->GetMethodID(env, (*env)->FindClass(env, "java/io/File"),
                                                    "getAbsolutePath", "()Ljava/lang/String;");
    jstring jstring1 = (*env)->CallObjectMethod(env, jobject1, getAbsolutePath);
    const char *path_utf = (*env)->GetStringUTFChars(env, jstring1, NULL);
    LOGD("path_utf: %s", path_utf);

    //获得手机存储的h264文件
    char *outfilename = strcat((char *) path_utf, "/test.h264");
    char *filename = (char *) AV_CODEC_ID_H264;
    LOGI("outfilename: %s", outfilename);

    //--------------------------------------------------------------------------------------------------
    avcodec_register_all();

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

    LOGW("Decode video file to %s\n", outfilename);

    /* find the mpeg1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        LOGE("Codec not found");
        return;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        LOGE("Could not allocate video codec context");
        return;
    }

    if (codec->capabilities & CODEC_CAP_TRUNCATED) {
        c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
    }

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
   MUST be initialized there because this information is not
   available in the bitstream. */

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    f = fopen(outfilename, "rb");
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

//        LOGI("check avpkt.size  %d", avpkt.size);

        if (avpkt.size == 0) {
            break;
        }

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
        while (avpkt.size > 0) {
            if (decode_write_frame(outfilename, c, frame, &frame_count, &avpkt, 0) < 0) {
                return;
            }
        }
    }

    LOGW("======= for end");

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


jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGW("jni loading");

    JNIEnv *env = NULL;
    jint result;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }

//    if (!registerNatives(env)) {
//        return -1;
//    }

    result = JNI_VERSION_1_4;

    //TODO: 加载响应类方法
    jclass tmp = (*env)->FindClass(env, JNI_RES_CLASS);
    g_res_class = (jclass) ((*env)->NewGlobalRef(env, tmp));

    if (g_res_class != NULL) {
        g_method_onResponse = (*env)->GetStaticMethodID(env, g_res_class, "onResponse", "([B)V");
    }

    return result;
}

#ifdef __cplusplus
}
#endif
