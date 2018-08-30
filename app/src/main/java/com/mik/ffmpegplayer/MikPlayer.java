package com.mik.ffmpegplayer;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MikPlayer implements SurfaceHolder.Callback {
    static {
//        System.loadLibrary("avcodec");
//        System.loadLibrary("avdevice");
//        System.loadLibrary("avfilter");
//        System.loadLibrary("avformat");
//        System.loadLibrary("avutil");
//        System.loadLibrary("postproc");
//        System.loadLibrary("swresample");
//        System.loadLibrary("swscale");
        System.loadLibrary("MikPlayer");
    }

    private SurfaceView surfaceView;

    public void playSurface(String path){
        if (surfaceView == null){
            return;
        }
        play(path);
    }

    public void setSurfaceView(SurfaceView surfaceView) {
        if (null != this.surfaceView) {
            this.surfaceView.getHolder().removeCallback(this);
        }
        this.surfaceView = surfaceView;
        disPlay(surfaceView.getHolder().getSurface());
        surfaceView.getHolder().addCallback(this);
    }

    public native void play(String path);
    public native void disPlay(Surface surface);
    private native void native_stop();
    private native void native_release();


    public void stop(){
        native_stop();
    }

    public void release(){
        native_stop();
        native_release();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        disPlay(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }
}
