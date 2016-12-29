package viaphone.com.whistle;


import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;

import static viaphone.com.whistle.ChartView.ChartType.BAR;
import static viaphone.com.whistle.ChartView.HorScaleMode.ORIG;
import static viaphone.com.whistle.ChartView.VerScaleMode.FRAME_SCALE;


public class ChartView extends View {

    public static final int MAX_SAMPLES_PER_CHART = 2000;

    public enum ChartType {BAR, LINE}

    public enum VerScaleMode {FIXED_HEIGHT, FRAME_SCALE}

    public enum HorScaleMode {ORIG, FIXED_WIDTH}

    private final Paint paint;
    private final ChartType chartType;
    private final VerScaleMode verScaleMode;
    private final HorScaleMode horScaleMode;
    private final int samplesPerScreen;
    private final short[] values;
    private int cursor;
    private int fixedMin;
    private int fixedMax;
    private int smoothFactor = 1;

    public ChartView(Context context) {
        this(context, 500, BAR, FRAME_SCALE, ORIG, 0, 0);
    }

    public ChartView(Context context, int samplesPerScreen,
                     ChartType chartType,
                     VerScaleMode verScaleMode,
                     HorScaleMode horScaleMode,
                     int fixedMin, int fixedMax) {
        super(context);
        this.samplesPerScreen = samplesPerScreen;
        int size;
        if (samplesPerScreen > MAX_SAMPLES_PER_CHART) {
            size = MAX_SAMPLES_PER_CHART;
            smoothFactor = samplesPerScreen / MAX_SAMPLES_PER_CHART;
        } else {
            size = samplesPerScreen;
            smoothFactor = 1;
        }

        this.chartType = chartType;
        this.verScaleMode = verScaleMode;
        this.horScaleMode = horScaleMode;
        this.fixedMin = fixedMin;
        this.fixedMax = fixedMax;
        paint = new Paint();
        values = new short[size];
        for (int i = 0; i < size; i++) {
            values[i] = 0;
        }


    }

    public void appendValues(short[] newvals) {
        if (horScaleMode == ORIG || smoothFactor == 1) {
            int size;
            if (newvals.length < samplesPerScreen) {
                size = newvals.length;
            } else {
                size = samplesPerScreen;
            }

            if (cursor + size < samplesPerScreen) {
                System.arraycopy(newvals, 0, values, cursor, size);
                cursor += size;
            } else {
                System.arraycopy(newvals, 0, values, cursor, samplesPerScreen - cursor);
                int restSize = size - (samplesPerScreen - cursor);
                System.arraycopy(newvals, 0, values, 0, restSize);
                cursor = restSize;
            }
        } else {
            int sm = 0;
            int cm = 0;
            for (short v : newvals) {
                if (cm++ < smoothFactor) {
                    sm += v;
                } else {
                    values[cursor++] = (short) ((1.0 * sm) / cm);
                    if (cursor >= values.length) cursor = 0;
                    cm = 0;
                }
            }
        }


    }

    @Override
    protected void onDraw(Canvas canvas) {
        float border = 20;
        float horstart = border * 2;
        int height = getHeight();
        int width = getWidth() - 1;
        int min;
        int max;
        if (verScaleMode == FRAME_SCALE) {
            min = getMin();
            max = getMax();
        } else {
            min = fixedMin;
            max = fixedMax;
        }

        int diff = max - min;
        float graphheight = height - (2 * border);
        float graphwidth = width - (2 * border);

        if (max != min) {
            paint.setColor(Color.LTGRAY);
            if (chartType == BAR) {
                float colwidth = (width - (2 * border)) / values.length;
                for (int i = 0; i < values.length; i++) {
                    int idx = cursor + i;
                    if (idx >= values.length) idx -= values.length;
                    float val = values[idx] - min;
                    float rat = val / diff;
                    float h = graphheight * rat;
                    canvas.drawRect((i * colwidth) + horstart, (border - h) + graphheight, ((i * colwidth) + horstart) + (colwidth - 1), height - (border - 1), paint);
                }
            } else {
                float colwidth = (width - (2 * border)) / values.length;
                float halfcol = colwidth / 2;
                float lasth = 0;
                for (int i = 0; i < values.length; i++) {
                    int idx = cursor + i;
                    if (idx >= values.length) idx -= values.length;
                    float val = values[idx] - min;
                    float rat = val / diff;
                    float h = graphheight * rat;
                    if (i > 0)
                        canvas.drawLine(((i - 1) * colwidth) + (horstart + 1) + halfcol, (border - lasth) + graphheight, (i * colwidth) + (horstart + 1) + halfcol, (border - h) + graphheight, paint);
                    lasth = h;
                }
            }
        }
    }

    private short getMax() {
        if (values == null) return 0;
        short largest = Short.MIN_VALUE;
        for (short value : values)
            if (value > largest)
                largest = value;
        return largest;
    }

    private short getMin() {
        if (values == null) return 0;
        short smallest = Short.MAX_VALUE;
        for (short value : values)
            if (value < smallest)
                smallest = value;
        return smallest;
    }

}
