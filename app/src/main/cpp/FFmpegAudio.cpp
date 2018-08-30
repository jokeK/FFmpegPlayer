//
// Created by Adim on 2018/8/21.
//
#include "FFmpegAudio.h"

void playerCallBack(SLAndroidSimpleBufferQueueItf queueItf, void *context) {
    FFmpegAudio *audio = static_cast<FFmpegAudio *>(context);
    //取到音频数据
    int size = audio->decodeAudio();
    if (size > 0) {
        double time = size / ((double) A_SAMPLE_RATE * audio->channels * 2);
        audio->clock += time;
        //播放的关键地方
        (*queueItf)->Enqueue(queueItf, audio->out_buffer, size);
    }
}

FFmpegAudio::FFmpegAudio() : codecContext(0), index(-1), clock(0), isPlay(0), engineObject(0),
                             engineItf(0),
                             outputMixObject(0), playObject(0), volumeItf(0),
                             playItf(0), playerQueue(0), swrContext(0) {
    //重采样后的缓冲数据
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    //采样率48000，双通道16位 8位一个字节，两个字节
    //音频所用内存字节数 = (通道数*采样频率(HZ)*采样位数(byte))÷8
    //按道理应该是4100*2*2  少*2也没问题，能播放
    out_buffer = (uint8_t *) malloc(MAX_AUDIO_FRME_SIZE * sizeof(uint8_t));
    //获取音频的通道数
    channels = av_get_channel_layout_nb_channels(A_CH_LAYOUT);
}


FFmpegAudio::~FFmpegAudio() {
    if (out_buffer) {
        free(out_buffer);
    }
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

//消费者
int FFmpegAudio::deQueue(AVPacket *packet) {
    int ret = 0;
    pthread_mutex_lock(&mutex);
    while (isPlay) {
        if (!queue.empty()) {
            //从队列取出一个packet  clone一个给入参对象
            AVPacket *pkt = queue.front();
            if (av_packet_ref(packet, pkt) < 0) {
                ret = -1;
                break;
            }
            //取成功 弹出队列 销毁packet
            //AVPacket *pkt = queue.front();
            queue.pop();
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            ret = 1;
            //av_packet_free(&pkt);
            break;

        } else {
            //如果队列中没有数据 就一直等待
            pthread_cond_wait(&cond, &mutex);
        }
    }

    pthread_mutex_unlock(&mutex);
    return ret;
}

//生产者
int FFmpegAudio::enQueue(AVPacket *packet) {
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


/**
 *得到音频解码后的数据
 * @param pcm 缓冲区数组
 * @param size 缓冲区大小
 * @return
 */
int FFmpegAudio::decodeAudio() {
   // int got_grame;
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    int data_size = 0;
    while (1) {

        deQueue(packet);
        if (packet->pts != AV_NOPTS_VALUE) {
            clock = packet->pts * av_q2d(time_base);
        }
        //解码

        int ret = avcodec_send_packet(codecContext, packet);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            break;
        }
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {

            continue;
        } else if (ret < 0) {

            break;
        }

        //avcodec_decode_audio4(codecContext, frame, &got_grame, packet);
        uint64_t dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
                frame->sample_rate,
                frame->sample_rate, AVRounding(1));
        //开始解码
        int nb = swr_convert(swrContext, &out_buffer, dst_nb_samples,
                             (const uint8_t **) frame->data, frame->nb_samples);

        data_size = nb * channels * 2;
        av_packet_unref(packet);
        av_frame_unref(frame);
        break;

    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    return data_size;
}

void *play_audio(void *args) {
    //开启音频线程
    FFmpegAudio *audio = static_cast<FFmpegAudio *>(args);
    //一直在播放 阻塞
    audio->createPlayer();
    //播放结束
    pthread_exit(0);

}

void FFmpegAudio::play() {
    isPlay = 1;
    pthread_create(&audio_play_id, NULL, play_audio, this);
}

void FFmpegAudio::stop() {
    LOGE("AUDIO stop");
    //因为可能卡在 deQueue
    pthread_mutex_lock(&mutex);
    isPlay = 0;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    pthread_join(audio_play_id, 0);
    if (playItf) {
        (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
        playItf = 0;
    }
    if (playObject) {
        (*playObject)->Destroy(playObject);
        playObject = 0;

        playerQueue = 0;
        volumeItf = 0;
    }

    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }

    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineItf = 0;
    }
    size_t size = queue.size();
    for (int i = 0; i < size; ++i) {
        AVPacket *pkt = queue.front();
        av_packet_free(&pkt);
        queue.pop();
    }
    if (swrContext)
        swr_free(&swrContext);
    if (this->codecContext) {
        if (avcodec_is_open(this->codecContext))
            avcodec_close(this->codecContext);
        avcodec_free_context(&this->codecContext);
        this->codecContext = 0;
    }
    LOGE("AUDIO clear");
}

void FFmpegAudio::setAVCodecContext(AVCodecContext *avCodecContext) {
    if (this->codecContext) {
        if (avcodec_is_open(this->codecContext))
            avcodec_close(this->codecContext);
        avcodec_free_context(&this->codecContext);
        this->codecContext = 0;
    }
    this->codecContext = avCodecContext;
    //设置音频转换参数
    swrContext = swr_alloc_set_opts(0, A_CH_LAYOUT, A_PCM_BITS, A_SAMPLE_RATE,
                                    avCodecContext->channel_layout, avCodecContext->sample_fmt,
                                    avCodecContext->sample_rate, 0, NULL);

    swr_init(swrContext);
}


int FFmpegAudio::createPlayer() {
    SLresult result;
    //初始化OpenSL_ES引擎
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }
    /**
     * 初始化状态
     * SL_BOOLEAN_FALSE 代表同步
     */
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }
    /**
     * 实例化引擎接口
     * 第二个参数是接口的ID
     */
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }

    /**
     * 创建混音器
     */

    result = (*engineItf)->CreateOutputMix(engineItf, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }
    //设置环境混响
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputEnvironmentalReverbItf);
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    if (result == SL_RESULT_SUCCESS) {
        (*outputEnvironmentalReverbItf)->
                SetEnvironmentalReverbProperties(outputEnvironmentalReverbItf, &settings);
    }


    //设置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    //混音器关联
    SLDataSink slDataSink = {&outputMix, NULL};


    //为什么播放器缓冲区数量是2(numBuffers)
    //当一个在播放的时候可以在另一个填充新的数据，增加缓冲区的数量时
    //也增加了延迟（数据从加入缓冲区到播放缓冲区的时间）
    SLDataLocator_AndroidSimpleBufferQueue bufferQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                          2};

    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                            SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};

    SLDataSource slDataSource = {&bufferQueue, &pcm};

    SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};

    SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    //创建播放器
    (*engineItf)->CreateAudioPlayer(engineItf, &playObject, &slDataSource, &slDataSink, 3, ids,
                                    req);
    (*playObject)->Realize(playObject, SL_BOOLEAN_FALSE);
    (*playObject)->GetInterface(playObject, SL_IID_PLAY, &playItf);

    //注册缓冲区队列
    (*playObject)->GetInterface(playObject, SL_IID_BUFFERQUEUE, &playerQueue);

    //设置回调接口
    (*playerQueue)->RegisterCallback(playerQueue, playerCallBack, this);

    //初始化音量对象
    (*playObject)->GetInterface(playObject, SL_IID_VOLUME, &volumeItf);
    //设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    //播放第一帧
    playerCallBack(playerQueue, this);
    return 1;
}

double FFmpegAudio::getClock() {
    return clock;
}




