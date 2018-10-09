package com.mik.ffmpegplayer.push;

import android.app.Activity;
import android.hardware.Camera;
import android.util.Log;
import android.view.SurfaceHolder;

import com.mik.ffmpegplayer.LiveStateChangeListener;
import com.mik.ffmpegplayer.RtmpActivity;
import com.mik.ffmpegplayer.params.AudioParams;
import com.mik.ffmpegplayer.params.VideoParams;

public class LivePush {
    private AudioPush audioPush;
    private VideoPush videoPush;
    private AudioParams audioParams;
    private VideoParams videoParams;
    private PushNative mNative;
    private Activity mActivity;
    private LiveStateChangeListener mListener;

    public LivePush(Activity activity, int width, int height, int bitrate, int fps, int cameraId) {
        this.mActivity = activity;
        audioParams = new AudioParams(44100,1);
        videoParams = new VideoParams(width,height,bitrate, fps, cameraId);
        mNative = new PushNative();
    }

    public void prepare(SurfaceHolder mSurfaceHolder) {
        mSurfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        audioPush = new AudioPush(audioParams,mNative);
        videoPush = new VideoPush(mActivity,videoParams,mSurfaceHolder,mNative);

        videoPush.setLiveStateChangeListener(mListener);
        audioPush.setLiveStateChangeListener(mListener);
    }

    public void startPush(String url) {
        videoPush.startPush();
        audioPush.startPush();
        Log.d("TAG","startPush");
        mNative.startPush(url);

    }

    public void stopPush() {
        videoPush.stopPush();
        audioPush.stopPush();
        mNative.stopPush();
    }

    public void swichCamera() {
        videoPush.switchCamera();
    }



    public void setLiveStateChangeListener(LiveStateChangeListener listener){
        mListener = listener;
        mNative.setLiveStateChangeListener(listener);
        if (null != videoPush) {
            videoPush.setLiveStateChangeListener(listener);
        }
        if (null != audioPush) {
            audioPush.setLiveStateChangeListener(listener);
        }

    }

    public void relase() {
        mActivity = null;
        stopPush();
        videoPush.setLiveStateChangeListener(null);
        audioPush.setLiveStateChangeListener(null);
        mNative.setLiveStateChangeListener(null);
        videoPush.release();
        audioPush.release();
        mNative.release();
    }


}
