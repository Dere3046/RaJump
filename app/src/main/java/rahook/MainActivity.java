package rahook;

import android.app.Activity;
import android.os.Bundle;
import android.widget.*;
import android.graphics.Color;
import android.graphics.Typeface;

public class MainActivity extends Activity {
    static { System.loadLibrary("rahook_jni"); }

    private TextView resultBox;
    private boolean isTestRunning = false;

    @Override
    protected void onCreate(Bundle s) {
        super.onCreate(s);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#1a1a2e"));
        int pd = dp(12);
        root.setPadding(pd, dp(48), pd, pd);

        Button testBtn = new Button(this);
        testBtn.setText("TEST");
        testBtn.setTextSize(22);
        testBtn.setTextColor(Color.parseColor("#1a1a2e"));
        testBtn.setBackgroundColor(Color.parseColor("#00d2ff"));
        testBtn.setPadding(dp(16), dp(16), dp(16), dp(16));
        testBtn.setTypeface(Typeface.DEFAULT_BOLD);
        testBtn.setOnClickListener(v -> runTests());
        root.addView(testBtn, new LinearLayout.LayoutParams(-1, dp(60)));

        resultBox = new TextView(this);
        resultBox.setTextSize(10);
        resultBox.setTextColor(Color.parseColor("#00ff88"));
        resultBox.setPadding(dp(8), dp(8), dp(8), dp(8));
        resultBox.setBackgroundColor(Color.parseColor("#16213e"));
        resultBox.setTypeface(Typeface.MONOSPACE);
        resultBox.setText("RaHook " + RaHook.nativeVersion() + "\ntap TEST");
        setContentView(root);
    }

    private void runTests() {
        if (isTestRunning) return;
        isTestRunning = true;
        resultBox.setText("testing...");
        new Thread(() -> {
            String r = RaHook.nativeRunTests();
            runOnUiThread(() -> {
                resultBox.setText(r);
                isTestRunning = false;
            });
        }).start();
    }

    private int dp(int n) { return (int)(n * getResources().getDisplayMetrics().density); }
}
