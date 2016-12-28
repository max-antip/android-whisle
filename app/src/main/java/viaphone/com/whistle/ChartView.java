package viaphone.com.whistle;


import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;


public class ChartView extends View {

    public enum ChartType {BAR, LINE}

    public enum ScaleMode {FIXED_HEIGHT, FRAME_SCALE}

    private final Paint paint;
    private final ChartType chartType;
    private final ScaleMode scaleMode;
    private final int samplesPerScreen;
    private final short[] values;
    private int cursor;
    private int fixedMin;
    private int fixedMax;

    public ChartView(Context context) {
        this(context, 500, ChartType.BAR, ScaleMode.FRAME_SCALE, 0, 0);
    }

    public ChartView(Context context, int samplesPerScreen,
                     ChartType chartType,
                     ScaleMode scaleMode,
                     int fixedMin, int fixedMax) {
        super(context);
        this.samplesPerScreen = samplesPerScreen;
        this.chartType = chartType;
        this.scaleMode = scaleMode;
        this.fixedMin = fixedMin;
        this.fixedMax = fixedMax;
        paint = new Paint();
        values = new short[samplesPerScreen];
        for (int i = 0; i < samplesPerScreen; i++) {
            values[i] = 0;
        }
    }

    public void appendValues(short[] newvals) {
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
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float border = 20;
        float horstart = border * 2;
        int height = getHeight();
        int width = getWidth() - 1;
        int min;
        int max;
        if (scaleMode == ScaleMode.FRAME_SCALE) {
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
            if (chartType == ChartType.BAR) {
                float datalength = values.length;
                float colwidth = (width - (2 * border)) / datalength;
//                for (int i = 0; i < values.length; i++) {
                for (int i = 0; i < samplesPerScreen; i++) {
                    int idx = cursor + i;
                    if (idx >= samplesPerScreen) idx -= samplesPerScreen;
                    float val = values[idx] - min;
                    float rat = val / diff;
                    float h = graphheight * rat;
                    canvas.drawRect((i * colwidth) + horstart, (border - h) + graphheight, ((i * colwidth) + horstart) + (colwidth - 1), height - (border - 1), paint);
                }
            } else {
                float datalength = values.length;
                float colwidth = (width - (2 * border)) / datalength;
                float halfcol = colwidth / 2;
                float lasth = 0;
                for (int i = 0; i < samplesPerScreen; i++) {
                    int idx = cursor + i;
                    if (idx >= samplesPerScreen) idx -= samplesPerScreen;
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
