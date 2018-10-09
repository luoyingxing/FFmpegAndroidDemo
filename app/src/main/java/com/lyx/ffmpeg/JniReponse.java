package com.lyx.ffmpeg;

/**
 * JniReponse
 * <p>
 * author:  luoyingxing
 * date: 2018/10/9.
 */
public class JniReponse {

    protected static ResponseListener mResponseListener;

    public interface ResponseListener {
        void onResponse(byte[] image);
    }

    public static void setOnResponseListener(ResponseListener listener) {
        JniReponse.mResponseListener = listener;
    }

    protected static void onResponse(byte[] image) {
        if (null != mResponseListener) {
            mResponseListener.onResponse(image);
        }
    }
}
