package rahook;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;
import android.graphics.Color;
import android.graphics.Typeface;

public class MainActivity extends Activity {
    static { System.loadLibrary("rahook_jni"); }

    private TextView display;
    private Button[] hk = new Button[4];
    private boolean[] hkOn = new boolean[4];

    private int curA, curB;
    private char curOp = ' ';
    private boolean hasOp = false;
    private String buf = "0";


    @Override
    protected void onCreate(Bundle s) {
        super.onCreate(s);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#1a1a2e"));
        int pd = dp(12);
        root.setPadding(pd, dp(48), pd, pd);

        root.addView(tv("#00d2ff", 20, "RaHook Tester", true));

        display = new TextView(this);
        display.setTextSize(36);
        display.setTextColor(Color.WHITE);
        display.setGravity(Gravity.END);
        display.setPadding(dp(12), dp(16), dp(12), dp(16));
        display.setBackgroundColor(Color.parseColor("#16213e"));
        display.setTypeface(Typeface.MONOSPACE);
        display.setText("0");
        root.addView(display);

        LinearLayout hrow = new LinearLayout(this);
        hrow.setPadding(0, dp(8), 0, dp(8));
        String[] labels = {"AddH", "MulH", "DblH", "TriH"};
        for (int i = 0; i < 4; i++) {
            final int fi = i;
            hk[i] = new Button(this);
            hk[i].setText(labels[i]);
            hk[i].setTextSize(14);
            hk[i].setTextColor(Color.WHITE);
            hk[i].setBackgroundColor(Color.parseColor("#555555"));
            hk[i].setPadding(dp(4), dp(4), dp(4), dp(4));
            hk[i].setOnClickListener(v -> toggleHook(fi));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(40), 1);
            lp.setMargins(dp(2), 0, dp(2), 0);
            hrow.addView(hk[i], lp);
        }
        root.addView(hrow);

        // control buttons
        LinearLayout crow = new LinearLayout(this);
        crow.setPadding(0, 0, 0, dp(8));

        Button applyAll = new Button(this);
        applyAll.setText("Hook All");
        applyAll.setTextSize(14);
        applyAll.setBackgroundColor(Color.parseColor("#0f3460"));
        applyAll.setTextColor(Color.WHITE);
        applyAll.setOnClickListener(v -> { for (int i = 0; i < 4; i++) if (!hkOn[i]) toggleHook(i); });
        crow.addView(applyAll, new LinearLayout.LayoutParams(0, dp(40), 1));

        Button restoreAll = new Button(this);
        restoreAll.setText("Unhook All");
        restoreAll.setTextSize(14);
        restoreAll.setBackgroundColor(Color.parseColor("#533483"));
        restoreAll.setTextColor(Color.WHITE);
        restoreAll.setOnClickListener(v -> { RaHook.nativeUnhookAll(); for (int i = 0; i < 4; i++) { hkOn[i] = false; setHkColor(i); } });
        crow.addView(restoreAll, new LinearLayout.LayoutParams(0, dp(40), 1));

        root.addView(crow);

        // calculator keypad
        String[][] keys = {
            {"7","8","9","/"},
            {"4","5","6","x"},
            {"1","2","3","-"},
            {"C","0","=","+"}
        };
        int[] colors = {0,0,0,2, 0,0,0,2, 0,0,0,2, 1,0,1,2};
        String[] cc = {"#0f3460","#533483","#16213e"};
        for (int r = 0; r < 4; r++) {
            LinearLayout row = new LinearLayout(this);
            for (int c = 0; c < 4; c++) {
                final String k = keys[r][c];
                Button btn = new Button(this);
                btn.setText(k);
                btn.setTextSize(24);
                btn.setTextColor(Color.WHITE);
                btn.setBackgroundColor(Color.parseColor(cc[colors[r*4+c]]));
                btn.setOnClickListener(v -> keyPress(k));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(60), 1);
                lp.setMargins(dp(2), dp(2), dp(2), dp(2));
                row.addView(btn, lp);
            }
            root.addView(row);
        }

        root.addView(tv("#888888", 12, "RaHook " + RaHook.nativeVersion(), false));

        new Thread(() -> {
            try { Thread.sleep(200); } catch (Exception e) {}
            runOnUiThread(() -> {
                int r = RaHook.nativeAdd(3, 4);
                int m = RaHook.nativeMul(5, 6);
                int d = RaHook.nativeDouble(10);
                int t = RaHook.nativeTriple(10);
                display.setText("add=" + r + " mul=" + m + "\ndbl=" + d + " tri=" + t);
            });
        }).start();

        setContentView(root);
    }

    private int dp(int n) { return (int)(n * getResources().getDisplayMetrics().density); }
    private TextView tv(String c, int sz, String txt, boolean b) {
        TextView t = new TextView(this);
        t.setText(txt);
        t.setTextSize(sz);
        t.setTextColor(Color.parseColor(c));
        if (b) t.setTypeface(Typeface.DEFAULT_BOLD);
        return t;
    }

    private void setHkColor(int i) {
        hk[i].setBackgroundColor(Color.parseColor(hkOn[i] ? "#00d2ff" : "#555555"));
        hk[i].setTextColor(Color.parseColor(hkOn[i] ? "#1a1a2e" : "#ffffff"));
    }

    private int num() { try { return Integer.parseInt(buf); } catch (Exception e) { return 0; } }
    private void clear() { display.setText("0"); buf = "0"; curA = curB = 0; curOp = ' '; hasOp = false; }

    private int calc(char o, int a, int b) {
        switch (o) {
            case '+': return RaHook.nativeAdd(a, b);
            case '-': return a - b;
            case 'x': return RaHook.nativeMul(a, b);
            case '/': return b == 0 ? 0 : a / b;
            default: return 0;
        }
    }

    private void keyPress(String k) {
        if (k.equals("C")) { clear(); return; }
        if (k.equals("=")) {
            if (hasOp) { curB = num(); int r = calc(curOp, curA, curB);
                display.setText(curA + " " + curOp + " " + curB + " = " + r);
            }
            return;
        }
        String ops = "+-x/";
        if (ops.contains(k)) {
            if (hasOp) { curB = num(); curA = calc(curOp, curA, curB); } else curA = num();
            curOp = k.charAt(0); buf = "0"; hasOp = true;
            display.setText(curA + " " + curOp + " ");
            return;
        }
        if (buf.equals("0")) buf = k; else buf += k;
        display.setText(buf);
    }

    private void toggleHook(int i) {
        try {
            if (hkOn[i]) {
                if (i == 0) RaHook.nativeUnhookAdd();
                else if (i == 1) RaHook.nativeUnhookMul();
                else if (i == 2) RaHook.nativeUnhookDouble();
                else RaHook.nativeUnhookDouble();
            } else {
                int r = 0;
                if (i == 0) r = RaHook.nativeHookAdd();
                else if (i == 1) r = RaHook.nativeHookMul();
                else if (i == 2) r = RaHook.nativeHookDouble();
                if (r == 0) { Toast.makeText(this, "hook fail", Toast.LENGTH_SHORT).show(); return; }
            }
            hkOn[i] = !hkOn[i];
            setHkColor(i);
        } catch (Exception e) {
            Toast.makeText(this, "error", Toast.LENGTH_SHORT).show();
        }
    }
}
