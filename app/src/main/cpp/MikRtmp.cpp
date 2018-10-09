//
// Created by Adim on 2018/9/8.
//
#include <jni.h>
#include <x264.h>
#include <malloc.h>
#include "Log.h"
#include <stdio.h>
#include <string>
#include <queue>
#include <thread>

extern "C" {
#include "x264.h"
#include "faac.h"
#include "librtmp/rtmp.h"
}

#ifndef ULONG
typedef unsigned long ULONG;
#endif
x264_picture_t *pic_in;
x264_picture_t *pic_out;
RTMP *rtmp;
int y_len, u_len, v_len;
x264_t *videoEncHandle = NULL;
int readyRtmp;

std::queue<RTMPPacket *> queue;
pthread_cond_t cond;
pthread_mutex_t mutex;
int isPushing = 0;//是否正在推流
/**
 * jni相关
 */
JavaVM *jvm;
jobject jPublisherObj;
long start_time;

char *url_path;
faacEncHandle audioEncHandle;
unsigned long inputSamples;
unsigned long maxOutputBytes;

#define LOGI(...) __android_log_print(4,"NDK",__VA_ARGS__)

int mWidth;
int mHeight;
int mBitrate;
int mFps;
/**
 * rtmp处理
 */
#define _RTMP_Free(_rtmp)  if(_rtmp) {RTMP_Free(_rtmp); _rtmp = NULL;}
#define _RTMP_Close(_rtmp)  if(_rtmp ) RTMP_Close(_rtmp);
void add_264_sequence_header(unsigned char *sps, unsigned char *pps, int len, int pps_len);

void enQueue(RTMPPacket *pPacket);

RTMPPacket *deQueue();

void add_264_body(uint8_t *payload, int i_payload);

void add_aac_sequence_header();

void add_aac_body(unsigned char *bitbuf, int length);

void throwNativeInfo(JNIEnv *env, jmethodID methodId, int code) {
    if (env && methodId && jPublisherObj) {
        env->CallVoidMethodA(jPublisherObj, methodId, (jvalue *) &code);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_setVideoOptions(JNIEnv *env, jobject thiz, jint width,
                                                          jint height, jint bitrate, jint fps) {

    if (videoEncHandle) {
        LOGE("视频编码器已打开");
        if (mFps != fps || mBitrate != bitrate || mHeight != height
            || mWidth != width) {
            //属性不同
            x264_encoder_close(videoEncHandle);
            videoEncHandle = 0;
            free(pic_in);
            free(pic_out);
        } else {
            //属性相同
            return;
        }
    }
    // x264的参数
    //画面相关设置
    x264_param_t param;
    //    画面参数相关设置
    mWidth = width;
    mHeight = height;
    mBitrate = bitrate;
    mFps = fps;
    y_len = width * height;
    u_len = y_len / 4;
    v_len = u_len;
    //zerolatency 无缓存
    x264_param_default_preset(&param, x264_preset_names[0], x264_tune_names[7]);
    //编码复杂度
    param.i_level_idc = 51;
    //推流的格式
    param.i_csp = X264_CSP_I420;
    param.i_width = width;
    param.i_height = height;
    //编码线程
    param.i_threads = 1;
    param.i_fps_num = fps;//* 帧率分子
    param.i_fps_den = 1;//* 帧率分母
    param.i_timebase_num = param.i_fps_num;//timebase的分子
    param.i_timebase_den = param.i_fps_den;//timebase的分母

    param.i_keyint_max = fps * 2;//关键帧间隔时间的帧率
    //码率相关设置
    param.rc.i_rc_method = X264_RC_ABR;//CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
    //设置码率
    param.rc.i_bitrate = bitrate / 1000;
    //最大码率
    param.rc.i_vbv_max_bitrate = bitrate / 1000 * 1.2;
    //设置缓冲区大小
    param.rc.i_vbv_buffer_size = bitrate / 1000;

    //设置输入
    //0 用fps来做音视频同步
    param.b_vfr_input = 0;
    //流文件中判断两帧的间隔
    param.b_repeat_headers = 1;
    //设置画面质量  baseline只提供 I帧和P帧
    x264_param_apply_profile(&param, x264_profile_names[0]);

    //设置完成之后 打开解码器
    videoEncHandle = x264_encoder_open(&param);
    if (!videoEncHandle) {
        LOGI("视频编码器打开失败");
        jmethodID methodId = env->GetMethodID(env->GetObjectClass(thiz),
                                              "onPostNativeError", "(I)V");
        env->CallVoidMethodA(thiz, methodId, (jvalue *) -103);
        return;
    }
    //原数据
    pic_in = static_cast<x264_picture_t *>(malloc(sizeof(x264_picture_t)));
    pic_out = static_cast<x264_picture_t *>(malloc(sizeof(x264_picture_t)));
    x264_picture_alloc(pic_in, X264_CSP_I420, width, height);

}

void add_264_sequence_header(unsigned char *sps, unsigned char *pps, int sps_len, int pps_len) {

    //按照264标准配置SPS和PPS共使用了16字节
    int body_size = sps_len + pps_len + 16;
    RTMPPacket *packet = static_cast<RTMPPacket *>(malloc(sizeof(RTMPPacket)));
    //初始化RTMPPacket内部缓冲区
    RTMPPacket_Alloc(packet, body_size);
    RTMPPacket_Reset(packet);
    int i = 0;
    //16字节 一一赋值
    char *body = packet->m_body;
    body[i++] = 0x17;
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;
    //设置版本号
    body[i++] = 0x01;
    //profile
    body[i++] = sps[1];
    //存放兼容性
    body[i++] = sps[2];
    //baseline
    body[i++] = sps[3];
    body[i++] = 0xFF;
    body[i++] = 0xE1;
    //spp的长度 把int的低16位存放到body
    body[i++] = (sps_len >> 8) & 0xff;
    body[i++] = sps_len & 0xff;
    //sps内容存放到body
    memcpy(&body[i], sps, sps_len);
    //偏移量加上复制内容的长度
    i += sps_len;
    body[i++] = 0x01;
    //PPS长度
    body[i++] = (pps_len >> 8) & 0xff;
    body[i++] = pps_len & 0xff;
    memcpy(&body[i], pps, pps_len);
    i += pps_len;

    //packet参数设置
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nBodySize = body_size;
    //头信息永远是第一帧 所以是0
    packet->m_nTimeStamp = 0;//时间戳
    packet->m_hasAbsTimestamp = 0;//绝对时间
    packet->m_nChannel = 0x04;
    packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    enQueue(packet);

}

void enQueue(RTMPPacket *packet) {
    pthread_mutex_lock(&mutex);
    if (isPushing&&readyRtmp) {
        queue.push(packet);
    }
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

RTMPPacket *deQueue() {
    RTMPPacket *packet;
    pthread_mutex_lock(&mutex);
    if (queue.empty()) {
        //阻塞 等待
        pthread_cond_wait(&cond, &mutex);
    }
    packet = queue.front();
    queue.pop();
    pthread_mutex_unlock(&mutex);
    return packet;
}

/**
 * 消费者
 * @param arg
 * @return
 */
void *push_thread_call(void *arg) {
    isPushing = 1;
    LOGE("启动线程");
    JNIEnv *env;
    LOGE("Native Attach Thread");
    jvm->AttachCurrentThread(&env, 0);
    LOGI("Native found Java Pubulisher Object:%d", jPublisherObj ? 1 : 0);
    jclass clazz = env->GetObjectClass(jPublisherObj);
    jmethodID errorId = env->GetMethodID(clazz, "onPostNativeError", "(I)V");
    jmethodID stateId = env->GetMethodID(clazz, "onPostNativeState", "(I)V");
    do{
//实例化RTMP
        rtmp = RTMP_Alloc();
        if (!rtmp) {
            LOGE("rtmp 初始化失败");
            throwNativeInfo(env, errorId, -104);
            goto END;
        }
        RTMP_Init(rtmp);
        //设置超时时间 5秒 单位秒，默认30秒
        rtmp->Link.timeout = 5;
        /*设置URL*/
        if (!RTMP_SetupURL(rtmp, url_path)){
            LOGE("RTMP_SetupURL() failed!");
            throwNativeInfo(env, errorId, -104);
            goto END;
        }
        RTMP_EnableWrite(rtmp);
        if (!RTMP_Connect(rtmp, NULL)) {
            LOGE("RTMP_Connect() failed!");
            throwNativeInfo(env, errorId, -104);
            goto END;
        }
        //连接流
        if (!RTMP_ConnectStream(rtmp, 0)) {
            LOGE("RTMP_ConnectStream() failed!");
            throwNativeInfo(env, errorId, -104);
            goto END;
        }
        throwNativeInfo(env, stateId, 100);
        readyRtmp = 1;
        add_aac_sequence_header();
        //无限循环直到停止推流
        while (isPushing) {
//            if (!queue.empty()){}
                RTMPPacket *packet = deQueue();
                //推流
                packet->m_nInfoField2 = rtmp->m_stream_id;
                // 1代表rtmp 上传队列
                int i = RTMP_SendPacket(rtmp, packet, 1);
                if (!i) {
                    RTMPPacket_Free(packet);
                    throwNativeInfo(env, errorId, -104);
//                    pthread_mutex_unlock(&mutex);
                    goto END;
                } else {
                    //it = vec.erase(it);
                    RTMPPacket_Free(packet);
                }
//            }
            pthread_mutex_unlock(&mutex);
        }
        END:
        _RTMP_Close(rtmp);
        _RTMP_Free(rtmp);
    }while (0);
    readyRtmp = 0;
    LOGE("推流结束");
//推流结束
    isPushing = 0;
    free(url_path);
    url_path = NULL;
    throwNativeInfo(env, stateId, 101);
    jvm->DetachCurrentThread();
    pthread_exit(NULL);

}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jvm = vm;
    JNIEnv *env = NULL;
    jint result = -1;
    if (jvm) {
        LOGE("jvm init success");
    }
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return result;
    }
    return JNI_VERSION_1_4;
}



void add_264_body(uint8_t *buf, int len) {
    if (buf[2] == 0x00) { //00 00 00 01
        buf += 4;
        len -= 4;
    } else if (buf[2] == 0x01) { //00 00 01
        buf += 3;
        len -= 3;
    }
    //拼接body

    int body_size = len + 9;
    RTMPPacket *packet = static_cast<RTMPPacket *>(malloc(sizeof(RTMPPacket)));
    RTMPPacket_Alloc(packet, body_size);
    char *body = packet->m_body;
    //判断关键帧和非关键帧
    int type = buf[0] & 0x1f;
    //关键帧
    body[0] = 0x27;
    if (type == NAL_SLICE_IDR) {
        //非关键帧
        body[0] = 0x17;
    }
    //i++;
    body[1] = 0x01;
    body[2] = 0x00;
    body[3] = 0x00;
    body[4] = 0x00;
    //写入数据长度
    body[5] = (len >> 24) & 0xff;
    body[6] = (len >> 16) & 0xff;
    body[7] = (len >> 8) & 0xff;
    body[8] = len & 0xff;
    //写入数据
    memcpy(&body[9], buf, len);
    packet->m_hasAbsTimestamp = 0;
    packet->m_nBodySize = body_size;
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nChannel = 0x04;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nTimeStamp = RTMP_GetTime() - start_time;
    enQueue(packet);

}


extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_startPush(JNIEnv *env, jobject thiz, jstring url_) {

    if (!jPublisherObj) {
        jPublisherObj = env->NewGlobalRef(thiz);
    }
    const char *url = env->GetStringUTFChars(url_, 0);
    url_path = static_cast<char *>(malloc(strlen(url) + 1));
    memset(url_path, 0, strlen(url) + 1);
    memcpy(url_path, url, strlen(url));
    LOGE("startPush");
    pthread_t push_pid;
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

    start_time = RTMP_GetTime();
    pthread_create(&push_pid, NULL,push_thread_call, NULL);

    env->ReleaseStringUTFChars(url_, url);
}

void add_aac_sequence_header() {
    if (!audioEncHandle) {
        return;
    }
    unsigned char *buf;
    ULONG len;/*buf长度,一般是2*/
    faacEncGetDecoderSpecificInfo(audioEncHandle, &buf, &len);
    //封装声音的rtmp包
    int body_size = len+2;
    RTMPPacket *audio_packet = static_cast<RTMPPacket *>(malloc(sizeof(RTMPPacket)));
    RTMPPacket_Alloc(audio_packet,body_size);
    RTMPPacket_Reset(audio_packet);
    unsigned char *body = reinterpret_cast<unsigned char *>(audio_packet->m_body);
    body[0] = 0xAF;
    body[1] = 0x00;
    memcpy(&body[2],buf,len); /*spec_buf是AAC sequence header数据*/
    audio_packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    audio_packet->m_nBodySize = body_size;
    audio_packet->m_hasAbsTimestamp = 0;
    audio_packet->m_nChannel = 0x04;
    audio_packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    audio_packet->m_nTimeStamp =0;
    enQueue(audio_packet);
    free(buf);
}

void add_aac_body(unsigned char *bitbuf, int len) {
    int body_size = len + 2;
    RTMPPacket *packet = (RTMPPacket *) malloc(sizeof(RTMPPacket));
    RTMPPacket_Alloc(packet, body_size);
    char *body = packet->m_body;
    /*AF 01 + AAC RAW data*/
    body[0] = 0xAF;
    body[1] = 0x01;
    memcpy(&body[2], bitbuf, len);
    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nBodySize = body_size;
    packet->m_nChannel = 0x04;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
//	packet->m_nTimeStamp = -1;
    packet->m_nTimeStamp = RTMP_GetTime() - start_time;
    enQueue(packet);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_setAudioOptions(JNIEnv *env, jobject thiz,
                                                          jint sampleRate, jint channel) {

    if (audioEncHandle) { //如果已经打开 不支持修改
        LOGE("音频编码器已打开");
        return;
    }
    unsigned long mChannel = channel;
    int mSampleRate = sampleRate;
    audioEncHandle = faacEncOpen(mSampleRate, mChannel, &inputSamples, &maxOutputBytes);
    if (!audioEncHandle) {
        //失败
        LOGE("音频编码器打开失败");
        jmethodID methodId = env->GetMethodID(env->GetObjectClass(thiz),
                                              "onPostNativeError", "(I)V");
        env->CallVoidMethodA(thiz, methodId, (jvalue *) -102);
        return;
    }
    faacEncConfigurationPtr ptr = faacEncGetCurrentConfiguration(audioEncHandle);
    ptr->mpegVersion = MPEG4;
    ptr->allowMidside = 1;//中等压缩
    ptr->aacObjectType = LOW;
    ptr->outputFormat = 0;//输出是否包含ADTS头
    //消除爆破声
    ptr->useTns = 1;
    ptr->useLfe = 0;
    ptr->inputFormat = FAAC_INPUT_16BIT;
    ptr->quantqual = 100;
    ptr->bandWidth = 0; //频宽
    ptr->shortctl = SHORTCTL_NORMAL;
    //使配置生效
    if (!faacEncSetConfiguration(audioEncHandle, ptr)) {
        LOGE("配置失败");
    }
    LOGE("音频编码器打开完成");

}


extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_fireVideo(JNIEnv *env, jobject instance,
                                                    jbyteArray bytes_) {

    if (!isPushing || !readyRtmp || !videoEncHandle || !rtmp
        || !RTMP_IsConnected(rtmp)) {
        return;
    }
    //含有NV21的视频流数据
    jbyte *data = env->GetByteArrayElements(bytes_, NULL);
    /**
     *  NV21转YUV  NV21相对于YUV420 只是U和V的存储格式不一样所以只转换这两个
     */
    jbyte *u = reinterpret_cast<jbyte *>(pic_in->img.plane[1]);//U的首地址
    jbyte *v = reinterpret_cast<jbyte *>(pic_in->img.plane[2]);//V的首地址
    memcpy(pic_in->img.plane[0], data, y_len);//所有的Y
    for (int i = 0; i < u_len; ++i) {
        *(u + i) = *(data + y_len + i * 2 + 1);//所有的奇数是U
        *(v + i) = *(data + y_len + i * 2);//所有的偶数是V
    }

    //是一个NALUS数组
    x264_nal_t *nal = NULL;
    int pi_nal = -1;
    //YUV编码压缩
    if (x264_encoder_encode(videoEncHandle, &nal, &pi_nal, pic_in, pic_out) < 0) {
        LOGE("编码失败");
        return;
    }
    unsigned char sps[100];
    unsigned char pps[100];

    int sps_len;
    int pps_len;
    for (int i = 0; i < pi_nal; ++i) {
        // 序列参数集
        // （包括一个图像序列的所有信息，即两个 IDR 图像间的所有图像信息，如图像尺寸、视频格式等）
        if (nal[i].i_type == NAL_SPS) {
            sps_len = nal[i].i_payload - 4;
            memcpy(sps, nal[i].p_payload + 4, sps_len);

        }
            // 图像参数集
            // （包括一个图像的所有分片的所有相关信息， 包括图像类型、序列号等，
            // 解码时某些序列号的丢失可用来检验信息包的丢失与否）
        else if (nal[i].i_type == NAL_PPS) {
            pps_len = nal[i].i_payload - 4;
            memcpy(pps, nal[i].p_payload + 4, pps_len);
            //封包头信息 发送整个头信息
            add_264_sequence_header(sps, pps, sps_len, pps_len);
        } else {
            //发送关键帧与非关键帧
            add_264_body(nal[i].p_payload, nal[i].i_payload);
        }

    }

    env->ReleaseByteArrayElements(bytes_, data, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_fireAudio(JNIEnv *env, jobject instance,
                                                    jbyteArray bytes_, jint len) {
    jbyte *bytes = env->GetByteArrayElements(bytes_, NULL);

    if (!isPushing || !readyRtmp || !audioEncHandle || !rtmp
        || !RTMP_IsConnected(rtmp)) {
        return;
    }

    unsigned char *bitbuf = static_cast<unsigned char *>(malloc(
            sizeof(unsigned char) * maxOutputBytes));
    //PCM -> MPEG4
    int byte_length = faacEncEncode(audioEncHandle, reinterpret_cast<int32_t *>(bytes),
                                    inputSamples, bitbuf,
                                    maxOutputBytes);

    if (byte_length > 0) {
        add_aac_body(bitbuf,byte_length);
    }
    env->ReleaseByteArrayElements(bytes_, bytes, 0);
    if (bitbuf)
        free(bitbuf);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_stopPush(JNIEnv *env, jobject instance) {

    pthread_mutex_lock(&mutex);
    isPushing = 0;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_mik_ffmpegplayer_push_PushNative_release(JNIEnv *env, jobject thiz) {
    Java_com_mik_ffmpegplayer_push_PushNative_stopPush(env,thiz);
    if (audioEncHandle) {
        faacEncClose(audioEncHandle);
        audioEncHandle = 0;
    }
    if (videoEncHandle) {
        x264_encoder_close(videoEncHandle);
        videoEncHandle = 0;
    }
    if (jPublisherObj) {
        env->DeleteGlobalRef(jPublisherObj);
        jPublisherObj = NULL;
    }
    free(pic_in);
    free(pic_out);


}