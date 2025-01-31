package gui;

import pieces.*;
import board.*;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import javax.imageio.*;
import java.io.*;
import java.util.*;
import java.util.List;

public class Table {
    private final JFrame gameFrame;
    private final BoardPanel boardPanel;
    private final Board chessBoard;
    private final int TILE_PANEL_DIM_WIDTH, TILE_PANEL_DIM_HEIGHT;

    public Table() {
        System.out.println("Table constructor started.");

        // Basic JFrame setup
        this.chessBoard = new Board();
        this.gameFrame = new JFrame("Chess Game");
        this.gameFrame.setSize(600, 600);
        TILE_PANEL_DIM_WIDTH = this.gameFrame.getWidth() / 8;
        TILE_PANEL_DIM_HEIGHT = this.gameFrame.getHeight() / 8;

        this.gameFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        // Add BoardPanel to the frame
        this.boardPanel = new BoardPanel();
        this.gameFrame.add(this.boardPanel, BorderLayout.CENTER);

        // Add MenuBar
        final JMenuBar tableMenuBar = createTableMenuBar();
        this.gameFrame.setJMenuBar(tableMenuBar);

        this.gameFrame.setVisible(true);
        System.out.println("Table frame created.");
    }

    private JMenuBar createTableMenuBar() {
        final JMenuBar tableMenuBar = new JMenuBar();
        tableMenuBar.add(createFileMenu());
        return tableMenuBar;
    }

    private JMenu createFileMenu() {
        final JMenu fileMenu = new JMenu("File");
        final JMenuItem openPGN = new JMenuItem("Load PGN File");
        openPGN.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                System.out.println("open up that pgn file");
            }  
        });

        final JMenuItem exitMenuItem = new JMenuItem("Exit");
        exitMenuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                System.exit(0);
            }
        });

        fileMenu.add(openPGN);
        fileMenu.add(exitMenuItem);
        return fileMenu;
    }

    private class BoardPanel extends JPanel {
        BoardPanel() {
            super(new GridLayout(8, 8));  // 8x8 grid for the chessboard
            System.out.println("BoardPanel created.");

            // Add TilePanel instances to the grid
            for (int i = 0; i < 64; i++) {
                TilePanel tilePanel = new TilePanel(i); // Passing tile ID
                add(tilePanel);
            }
        }
    }

    private class TilePanel extends JPanel {
        private final int tileID;

        TilePanel(final int tile_id) {
            super(new GridBagLayout());
            this.tileID = tile_id;
            setPreferredSize(new Dimension(50, 50));  // Setting tile size to 50x50
            assignTileColor();
            assignTilePieceIcon();  // Load image for this tile asynchronously
        }

        private void assignTilePieceIcon() {
            this.removeAll();  // Clear the previous icon if any
        
            if (chessBoard.getTile(this.tileID).isTileOccupied()) {
                try {
                    // Construct the path to the PNG file for the piece icon
                    String pieceIconFilePath = "C:\\Users\\maxol\\code\\chess\\visual\\piece_icons\\" +
                        chessBoard.getTile(this.tileID).getPiece().lowerCharAlliance() +
                        chessBoard.getTile(this.tileID).getPiece().upperCharType() + ".png";
        
                    File pngFile = new File(pieceIconFilePath);
                    if (pngFile.exists()) {
                        // Load the PNG image using ImageIO
                        BufferedImage image = ImageIO.read(pngFile);
        
                        // Resize the image to fit the tile size
                        Image scaledImage = image.getScaledInstance(TILE_PANEL_DIM_WIDTH, TILE_PANEL_DIM_HEIGHT, Image.SCALE_SMOOTH);
        
                        // Use ImageIcon to display the resized image in the JLabel
                        add(new JLabel(new ImageIcon(scaledImage)));
                    } else {
                        System.out.println("PNG file not found: " + pieceIconFilePath);
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        

        private void assignTileColor() {
            Color lightSquareColor = new Color(0xE4D1A3); // Light brown
            Color darkSquareColor = new Color(0xA66E39);  // Dark brown

            boolean isLightTile = (tileID + tileID / 8) % 2 == 0;  // Chessboard tile color logic
            setBackground(isLightTile ? lightSquareColor : darkSquareColor);
        }
    }
}



/*
public class Table {
    private final JFrame gameFrame;
    private final BoardPanel boardPanel;
    private final Board chessBoard = new Board();
    String pieceIconPath = "C:\\Users\\maxol\\code\\chess\\visual\\piece_icons\\"; // Ensure this path ends with a backslash.

    private final static Dimension OUTER_FRAME_DIMENSION = new Dimension(600,600);
    private final static Dimension BOARD_PANEL_DIM = new Dimension(400, 350);
    private final static Dimension TILE_PANEL_DIM = new Dimension(10,10);

    public Table() {
        System.out.println("table started");
        this.gameFrame = new JFrame();
        this.gameFrame.setLayout(new BorderLayout());
        final JMenuBar tableMenuBar = createTableMenuBar();
        this.gameFrame.setJMenuBar(tableMenuBar);
        this.gameFrame.setSize(OUTER_FRAME_DIMENSION);

        this.boardPanel = new BoardPanel();
        this.gameFrame.add(this.boardPanel, BorderLayout.CENTER);

        this.gameFrame.setVisible(true);

        // multi-monitor support
        GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
        GraphicsDevice[] devices = ge.getScreenDevices();
        if (devices.length > 1) {
            gameFrame.setLocation(devices[1].getDefaultConfiguration().getBounds().x + 400,
                    devices[1].getDefaultConfiguration().getBounds().y);
        } else {
            gameFrame.setLocationRelativeTo(null);  // center
        }

        this.gameFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
    }

    private JMenuBar createTableMenuBar() {
        final JMenuBar tableMenuBar = new JMenuBar();
        tableMenuBar.add(createFileMenu());
        return tableMenuBar;
    }

    private JMenu createFileMenu() {
        final JMenu fileMenu = new JMenu("File");
        final JMenuItem openPGN = new JMenuItem("Load PGN File");
        openPGN.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                System.out.println("open up that pgn file");
            }  
        });

        final JMenuItem exitMenuItem = new JMenuItem("Exit");
        exitMenuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                System.exit(0);
            }
        });

        fileMenu.add(openPGN);
        fileMenu.add(exitMenuItem);
        return fileMenu;
    }

    private class BoardPanel extends JPanel {
        final List<TilePanel> boardTiles;

        BoardPanel() {
            super(new GridLayout(BoardUtils.BOARD_DIM, BoardUtils.BOARD_DIM));
            this.boardTiles = new ArrayList<>();

            for (int i = 0; i < BoardUtils.NUM_TILES; i++) {
                final TilePanel tilePanel = new TilePanel(this, i);
                this.boardTiles.add(tilePanel);
                add(tilePanel);
            }

            setPreferredSize(BOARD_PANEL_DIM);
            // validate() removed, Swing automatically manages layout
        }
    }

    private class TilePanel extends JPanel {
        private final int tileID;

        TilePanel(final BoardPanel boardPanel, final int tile_id) {
            super(new GridBagLayout());
            this.tileID = tile_id;
            setPreferredSize(TILE_PANEL_DIM);
            assignTileColor();
            assignTilePieceIcon(chessBoard);  // Move image loading to SwingWorker
        }

        private void assignTilePieceIcon(final Board board) {
            this.removeAll();  // Clear the previous icon if any

            if (board.getTile(this.tileID).isTileOccupied()) {
                SwingWorker<Void, Void> worker = new SwingWorker<Void, Void>() {
                    @Override
                    protected Void doInBackground() {
                        try {
                            // Construct the path to the PNG file for the piece icon
                            String pieceIconFilePath = pieceIconPath +
                                    board.getTile(tileID).getPiece().lowerCharAlliance() +
                                    board.getTile(tileID).getPiece().upperCharType() + ".png";

                            File pngFile = new File(pieceIconFilePath);
                            if (pngFile.exists()) {
                                // Load the PNG image using ImageIO
                                BufferedImage image = ImageIO.read(pngFile);
                                // Update the UI after loading the image
                                SwingUtilities.invokeLater(() -> add(new JLabel(new ImageIcon(image))));
                            } else {
                                System.out.println("PNG file not found: " + pieceIconFilePath);
                            }
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                        return null;
                    }

                    @Override
                    protected void done() {
                        // This will ensure the layout is updated after the image loading
                        revalidate();
                        repaint();
                    }
                };

                worker.execute();  // Start the worker to load the image asynchronously
            }
        }

        private void assignTileColor() {
            Color lightSquareColor = new Color(0xE4D1A3); // Light brown
            Color darkSquareColor = new Color(0xA66E39);  // Dark brown

            boolean isLightTile = (tileID + tileID / BoardUtils.BOARD_DIM) % 2 == 0;
            setBackground(isLightTile ? lightSquareColor : darkSquareColor);
        }
    }
}
*/