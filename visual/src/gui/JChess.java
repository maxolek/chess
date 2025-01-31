package gui;

import javax.swing.*; // Import Swing components
import gui.Table;      // Your Table class that contains the GUI logic

public class JChess {
    public static void main(String[] args) {
        System.out.println("starting jchess");

        // Set the library path for native libraries (like DLLs)
        System.setProperty("java.library.path", "C:/Users/maxol/code/chess/tomahawk");
        System.out.println("property set");

        // Explicitly load the DLL after setting the property (try commenting this out for testing)
        try {
            // System.load("C:/Users/maxol/code/chess/tomahawk/engine.dll");
            // System.out.println("DLL loaded successfully.");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load DLL: " + e.getMessage());
        }

        // Simplified test: directly invoke SwingUtilities.invokeLater without extra complexity
        SwingUtilities.invokeLater(new Runnable() {
            @Override
            public void run() {
                System.out.println("In EDT: creating Table GUI...");
                try {
                    Table table = new Table();  // Create the Table GUI
                    System.out.println("Table GUI created.");
                } catch (Exception e) {
                    System.err.println("Exception in Table constructor: " + e.getMessage());
                    e.printStackTrace();
                }
            }
        });

        System.out.println("Exiting main method.");
    }
}
