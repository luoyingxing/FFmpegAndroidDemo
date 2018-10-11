package com.lyx.ffmpeg;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.InputStream;

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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        imageView = findViewById(R.id.image);
        surface = findViewById(R.id.surface);

        tv.setText(stringFromJNI());
        tv.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                play();
                parserStream();
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

    private void parserStream() {
        InputStream inputStream = getResources().openRawResource(R.raw.cuc_ieschool);
        byte[] buffer = new byte[1024 * 8];

        int len;
        try {
            while ((len = inputStream.read(buffer)) != -1) {
                parserStream(buffer, len, surface.getHolder().getSurface());
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
