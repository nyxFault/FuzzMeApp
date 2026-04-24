package com.example.fuzzmeapp;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import com.google.android.material.button.MaterialButton;

public class MainActivity extends AppCompatActivity {
    private static final int LISTEN_PORT = 4444;
    private static final long UI_POLL_INTERVAL_MS = 120L;
    private static final String STATUS_STOPPED = "Server stopped";
    private static final String STATUS_OFFLINE = "Server offline";

    private TextView statusText;
    private TextView receivedText;
    private final Handler uiHandler = new Handler(Looper.getMainLooper());

    static {
        System.loadLibrary("fuzzme");
    }

    private native String startNativeServer(int port);
    private native void stopNativeServer();
    private native boolean isServerRunning();
    private native String getLastReceived();
    private native String consumeConnectionEvent();

    private final Runnable uiPollTask = new Runnable() {
        @Override
        public void run() {
            updateReceivedPreview();
            flushConnectionToasts();
            uiHandler.postDelayed(this, UI_POLL_INTERVAL_MS);
        }
    };

    private void updateReceivedPreview() {
        String payload = getLastReceived();
        if (payload != null && !payload.isEmpty()) {
            receivedText.setText(payload);
        }
    }

    private void flushConnectionToasts() {
        String event;
        while ((event = consumeConnectionEvent()) != null && !event.isEmpty()) {
            Toast.makeText(this, event, Toast.LENGTH_SHORT).show();
        }
    }

    private void applyWindowInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void hookButtons(MaterialButton startButton, MaterialButton stopButton) {
        startButton.setOnClickListener(v -> statusText.setText(startNativeServer(LISTEN_PORT)));
        stopButton.setOnClickListener(v -> {
            stopNativeServer();
            statusText.setText(STATUS_OFFLINE);
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        statusText = findViewById(R.id.statusText);
        receivedText = findViewById(R.id.receivedText);
        MaterialButton startButton = findViewById(R.id.startButton);
        MaterialButton stopButton = findViewById(R.id.stopButton);

        applyWindowInsets();
        hookButtons(startButton, stopButton);
        statusText.setText(STATUS_STOPPED);
        uiHandler.post(uiPollTask);
    }

    @Override
    protected void onDestroy() {
        uiHandler.removeCallbacks(uiPollTask);
        if (isServerRunning()) {
            stopNativeServer();
        }
        super.onDestroy();
    }
}