package rahook;

public class RaHook {
    static { System.loadLibrary("rahook_jni"); }

    public static final int FLAG_SHARED = 1;
    public static final int FLAG_MULTI  = 2;
    public static final int FLAG_UNIQUE = 4;

    public static final int ERR_OK            = 0;
    public static final int ERR_PENDING       = 1;
    public static final int ERR_UNINIT        = 2;
    public static final int ERR_INVALID_ARG   = 3;
    public static final int ERR_OOM           = 4;
    public static final int ERR_DISABLED      = 10;
    public static final int ERR_HOOK_FAILED   = 99;

    // lifecycle
    public static native int    init();
    public static native void   deinit();
    public static native String version();

    // error
    public static native int    getErrno();
    public static native String toErrmsg(int err);

    // disable/debug
    public static native void    setDisable(boolean disable);
    public static native boolean getDisable();
    public static native void    setDebug(boolean debug);

    // symbol
    public static native long dlopen(String lib);
    public static native void dlclose(long handle);
    public static native long dlsym(long handle, String sym);

    // hook
    public static native long hook(long target, long replace, int flags);
    public static native long hookSymbol(String lib, String sym, long replace, int flags);
    public static native int  remove(long stub);

    // pre/post
    public static native long prePost(long target, int flags);

    // intercept
    public static native long intercept(long addr);
    public static native int  unintercept(long stub);

    // PLT
    public static native long plt(long target, long replace);
    public static native long pltSymbol(String lib, String sym, long replace);

    // transaction
    public static native int beginTransaction();
    public static native int endTransaction();
    public static native int abortTransaction();

    // recording
    public static native void    recordStart();
    public static native void    recordStop();
    public static native boolean recordIsActive();
    public static native String  recordExport();

    // thread
    public static native void ignoreThread();
    public static native void unignoreThread();

    // patch
    public static native int patch(long addr, byte[] code);

    // convenience
    public static long quick(long target, long replace) {
        return hook(target, replace, FLAG_UNIQUE);
    }
}
