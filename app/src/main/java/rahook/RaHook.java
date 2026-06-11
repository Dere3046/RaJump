package rahook;

public class RaHook {
    public static native int nativeAdd(int a, int b);
    public static native int nativeMul(int a, int b);
    public static native int nativeDouble(int x);
    public static native int nativeTriple(int x);
    public static native String nativeVersion();
    public static native String nativeRunTests();
}
