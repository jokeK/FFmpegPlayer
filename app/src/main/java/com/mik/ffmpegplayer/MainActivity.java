package com.mik.ffmpegplayer;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.text.Html;
import android.view.View;
import android.webkit.WebView;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("MikPlayer");
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        requestAllPower();
//        TextView textView = findViewById(R.id.textView);
//        textView.setText(Html.fromHtml(tx,0));

    }


    public void play(View view) {
        Intent playIntent = new Intent(this,PlayerActivity.class);
        startActivity(playIntent);
    }

    public void rtmp(View view) {
        Intent rtmpIntent = new Intent(this,RtmpActivity.class);
        startActivity(rtmpIntent);
    }
    public void requestAllPower() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
            } else {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE,
                                Manifest.permission.READ_EXTERNAL_STORAGE,Manifest.permission.RECORD_AUDIO,
                        Manifest.permission.CAMERA,Manifest.permission.RECORD_AUDIO}, 10);
            }
        }
    }


}
