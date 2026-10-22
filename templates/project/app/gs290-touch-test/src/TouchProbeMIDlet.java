import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

/**
 * Minimal unprivileged MIDP 2.0 touch/input test for LG GS290.
 * No networking, files, reflection, proprietary LG API, or native code.
 */
public final class TouchProbeMIDlet extends MIDlet {
    private Display display;
    private TouchScreen screen;

    public void startApp() {
        if (display == null) {
            display = Display.getDisplay(this);
            screen = new TouchScreen(this);
        }
        display.setCurrent(screen);
    }

    public void pauseApp() { }
    public void destroyApp(boolean unconditional) { }

    private static final class TouchScreen extends Canvas implements CommandListener {
        private final TouchProbeMIDlet midlet;
        private final Command exit = new Command("Exit", Command.EXIT, 1);
        private int x = -1;
        private int y = -1;
        private int events = 0;
        private String event = "WAITING";

        private TouchScreen(TouchProbeMIDlet owner) {
            midlet = owner;
            addCommand(exit);
            setCommandListener(this);
        }

        protected void paint(Graphics g) {
            int w = getWidth();
            int h = getHeight();
            g.setColor(0x000000);
            g.fillRect(0, 0, w, h);
            g.setColor(0xFFFFFF);
            g.drawString("GS290 / J2ME Touch", 4, 4, Graphics.TOP | Graphics.LEFT);
            g.drawString("Hello, LG World!", 4, 30, Graphics.TOP | Graphics.LEFT);
            g.drawString("U H4VE 8EEN", 4, 52, Graphics.TOP | Graphics.LEFT);
            g.drawString("PWND BR0! <3", 4, 72, Graphics.TOP | Graphics.LEFT);
            g.drawLine(4, 98, w - 4, 98);
            g.drawString("Canvas: " + w + "x" + h, 4, 106, Graphics.TOP | Graphics.LEFT);
            g.drawString("Pointer: " + (hasPointerEvents() ? "YES" : "NO"), 4, 128, Graphics.TOP | Graphics.LEFT);
            g.drawString("Drag: " + (hasPointerMotionEvents() ? "YES" : "NO"), 4, 150, Graphics.TOP | Graphics.LEFT);
            g.drawString("Event: " + event, 4, 176, Graphics.TOP | Graphics.LEFT);
            g.drawString("X=" + x + " Y=" + y, 4, 198, Graphics.TOP | Graphics.LEFT);
            g.drawString("Count: " + events, 4, 220, Graphics.TOP | Graphics.LEFT);
            if (x >= 0 && y >= 0) {
                g.drawRect(Math.max(0, x - 5), Math.max(0, y - 5), 10, 10);
            }
        }

        protected void pointerPressed(int px, int py) { record("PRESS", px, py); }
        protected void pointerReleased(int px, int py) { record("RELEASE", px, py); }
        protected void pointerDragged(int px, int py) { record("DRAG", px, py); }

        protected void keyPressed(int key) {
            event = "KEY " + key;
            events++;
            repaint();
        }

        private void record(String name, int px, int py) {
            event = name;
            x = px;
            y = py;
            events++;
            repaint();
        }

        public void commandAction(Command c, Displayable d) {
            if (c == exit) {
                midlet.notifyDestroyed();
            }
        }
    }
}
