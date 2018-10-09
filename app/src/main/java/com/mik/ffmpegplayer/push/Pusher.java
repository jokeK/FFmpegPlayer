package com.mik.ffmpegplayer.push;

import android.view.View;

import com.mik.ffmpegplayer.LiveStateChangeListener;

public abstract class Pusher {
    protected LiveStateChangeListener mListener;
    protected boolean isPushing = false;
    public abstract void startPush();

    public abstract void stopPush();

    public abstract void release();
    public void setLiveStateChangeListener(LiveStateChangeListener listener) {
        mListener = listener;
    }
}
