package com.mik.ffmpegplayer;

import android.content.Intent;
import android.hardware.Camera;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.mik.ffmpegplayer.push.LivePush;

public class RtmpActivity extends AppCompatActivity implements LiveStateChangeListener, View.OnClickListener {
    private LivePush livePush;
    private boolean isStart = false;
    private static String URL = "rtmp://47.98.157.77:1935/myapp/mik";
    private SurfaceHolder mSurfaceHolder;
    private Button button;

    private Handler mHandler = new Handler() {
        public void handleMessage(android.os.Message msg) {
            switch (msg.what) {

                case -100:
                    Toast.makeText(RtmpActivity.this, "视频预览开始失败", Toast.LENGTH_SHORT).show();
                    livePush.stopPush();
                    break;
                case -101:
                    Toast.makeText(RtmpActivity.this, "音频录制失败", Toast.LENGTH_SHORT).show();
                    livePush.stopPush();
                    break;
                case -102:
                    Toast.makeText(RtmpActivity.this, "音频编码器配置失败", Toast.LENGTH_SHORT).show();
                    livePush.stopPush();
                    break;
                case -103:
                    Toast.makeText(RtmpActivity.this, "视频频编码器配置失败", Toast.LENGTH_SHORT).show();
                    livePush.stopPush();
                    break;
                case -104:
                    Toast.makeText(RtmpActivity.this, "流媒体服务器/网络等问题", Toast.LENGTH_SHORT).show();
                    livePush.stopPush();
                    break;
            }
            button.setText(R.string.start_live);
            isStart = false;
        };
    };
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.live_activity);
        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        button = findViewById(R.id.button3);
        mSurfaceHolder = surfaceView.getHolder();
        livePush = new LivePush(this,800, 480, 1200_000, 10,
                Camera.CameraInfo.CAMERA_FACING_BACK);
        livePush.setLiveStateChangeListener(this);
        button.setOnClickListener(this);
        livePush.prepare(mSurfaceHolder);
    }


    @Override
    public void onClick(View v) {
        if (isStart){
            livePush.stopPush();
            button.setText(R.string.start_live);
            isStart = true;
        }else {
            livePush.startPush(URL);
            button.setText(R.string.stop_live);
            isStart = false;

        }
    }

    public void switchVideo(View view) {
        livePush.swichCamera();
    }

    @Override
    public void onErrorPusher(int code) {
        mHandler.sendEmptyMessage(code);
    }

    @Override
    public void onStartPusher() {
        Log.d("TAG", "开始推流");
    }

    @Override
    public void onStopPusher() {
        Log.d("TAG", "结束推流");
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        livePush.relase();
    }


}
