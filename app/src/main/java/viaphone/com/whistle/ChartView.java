package viaphone.com.whistle;


import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;

import java.util.Arrays;

import static java.lang.Math.min;


public class ChartView extends View {

    public enum Type {BAR, LINE}

    private final Paint paint;
    private final Type type;
    private final int samplesPerScreen;
    private final short[] values;
    private int cursor;

    public ChartView(Context context) {
        this(context, 500, Type.BAR);
    }

    public ChartView(Context context, int samplesPerScreen, Type type) {
        super(context);
        this.samplesPerScreen = samplesPerScreen;
        this.type = type;
        paint = new Paint();
        values = new short[samplesPerScreen];
        for (int i = 0; i < samplesPerScreen; i++) {
            values[i] = 0;
        }
    }

    public void appendSamples(short[] newvals) {
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


//        int size, start;
//        if (newvals.length < samplesPerScreen) {
//            size = newvals.length;
//        } else {
//            size = samplesPerScreen;
//        }
//        start = samplesPerScreen - size;
//        if (start > 0) {
//            if (start > size) {
//                System.arraycopy(values, start, values, start - size, size);
//            } else {
//                System.arraycopy(values, start, values, 0, size - start);
//            }
//        }
//        System.arraycopy(newvals, 0, values, start, size);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float border = 20;
        float horstart = border * 2;
        float height = getHeight();
        float width = getWidth() - 1;
        float max = getMax();
        float min = getMin();
        float diff = max - min;
        float graphheight = height - (2 * border);
        float graphwidth = width - (2 * border);

        if (max != min) {
            paint.setColor(Color.LTGRAY);
            if (type == Type.BAR) {
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

    private float getMax() {
        if (values == null) return 0;
        float largest = Integer.MIN_VALUE;
        for (float value : values)
            if (value > largest)
                largest = value;
        return largest;
    }

    private float getMin() {
        if (values == null) return 0;
        float smallest = Integer.MAX_VALUE;
        for (float value : values)
            if (value < smallest)
                smallest = value;
        return smallest;
    }

}
