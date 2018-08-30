//
// Created by Adim on 2018/8/20.
//

#ifndef FFMPEGPLAY_FFMPEGAUDIO_H
#define FFMPEGPLAY_FFMPEGAUDIO_H

#endif //FFMPEGPLAY_FFMPEGAUDIO_H
#include <queue>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Log.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <pthread.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
};
#define MAX_AUDIO_FRME_SIZE 48000 * 4
#define A_CH_LAYOUT AV_CH_LAYOUT_STEREO
#define A_PCM_BITS AV_SAMPLE_FMT_S16
#define A_SAMPLE_RATE 44100

class FFmpegAudio{
public:
    FFmpegAudio();

    ~FFmpegAudio();

    int deQueue(AVPacket *packet);
    int enQueue(AVPacket *packet);

    void play();

    void stop();

    void setAVCodecContext(AVCodecContext *codecContext);

    int createPlayer();
    //音频播放一帧的时间
    double getClock();
    int decodeAudio();
public:
    //是否正在播放
    int isPlay;
    //流的引索
    int index;
    //音频队列
    std::queue<AVPacket*> queue;
    //处理线程
    pthread_t audio_play_id;
    //解码器上下文
    AVCodecContext *codecContext;
    //同步锁
    pthread_mutex_t mutex;
    //条件变量
    pthread_cond_t cond;

    SwrContext *swrContext;

    uint8_t *out_buffer;

    SLObjectItf engineObject;
    SLEngineItf engineItf;
//混音器
    SLObjectItf outputMixObject;
    SLEnvironmentalReverbItf outputEnvironmentalReverbItf ;

//播放器
    SLObjectItf playObject;
    SLPlayItf playItf;
//队列缓冲区
//    SLBufferQueueItf playerQueue ;
    SLAndroidSimpleBufferQueueItf playerQueue;
//音量对象
    SLVolumeItf volumeItf;
    //音频播放一帧的时间
    double clock;

    AVRational time_base;
    int channels;

};