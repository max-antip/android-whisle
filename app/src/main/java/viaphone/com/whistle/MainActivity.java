package viaphone.com.whistle;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import java.util.Arrays;

import rm.com.audiowave.AudioWaveView;

public class MainActivity extends AppCompatActivity {

    EditText textView;
    AudioWaveView wave;


    static {
        System.loadLibrary("whistle-lib");
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = (EditText) findViewById(R.id.test_text);
        wave = (AudioWaveView) findViewById(R.id.wave);

        createEngine();

        int sampleRate = 44100;
        int bufSize = 0;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            String nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
//            sampleRate = Integer.parseInt(nativeParam);
            nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            bufSize = Integer.parseInt(nativeParam);
        }
        createBufferQueueAudioPlayer(sampleRate, bufSize);


        Button playBut = (Button) findViewById(R.id.play_code);

        assert playBut != null;
        playBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                byte[] result = play("hjntdb982ilj6etj6e3l");
                if (textView.getText().length() > 0) {
                    byte[] result = play(textView.getText().toString());
                    wave.setRawData(result);
                    System.out.println(Arrays.toString(result));
                }
            }
        });

        Button decodeBut = (Button) findViewById(R.id.decode);

        assert decodeBut != null;

        decodeBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                recordAudio();
//                decode(buf);
            }
        });

        Button playRecord  = (Button) findViewById(R.id.play_record);
        playRecord.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playRecord();
            }
        });
    }


    // Single out recording for run-permission needs
    static boolean created = false;
    private void recordAudio() {
        if (!created) {
            created = createAudioRecorder();
        }
        if (created) {
            startRecording();
        }
    }

    public native byte[] play(String mess);

    public static native void createEngine();

    public static native String decode(byte buf[]);

    public static native boolean createAudioRecorder();

    public static native void startRecording();

    public static native void playRecord();


    public static native void createBufferQueueAudioPlayer(int sampleRate, int samplesPerBuf);

}
