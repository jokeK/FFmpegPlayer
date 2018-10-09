package com.mik.ffmpegplayer.push;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.util.Log;

import com.mik.ffmpegplayer.params.AudioParams;

public class AudioPush extends Pusher {
    private AudioParams audioParams;
    private int mimBufferSize;
    private static final String TAG = "AudioPush";

    private AudioTrack audioTrack;
    private AudioRecord audioRecord;
    private PushNative mNative;

    public AudioPush(AudioParams audioParams,PushNative mNative) {
        this.mNative = mNative;
        this.audioParams = audioParams;
        //CHANNEL_IN_MONO 单通道
        //CHANNEL_IN_STEREO立体声
        int channelConfig = audioParams.getChannel() == 1?
                AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO;

        //最小缓冲区
        mimBufferSize = AudioRecord.getMinBufferSize(audioParams.getSampleRateInHz(),channelConfig,AudioFormat.ENCODING_PCM_16BIT);
        audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC,
                audioParams.getSampleRateInHz(),
                channelConfig,
                AudioFormat.ENCODING_PCM_16BIT, mimBufferSize);
        mNative.setAudioOptions(audioParams.getSampleRateInHz(),audioParams.getChannel());

    }

    @Override
    public void startPush() {
        if (null == audioRecord) {
            return;
        }
        isPushing = true;
        if (audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_STOPPED){

            try{
                //开启录音
                audioRecord.startRecording();
                new Thread(new AudioRecordTask()).start();
            }catch (Throwable th){
                th.printStackTrace();
                if (null != mListener) {
                    mListener.onErrorPusher(-101);
                }
            }
        }
    }

    @Override
    public void stopPush() {
        if (null == audioRecord) {
            return;
        }
        isPushing = false;
        if (audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING)
            audioRecord.stop();
    }

    @Override
    public void release() {
        if (null == audioRecord) {
            return;
        }
        isPushing = false;
        if (audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_STOPPED)
            audioRecord.release();
        audioRecord = null;
    }

    private class AudioRecordTask implements Runnable{

        @Override
        public void run() {

            while (isPushing&& audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING){
                //通过audioRecord不断读取音频数据
                byte[] buffer = new byte[mimBufferSize];
                int len = audioRecord.read(buffer, 0, buffer.length);
                if (len>0){
                    //传给native代码进行音频编码
                    mNative.fireAudio(buffer,len);

                }
            }
        }
    }


}
