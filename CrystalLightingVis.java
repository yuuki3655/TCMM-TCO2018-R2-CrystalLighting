import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.awt.geom.*;
import java.io.*;
import java.security.*;
import java.util.*;
import java.util.List;
import javax.imageio.*;
import javax.swing.*;

class Item {
    public int r, c;
    public char type;
    public boolean valid;

    public Item(int R, int C, char Type) {
        r = R;
        c = C;
        type = Type;
    }

    public boolean isLantern() {
        return type == '1' || type == '2' || type == '4';
    }

    public boolean isMirror() {
        return type == '\\' || type == '/';
    }

    public boolean isObstacle() {
        return type == 'X';
    }

    public int getColor() {
        if (isLantern()) return type - '0';
        return 0;
    }
}

class Ray {
    public int r0, c0, r1, c1, dr, dc;
    Item source;

    public Ray(int r0, int c0, int r1, int c1, int dr, int dc, Item source) {
        this.r0 = r0;
        this.c0 = c0;
        this.r1 = r1;
        this.c1 = c1;
        this.dr = dr;
        this.dc = dc;
        this.source = source;
    }
}

public class CrystalLightingVis {
    static char[] types = new char[] {'1','2','4','\\','/','X'};
    static int minS = 10, maxS = 100;
    static int minLanternCost = 1, maxLanternCost = 10;
    static int minMirrorCost = 3, maxMirrorCost = 30;
    static int minObstacleCost = 2, maxObstacleCost = 20;

    static final int invalidScore = -1000000;

    int H, W;
    int costLantern, costMirror, costObstacle;
    int maxMirrors, maxObstacles;
    int numCrystals, numCorrectPrimary, numCorrectSecondary, numIncorrect;
    volatile char[][] targetBoard;  // valid crystal colors are 1..6 (RYB color model), X is an obstacle
    volatile List<Item> addedItems;
    volatile List<Ray> rays;
    volatile char[][] resultBoard;  // marks the results of placing the lanterns and lighting the crystals up
    // -----------------------------------------
    boolean isInside(int r, int c) {
        return (r >= 0 && r < H && c >= 0 && c < W);
    }
    // -----------------------------------------
    int getTypeIdx(char type) {
        for (int i = 0; i < types.length; i++) {
            if (types[i] == type) return i;
        }
        return -1;
    }
    // -----------------------------------------
    String generate(String seedStr) {
      try {
        // generate test case
        SecureRandom r1 = SecureRandom.getInstance("SHA1PRNG"); 
        long seed = Long.parseLong(seedStr);
        r1.setSeed(seed);
        H = r1.nextInt(maxS - minS + 1) + minS;
        W = r1.nextInt(maxS - minS + 1) + minS;
        if (seed == 1)
            W = H = minS;
        else if (seed == 2)
            W = H = (minS + maxS) / 2;
        else if (seed == 3)
            W = H = maxS;

        // generate the board
        int pObstacle = r1.nextInt(11) + 5;
        int pCrystal = r1.nextInt(11) + 15;

        targetBoard = new char[H][W];
        numCrystals = 0;
        for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            int t = r1.nextInt(100);
            targetBoard[r][c] = (t < pCrystal ? (char)(r1.nextInt(6)+1+'0') : (t < pCrystal + pObstacle ? 'X' : '.'));
            if (t < pCrystal) numCrystals++;
        }

        // generate costs
        costLantern = r1.nextInt(maxLanternCost - minLanternCost + 1) + minLanternCost;
        costMirror = r1.nextInt(maxMirrorCost - minMirrorCost + 1) + minMirrorCost;
        costObstacle = r1.nextInt(maxObstacleCost - minObstacleCost + 1) + minObstacleCost;

        // generate limits for mirrors and obstacles
        maxMirrors = r1.nextInt(numCrystals/8 + 1);
        maxObstacles = r1.nextInt(numCrystals/16 + 1);
        if (seed == 1) 
            maxMirrors = maxObstacles = 3;

        StringBuffer sb = new StringBuffer();
        sb.append("H = ").append(H).append('\n');
        sb.append("W = ").append(W).append('\n');
        sb.append("Probability of a crystal = ").append(pCrystal).append('\n');
        sb.append("Probability of an obstacle = ").append(pObstacle).append('\n');
        sb.append("Lantern cost = ").append(costLantern).append('\n');
        sb.append("Mirror cost = ").append(costMirror).append('\n');
        sb.append("Obstacle cost = ").append(costObstacle).append('\n');
        sb.append("Max mirrors = ").append(maxMirrors).append('\n');
        sb.append("Max obstacles = ").append(maxObstacles).append('\n');
        for (int i = 0; i < H; ++i) {
            sb.append(new String(targetBoard[i])).append('\n');
        }
        return sb.toString();
      }
      catch (Exception e) {
        addFatalError("An exception occurred while generating test case.");
        e.printStackTrace(); 
        return "";
      }
    }
    // -----------------------------------------
    void calculateLighting() {
        // recalculate all lighting from scratch
        // mark obstacles and empty places (crystals start empty)
        for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            if (targetBoard[r][c] == '.' || targetBoard[r][c] == 'X')
                resultBoard[r][c] = targetBoard[r][c];
            else
                resultBoard[r][c] = '0';
        }
        //Place all added items
        rays.clear();
        for (int i = 0; i < addedItems.size(); ++i) {
            Item item = addedItems.get(i);
            resultBoard[item.r][item.c] = item.type;
            if (item.isLantern()) {
                //Mark initially as valid
                item.valid = true;
                //Add rays in all 4 direction, if inside the board
                for (int dir = 0; dir < 4; dir++) {
                    int dr = dir == 0 ? 1 : dir == 1 ? -1 : 0;
                    int r1 = item.r + dr;
                    int dc = dir == 2 ? 1 : dir == 3 ? -1 : 0;
                    int c1 = item.c + dc;
                    rays.add(new Ray(item.r, item.c, r1, c1, dr, dc, item));
                }
            }
        }
        //Expand rays, checking for obstacles, crystals or other added items
        //Here is checked if each lantern is valid (i.e. do not light another lantern)
        int currRay = 0;
        while(currRay < rays.size()) {
            Ray ray = rays.get(currRay++);
            if (!isInside(ray.r1,ray.c1)) continue;
            char res = resultBoard[ray.r1][ray.c1];
            char tgt = targetBoard[ray.r1][ray.c1];
            int dr = ray.dr;
            int dc = ray.dc;
            if (res == 'X') {
                //Obstacle -> stop expanding
                continue;
            } else if (res == '/') {
                //Mirror -> update new ray direction
                dr = -ray.dc;
                dc = -ray.dr;
            } else if (res == '\\') {
                //Mirror -> update new ray direction
                dr = ray.dc;
                dc = ray.dr;
            } else if (tgt != '.') {
                //Crystal -> merge with the ray color and stop expanding.
                resultBoard[ray.r1][ray.c1] = (char)(((res - '0') | (ray.source.type - '0')) + '0');
                continue;
            } else if (res != '.' && tgt == '.') {
                //Another Lantern -> Invalid
                ray.source.valid = false;
                continue;
            }
            rays.add(new Ray(ray.r1, ray.c1, ray.r1 + dr, ray.c1 + dc, dr, dc, ray.source));
        }
    }
    // -----------------------------------------
    String removeOneItem(int R, int C) {
        if (!isInside(R, C)) {
            return "You can only remove items from within the board.";
        }
        for (int i = 0; i < addedItems.size(); ++i) {
            Item item = addedItems.get(i);
            if (item.r == R && item.c == C) {
                addedItems.remove(i);
                resultBoard[item.r][item.c] = '.';
                return "";
            }
        }
        return "You can only remove items already placed on the board.";
    }
    // -----------------------------------------
    String placeOneItem(int R, int C, char type) {
        // the logic of lantern placing
        if (!isInside(R, C)) {
            return "You can only place items within the board.";
        }
        if (getTypeIdx(type) < 0) {
            return "Invalid item type: " + type + ". You can only place lanterns of primary colors (1, 2 or 4), mirrors (\\ or /) or obstacles (X).";
        }
        if (targetBoard[R][C] != '.') {
            return "You can only place items on empty cells of the board.";
        }
        if (resultBoard[R][C] != '.') {
            return "You can not place two items on the same cell.";
        }
        resultBoard[R][C] = type;
        if (type == 'X') {
            int usedObstacles = 0;
            for(Item item : addedItems) 
                if (item.isObstacle()) usedObstacles++;
            if (usedObstacles >= maxObstacles) 
                return "You can place at most " + maxObstacles + " obstacles.";
        } else if (type == '/' || type == '\\') {
            int usedMirrors = 0;
            for(Item item : addedItems) 
                if (item.isMirror()) usedMirrors++;
            if (usedMirrors >= maxMirrors) 
                return "You can place at most " + maxMirrors + " mirrors.";
        }
        addedItems.add(new Item(R, C, type));
        return "";
    }
    // -----------------------------------------
    double getScore() {
        // Check first for invalid lanterns
        for(Item item : addedItems) 
            if (item.isLantern() && !item.valid) 
                return invalidScore;

        // Score is: 20 for each correct primary color crystal, 30 for each correct secondary color crystal,
        // 0 for each ignored crystal, -10 for each crystal of incorrect color
        // Minus the total cost for lanterns, mirrors and obstacles.
        numCorrectPrimary = numCorrectSecondary = numIncorrect = 0;
        double score = 0.0;
        for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            char tgt = targetBoard[r][c];
            char res = resultBoard[r][c]; 
            if (tgt == 'X')
                continue;
            if (tgt == '.') {
                if (res == '/' || res == '\\')
                    score -= costMirror;
                else if (res == 'X')
                    score -= costObstacle;
                else if (res == '1' || res == '2' || res == '4')
                    score -= costLantern;
                continue;
            }
            if (tgt == res) {
                if (tgt == '1' || tgt == '2' || tgt == '4') {
                    score += 20;
                    numCorrectPrimary++;
                } else {
                    score += 30;
                    numCorrectSecondary++;
                }
            } else {
                if (res != '0') {
                    score -= 10;
                    numIncorrect++;
                }
            }
        }
        return score;
    }
    // -----------------------------------------
    public double runTest(String seed) {
      try {
        String test = generate(seed);
        if (debug)
            System.out.println(test);

        addedItems = new ArrayList<Item>();
        rays = new ArrayList<Ray>();
        resultBoard = new char[H][W];
        calculateLighting();
        if (vis) {
            jf.setVisible(true);
            Insets frameInsets = jf.getInsets();
            int fw = frameInsets.left + frameInsets.right + 8;
            int fh = frameInsets.top + frameInsets.bottom + 8;
            Toolkit toolkit = Toolkit.getDefaultToolkit();
            Dimension screenSize = toolkit.getScreenSize();
            Insets screenInsets = toolkit.getScreenInsets(jf.getGraphicsConfiguration());
            screenSize.width -= screenInsets.left + screenInsets.right;
            screenSize.height -= screenInsets.top + screenInsets.bottom;
            if (SZ == 0) {
                SZ = Math.min((screenSize.width - fw - 120) / W, (screenSize.height - fh) / H);
                if (!plain && SZ < 20) plain = true;
            }
            Dimension dim = v.getVisDimension();
            v.setPreferredSize(dim);
            jf.setSize(Math.min(dim.width + fw, screenSize.width), Math.min(dim.height + fh, screenSize.height));
            manualReady = false;
            draw();
        }

        if (proc != null) {
            // call the solution
            String[] targetBoardArg = new String[H];
            for (int i = 0; i < H; ++i) {
                targetBoardArg[i] = new String(targetBoard[i]);
            }

            String[] itemsRet;
            try {
                itemsRet = placeItems(targetBoardArg, costLantern, costMirror, costObstacle, maxMirrors, maxObstacles);
            } catch (Exception e) {
                addFatalError("Failed to get result from placeItems.");
                return invalidScore;
            }

            // check the return and score it
            if (itemsRet == null) {
                addFatalError("Your return contained invalid number of elements.");
                return invalidScore;
            }
            if (itemsRet.length > W*H) {
                addFatalError("Your return contained more than " + (W*H) + " elements.");
                return invalidScore;
            }

            // parse each item placed and place it on the result board
            // pure manual mode just returns 0 items to process, because they will be placed by the player
            for (int i = 0; i < itemsRet.length; ++i) {
                // items placement format is "ROW COL TYPE"
                // check return formatting
                String[] s = itemsRet[i].split(" ");
                if (s.length != 3) {
                    addFatalError("Item " + i + ": Each element of your return must be formatted as \"ROW COL TYPE\"");
                    return invalidScore;
                }
                int R, C;
                try {
                    R = Integer.parseInt(s[0]);
                    C = Integer.parseInt(s[1]);
                }
                catch (Exception e) {
                    addFatalError("Item " + i + ": R and C in each element of your return must be integers.");
                    return invalidScore;
                }
                if (s[2].length() != 1) {
                    addFatalError("Item " + i + ": Invalid item type: " + s[2] + ". Item type must be a single character.");
                    return invalidScore;
                }
                char type = s[2].charAt(0);

                // try to place the item
                String errmes = placeOneItem(R, C, type);
                if (!errmes.equals("")) {
                    addFatalError("Item " + i + ": " + errmes);
                    return invalidScore;
                }
            }

            calculateLighting();

            if (vis)
                draw();

            // Check for invalid lanterns. This is done after visualization (if enabled). 
            for (int i = 0; i < itemsRet.length; ++i) { 
                Item item = addedItems.get(i); 
                if (item.isLantern() && !item.valid) { 
                    addFatalError("Item " + i + ": A lantern should not be illuminated by any light ray.");
                    return invalidScore;
                }
            }
        }

        if (manual) {
            // let player know that we're waiting for their input
            addFatalError("Manual play on");

            // wait till player finishes (possibly on top of automated return)
            while (!manualReady)
            {   try { Thread.sleep(50);}
                catch (Exception e) { e.printStackTrace(); } 
            }
            // don't convert manually placed lanterns to return value
        }

        return getScore();
      }
      catch (Exception e) { 
        addFatalError("An exception occurred while trying to get your program's results.");
        e.printStackTrace(); 
        return invalidScore;
      }
    }
// ------------- visualization part ------------
    static String seed;
    JFrame jf;
    Vis v;
    static String exec;
    static boolean vis, manual, debug, plain, mark, showRays, save;
    static Process proc;
    InputStream is;
    OutputStream os;
    BufferedReader br;
    static int SZ;
    volatile boolean manualReady;
    // -----------------------------------------
    String[] placeItems(String[] targetBoard, int costLantern, int costMirror, int costObstacle, int maxMirrors, int maxObstacles) throws IOException {
        StringBuffer sb = new StringBuffer();
        sb.append(H).append("\n");
        for (int i = 0; i < H; ++i) {
            sb.append(targetBoard[i]).append("\n");
        }
        sb.append(costLantern).append("\n");
        sb.append(costMirror).append("\n");
        sb.append(costObstacle).append("\n");
        sb.append(maxMirrors).append("\n");
        sb.append(maxObstacles).append("\n");
        os.write(sb.toString().getBytes());
        os.flush();

        // and get the return value
        int N = Integer.parseInt(br.readLine());
        String[] ret = new String[N];
        for (int i = 0; i < N; i++)
            ret[i] = br.readLine();
        return ret;
    }
    // -----------------------------------------
    void draw() {
        if (!vis) return;
        v.repaint();
    }
    // -----------------------------------------
    // RYB: 001 = blue,  010 = yellow, 100 = red, 
    //      011 = green, 101 = violet, 110 = orange
    //      000 = white (not drawn)
    //                   white     blue      yellow    green     red       violet    orange    brown
    int[] lightColor =  {0xFFFFFF, 0x0066FF, 0xF0F000, 0x33FF33, 0xFF4D4D, 0xE600E6, 0xFFAD31, 0x663300};
    int[] targetColor = {0xFFFFFF, 0x0000CC, 0xE0E000, 0x00EE00, 0xEE0000, 0xAA00AA, 0xFF9900, 0x663300};
    // -----------------------------------------
    public class Vis extends JPanel implements MouseListener, WindowListener {
        public void paint(Graphics g) {
            super.paint(g);
            Dimension dim = getVisDimension();
            BufferedImage bi = new BufferedImage(dim.width,dim.height,BufferedImage.TYPE_INT_RGB);
            Graphics2D g2 = (Graphics2D)bi.getGraphics();
            g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            // background
            g2.setColor(new Color(0xDDDDDD));
            g2.fillRect(0,0,dim.width,dim.height);
            // board
            g2.setColor(Color.WHITE);
            g2.fillRect(0,0,W*SZ,H*SZ);
            g2.setBackground(Color.WHITE);

            // mark crystal cells with correct (green) and wrong (red) colors.
            if (mark) {
                for (int r = 0; r < H; ++r) {
                    for (int c = 0; c < W; ++c) {
                        if (targetBoard[r][c] != '.'&& targetBoard[r][c] != 'X') {
                            if (targetBoard[r][c] == resultBoard[r][c]) {
                                g2.setColor(new Color(0xBBFFBB));
                                g2.fillRect(c * SZ + 1, r * SZ + 1, SZ - 1, SZ - 1);
                            } else if (targetBoard[r][c] != resultBoard[r][c] && resultBoard[r][c] != '0') {
                                g2.setColor(new Color(0xFFBBBB));
                                g2.fillRect(c * SZ + 1, r * SZ + 1, SZ - 1, SZ - 1);
                            }
                        }
                    }
                }
            }
            // Highlight invalid lanterns (illuminated by other lantern or by itself)
            g2.setColor(Color.RED);
            for (Item item : addedItems) {
                if (item.isLantern() && !item.valid) {
                    g2.drawRect(item.c * SZ + 1, item.r * SZ + 1, SZ - 2, SZ - 2);
                    if (SZ > 15)
                        g2.drawRect(item.c * SZ + 2, item.r * SZ + 2, SZ - 4, SZ - 4);
                }
            }

            // Rays from lanterns
            if (showRays) {
                if (SZ > 10) g2.setStroke(new BasicStroke(2f));
                for(Ray ray : rays) {
                    g2.setColor(new Color(lightColor[ray.source.type - '0']));
                    g2.drawLine(ray.c0 * SZ + SZ/2, ray.r0 * SZ + SZ/2, Math.min(W * SZ, ray.c1 * SZ + SZ/2), Math.min(H * SZ, ray.r1 * SZ + SZ/2));
                }
            }

            // actual colors of the crystals (shown as color of their interior)
            for (int r = 0; r < H; ++r)
                for (int c = 0; c < W; ++c) {
                    if (targetBoard[r][c] != '.' && targetBoard[r][c] != 'X' && resultBoard[r][c] != '.') {
                        int r0 = r * SZ + SZ / 2, c0 = c * SZ + SZ / 2;
                        g2.setColor(new Color(lightColor[resultBoard[r][c] - '0']));
                        g2.fillPolygon(new int[]{c0 - SZ/3, c0,        c0 + SZ/3, c0 + SZ/6, c0 - SZ/6, c0 - SZ/3},
                                       new int[]{r0,        r0 + SZ/3, r0,        r0 - SZ/6, r0 - SZ/6, r0       }, 5);
                    }
                }

            if (SZ > 10)
                g2.setStroke(new BasicStroke(1.5f));
            for (int i = 0; i < addedItems.size(); ++i) {
                Item item = addedItems.get(i);
                if (item.isLantern()) {
                    // lanterns as suns of the given color
                    int r0 = item.r * SZ + SZ / 2, c0 = item.c * SZ + SZ / 2, d = (int)(SZ / 1.0 / 4);
                    g2.setColor(new Color(targetColor[item.getColor()]));
                    g2.drawLine(c0 - SZ/3, r0, c0 + SZ/3, r0);
                    g2.drawLine(c0, r0 - SZ/3, c0, r0 + SZ/3);
                    g2.drawLine(c0 - d, r0 - d, c0 + d, r0 + d);
                    g2.drawLine(c0 - d, r0 + d, c0 + d, r0 - d);
                    g2.drawOval(c0 - SZ/6, r0 - SZ/6, SZ/3, SZ/3);
                    g2.setColor(new Color(lightColor[item.getColor()]));
                    g2.fillOval(c0 - SZ/6, r0 - SZ/6, SZ/3, SZ/3);
                    continue;
                } 
                g2.setColor(Color.DARK_GRAY);
                if (item.type == 'X') {
                    //Added obstacles, as a small square in the cell center
                    g2.clearRect(item.c * SZ + SZ/3, item.r * SZ + SZ/3, SZ/3, SZ/3);
                    g2.drawRect(item.c * SZ + SZ/3, item.r * SZ + SZ/3, SZ/3, SZ/3);
                    g2.drawLine(item.c * SZ + SZ/3, item.r * SZ + SZ/3, item.c * SZ + 2*SZ/3, item.r * SZ + 2*SZ/3);
                    g2.drawLine(item.c * SZ + 2*SZ/3, item.r * SZ + SZ/3, item.c * SZ + SZ/3, item.r * SZ + 2*SZ/3);
                } else if (item.type == '\\') {
                    //Mirror, as a diagonal line
                    g2.drawLine(item.c * SZ, item.r * SZ, (item.c+1) * SZ, (item.r+1) * SZ);
                } else if (item.type == '/') {
                    //Mirror, as a diagonal line
                    g2.drawLine(item.c * SZ, (item.r+1) * SZ, (item.c+1) * SZ, item.r * SZ);
                }
            }

            // finally, the required colors of the crystals (shown as color of their outline)
            for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c) {
                if (targetBoard[r][c] == '.')
                    continue;
                if (targetBoard[r][c] == 'X') {
                    g2.setColor(Color.DARK_GRAY);
                    g2.fillRect(c * SZ + 1, r * SZ + 1, SZ - 1, SZ - 1);
                } else {
                    int r0 = r * SZ + SZ / 2, c0 = c * SZ + SZ / 2;
                    g2.setColor(new Color(targetColor[targetBoard[r][c]-'0']));
                    g2.drawPolyline(new int[]{c0 - SZ/3, c0,        c0 + SZ/3, c0 + SZ/6, c0 - SZ/6, c0 - SZ/3},
                                    new int[]{r0,        r0 + SZ/3, r0,        r0 - SZ/6, r0 - SZ/6, r0,      }, 6);
                    if (!plain) {
                        g2.drawPolyline(new int[]{c0 - SZ/3, c0 + SZ/3}, new int[]{r0, r0}, 2);
                        g2.drawPolyline(new int[]{c0 + SZ/6, c0,        c0 - SZ/6, c0       , c0 + SZ/6},
                                        new int[]{r0       , r0 + SZ/3, r0,        r0 - SZ/6, r0       }, 5);
                    }
                }
            }

            // lines between cells
            g2.setStroke(new BasicStroke(1f));
            g2.setColor(Color.BLACK);
            for (int i = 0; i <= H; i++)
                g2.drawLine(0,i*SZ,W*SZ,i*SZ);
            for (int i = 0; i <= W; i++)
                g2.drawLine(i*SZ,0,i*SZ,H*SZ);

            g2.setFont(new Font("Arial",Font.BOLD,13));

            // "buttons" to control visualization options
            int xText = SZ * W + 10;
            int wText = 100;
            int yText = 10;
            int hButton = 30;
            int hText = 20;
            int vGap = 10;

            if (manualReady) g2.clearRect(xText,yText,wText,hButton);
            drawString(g2,"READY",xText,yText,wText,hButton,0);
            g2.drawRect(xText,yText,wText,hButton);
            yText += hButton + vGap;

            if (plain) g2.clearRect(xText,yText,wText,hButton);
            drawString(g2,"PLAIN",xText,yText,wText,hButton,0);
            g2.drawRect(xText,yText,wText,hButton);
            yText += hButton + vGap;

            if (mark) g2.clearRect(xText,yText,wText,hButton);
            drawString(g2,"MARK",xText,yText,wText,hButton,0);
            g2.drawRect(xText,yText,wText,hButton);
            yText += hButton + vGap;

            if (showRays) g2.clearRect(xText,yText,wText,hButton);
            drawString(g2,"RAYS",xText,yText,wText,hButton,0);
            g2.drawRect(xText,yText,wText,hButton);
            yText += hButton + vGap;

            // current score
            drawString(g2,"SCORE",xText,yText,wText,hText,0);
            yText += hText;
            drawString(g2,String.format("%d", (int) getScore()),xText,yText,wText,hText,0);
            yText += hText * 2;

            // other information about the current test case / state.
            // Item costs
            drawString(g2,"COSTS",xText,yText,wText,hText,0);
            yText += hText;
            drawString(g2,"Lantern:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(costLantern),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Mirror:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(costMirror),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Obstacle:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(costObstacle),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText * 2;

            // Number of items (Used / Limit)
            int numLantern = 0;
            int numMirror = 0;
            int numObstacle = 0;
            int numInvalidLanterns = 0;
            for(Item item : addedItems) {
                if (item.isLantern()) {
                    numLantern++;
                    if (!item.valid) numInvalidLanterns++;
                } else if (item.isMirror()) 
                    numMirror++;
                else if (item.isObstacle()) 
                    numObstacle++;
            }
            drawString(g2,"ADDED ITEMS",xText,yText,wText,hText,0);
            yText += hText;
            drawString(g2,"Lanterns:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(numLantern),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Invalid:",xText,yText,wText*2/3,hText,1);
            if (numInvalidLanterns > 0) g2.setColor(Color.RED);
            drawString(g2,String.valueOf(numInvalidLanterns),xText+wText*2/3+2,yText,wText/3,hText,-1);
            g2.setColor(Color.BLACK);
            yText += hText;
            drawString(g2,"Mirrors:",xText,yText,wText*2/3,hText,1);
            drawString(g2,numMirror+"/"+maxMirrors,xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Obstacles:",xText,yText,wText*2/3,hText,1);
            drawString(g2,numObstacle+"/"+maxObstacles,xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText * 2;

            // Number of crystals
            drawString(g2,"CRYSTALS",xText,yText,wText,hText,0);
            yText += hText;
            drawString(g2,"Total:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(numCrystals),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Prim.OK:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(numCorrectPrimary),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Sec.OK:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(numCorrectSecondary),xText+wText*2/3+2,yText,wText/3,hText,-1);
            yText += hText;
            drawString(g2,"Incorrect:",xText,yText,wText*2/3,hText,1);
            drawString(g2,String.valueOf(numIncorrect),xText+wText*2/3+2,yText,wText/3,hText,-1);

            if (save) {
                try {
                    ImageIO.write(bi,"png", new File(seed + ".png"));
                } catch (Exception e) { e.printStackTrace(); }
            }

            g.drawImage(bi,0,0,null);
        }
        // -------------------------------------
        void drawString(Graphics2D g2, String text, int x, int y, int w, int h, int align) {
            FontMetrics metrics = g2.getFontMetrics();
            Rectangle2D rect = metrics.getStringBounds(text, g2);
            int th = (int)(rect.getHeight());
            int tw  = (int)(rect.getWidth());
            if (align == 0) x = x + (w - tw) / 2;
            else if (align > 0) x = (x + w) - tw;
            y = y + (h - th) / 2 + metrics.getAscent();
            g2.drawString(text, x, y);
        }
        // -------------------------------------
        public Vis() {
            addMouseListener(this);
            jf.addWindowListener(this);
        }
        // -------------------------------------
        public Dimension getVisDimension() {
            return new Dimension(W * SZ + 126, Math.max(H * SZ + 1, 550));
        }
        // -------------------------------------
        // WindowListener
        public void windowClosing(WindowEvent e){ 
            if(proc != null)
                try { proc.destroy(); } 
                catch (Exception ex) { ex.printStackTrace(); }
            System.exit(0); 
        }
        public void windowActivated(WindowEvent e) { }
        public void windowDeactivated(WindowEvent e) { }
        public void windowOpened(WindowEvent e) { }
        public void windowClosed(WindowEvent e) { }
        public void windowIconified(WindowEvent e) { }
        public void windowDeiconified(WindowEvent e) { }
        // -------------------------------------
        // MouseListener
        public void mousePressed(MouseEvent e) {
            // Treat "plain", "mark" and "rays" buttons
            int x = e.getX() - SZ * W - 10, y = e.getY() - 10;
            if (x >= 0 && x <= 100 && y >= 40 && y <= 70) {
                plain = !plain;
                repaint();
                return;
            }
            if (x >= 0 && x <= 100 && y >= 80 && y <= 110) {
                mark = !mark;
                repaint();
                return;
            }
            if (x >= 0 && x <= 100 && y >= 120 && y <= 150) {
                showRays = !showRays;
                repaint();
                return;
            }

            // for manual play
            if (!manual || manualReady) return;

            // "ready" button submits current state of the board
            if (x >= 0 && x <= 100 && y >= 0 && y <= 30) {
                manualReady = true;
                repaint();
                return;
            }

            // regular click places a new item, modifies an existing one or removes it
            int row = e.getY()/SZ, col = e.getX()/SZ;
            // convert to args only clicks with valid coordinates
            if (!isInside(row, col))
                return;
            // ignore clicks on obstacles and crystals (can't place an item there or remove it)
            if (targetBoard[row][col] != '.' && SwingUtilities.isLeftMouseButton(e)) {
                addFatalError("You can only place lanterns on empty cells of the board.");
                return;
            }

            String errmes;
            // decide whether we're removing an item or adding/modifying one
            if (SwingUtilities.isRightMouseButton(e)) {
                // right click removes an item
                errmes = removeOneItem(row, col);
                if (!errmes.equals("")) {
                    addFatalError(errmes);
                    return;
                }
            }
            if (SwingUtilities.isLeftMouseButton(e)) {
                // left click adds a new item or changes the existing one
                // changing is implemented as removal followed by addition 
                char type = 0;
                for (int i = 0; i < addedItems.size(); ++i) {
                    Item item = addedItems.get(i);
                    if (item.r == row && item.c == col) {
                        int idx = getTypeIdx(item.type);
                        //Check if the next item type is available
                        int usedObstacles = 0;
                        int usedMirrors = 0;
                        for(Item a : addedItems) { 
                            if (a.isObstacle()) usedObstacles++;
                            else if (a.isMirror()) usedMirrors++;
                        }
                        while(true) {
                            type = types[++idx % types.length];
                            if (type == 'X' && usedObstacles >= maxObstacles) continue; //All obstacles used 
                            if ((type == '\\' || type == '/') && item.type != '\\' && item.type != '/' && usedMirrors >= maxMirrors) continue;  //All mirrors used 
                            break; //OK, accept this type
                        }
                    }
                }
                if (type != 0) {
                    // remove current item
                    errmes = removeOneItem(row, col);
                    if (!errmes.equals("")) {
                        addFatalError(errmes);
                        return;
                    }
                } else {
                    //Initial type, in case of adding new item
                    type = types[0];
                }
                errmes = placeOneItem(row, col, type);
                if (!errmes.equals("")) {
                    addFatalError(errmes);
                    return;
                }
            }
            // board change succeeded
            calculateLighting();
            repaint();
        }
        public void mouseClicked(MouseEvent e) { }
        public void mouseReleased(MouseEvent e) { }
        public void mouseEntered(MouseEvent e) { }
        public void mouseExited(MouseEvent e) { }
    }
    // -----------------------------------------
    public CrystalLightingVis(String seed) {
      try {
        if (vis) {
            jf = new JFrame();
            jf.setTitle("Seed " + seed);
            v = new Vis();
            JScrollPane sp = new JScrollPane(v);
            jf.getContentPane().add(sp);
        }
        if (exec != null) {
            try {
                Runtime rt = Runtime.getRuntime();
                proc = rt.exec(exec);
                os = proc.getOutputStream();
                is = proc.getInputStream();
                br = new BufferedReader(new InputStreamReader(is));
                new ErrorReader(proc.getErrorStream()).start();
            } catch (Exception e) { e.printStackTrace(); }
        }
        System.out.println("Score = " + runTest(seed));
        if (proc != null)
            try { proc.destroy(); } 
            catch (Exception e) { e.printStackTrace(); }
      }
      catch (Exception e) { e.printStackTrace(); }
    }
    // -----------------------------------------
    public static void main(String[] args) {
        seed = "1";
        vis = true;
        manual = false;
        SZ = 0; //Auto-fit to desktop size
        plain = false;
        mark = false;
        showRays = false;
        save = false;
        for (int i = 0; i<args.length; i++)
        {   if (args[i].equals("-seed"))
                seed = args[++i];
            if (args[i].equals("-exec"))
                exec = args[++i];
            if (args[i].equals("-novis"))
                vis = false;
            if (args[i].equals("-manual"))
                manual = true;
            if (args[i].equals("-size"))
                SZ = Integer.parseInt(args[++i]);
            if (args[i].equals("-debug"))
                debug = true;
            if (args[i].equals("-plain"))
                plain = true;
            if (args[i].equals("-mark"))
                mark = true;
            if (args[i].equals("-rays"))
                showRays = true;
            if (args[i].equals("-save"))
                save = true;
        }
        if (exec == null)
            manual = true;
        if (manual)
            vis = true;

        CrystalLightingVis f = new CrystalLightingVis(seed);
    }
    // -----------------------------------------
    void addFatalError(String message) {
        System.out.println(message);
    }
}

class ErrorReader extends Thread{
    InputStream error;
    public ErrorReader(InputStream is) {
        error = is;
    }
    public void run() {
        try {
            byte[] ch = new byte[50000];
            int read;
            while ((read = error.read(ch)) > 0)
            {   String s = new String(ch,0,read);
                System.out.print(s);
                System.out.flush();
            }
        } catch(Exception e) { }
    }
}
