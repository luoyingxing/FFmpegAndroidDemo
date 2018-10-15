package com.lyx.ffmpeg;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;
import java.util.List;
import java.util.Map;

public class MainActivity extends AppCompatActivity implements JniReponse.ResponseListener {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("avcodec-57");
        System.loadLibrary("avdevice-57");
        System.loadLibrary("avfilter-6");
        System.loadLibrary("avformat-57");
        System.loadLibrary("avutil-55");
        System.loadLibrary("postproc-54");
        System.loadLibrary("swresample-2");
        System.loadLibrary("swscale-4");

    }

    private ImageView imageView;
    private SurfaceView surface;
    private EditText ip;
    private EditText port;
    private TextView tipTV;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        imageView = findViewById(R.id.image);
        surface = findViewById(R.id.surface);
        ip = findViewById(R.id.ip);
        port = findViewById(R.id.port);
        tipTV = findViewById(R.id.tip);

        tv.setText(stringFromJNI());
        tv.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
//                        play();
                        parserStream();
                    }
                }).start();
            }
        });

        JniReponse.setOnResponseListener(this);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native void play();

    public native void parserStream(byte[] data, int length, Surface surface);

    @Override
    public void onResponse(byte[] image) {
        Log.i("MainActivity", "length: " + image.length);

        Bitmap bitmap = BitmapFactory.decodeByteArray(image, 0, image.length);

        Log.e("MainActivity", "bitmap: " + bitmap);

        imageView.setImageBitmap(bitmap);
    }

    @SuppressLint("HandlerLeak")
    private Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            tipTV.setText("接收到的数据：" + msg.what);
        }
    };

    private void parserStream() {
//        InputStream inputStream = getResources().openRawResource(R.raw.cuc_ieschool);
//        byte[] buffer = new byte[1024 * 8];
//
//        int len;
//        try {
//            while ((len = inputStream.read(buffer)) != -1) {
//                parserStream(buffer, len, surface.getHolder().getSurface());
//            }
//        } catch (Exception e) {
//            e.printStackTrace();
//        }
        String url = "http://" + ip.getText().toString().trim() + ":" + port.getText().toString().trim() + "/api/video/h264";
        try {
            HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(20 * 1000);
            conn.setReadTimeout(20 * 1000);
            conn.setRequestProperty("Connection", "Keep-Alive");
            conn.setRequestProperty("User-Agent", "CONWIN");
            conn.setRequestMethod("GET");
            conn.connect();

            InputStream inputStream = conn.getInputStream();
            Map<String, List<String>> map = conn.getHeaderFields();
            Log.w("parserStream", map.toString());

            if (HttpURLConnection.HTTP_OK == conn.getResponseCode()) {

                while (true) {
                    int len = inputStream.available();

                    if (len != -1) {
                        byte[] read = new byte[len];

                        int readLen = inputStream.read(read);
                        handler.sendEmptyMessage(readLen);
                        parserStream(read, readLen, surface.getHolder().getSurface());

                    }
                }
            }

            inputStream.close();
            conn.disconnect();
        } catch (ProtocolException e) {
            e.printStackTrace();
        } catch (MalformedURLException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
