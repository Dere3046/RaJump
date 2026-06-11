package rahook;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle s) {
        super.onCreate(s);
        TextView tv = new TextView(this);
        tv.setText("RaHook Test running in logcat\nFilter: RaHookTest");
        tv.setTextSize(16);
        tv.setPadding(48, 48, 48, 48);
        setContentView(tv);

        new Thread(() -> {
            RaHook.init();
            RaHookTest.runAll();
            RaHook.deinit();
        }).start();
    }
}
