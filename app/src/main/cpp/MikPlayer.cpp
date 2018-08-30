//
// Created by Adim on 2018/8/17.
//
#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "FFmpegVideo.h"


#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,"mik",__VA_ARGS__);
char *path;
FFmpegAudio *audio;
FFmpegVideo *video;
pthread_t p_tid;
//播放状态
int isPlay = 0;

ANativeWindow *window = 0;

//解码函数
void *process(void *args) {
    //1、注册组建
    av_register_all();
    avformat_network_init();

    //封装格式上下文
    AVFormatContext *formatContext = avformat_alloc_context();

    //2、打开视频文件
    if (avformat_open_input(&formatContext, path, NULL, NULL) < 0) {
        LOGE("视频文件打开失败");

    }
    //3、获取视频信息
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取信息失败");
    }

    //视频信息的流中包含视频流，音频流等多种流
    //找到视频流 nb_streams流的数量
    for (int i = 0; i < formatContext->nb_streams; i++) {
        //获取解码器上下文
        AVCodecParameters *parameters = formatContext->streams[i]->codecpar;
        //4、获得解码器
        AVCodec *pCodex = avcodec_find_decoder(parameters->codec_id);

        AVCodecContext *codec = avcodec_alloc_context3(pCodex);

        avcodec_parameters_to_context(codec, parameters);

        //打开解码器
        if (avcodec_open2(codec, pCodex, NULL) < 0) {
            LOGE("解码失败");
            continue;
        }

        //codec 每一个流对应的解码上下文
        if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            //pContext->streams[i]->codecpar->codec_type
            //找到视频流
            video->setAVCodecContext(codec);
            video->time_base = formatContext->streams[i]->time_base;
            video->index = i;

        } else if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            //找到音频流
            audio->setAVCodecContext(codec);
            audio->time_base = formatContext->streams[i]->time_base;
            audio->index = i;
            if (window)
                ANativeWindow_setBuffersGeometry(window, video->codecContext->width,
                                                 video->codecContext->height,
                                                 WINDOW_FORMAT_RGBA_8888);
        }

    }

    isPlay = 1;
    //开启 音频 视频的播放死循环
    video->play(audio);
    audio->play();

    AVPacket *packet = av_packet_alloc();
    //av_init_packet(packet);
    int ret = 0;
    while (isPlay) {
        ret = av_read_frame(formatContext, packet);
        if (ret == 0) {
            if (video && video->index == packet->stream_index) {
                video->enQueue(packet);

            } else if (audio && audio->index == packet->stream_index) {
                audio->enQueue(packet);
            }
            av_packet_unref(packet);
        } else if (ret == AVERROR_EOF) {
            //读取完毕 但是不一定播放完毕
            while (isPlay) {
                if (video->queue.empty() && audio->queue.empty()) {
                    break;
                }
//                LOGI("等待播放完成");
                av_usleep(10000);
            }
            break;
        } else {
            break;
        }
    }
    //视频解码完
    isPlay = 0;
    if (video && video->isPlay) {
        video->stop();
    }
    if (audio && audio->isPlay) {
        audio->stop();
    }
    av_packet_free(&packet);
    avformat_free_context(formatContext);
    pthread_exit(0);

}

void video_palyer_call(AVFrame *frame) {
    if (!window) {
        return;
    }
//视频缓冲区
    ANativeWindow_Buffer window_buffer;
    //绘制开始
    //先锁定画布 坑点：&window_buffer 不能传*指针
    //不然会报一个 too mach works on main thread 的错误 很莫名其妙
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        return;
    }

    //拿到window画布的首地址
    //void *dst = window_buffer->bits;
    //The actual bits 实际的位置
    uint8_t *dst = static_cast<uint8_t *>(window_buffer.bits);
    //拿到一行有多少个像素  ARGBS所以*4
    //内存地址
    int destStride = window_buffer.stride * 4;
    //像素数据的首地址(数据源的首地址)
    uint8_t *src = frame->data[0];
    //实际数据在内存中一行的字节
    int srcStride = frame->linesize[0];
    for (int i = 0; i < window_buffer.height; ++i) {
        memcpy(dst + i * destStride, src + i * srcStride, srcStride);
    }
    ANativeWindow_unlockAndPost(window);
    //保证每秒60帧
    // av_usleep(1000 * 16);//微秒

}

extern "C" JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_MikPlayer_play(JNIEnv *env, jobject instance, jstring url_) {

    const char *url = env->GetStringUTFChars(url_, 0);

    path = (char *) malloc(strlen(url) + 1);
    memset(path, 0, strlen(url) + 1);
    memcpy(path, url, strlen(url));

    audio = new FFmpegAudio();
    video = new FFmpegVideo();
    video->setPlayCall(video_palyer_call);
    pthread_create(&p_tid, NULL, process, NULL);

    env->ReleaseStringUTFChars(url_, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_MikPlayer_disPlay(JNIEnv *env, jobject instance, jobject surface) {

    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    window = ANativeWindow_fromSurface(env, surface);
    if (video && video->codecContext) {
        ANativeWindow_setBuffersGeometry(window, video->codecContext->width,
                                         video->codecContext->height,
                                         WINDOW_FORMAT_RGBA_8888);
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_MikPlayer_native_1stop(JNIEnv *env, jobject instance) {

    if (path) {
        free(path);
        path = 0;
    }
    if (isPlay) {
        isPlay = 0;
        pthread_join(p_tid, 0);
    }
    if (video) {
        if (video->isPlay) {
            video->stop();
        }
        delete (video);
        video = 0;
    }
    if (audio) {
        if (audio->isPlay) {
            audio->stop();
        }
        delete (audio);
        audio = 0;
    }

}extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_MikPlayer_native_1release(JNIEnv *env, jobject instance) {

    if (window)
        ANativeWindow_release(window);
    window = 0;

}