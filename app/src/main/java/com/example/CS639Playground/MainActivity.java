package com.example.CS639Playground;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.os.Bundle;
import android.util.Base64;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import android.view.View;
import com.anychart.AnyChart;
import com.anychart.AnyChartView;
import com.anychart.chart.common.dataentry.DataEntry;
import com.anychart.chart.common.dataentry.ValueDataEntry;
import com.anychart.charts.Scatter;
import com.anychart.core.scatter.series.Marker;
import com.anychart.enums.MarkerType;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.zip.Deflater;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    // stuff for the UI
    private TextView consoleDisplay;
    private TextView countDisplay;
    private ScrollView scrollView;
    private Scatter scatter;
    private Marker marker;
    private AnyChartView chart;

    // keep track of things to send to C++
    private int thread_count = 1;
    private String[] modes = { "Cast", "PtrArr", "Random", "FlipXY" };
    private String[] modeCols = { "Red", "Green", "Blue", "Black" };
    private int mode = 0;

    public static Bitmap[] dividirBitmap(Bitmap bitmapOriginal) {
        int larguraOriginal = bitmapOriginal.getWidth();
        int alturaOriginal = bitmapOriginal.getHeight();

        int larguraParte = larguraOriginal / 2;
        int alturaParte = alturaOriginal / 2;

        // Criar quatro novos bitmaps
        Bitmap parte1 = Bitmap.createBitmap(bitmapOriginal, 0, 0, larguraParte, alturaParte);
        Bitmap parte2 = Bitmap.createBitmap(bitmapOriginal, larguraParte, 0, larguraParte, alturaParte);
        Bitmap parte3 = Bitmap.createBitmap(bitmapOriginal, 0, alturaParte, larguraParte, alturaParte);
        Bitmap parte4 = Bitmap.createBitmap(bitmapOriginal, larguraParte, alturaParte, larguraParte, alturaParte);

        Bitmap[] partes = {parte1, parte2, parte3, parte4};
        return partes;
    }

    private Bitmap criarBitmapComCruz() {
        int largura = 369;
        int altura = 800;

        // Cria um Bitmap com a largura, altura e configuração ARGB_8888
        Bitmap bitmap = Bitmap.createBitmap(largura, altura, Bitmap.Config.ARGB_8888);

        // Cria um objeto Canvas para desenhar no Bitmap
        Canvas canvas = new Canvas(bitmap);

        // Preenche o Bitmap com a cor azul
        canvas.drawColor(Color.BLUE);

        // Define as coordenadas do centro do Bitmap
        float centerX = largura / 2.0f;
        float centerY = altura / 2.0f;

        // Define as dimensões da cruz
        float crossSize = Math.min(largura, altura) * 0.4f;

        // Desenha uma cruz no centro do Bitmap
        drawCross(canvas, centerX, centerY, crossSize, Color.WHITE);

        return bitmap;
    }

    private void drawCross(Canvas canvas, float centerX, float centerY, float size, int color) {
        Paint paint = new Paint();
        paint.setColor(color);
        paint.setStyle(Paint.Style.FILL);

        // Desenha a linha horizontal da cruz
        canvas.drawRect(centerX - size / 2, centerY - size / 10, centerX + size / 2, centerY + size / 10, paint);

        // Desenha a linha vertical da cruz
        canvas.drawRect(centerX - size / 10, centerY - size / 2, centerX + size / 10, centerY + size / 2, paint);
    }
    public static byte[] compress(byte[] data) throws IOException {
        Deflater deflater = new Deflater(Deflater.DEFAULT_COMPRESSION);
        deflater.setInput(data);
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream(data.length);
        deflater.finish();
        byte[] buffer = new byte[1024];
        while (!deflater.finished()) {
            int count = deflater.deflate(buffer);
            outputStream.write(buffer, 0, count);
        }
        outputStream.close();
        return outputStream.toByteArray();
    }


    private static String encodeToBase64(byte[] binaryData) {
        byte[] base64Encoded = android.util.Base64.encode(binaryData, Base64.DEFAULT);
        return new String(base64Encoded);
    }

    private static byte[] compressBitmapWriteBinary(Bitmap image) throws IOException {
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        image.compress(Bitmap.CompressFormat.JPEG, 1, byteArrayOutputStream);
        byte[] a = compress(byteArrayOutputStream.toByteArray());
        String s = encodeToBase64(a);

        return a;
    }

    public static Bitmap captureAndResizeView(Bitmap originalBitmap, int originalWidth, int originalHeight, int newHeight) {
        float aspectRatio = (float) originalWidth / originalHeight;
        int newWidth = Math.round(newHeight * aspectRatio);

        return Bitmap.createScaledBitmap(originalBitmap, newWidth, newHeight, true);
    }

    private Bitmap convertViewToDrawable(View view) {
        Bitmap bitmap = Bitmap.createBitmap(view.getMeasuredWidth(), view.getMeasuredHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.translate(-view.getScrollX(), -view.getScrollY());
        //draw(view);
        view.draw(canvas);
        return captureAndResizeView(bitmap, view.getMeasuredWidth(), view.getMeasuredHeight(), 800);
    }

    private static class ColorExtractorTask implements Runnable {
        private final int[][] colors;
        private final int[] pixels;
        private final int width;
        private final int startY;
        private final int endY;

        public ColorExtractorTask(int[][] colors, int[] pixels, int width, int startY, int endY) {
            this.colors = colors;
            this.pixels = pixels;
            this.width = width;
            this.startY = startY;
            this.endY = endY;
        }

        @Override
        public void run() {
            for (int x = 0; x < width; x++) {
                for (int y = startY; y < endY; y++) {
                    int pixelColor = pixels[y * width + x];
                    colors[x][y] = pixelColor;
                }
            }
        }
    }

    public static int[][] extractColors(Bitmap bitmap) {
        if (bitmap == null) {
            return null;
        }

        long startTime = System.currentTimeMillis();  // Tempo de início da execução

        int width = bitmap.getWidth();
        int height = bitmap.getHeight();

        int[][] colors = new int[width][height];
        int pixelCount = 0;

        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);

        int numThreads = Runtime.getRuntime().availableProcessors();  // Obtém o número de processadores disponíveis
        ExecutorService executorService = Executors.newFixedThreadPool(numThreads);

        int regionSize = height / numThreads;
        for (int i = 0; i < numThreads; i++) {
            int startY = i * regionSize;
            int endY = (i == numThreads - 1) ? height : (i + 1) * regionSize;

            executorService.submit(new ColorExtractorTask(colors, pixels, width, startY, endY));
        }

        executorService.shutdown();
        try {
            executorService.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        long endTime = System.currentTimeMillis();
        long elapsedTime = endTime - startTime;

        System.out.println("Número total de pixels: " + pixelCount);
        System.out.println("Tempo de execução: " + elapsedTime + " milissegundos");

        return colors;
    }


    public static int[][] extractColorsSample(Bitmap bitmap) {
        if (bitmap == null) {
            return null;
        }

        long startTime = System.currentTimeMillis();  // Tempo de início da execução

        int width = bitmap.getWidth();
        int height = bitmap.getHeight();

        int[][] colors = new int[width][height];
        int pixelCount = 0;  // Contador de pixels

        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int pixelColor = pixels[y * width + x];
                colors[x][y] = pixelColor;
                pixelCount++;
            }
        }

        long endTime = System.currentTimeMillis();  // Tempo de término da execução
        long elapsedTime = endTime - startTime;  // Tempo total de execução

        System.out.println("Número total de pixels: " + pixelCount);
        System.out.println("Tempo de execução: " + elapsedTime + " milissegundos");

        return colors;
    }

    private Bitmap criarBitmapComCores(int[][] colors) {
        if (colors == null || colors.length == 0 || colors[0].length == 0) {
            return null;
        }

        int largura = colors.length;
        int altura = colors[0].length;

        // Cria um novo Bitmap com a largura, altura e configuração ARGB_8888
        Bitmap novoBitmap = Bitmap.createBitmap(largura, altura, Bitmap.Config.ARGB_8888);

        // Preenche o novo Bitmap com as cores extraídas
        for (int x = 0; x < largura; x++) {
            for (int y = 0; y < altura; y++) {
                int cor = colors[x][y];
                novoBitmap.setPixel(x, y, cor);
            }
        }

        return novoBitmap;
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        chart = findViewById(R.id.viewGraph);
        consoleDisplay = findViewById(R.id.consoleView);
        countDisplay = findViewById(R.id.txtCount);

        // setup for chart
        scatter = AnyChart.scatter();
        scatter.animation(false);  // no need for being fancy
        scatter.title("Time (ms) vs. Thread count");
        List<DataEntry> data = new ArrayList<>();
        data.add(new ValueDataEntry(0, 0));  // need some element for chart to start
        marker = scatter.marker(data);
        marker.type(MarkerType.CIRCLE).size(1);
        chart.setChart(scatter);
        scatter.removeAllSeries();  // remove the start element for clear chart


        // add listeners for buttons
        // execute button: calls c++ and lets it handle things from there
        final Button btnExecute = findViewById(R.id.btnExecute);
        btnExecute.setOnClickListener(v -> {
            View currentView = findViewById(android.R.id.content);

            Bitmap bitmap = criarBitmapComCruz();
            int[][] a = extractColors(bitmap);
            System.out.println();
            Bitmap bitmap2 = criarBitmapComCores(a);
            /*
            try {
                byte[] a = compressBitmapWriteBinary(bitmap);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
             */
            //executeCPP(mode, bitmap);
            //int[] bitmaps = executeBitmap(bitmap);
            System.out.println();
        });
        // plus button: increase the desired thread count, no limit
        final Button incre = findViewById(R.id.btnPlus);
        incre.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                MainActivity.this.thread_count++;
                MainActivity.this.countDisplay.setText(Integer.toString(MainActivity.this.thread_count));
            }
        });
        // minus button: decrease the desired thread count, n > 0
        final Button decre = findViewById(R.id.btnMinus);
        decre.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (MainActivity.this.thread_count > 1) {
                    MainActivity.this.thread_count--;
                    MainActivity.this.countDisplay.setText(Integer.toString(MainActivity.this.thread_count));
                }
            }
        });
        // cycle between the modes i.e tests for array layouts
        final Button btnMode = findViewById(R.id.btnMode);
        btnMode.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                btnMode.setText(modes[mode=(mode+1)%modes.length]);
            }
        });
        // clear button: clears the chart and consoleDisplay text
        final Button clr = findViewById(R.id.btnClear);
        clr.setOnClickListener(v -> runOnUiThread(() -> {
            MainActivity.this.consoleDisplay.setText("");
            MainActivity.this.scatter.removeAllSeries();
        }));

        scrollView = (ScrollView) findViewById(R.id.scrollView);
    }

    // add data to the consoleDisplay and the chart
    public void displayData(double[] res, int t_cnt) {
        final List<DataEntry> graphData = new ArrayList<>(res.length);
        for(int i = 0; i < res.length; i ++){
            graphData.add(new ValueDataEntry(t_cnt, res[i]));
        }

        final double[] results = res;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MainActivity.this.consoleDisplay.append("\n");
                for (int i = 0; i < results.length; i++) {
                    MainActivity.this.consoleDisplay.append(Integer.toString(i+1) + " [" + Double.toString(results[i]) + "]\n");
                }
                MainActivity.this.scrollView.fullScroll(ScrollView.FOCUS_DOWN);
            }
        });

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MainActivity.this.marker = MainActivity.this.scatter.marker(graphData);
                MainActivity.this.marker.type(MarkerType.CIRCLE).size(1).color(modeCols[mode]);
            }
        });
    }

    public int getThreadCount() {
        return MainActivity.this.thread_count;
    }

    // append single line of text to the consoleDisplay
    public void appendToView(String newText) {
        final String text = "\n" + newText;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MainActivity.this.consoleDisplay.append(text);
            }
        });
    }

    // only outputs data to consoleDisplay and does not add to the chart
    public void dumpToView(double[] res) {
        final double[] results = res;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MainActivity.this.consoleDisplay.append("\n");
                for (int i = 0; i < results.length; i++) {
                    MainActivity.this.consoleDisplay.append(Integer.toString(i+1) + " [" + Double.toString(results[i]) + "]\n");
                }
            }
        });
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native void executeCPP(int mode, Bitmap bitmaps);
    public native int[] executeBitmap(Bitmap bitmaps);
    public native int[][] extractColorsNative(Bitmap bitmaps);
}
