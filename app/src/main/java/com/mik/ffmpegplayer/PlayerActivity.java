package com.mik.ffmpegplayer;

import android.Manifest;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceView;
import android.view.View;

public class PlayerActivity extends AppCompatActivity {

    private MikPlayer player;
    private SurfaceView surfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.player);

        // Example of a call to a native method
        surfaceView = findViewById(R.id.surface);
        player = new MikPlayer();
        player.setSurfaceView(surfaceView);

    }


    public void start(View view) {
        player.playSurface("rtmp://live.hkstv.hk.lxdns.com/live/hks");
    }

    public void stop(View view) {
        player.stop();
    }
    @Override
    protected void onStop() {
        super.onStop();
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.release();
    }



}
