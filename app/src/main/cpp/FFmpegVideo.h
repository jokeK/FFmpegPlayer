//
// Created by Adim on 2018/8/20.
//

#ifndef FFMPEGPLAY_FFMPEGVIDEO_H
#define FFMPEGPLAY_FFMPEGVIDEO_H

#endif //FFMPEGPLAY_FFMPEGVIDEO_H
#include <queue>
#include "Log.h"
#include "FFmpegAudio.h"

extern "C"{
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};
class FFmpegVideo{
public:
    FFmpegVideo();

    ~FFmpegVideo();

    int deQueue(AVPacket *packet);
    int enQueue(AVPacket *packet);

    //void play();
    void play(FFmpegAudio *audio);

    void stop();

    void setAVCodecContext(AVCodecContext *codecContext);

    void setPlayCall(void(*call)(AVFrame *frame));


    double synchronize(AVFrame *frame, double play);

public:
    //是否正在播放
    int isPlay;
    //流的引索
    int index;
    //音频队列
    std::queue<AVPacket*> queue;
    //处理线程
    pthread_t video_play_id;
    //解码器上下文
    AVCodecContext *codecContext;
    //同步锁
    pthread_mutex_t mutex;
    //条件变量
    pthread_cond_t cond;
    AVRational time_base;
    double clock;
    double frame_timer;//从第一帧开始的绝对时间  单位秒
};


