package com.mik.ffmpegplayer.params;

public class VideoParams {
    private int width;
    private int height;
    //码率
    private int bitrate = 480000;
    //帧率
    private int fps = 25;
    //判断是前置摄像头还是后置摄像头
    private int cameraId;

    public VideoParams(int width, int height, int bitrate,int fps,int cameraId) {
        this.width = width;
        this.height = height;
        this.bitrate = bitrate;
        this.fps = fps;
        this.cameraId = cameraId;
    }

    public int getWidth() {
        return width;
    }

    public void setWidth(int width) {
        this.width = width;
    }

    public int getHeight() {
        return height;
    }

    public void setHeight(int height) {
        this.height = height;
    }

    public int getCameraId() {
        return cameraId;
    }

    public void setCameraId(int cameraId) {
        this.cameraId = cameraId;
    }

    public int getBitrate() {
        return bitrate;
    }

    public void setBitrate(int bitrate) {
        this.bitrate = bitrate;
    }

    public int getFps() {
        return fps;
    }

    public void setFps(int fps) {
        this.fps = fps;
    }
}
