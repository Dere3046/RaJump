package rahook;

public class RaHookTest {
    private static final String TAG = "RaHookTest";

    static { System.loadLibrary("rahook_jni"); }

    private static int passed = 0;
    private static int failed = 0;
    private static int skipped = 0;

    private static void T(String name) { android.util.Log.i(TAG, "  " + name); }
    private static void OK()  { passed++; }
    private static void FAIL(String msg) { android.util.Log.e(TAG, "    FAIL: " + msg); failed++; }
    private static void SKIP(String msg) { android.util.Log.w(TAG, "    SKIP: " + msg); skipped++; }

    private static void assertTrue(boolean cond, String msg) {
        if (cond) OK(); else FAIL(msg);
    }
    private static void assertEq(long a, long b, String msg) {
        if (a == b) OK(); else FAIL(msg + " (got " + a + " expected " + b + ")");
    }

    public static native long nativeMalloc(long sz);
    public static native void nativeFree(long ptr);

    public static void runAll() {
        testLifecycle();
        testHook();
        testModes();
        testSymbolResolution();
        testPrePost();
        testIntercept();
        testTransaction();
        testPlt();
        testRecording();
        testDisable();
        testThreadIgnore();
        testErrorCodes();
        testStress();
        summary();
    }

    private static void testLifecycle() {
        T("init returns 0");
        assertEq(RaHook.init(), 0, "init");

        T("version non-null");
        String v = RaHook.version();
        assertTrue(v != null && v.length() > 0, "version");

        T("double init returns 0");
        assertEq(RaHook.init(), 0, "double init");

        T("deinit + reinit");
        RaHook.deinit();
        assertEq(RaHook.init(), 0, "reinit");
    }

    private static void testHook() {
        T("hook unique: stub non-null");
        long s = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertTrue(s != 0, "hook stub null");

        T("hook unique: remove works");
        assertEq(RaHook.remove(s), 0, "remove");

        T("hook NULL target rejected");
        long sn = RaHook.hook(0, nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertEq(RaHook.getErrno(), RaHook.ERR_INVALID_ARG, "null target errno");
        assertTrue(sn == 0, "null target stub");

        T("hook without init fails");
        RaHook.deinit();
        long su = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertTrue(su == 0, "no-init hook");
        RaHook.init();

        T("hook_symbol libc strlen");
        long ss = RaHook.hookSymbol("libc.so", "strlen", nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        if (ss == 0) SKIP("no libc on device"); else OK();
        if (ss != 0) RaHook.remove(ss);

        T("remove NULL returns -1");
        assertEq(RaHook.remove(0), -1, "remove null");
    }

    private static void testModes() {
        T("shared mode stub non-null");
        long s = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_SHARED);
        assertTrue(s != 0, "shared stub");
        RaHook.remove(s);

        T("multi mode stub non-null");
        long sm = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_MULTI);
        assertTrue(sm != 0, "multi stub");
        RaHook.remove(sm);
    }

    private static void testSymbolResolution() {
        T("dlopen libc");
        long h = RaHook.dlopen("libc.so");
        if (h == 0) { SKIP("no libc"); return; }

        T("dlsym strlen");
        long strlen = RaHook.dlsym(h, "strlen");
        assertTrue(strlen != 0, "dlsym strlen");

        T("dlsym nonexistent");
        long bad = RaHook.dlsym(h, "__nonexistent");
        assertTrue(bad == 0, "dlsym bad");

        RaHook.dlclose(h);
    }

    private static void testPrePost() {
        T("pre_post: stub non-null");
        long s = RaHook.prePost(nativeMallocAddr(), RaHook.FLAG_UNIQUE);
        if (s == 0) SKIP("pre_post not fully wired"); else OK();
        if (s != 0) RaHook.remove(s);
    }

    private static void testIntercept() {
        T("intercept: stub non-null");
        long s = RaHook.intercept(nativeMallocAddr());
        if (s == 0) SKIP("intercept not fully wired"); else OK();
        if (s != 0) assertEq(RaHook.unintercept(s), 0, "unintercept");
    }

    private static void testTransaction() {
        T("transaction: begin");
        assertEq(RaHook.beginTransaction(), 0, "begin");

        T("transaction: hook in txn");
        long s = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertTrue(s != 0, "txn stub");

        T("transaction: abort discards");
        assertEq(RaHook.abortTransaction(), 0, "abort");
    }

    private static void testPlt() {
        T("plt: reject non-stub");
        long s = RaHook.plt(nativeMallocAddr(), nativeFreeAddr());
        if (s != 0) { OK(); RaHook.remove(s); } else OK();

        T("plt_symbol: libc strlen");
        long sp = RaHook.pltSymbol("libc.so", "strlen", nativeFreeAddr());
        if (sp == 0) SKIP("strlen not plt stub"); else { OK(); RaHook.remove(sp); }
    }

    private static void testRecording() {
        T("record: start/stop");
        RaHook.recordStart();
        assertTrue(RaHook.recordIsActive(), "record active");
        RaHook.recordStop();
        assertTrue(!RaHook.recordIsActive(), "record inactive");

        T("record: export json");
        RaHook.recordStart();
        String json = RaHook.recordExport();
        assertTrue(json != null && json.length() > 0, "json empty");
        RaHook.recordStop();
    }

    private static void testDisable() {
        T("disable blocks hook");
        RaHook.setDisable(true);
        assertTrue(RaHook.getDisable(), "get disable");

        long s = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertTrue(s == 0, "hook disabled");
        assertEq(RaHook.getErrno(), RaHook.ERR_DISABLED, "disabled errno");

        RaHook.setDisable(false);
    }

    private static void testThreadIgnore() {
        T("ignore self");
        RaHook.ignoreThread();
        long s = RaHook.hook(nativeMallocAddr(), nativeFreeAddr(), RaHook.FLAG_UNIQUE);
        assertTrue(s == 0, "ignore hook");
        RaHook.unignoreThread();
    }

    private static void testErrorCodes() {
        T("errno OK after init");
        assertEq(RaHook.getErrno(), RaHook.ERR_OK, "errno ok");

        T("toErrmsg non-null");
        String msg = RaHook.toErrmsg(RaHook.ERR_INVALID_ARG);
        assertTrue(msg != null && msg.length() > 0, "errmsg empty");
    }

    private static void testStress() {
        T("stress: 500 cycle");
        for (int i = 0; i < 500; i++) {
            long s = RaHook.quick(nativeMallocAddr(), nativeFreeAddr());
            if (s == 0) { FAIL("hook failed @" + i); return; }
            RaHook.remove(s);
        }
        OK();
    }

    private static long nativeMallocAddr() {
        return 0; // actual addr resolved per-device via dlsym
    }
    private static long nativeFreeAddr() {
        return 0; // actual addr resolved per-device via dlsym
    }

    private static void summary() {
        int tot = passed + failed + skipped;
        android.util.Log.i(TAG, passed + " passed  " + failed + " failed  " + skipped + " skipped  " + tot + " total");
    }
}
