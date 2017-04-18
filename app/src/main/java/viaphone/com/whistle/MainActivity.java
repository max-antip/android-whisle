package viaphone.com.whistle;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;


public class MainActivity extends AppCompatActivity {


    EditText textView;

    static {
        System.loadLibrary("whistle-lib");
    }

    private ChartView samplesChartView;
    private ChartView pitchesChartView;


    AudioTrack audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, 44100, AudioFormat.CHANNEL_OUT_MONO,
            AudioFormat.ENCODING_PCM_16BIT, 44100/* 1 second buffer */,
            AudioTrack.MODE_STREAM);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = (EditText) findViewById(R.id.test_text);

        Button playBut = (Button) findViewById(R.id.play_code);
        assert playBut != null;
        playBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (textView.getText().length() > 0) {

                    short sound[] = slPlay(textView.getText().toString());
                    audioTrack.write(sound, 0, sound.length);
                    audioTrack.play();
//                    textView.setText(test());
                }
            }
        });

        Button decodeBut = (Button) findViewById(R.id.listen_btn);
        assert decodeBut != null;
        decodeBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                slRecord();

       /*         slInit(DEFAULT_SAMPLE_RATE, DEFAULT_BUFSIZE,
                        "appendSamples", "([S)V",
                        "appendPitch", "(F)V"
                );*/
            }
        });
    }


    public native short[] slPlay(String mess);

    public native String test();
//
//    public native void slRecord();
//
//    public native int slGetDecFrameSize();


}
