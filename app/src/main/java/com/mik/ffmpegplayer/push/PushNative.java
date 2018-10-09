package com.mik.ffmpegplayer.push;

import android.util.Log;

import com.mik.ffmpegplayer.LiveStateChangeListener;

public class PushNative {
    private LiveStateChangeListener mListener;

    public void setLiveStateChangeListener(LiveStateChangeListener listener) {
        mListener = listener;
    }

    public void onPostNativeError(int code) {
        Log.d("PushNative", code + "");
        if (null != mListener) {
            mListener.onErrorPusher(code);
        }
    }

    public void onPostNativeState(int state) {
        if (state == 100) {
            mListener.onStartPusher();
        } else if (state == 101) {
            mListener.onStopPusher();
        }
    }

    //设置视频参数
    public native void setVideoOptions(int width,int height,int bitrate,int fps);

    //设置音频参数
    public native void setAudioOptions(int sampleRate,int channel);

    //推视频流
    public native void fireVideo(byte[] bytes);

    //音频推流
    public native void fireAudio(byte[] bytes,int len);

    public native void startPush(String url);

    public native void stopPush();

    public native void release();

}
