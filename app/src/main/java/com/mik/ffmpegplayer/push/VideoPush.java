package com.mik.ffmpegplayer.push;


import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.hardware.camera2.CameraManager;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import com.mik.ffmpegplayer.params.VideoParams;

import java.io.IOException;
import java.util.Iterator;
import java.util.List;

public class VideoPush extends Pusher implements SurfaceHolder.Callback, Camera.PreviewCallback {
    private VideoParams videoParams;
    private Camera camera;
    private SurfaceHolder surfaceHolder;
    private byte[] buffers;
    private static final String TAG = "VideoPush";
    private PushNative pushNative;
    private Activity mActivity;
    private int screen;
    private byte[] raw;
    private final static int SCREEN_PORTRAIT = 0;
    private final static int SCREEN_LANDSCAPE_LEFT = 90;
    private final static int SCREEN_LANDSCAPE_RIGHT = 270;

    public VideoPush(Activity activity, VideoParams videoParams, SurfaceHolder surfaceHolder, PushNative mNative) {
        this.mActivity = activity;
        this.videoParams = videoParams;
        this.surfaceHolder = surfaceHolder;
        this.pushNative = mNative;
        this.surfaceHolder.addCallback(this);
    }


    @Override
    public void startPush() {
        startPreview();
        isPushing = true;
    }

    @Override
    public void stopPush() {

    }

    @Override
    public void release() {

    }

    public void switchCamera() {
        if (videoParams.getCameraId() == Camera.CameraInfo.CAMERA_FACING_BACK) {
            videoParams.setCameraId(Camera.CameraInfo.CAMERA_FACING_FRONT);
        } else {
            videoParams.setCameraId(Camera.CameraInfo.CAMERA_FACING_BACK);
        }
        //重新预览
        stopPreview();
        startPreview();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {


    }


    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        stopPreview();
        startPreview();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    /**
     * 停止预览
     */
    private void stopPreview() {
        if (camera != null) {
            camera.stopPreview();
            camera.release();
            camera = null;
        }
    }

    //开始预览
    private void startPreview() {
        try {
            pushNative.setVideoOptions(videoParams.getWidth(), videoParams.getHeight(),
                    videoParams.getBitrate(), videoParams.getFps());
            camera = Camera.open(videoParams.getCameraId());
            Camera.Parameters parameters = camera.getParameters();
            //YUV 预览图像的像素格式
            parameters.setPreviewFormat(ImageFormat.NV21);
            setPreviewSize(parameters);
            setPreviewOrientation(parameters);
            //parameters.setPictureSize(videoParams.getWidth(), videoParams.getHeight());
            camera.setParameters(parameters);
            camera.setPreviewDisplay(surfaceHolder);
            int bits = ImageFormat.getBitsPerPixel(ImageFormat.NV21);
            buffers = new byte[videoParams.getWidth() * videoParams.getHeight() * bits/8];
            raw = new byte[videoParams.getWidth() * videoParams.getHeight() * bits/8];
            //相机优化
            camera.addCallbackBuffer(buffers);
            camera.setPreviewCallbackWithBuffer(this);
            camera.startPreview();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * 相机预览导画面的回调
     *
     * @param bytes 预览的数据
     * @param camera
     */
    @Override
    public void onPreviewFrame(byte[] bytes, Camera camera) {
        if (isPushing) {
            switch (screen) {
                case SCREEN_PORTRAIT:
                    portraitData2Raw(bytes);
                    break;
//                头  是在左边  home  右边
                case SCREEN_LANDSCAPE_LEFT:
                    raw=buffers;
                    break;
                case SCREEN_LANDSCAPE_RIGHT:
                    landscapeData2Raw(bytes);
                    break;

            }
            pushNative.fireVideo(raw);
        }
        if (camera != null) {
            //不写这句话，onPreviewFrame只会调用一次
            camera.addCallbackBuffer(bytes);
        }

    }

    private void setPreviewSize(Camera.Parameters parameters) {
        List<Integer> supportedPreviewFormats = parameters.getSupportedPreviewFormats();
        for (Integer integer : supportedPreviewFormats) {
            System.out.println("支持:" + integer);
        }
        List<Camera.Size> supportedPreviewSizes = parameters.getSupportedPreviewSizes();
        Camera.Size size = supportedPreviewSizes.get(0);
        Log.d(TAG, "支持 " + size.width + "x" + size.height);
        int m = Math.abs(size.height * size.width - videoParams.getHeight() * videoParams.getWidth());
        supportedPreviewSizes.remove(0);
        Iterator<Camera.Size> iterator = supportedPreviewSizes.iterator();
        while (iterator.hasNext()) {
            Camera.Size next = iterator.next();
            Log.d(TAG, "支持 " + next.width + "x" + next.height);
            int n = Math.abs(next.height * next.width - videoParams.getHeight() *videoParams.getWidth());
            if (n < m) {
                m = n;
                size = next;
            }
        }
        videoParams.setHeight(size.height);
        videoParams.setWidth(size.width);
        parameters.setPreviewSize(videoParams.getWidth(),videoParams.getHeight());
        Log.d(TAG, "预览分辨率 width:" + size.width + " height:" + size.height);
    }

    private void setPreviewOrientation(Camera.Parameters parameters) {
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(videoParams.getCameraId(), info);
        int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        screen = 0;
        switch (rotation) {
            case Surface.ROTATION_0:
                screen = SCREEN_PORTRAIT;
                pushNative.setVideoOptions(videoParams.getHeight(), videoParams.getWidth(), videoParams.getBitrate(), videoParams.getFps());
                break;
            case Surface.ROTATION_90: // 横屏 左边是头部(home键在右边)
                screen = SCREEN_LANDSCAPE_LEFT;
                pushNative.setVideoOptions(videoParams.getWidth(), videoParams.getHeight(), videoParams.getBitrate(), videoParams.getFps());
                break;
            case Surface.ROTATION_180:
                screen = 180;
                break;
            case Surface.ROTATION_270:// 横屏 头部在右边
                screen = SCREEN_LANDSCAPE_RIGHT;
                pushNative.setVideoOptions(videoParams.getWidth(), videoParams.getHeight(), videoParams.getBitrate(), videoParams.getFps());
                break;
        }
        int result;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + screen) % 360;
            result = (360 - result) % 360; // compensate the mirror
        } else { // back-facing
            result = (info.orientation - screen + 360) % 360;
        }
        camera.setDisplayOrientation(result);
    }

    private void landscapeData2Raw(byte[] data) {
        int width =videoParams.getWidth();
        int height = videoParams.getHeight();
        int y_len = width * height;
        int k = 0;
        // y数据倒叙插入raw中
        for (int i = y_len - 1; i > -1; i--) {
            raw[k] = data[i];
            k++;
        }
        // System.arraycopy(data, y_len, raw, y_len, uv_len);
        // v1 u1 v2 u2
        // v3 u3 v4 u4
        // 需要转换为:
        // v4 u4 v3 u3
        // v2 u2 v1 u1
        int maxpos = data.length - 1;
        int uv_len = y_len >> 2; // 4:1:1
        for (int i = 0; i < uv_len; i++) {
            int pos = i << 1;
            raw[y_len + i * 2] = data[maxpos - pos - 1];
            raw[y_len + i * 2 + 1] = data[maxpos - pos];
        }
    }

    private void portraitData2Raw(byte[] data) {
        // if (mContext.getResources().getConfiguration().orientation !=
        // Configuration.ORIENTATION_PORTRAIT) {
        // raw = data;
        // return;
        // }
        int width = videoParams.getWidth(), height = videoParams.getHeight();
        int y_len = width * height;
        int uvHeight = height >> 1; // uv数据高为y数据高的一半
        int k = 0;
        if ( videoParams.getCameraId() == Camera.CameraInfo.CAMERA_FACING_BACK) {
            for (int j = 0; j < width; j++) {
                for (int i = height - 1; i >= 0; i--) {
                    raw[k++] = data[width * i + j];
                }
            }
            for (int j = 0; j < width; j += 2) {
                for (int i = uvHeight - 1; i >= 0; i--) {
                    raw[k++] = data[y_len + width * i + j];
                    raw[k++] = data[y_len + width * i + j + 1];
                }
            }
        } else {
            for (int i = 0; i < width; i++) {
                int nPos = width - 1;
                for (int j = 0; j < height; j++) {
                    raw[k] = data[nPos - i];
                    k++;
                    nPos += width;
                }
            }
            for (int i = 0; i < width; i += 2) {
                int nPos = y_len + width - 1;
                for (int j = 0; j < uvHeight; j++) {
                    raw[k] = data[nPos - i - 1];
                    raw[k + 1] = data[nPos - i];
                    k += 2;
                    nPos += width;
                }
            }
        }
    }

}
