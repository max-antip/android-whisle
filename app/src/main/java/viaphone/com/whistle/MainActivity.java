package viaphone.com.whistle;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Build;
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

import static viaphone.com.whistle.ChartView.HorScaleMode.FIXED_WIDTH;
import static viaphone.com.whistle.ChartView.HorScaleMode.ORIG;
import static viaphone.com.whistle.ChartView.VerScaleMode.FIXED_HEIGHT;


public class MainActivity extends AppCompatActivity {

    public static final int DEFAULT_SAMPLE_RATE = 44100;
    public static final int DEFAULT_BUFSIZE = 1024;
    public static final int REPAINTS_PER_SECOND = 30;
    public static final int MIN_REPAINT_DELAY = 1000 / REPAINTS_PER_SECOND;

    EditText textView;
    int processed = 0;
    long startMilis;
    long lastRepaintMilis;

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

        int sampleRate = DEFAULT_SAMPLE_RATE;
        int bufSize = DEFAULT_BUFSIZE;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            sampleRate = Integer.parseInt(myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
//            bufSize = Integer.parseInt(myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
        }
        if (!hasRecordAudioPermission()) {
            requestRecordAudioPermission();
        }


        Button playBut = (Button) findViewById(R.id.play_code);
        assert playBut != null;
        playBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (textView.getText().length() > 0) {
//                    slPlay(textView.getText().toString());
                }
            }
        });

        Button decodeBut = (Button) findViewById(R.id.listen_btn);
        assert decodeBut != null;
        decodeBut.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                slRecord();

                slInit(DEFAULT_SAMPLE_RATE, DEFAULT_BUFSIZE,
                        "appendSamples", "([S)V",
                        "appendPitch", "(F)V"
                );
            }
        });

//        int samplesPerScreen = sampleRate / 10;
//        int samplesPerScreen = sampleRate / 4;
        int samplesPerScreen = sampleRate * 3;

        FrameLayout samplesContainer = (FrameLayout) findViewById(R.id.samplesChartContainer);
        if (samplesContainer != null) {
//            int ampl = 2500;
            int ampl = 25000;
            samplesChartView = new ChartView(samplesContainer.getContext(), samplesPerScreen,
                    ChartView.ChartType.LINE, FIXED_HEIGHT, FIXED_WIDTH,
                    -ampl, ampl);
            samplesContainer.addView(samplesChartView);
        }
        FrameLayout pitchesContainer = (FrameLayout) findViewById(R.id.pitchesChartContainer);
        if (pitchesContainer != null) {
//            int size = sampleRate / slGetDecFrameSize();
            int size = sampleRate / 385;
            pitchesChartView = new ChartView(pitchesContainer.getContext(), size,
                    ChartView.ChartType.BAR, FIXED_HEIGHT, ORIG,
                    -1, 10000);
            pitchesContainer.addView(pitchesChartView);
        }

        startMilis = System.currentTimeMillis();
        lastRepaintMilis = startMilis;
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

    @SuppressWarnings("unused")
    public void appendSamples(short newSamples[]) {
        long now = System.currentTimeMillis();
//        Log.d("MainActivity", "Shown: " + processed + ", time: " + (now - startMilis));
        samplesChartView.appendValues(newSamples);
        if (now - lastRepaintMilis > MIN_REPAINT_DELAY) {
            samplesChartView.postInvalidate();
            lastRepaintMilis = now;
        }
        processed++;
    }

    @SuppressWarnings("unused")
    public void appendPitch(float pitch) {
        short[] vals = new short[1];
        vals[0] = (short) pitch;
        pitchesChartView.appendValues(vals);
        pitchesChartView.postInvalidate();
    }

    // Single out recording for run-permission needs


    private boolean hasRecordAudioPermission() {
        return (ContextCompat.checkSelfPermission(this,
                Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED);
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


    public native void slInit(int sampleRate, int bufSize, String drawSamplesMthd, String drawSamplesSg,
                              String drawPithcesMthd, String drawPitchesSg);

//    public native void slInitJavaCallBacks(String drawSamplesMthd, String drawSamplesSg,
//                                           String drawPithcesMthd, String drawPitchesSg);
//
//    public native byte[] slPlay(String mess);
//
//    public native void slRecord();
//
//    public native int slGetDecFrameSize();


}
