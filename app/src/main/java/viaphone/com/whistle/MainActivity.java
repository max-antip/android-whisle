package viaphone.com.whistle;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.Toast;


public class MainActivity extends AppCompatActivity {

    EditText textView;

    static {
        System.loadLibrary("whistle-lib");
    }

    private ChartView samplesChartView;
    private ChartView pitchesChartView;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = (EditText) findViewById(R.id.test_text);

        createEngine();

//        int sampleRate = 44100;
//        int bufSize = 0;

//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
//            AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
//            String nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
////            sampleRate = Integer.parseInt(nativeParam);
//            nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
////            bufSize = Integer.parseInt(nativeParam);
//        }
        createBufferQueueAudioPlayer(/*sampleRate, bufSize*/);

        Button playBut = (Button) findViewById(R.id.play_code);
        assert playBut != null;
        playBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (textView.getText().length() > 0) {
                    play(textView.getText().toString());
                }
            }
        });

        Button decodeBut = (Button) findViewById(R.id.decode);
        assert decodeBut != null;
        decodeBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!hasRecordAudioPermission()) {
                    requestRecordAudioPermission();
                }

                if (hasRecordAudioPermission()) {
                    startRec();
                }
            }
        });

        int samplesPerScreen = 44100 / 10;

        FrameLayout samplesContainer = (FrameLayout) findViewById(R.id.samplesChartContainer);
        if (samplesContainer != null) {

            samplesChartView = new ChartView(samplesContainer.getContext(), samplesPerScreen,
                    ChartView.ChartType.LINE, ChartView.ScaleMode.FIXED_HEIGHT,
                    -500, 500);
            samplesContainer.addView(samplesChartView);
        }
        FrameLayout pitchesContainer = (FrameLayout) findViewById(R.id.pitchesChartContainer);
        if (pitchesContainer != null) {
            int size = samplesPerScreen / 385;
            pitchesChartView = new ChartView(pitchesContainer.getContext(), size,
                    ChartView.ChartType.BAR, ChartView.ScaleMode.FIXED_HEIGHT,
                    -1, 15000);
            pitchesContainer.addView(pitchesChartView);
        }

//        final Random r = new Random();
//        final int len = 10;
//        final float newval[] = new float[len];
//
//        Timer t = new Timer();
//        t.scheduleAtFixedRate(new TimerTask() {
//            @Override
//            public void run() {
//                for (int i = 0; i < len; i++) {
//                    newval[i] = r.nextInt(100);
//                }
////                samplesChartView.appendValues(newval);
////                samplesChartView.postInvalidate();
//                MainActivity.this.appendChart(newval);
//            }
//        }, 0, 100);
    }


    public void appendSamples(short newSamples[]) {
        samplesChartView.appendValues(newSamples);
        samplesChartView.postInvalidate();
    }

    public void appendPitch(float pitch) {
        short[] vals = new short[1];
        vals[0] = (short) pitch;
        pitchesChartView.appendValues(vals);
        pitchesChartView.postInvalidate();
    }

    // Single out recording for run-permission needs
    static boolean created = false;

    private void startRec() {
        if (!created) {
            created = createAudioRecorder();
        }
        if (created) {
            startRecording(
                    "appendSamples", "([S)V",
                    "appendPitch", "(F)V"
            );
        }
    }

    private boolean hasRecordAudioPermission() {
        boolean hasPermission = (ContextCompat.checkSelfPermission(this,
                Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED);

//        log("Has RECORD_AUDIO permission? " + hasPermission);
        return hasPermission;
    }

    int PERMISSIONS_REQUEST_RECORD_AUDIO = 1;

    private void requestRecordAudioPermission() {

        String requiredPermission = Manifest.permission.RECORD_AUDIO;

        // If the user previously denied this permission then show a message explaining why
        // this permission is needed
        if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                requiredPermission)) {

            showToast("This app needs to record audio through the microphone....");
        }

        // request the permission.
        ActivityCompat.requestPermissions(this,
                new String[]{requiredPermission},
                PERMISSIONS_REQUEST_RECORD_AUDIO);
    }

    private void showToast(String s) {
        LayoutInflater myInflater = LayoutInflater.from(this);
        View view = myInflater.inflate(R.layout.activity_main, null);
        Toast mytoast = new Toast(this);
        mytoast.setView(view);
        mytoast.setDuration(Toast.LENGTH_LONG);
        mytoast.setText(s);
        mytoast.show();
    }


    public native byte[] play(String mess);

    public native void createEngine();

    public native boolean createAudioRecorder();

    public native void startRecording(String drawSamplesMthd, String drawSamplesSg,
                                      String drawPithcesMthd, String drawPitchesSg);

    public native void createBufferQueueAudioPlayer(/*int sampleRate, int samplesPerBuf*/);

}
