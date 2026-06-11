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
    private TextView resultBox;

    private int curA = 0, curB = 0;
    private char curOp = ' ';
    private boolean hasOp = false, hasEq = false;
    private String buf = "0";
    private boolean isTestRunning = false;

    @Override
    protected void onCreate(Bundle s) {
        super.onCreate(s);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#1a1a2e"));
        int pd = dp(12);
        root.setPadding(pd, dp(48), pd, pd);

        display = new TextView(this);
        display.setTextSize(36);
        display.setTextColor(Color.WHITE);
        display.setGravity(Gravity.END);
        display.setPadding(dp(12), dp(16), dp(12), dp(16));
        display.setBackgroundColor(Color.parseColor("#16213e"));
        display.setTypeface(Typeface.MONOSPACE);
        display.setText("0");
        root.addView(display);

        Button runBtn = new Button(this);
        runBtn.setText("Run Tests");
        runBtn.setTextSize(16);
        runBtn.setTextColor(Color.WHITE);
        runBtn.setBackgroundColor(Color.parseColor("#00d2ff"));
        runBtn.setPadding(dp(8), dp(8), dp(8), dp(8));
        runBtn.setOnClickListener(v -> runTests());
        root.addView(runBtn, new LinearLayout.LayoutParams(-1, dp(44)));

        resultBox = new TextView(this);
        resultBox.setTextSize(10);
        resultBox.setTextColor(Color.parseColor("#00ff88"));
        resultBox.setPadding(dp(8), dp(8), dp(8), dp(8));
        resultBox.setBackgroundColor(Color.parseColor("#16213e"));
        resultBox.setTypeface(Typeface.MONOSPACE);
        resultBox.setMinLines(3);
        resultBox.setText("tap Run Tests");
        root.addView(resultBox, new LinearLayout.LayoutParams(-1, dp(120)));

        String[][] keys = {
            {"7","8","9","\u00f7"},
            {"4","5","6","\u00d7"},
            {"1","2","3","-"},
            {"C","0","\u25a1","+"}
        };
        int[] colorForKey = {0,0,0,2, 0,0,0,2, 0,0,0,2, 1,0,1,2};
        String[] cc = {"#0f3460","#533483","#16213e"};
        for (int r = 0; r < 4; r++) {
            LinearLayout row = new LinearLayout(this);
            for (int c = 0; c < 4; c++) {
                final String k = keys[r][c];
                Button btn = new Button(this);
                btn.setText(k.equals("\u25a1") ? "=" : k);
                btn.setTextSize(24);
                btn.setTextColor(Color.WHITE);
                btn.setBackgroundColor(Color.parseColor(cc[colorForKey[r*4+c]]));
                btn.setOnClickListener(v -> keyPress(k));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(60), 1);
                lp.setMargins(dp(2), dp(2), dp(2), dp(2));
                row.addView(btn, lp);
            }
            root.addView(row);
        }

        root.addView(tv("#555555", 11, "RaHook " + RaHook.nativeVersion(), false));
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
    private TextView tv(String c, int sz, String txt, boolean b) {
        TextView t = new TextView(this); t.setText(txt); t.setTextSize(sz);
        t.setTextColor(Color.parseColor(c)); if(b) t.setTypeface(Typeface.DEFAULT_BOLD); return t;
    }

    private int num() { try { return Integer.parseInt(buf); } catch(Exception e) { return 0; } }
    private void clear() { display.setText("0"); buf="0"; curA=curB=0; curOp=' '; hasOp=hasEq=false; }
    private int calc(char o, int a, int b) {
        switch(o) {
            case '+': return RaHook.nativeAdd(a,b);
            case '-': return a - b;
            case '\u00d7': return RaHook.nativeMul(a,b);
            case '\u00f7': return b==0?0:a / b;
            default: return 0;
        }
    }
    private void keyPress(String k) {
        if(k.equals("C")){clear();return;}
        String ops="+-\u00d7\u00f7";
        if(k.equals("\u25a1")) { if(hasOp){curB=num();int r=calc(curOp,curA,curB);display.setText(curA+" "+curOp+" "+curB+" = "+r);curA=r;buf=""+r;hasEq=true;} return; }
        if(ops.contains(k)) {
            if(hasOp){curB=num();int r=calc(curOp,curA,curB);display.setText(curA+" "+curOp+" "+curB+" = "+r);curA=r;}
            else curA=num();
            curOp=k.charAt(0);buf="0";hasOp=true;hasEq=false;
            display.setText(curA+" "+curOp+" "); return;
        }
        if(hasEq)clear();
        if(buf.equals("0")&&!k.equals("←"))buf=k;else buf+=k;
        display.setText(buf);
    }
}
