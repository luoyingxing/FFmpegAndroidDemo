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

#include <android/bitmap.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

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

typedef struct _VoutInfo {
    /**
    WINDOW_FORMAT_RGBA_8888          = 1,
    WINDOW_FORMAT_RGBX_8888          = 2,
    WINDOW_FORMAT_RGB_565            = 4,*/
    uint32_t pix_format;

    uint32_t buffer_width;
    uint32_t buffer_height;
    uint8_t *buffer;
} VoutInfo;

typedef struct _NalInfo {
    uint8_t forbidden_zero_bit;
    uint8_t nal_ref_idc;
    uint8_t nal_unit_type;
} NalInfo;

typedef struct _RenderParam {
    struct SwsContext *swsContext;
    AVCodecContext *avCodecContext;
} RenderParam;

typedef struct _EnvPackage {
    JNIEnv *env;
    jobject *obj;
    jobject *surface;
} EnvPackage;

enum {
    PIXEL_FORMAT_RGBA_8888 = 1,
    PIXEL_FORMAT_RGBX_8888 = 2,
    PIXEL_FORMAT_RGB_565 = 3
};

typedef struct _VoutRender {
    uint32_t pix_format;
    uint32_t window_format;

    void (*render)(ANativeWindow_Buffer *nwBuffer, VoutInfo *voutInfo);
} VoutRender;

uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
int init = 0;
AVPacket avpkt;
RenderParam *renderParam;
AVCodec *codec;
AVCodecContext *codecContext;
AVFrame *frame;
AVCodecParserContext *parser;
int frame_count;
struct SwsContext *img_convert_ctx;
AVFrame *pFrameRGB;
enum AVPixelFormat pixelFormat;
int native_pix_format = PIXEL_FORMAT_RGB_565;

void render_on_rgb(ANativeWindow_Buffer *nwBuffer, VoutInfo *voutInfo, int bpp) {

    int stride = nwBuffer->stride;
    int dst_width = nwBuffer->width;
    int dst_height = nwBuffer->height;
    LOGE("ANativeWindow stride %d width %d height %d", stride, dst_width, dst_height);
    int line = 0;

    int src_line_size = voutInfo->buffer_width * bpp / 8;
    int dst_line_size = stride * bpp / 8;

    int min_height = dst_height < voutInfo->buffer_height ? dst_height : voutInfo->buffer_height;

    if (src_line_size == dst_line_size) {
        memcpy((__uint8_t *) nwBuffer->bits, (__uint8_t *) voutInfo->buffer,
               src_line_size * min_height);
    } else {
        //直接copy
        /*for(int i=0; i<height; i++){
            memcpy((__uint8_t *) (nwBuffer.bits + line), (__uint8_t *)(rgbFrame->data[0]+ width*i * 2), width * 2);
            line += stride * 2;
        }*/

        //使用ffmpeg的函数 实现相同功能
        av_image_copy_plane(nwBuffer->bits, dst_line_size, voutInfo->buffer, src_line_size,
                            src_line_size, min_height);
    }
}

void render_on_rgb8888(ANativeWindow_Buffer *nwBuffer, VoutInfo *voutInfo) {
    render_on_rgb(nwBuffer, voutInfo, 32);
}

void render_on_rgb565(ANativeWindow_Buffer *nwBuffer, VoutInfo *voutInfo) {
    render_on_rgb(nwBuffer, voutInfo, 16);
}

static VoutRender g_pixformat_map[] = {
        {PIXEL_FORMAT_RGBA_8888, WINDOW_FORMAT_RGBA_8888, render_on_rgb8888},
        {PIXEL_FORMAT_RGBX_8888, WINDOW_FORMAT_RGBX_8888, render_on_rgb8888},
        {PIXEL_FORMAT_RGB_565,   WINDOW_FORMAT_RGB_565,   render_on_rgb565}
};

JNIEXPORT jstring JNICALL
Java_com_lyx_ffmpeg_MainActivity_stringFromJNI(JNIEnv *env, jobject obj) {
    char *str = "Play H264 now";
    return (*env)->NewStringUTF(env, str);
}

int handleH264Header(uint8_t *ptr, NalInfo *nalInfo) {
    int startIndex = 0;
    uint32_t *checkPtr = (uint32_t *) ptr;
    if (*checkPtr == 0x01000000) {  // 00 00 00 01
        startIndex = 4;
    } else if (*(checkPtr) == 0 && *(checkPtr + 1) & 0x01000000) {  // 00 00 00 00 01
        startIndex = 5;
    }

    if (!startIndex) {
        return -1;
    } else {
        ptr = ptr + startIndex;
        nalInfo->nal_unit_type = 0x1f & *ptr;
        if (nalInfo->nal_unit_type == 5 || nalInfo->nal_unit_type == 7 ||
            nalInfo->nal_unit_type == 8 || nalInfo->nal_unit_type == 2) {  //I frame
            LOGE("I frame");
        } else if (nalInfo->nal_unit_type == 1) {
            LOGE("P frame");
        }
    }
    return 0;
}

AVFrame *
yuv420p_2_argb(AVFrame *frame, struct SwsContext *swsContext, AVCodecContext *avCodecContext,
               enum AVPixelFormat format) {
    AVFrame *pFrameRGB = NULL;
    uint8_t *out_bufferRGB = NULL;
    pFrameRGB = av_frame_alloc();

    pFrameRGB->width = frame->width;
    pFrameRGB->height = frame->height;

    //给pFrameRGB帧加上分配的内存;  //AV_PIX_FMT_ARGB
    int size = avpicture_get_size(format, avCodecContext->width, avCodecContext->height);
    //out_bufferRGB = new uint8_t[size];
    out_bufferRGB = av_malloc(size * sizeof(uint8_t));
    avpicture_fill((AVPicture *) pFrameRGB, out_bufferRGB, format, avCodecContext->width,
                   avCodecContext->height);
    //YUV to RGB
    sws_scale(swsContext, frame->data, frame->linesize, 0, avCodecContext->height, pFrameRGB->data,
              pFrameRGB->linesize);

    return pFrameRGB;
}

VoutRender *get_render_by_window_format(int window_format) {
    int len = sizeof(g_pixformat_map);
    for (int i = 0; i < len; i++) {
        if (g_pixformat_map[i].window_format == window_format) {
            return &g_pixformat_map[i];
        }
    }
}

void android_native_window_display(ANativeWindow *aNativeWindow, VoutInfo *voutInfo) {

    int curr_format = ANativeWindow_getFormat(aNativeWindow);
    VoutRender *render = get_render_by_window_format(curr_format);

    ANativeWindow_Buffer nwBuffer;
    //ANativeWindow *aNativeWindow = ANativeWindow_fromSurface(envPackage->env, *(envPackage->surface));
    if (aNativeWindow == NULL) {
        LOGE("ANativeWindow_fromSurface error");
        return;
    }

    //scaled buffer to fit window
    int retval = ANativeWindow_setBuffersGeometry(aNativeWindow, voutInfo->buffer_width,
                                                  voutInfo->buffer_height, render->window_format);
    if (retval < 0) {
        LOGE("ANativeWindow_setBuffersGeometry: error %d", retval);
//        return retval;
    }

    if (0 != ANativeWindow_lock(aNativeWindow, &nwBuffer, 0)) {
        LOGE("ANativeWindow_lock error");
        return;
    }

    render->render(&nwBuffer, voutInfo);

    if (0 != ANativeWindow_unlockAndPost(aNativeWindow)) {
        LOGE("ANativeWindow_unlockAndPost error");
        return;
    }
    //ANativeWindow_release(aNativeWindow);
}

void handle_data(AVFrame *pFrame, void *param, void *ctx) {

    RenderParam *renderParam = (RenderParam *) param;

    AVFrame *rgbFrame = yuv420p_2_argb(pFrame, renderParam->swsContext, renderParam->avCodecContext,
                                       pixelFormat);//AV_PIX_FMT_RGB565LE

    LOGE("width %d height %d", rgbFrame->width, rgbFrame->height);

    //for test decode image
    //save_rgb_image(rgbFrame);

    EnvPackage *envPackage = (EnvPackage *) ctx;
    ANativeWindow *aNativeWindow = ANativeWindow_fromSurface(envPackage->env,
                                                             *(envPackage->surface));

    VoutInfo voutInfo;
    voutInfo.buffer = rgbFrame->data[0];
    voutInfo.buffer_width = rgbFrame->width;
    voutInfo.buffer_height = rgbFrame->height;
    voutInfo.pix_format = native_pix_format;

    android_native_window_display(aNativeWindow, &voutInfo);

    ANativeWindow_release(aNativeWindow);

    av_free(rgbFrame->data[0]);
    av_free(rgbFrame);
}

int decodeFrame(const char *data, int length, void *ctx) {
    //void (*handle_data)(AVFrame *pFrame, void *param, void *ctx)

    int cur_size = length;
    int ret = 0;

    memcpy(inbuf, data, length);
    const uint8_t *cur_ptr = inbuf;
    // Parse input stream to check if there is a valid frame.
    //std::cout << " in data  --  -- " << length<< std::endl;
    while (cur_size > 0) {
        int parsedLength = av_parser_parse2(parser, codecContext, &avpkt.data,
                                            &avpkt.size, (const uint8_t *) cur_ptr, cur_size,
                                            AV_NOPTS_VALUE,
                                            AV_NOPTS_VALUE, AV_NOPTS_VALUE);
        cur_ptr += parsedLength;
        cur_size -= parsedLength;
        //std::cout <<" avpkt.size  "<< avpkt.size << " -- cur_size "<< cur_size <<" "<<parsedLength<< std::endl;
        // 67 sps
        // 68 pps
        // 65 i
        // 61 p
        //LOGE("parsedLength %d    %x %x %x %x %x %x %x %x", parsedLength, cur_ptr[0], cur_ptr[1], cur_ptr[2], cur_ptr[3], cur_ptr[4], cur_ptr[5], cur_ptr[6], cur_ptr[7]);
        //LOGE("parsedLength %d    %x %x %x %x %x %x %x %x", parsedLength, *(cur_ptr-parsedLength), *(cur_ptr-parsedLength+1), *(cur_ptr-parsedLength+2), *(cur_ptr-parsedLength+3), *(cur_ptr-parsedLength+4), *(cur_ptr-parsedLength+5), *(cur_ptr-parsedLength+6), *(cur_ptr-parsedLength+7));
        NalInfo nalInfo;
        ret = handleH264Header(cur_ptr - parsedLength, &nalInfo);
        if (ret == 0) {
        }

        if (!avpkt.size) {
            continue;
        } else {

            int len, got_frame;
            len = avcodec_decode_video2(codecContext, frame, &got_frame, &avpkt);

            if (len < 0) {
                LOGE("Error while decoding frame %d\n", frame_count);
                //break;
                continue;
                // exit(1);
            }

            if (got_frame) {
                frame_count++;

                LOGE("frame %d", frame_count);

                if (img_convert_ctx == NULL) {
                    img_convert_ctx = sws_getContext(codecContext->width, codecContext->height,
                                                     codecContext->pix_fmt, codecContext->width,
                                                     codecContext->height,
                                                     pixelFormat, SWS_BICUBIC, NULL, NULL, NULL);

                    renderParam = (RenderParam *) malloc(sizeof(RenderParam));
                    renderParam->swsContext = img_convert_ctx;
                    renderParam->avCodecContext = codecContext;
                }

                if (img_convert_ctx != NULL) {
                    handle_data(frame, renderParam, ctx);
                }

                //根据编码信息设置渲染格式
                /*if(img_convert_ctx == NULL){
                    //std::cout << " init img_convert_ctx \n";
                    img_convert_ctx = sws_getContext(codecContext->width, codecContext->height,
                            codecContext->pix_fmt, codecContext->width, codecContext->height,
                            AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
                }

                //----------------------opencv
                cv::Mat curmat;
                curmat.create(cv::Size(codecContext->width, codecContext->height),CV_8UC3);

                if(img_convert_ctx != NULL)
                {
                    AVFrame	*pFrameRGB = NULL;
                    uint8_t  *out_bufferRGB = NULL;
                    pFrameRGB = av_frame_alloc();
                    //给pFrameRGB帧加上分配的内存;
                    int size = avpicture_get_size(AV_PIX_FMT_BGR24, codecContext->width, codecContext->height);
                    out_bufferRGB = new uint8_t[size];
                    avpicture_fill((AVPicture *)pFrameRGB, out_bufferRGB, AV_PIX_FMT_BGR24, codecContext->width, codecContext->height);

                    //YUV to RGB
                    sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, codecContext->height, pFrameRGB->data, pFrameRGB->linesize);

                    memcpy(curmat.data,out_bufferRGB,size);

                    vecMat.push(curmat);

                    delete[] out_bufferRGB;
                    av_free(pFrameRGB);
                }*/
            }
        }
    }

    return length;
}

JNIEXPORT void JNICALL
Java_com_lyx_ffmpeg_MainActivity_parserStream(JNIEnv *env, jobject obj, jbyteArray jdata,
                                              jint length, jobject surface) {
    if (init == 0) {
        init++;

        /* register all the codecs */
        avcodec_register_all();
        av_init_packet(&avpkt);

        renderParam = NULL;

        /* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
        memset(inbuf, 0, INBUF_SIZE);

        memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

        /* find the x264 video decoder */
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            LOGE("Codec not found");
            return;
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            LOGE("Could not allocate video codec context");
            return;
        }

        /* put sample parameters */
        //codecContext->bit_rate = 500000;
        //codecContext->width = 640;
        //codecContext->height = 480;
        //codecContext->time_base = (AVRational ) { 1, 15 };
        //codecContext->framerate = (AVRational ) { 1, 15 };
        //codecContext->gop_size = 1;
        //codecContext->max_b_frames = 0;
        //codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        //codecContext->ticks_per_frame = 0;
        //codecContext->delay = 0;
        //codecContext->b_quant_offset = 0.0;
        //codecContext->refs = 0;
        //codecContext->slices = 1;
        //codecContext->has_b_frames = 0;
        //codecContext->thread_count = 2;
        //av_opt_set(codecContext->priv_data, "zerolatency", "ultrafast", 0);

        /* we do not send complete frames */
        if (codec->capabilities & CODEC_CAP_TRUNCATED) {
            codecContext->flags |= CODEC_FLAG_TRUNCATED;
        }

        /* open it */
        if (avcodec_open2(codecContext, codec, NULL) < 0) {
            LOGE("Could not open codec");
            return;
        }

        frame = av_frame_alloc();
        if (!frame) {
            LOGE("Could not allocate video frame");
            return;
        }

        parser = av_parser_init(AV_CODEC_ID_H264);
        if (!parser) {
            LOGE("cannot create parser");
            return;
        }

        //    parser->flags |= PARSER_FLAG_ONCE;
        LOGI("decoder init ..........");

        frame_count = 0;
        img_convert_ctx = NULL;

        pFrameRGB = NULL;

        //pixelFormat = AV_PIX_FMT_BGRA;
        //pixelFormat = AV_PIX_FMT_RGB565LE;
        //pixelFormat = AV_PIX_FMT_BGR24;
        pixelFormat = AV_PIX_FMT_RGB565LE;
    }


    //TODO -----------------------------------------------------play stream----------------------------------------------------
    jbyte *cdata = (*env)->GetByteArrayElements(env, jdata, JNI_FALSE);
    jbyte *cdata_rec = cdata;

    if (cdata != NULL) {
        EnvPackage package;
        package.env = env;
        package.obj = &obj;
        package.surface = &surface;

        int len = 0;
        while (1) {
            if (length > INBUF_SIZE) {
                len = INBUF_SIZE;
                length -= INBUF_SIZE;
            } else if (length > 0 && length <= INBUF_SIZE) {
                len = length;
                length = 0;
            } else {
                break;
            }
            //decode h264 cdata to yuv and save yuv data to avFrame which would be passed to handle_data
            decodeFrame(cdata, len, &package);
            cdata = cdata + len;
            LOGE("decode length: %d ", len);
            //this_obj->decodeFrame(cdata, length, handle_data, NULL);
        }
    } else {
        LOGE("stream data is NULL");
    }

    (*env)->ReleaseByteArrayElements(env, jdata, cdata_rec, 0);
}

void onResponse(JNIEnv *env, jbyteArray array) {
    if (g_method_onResponse != NULL && g_res_class != NULL) {
        (*env)->CallStaticVoidMethod(env, g_res_class, g_method_onResponse, array);
    }
}

void pgm_save(const char *buf, int wrap, int xsize, int ysize, char *filename) {
    LOGW("=====>> pgm_save   wrap:%d   [%d x %d]   bufSize: %d", wrap, xsize, ysize, strlen(buf));

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
    int one = 1;
    for (;;) {
        avpkt.size = fread(inbuf, 1, INBUF_SIZE, f);
        LOGD("for 循环第%d次  读取长度  %d", one, avpkt.size);
        one++;

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

        int two = 1;
        while (avpkt.size > 0) {
            LOGI("while第%d次  avpkt.size  %d", two, avpkt.size);
            two++;
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
