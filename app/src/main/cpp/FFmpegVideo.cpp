//
// Created by Adim on 2018/8/21.
//



#include "FFmpegVideo.h"

typedef struct {
    FFmpegAudio *audio;
    FFmpegVideo *video;
} Sync;

static const double SYNC_THRESHOLD = 0.01;
static const double NOSYNC_THRESHOLD = 10.0;

void default_video_call(AVFrame *frame) {

}

static void (*video_call)(AVFrame *frame) = default_video_call;

FFmpegVideo::FFmpegVideo() : clock(0), codecContext(0), index(-1),
                             isPlay(0), frame_timer(0) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

FFmpegVideo::~FFmpegVideo() {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

int FFmpegVideo::enQueue(AVPacket *packet) {
    if (!isPlay)
        return 0;
    AVPacket *avPacket = av_packet_alloc();

    if (av_packet_ref(avPacket, packet) < 0) {
        //克隆失败
        return 0;
    }
    //加锁
    pthread_mutex_lock(&mutex);
    //压入一帧的数据
    queue.push(avPacket);
    //发送信号
    pthread_cond_signal(&cond);
    //解锁
    pthread_mutex_unlock(&mutex);
    return 1;
}

int FFmpegVideo::deQueue(AVPacket *packet) {
    int ret = 0;
    pthread_mutex_lock(&mutex);
    while (isPlay) {
        if (!queue.empty()) {
            //从队列取出一个packet  clone一个给入参对象

            if (av_packet_ref(packet,  queue.front()) != 0) {
                ret = -1;
                break;
            }
            //取成功 弹出队列 销毁packet

            AVPacket *pkt = queue.front();
            queue.pop();
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            ret = 1;
            break;

        } else {
            //如果队列中没有数据 就一直等待

            pthread_cond_wait(&cond, &mutex);
        }
    }
    //发送信号
    //pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return ret;
}

void *paly_video(void *arg) {
    Sync *sync = (Sync *) arg;

    //转换rgba
    SwsContext *swsContext = sws_getContext(sync->video->codecContext->width,
                                            sync->video->codecContext->height,
                                            sync->video->codecContext->pix_fmt,
                                            sync->video->codecContext->width,
                                            sync->video->codecContext->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BILINEAR, NULL, NULL, NULL);

    AVFrame *frame = av_frame_alloc();
    AVFrame *rgb_frame = av_frame_alloc();
    //给缓冲区分配内存
    int out_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, sync->video->codecContext->width,
                                            sync->video->codecContext->height, 1);

    uint8_t *out_buffer = static_cast<uint8_t *>(av_malloc(sizeof(uint8_t) * out_size));
//    int re = avpicture_fill(reinterpret_cast<AVPicture *>(rgb_frame), out_buffer, AV_PIX_FMT_RGBA,
//                            sync->video->codecContext->width, sync->video->codecContext->height);

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, out_buffer, AV_PIX_FMT_RGBA,
                         sync->video->codecContext->width, sync->video->codecContext->height, 1);


    //int got_frame;

    AVPacket *packet = av_packet_alloc();

    double last_pts;
    //一阵一阵读取压缩的视频数据AVPacket
    double last_play  //上一帧的播放时间
    , play             //当前帧的播放时间
    , last_delay    // 上一次播放视频的两帧视频间隔时间
    , delay         //两帧视频间隔时间
    , audio_clock //音频轨道 实际播放时间
    , diff   //音频帧与视频帧相差时间
    , sync_threshold

    , pts
    , actual_delay//真正需要延迟时间
    ;
    if (!sync->video->frame_timer)
        sync->video->frame_timer = (double) av_gettime() / 1000000.0;
    while (sync->video->isPlay) {
        //视频解码一帧
        sync->video->deQueue(packet);

        //avcodec_decode_video2(sync->video->codecContext, frame, &got_frame, packet);

        int ret = avcodec_send_packet(sync->video->codecContext, packet);
        if (ret == AVERROR(EAGAIN)) {
            goto cont;
        } else if (ret < 0) {
            break;
        }

        ret = avcodec_receive_frame(sync->video->codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            goto cont;
        } else if (ret < 0) {
            break;
        }
        //得到当前帧的pts (播放顺序)
        if ((pts = av_frame_get_best_effort_timestamp(frame)) == AV_NOPTS_VALUE) {
            pts = 0;
        }

        //pts 同步
        //pts 单位就是time_base
        //av_q2d转为双精度浮点数 x pts 得到pts---显示时间:秒
        play = pts * av_q2d(sync->video->time_base);
        //        纠正时间
        //当解码到正真的显示会有一个延迟时间计算出真实的时间
        play = sync->video->synchronize(frame, play);

        //转码成rgb
        sws_scale(swsContext, reinterpret_cast<const uint8_t *const *>(frame->data),
                  frame->linesize, 0,
                  frame->height, rgb_frame->data, rgb_frame->linesize);

        delay = play - last_play;
        //下一帧 在上一帧之前播放或者一秒之后才播放下一帧 这都是不可能的
        if (delay < 0 || delay > 1.0) {
            //如果延误不正确 太高或太低，使用上一个
            //确保这个延迟有意义
            delay = last_delay;
        }
        audio_clock = sync->audio->getClock();
        last_play = play;
        last_delay = delay;
        //算出音频和视频相差的时间
        diff = sync->video->clock - audio_clock;

        //两帧时间差合理范围
        sync_threshold = (delay > SYNC_THRESHOLD ? delay : SYNC_THRESHOLD);
        //什么时候没有音频数据 diff >10
        if (fabs(diff) < NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold) {
                //视频慢了 视频加快
                delay = 0;
            } else if (diff > sync_threshold) {
                //视频快了 速度减慢
                delay = delay * 2;
            }
        }
        //得到当前帧应该显示的时间
        sync->video->frame_timer += delay;
        //当前帧应该显示的时间减去当前时间 就是需要等待的时间
        actual_delay = sync->video->frame_timer - (av_gettime() / 1000000.0);

        //让最小刷新时间是10ms
        if (actual_delay < 0.010) {
            actual_delay = 0.010;
        }
        av_usleep(actual_delay * 1000000.0 + 5000);
        //视频主动绘制  控制时间
        video_call(rgb_frame);
        cont:
        av_packet_unref(packet);
        av_frame_unref(frame);

    }
    //播放完成
    sync->video->frame_timer = 0;
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    sws_freeContext(swsContext);
    size_t size = sync->video->queue.size();
    for (int i = 0; i < size; ++i) {
        AVPacket *pkt = sync->video->queue.front();
        av_packet_free(&pkt);
        sync->video->queue.pop();
    }
    free(sync);
    pthread_exit(0);

}

void FFmpegVideo::setAVCodecContext(AVCodecContext *avCodecContext) {
    if (this->codecContext) {
        if (avcodec_is_open(this->codecContext))
            avcodec_close(this->codecContext);
        avcodec_free_context(&this->codecContext);
        this->codecContext = 0;
    }
    this->codecContext = avCodecContext;
}


void FFmpegVideo::play(FFmpegAudio *audio) {
    Sync *sync = (Sync *) malloc(sizeof(Sync));
    sync->audio = audio;
    sync->video = this;
    isPlay = 1;
    pthread_create(&video_play_id, NULL, paly_video, sync);
}

void FFmpegVideo::stop() {
    pthread_mutex_lock(&mutex);
    isPlay = 0;
    //因为可能卡在 deQueue
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    pthread_join(video_play_id, 0);

    if (this->codecContext) {
        if (avcodec_is_open(this->codecContext))
            avcodec_close(this->codecContext);
        avcodec_free_context(&this->codecContext);
        this->codecContext = 0;
    }
}


void FFmpegVideo::setPlayCall(void (*call)(AVFrame *)) {
    video_call = call;
}


double FFmpegVideo::synchronize(AVFrame *frame, double play) {
    //clock是当前播放的时间位置
    if (play != 0)
        clock = play;
    else //pst为0 则先把pts设为上一帧时间
        play = clock;
    //可能有pts为0 则主动增加clock
    //frame->repeat_pict = 当解码时，这张图片需要要延迟多少
    //需要求出扩展延时：
    //extra_delay = repeat_pict / (2*fps) 显示这样图片需要延迟这么久来显示
    double repeat_pict = frame->repeat_pict;
    //使用AvCodecContext的而不是stream的
    double frame_delay = av_q2d(codecContext->time_base);
    //如果time_base是1,25 把1s分成25份，则fps为25
    //fps = 1/(1/25)
    double fps = 1 / frame_delay;
    //pts 加上 这个延迟 是显示时间
    double extra_delay = repeat_pict / (2 * fps);
    double delay = extra_delay + frame_delay;
//    LOGI("extra_delay:%f",extra_delay);
    clock += delay;
    return play;
}
