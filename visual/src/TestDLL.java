public class TestDLL {
    static {
        // Attempt to load the DLL (native library)
        System.loadLibrary("engine");
    }

    public static void main(String[] args) {
        System.out.println("Library loaded successfully!");
    }
}
