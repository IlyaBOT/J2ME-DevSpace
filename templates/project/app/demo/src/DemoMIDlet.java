import java.util.Date;
import javax.microedition.lcdui.Alert;
import javax.microedition.lcdui.AlertType;
import javax.microedition.lcdui.Choice;
import javax.microedition.lcdui.ChoiceGroup;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.DateField;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Gauge;
import javax.microedition.lcdui.Spacer;
import javax.microedition.lcdui.StringItem;
import javax.microedition.lcdui.TextField;
import javax.microedition.midlet.MIDlet;

public final class DemoMIDlet extends MIDlet implements CommandListener {
    private final Command exitCommand = new Command("Exit", Command.EXIT, 0);
    private final Command okCommand = new Command("OK", Command.OK, 1);
    private final Command resetCommand = new Command("Reset", Command.SCREEN, 2);

    private Display display;
    private Form form;
    private TextField nameField;
    private TextField notesField;
    private ChoiceGroup modeChoice;
    private ChoiceGroup featureChoice;
    private Gauge volumeGauge;
    private Gauge progressGauge;
    private StringItem statusItem;

    public void startApp() {
        display = Display.getDisplay(this);
        if (form == null) {
            form = buildForm();
        }
        display.setCurrent(form);
    }

    public void pauseApp() {
    }

    public void destroyApp(boolean unconditional) {
    }

    private Form buildForm() {
        Form f = new Form("J2ME DevSpace Demo Midlet");

        StringItem title = new StringItem(null, "J2ME DevSpace Demo Midlet");
        title.setLayout(StringItem.LAYOUT_CENTER | StringItem.LAYOUT_NEWLINE_AFTER);
        f.append(title);
        f.append(new Spacer(1, 6));

        f.append(new StringItem("Screen", "Compact LCDUI controls for 240x320+ phones."));

        nameField = new TextField("Name", "Vector", 24, TextField.ANY);
        f.append(nameField);

        notesField = new TextField("Text field", "Short editable text", 48, TextField.ANY);
        f.append(notesField);

        modeChoice = new ChoiceGroup("Mode", Choice.POPUP);
        modeChoice.append("Normal", null);
        modeChoice.append("Touch phone", null);
        modeChoice.append("Low memory", null);
        modeChoice.setSelectedIndex(0, true);
        f.append(modeChoice);

        featureChoice = new ChoiceGroup("Tests", Choice.MULTIPLE);
        featureChoice.append("Buttons", null);
        featureChoice.append("Text input", null);
        featureChoice.append("Gauges", null);
        featureChoice.append("Scrolling", null);
        featureChoice.setSelectedIndex(0, true);
        featureChoice.setSelectedIndex(1, true);
        f.append(featureChoice);

        volumeGauge = new Gauge("Volume", true, 10, 5);
        f.append(volumeGauge);

        progressGauge = new Gauge("Progress", false, 100, 35);
        f.append(progressGauge);

        DateField dateField = new DateField("Build date", DateField.DATE_TIME);
        dateField.setDate(new Date());
        f.append(dateField);

        statusItem = new StringItem("Status", "Ready. Press OK to update this text.");
        f.append(statusItem);

        f.append(new StringItem("Long content", "This extra line is here to test vertical scrolling on 240x320 screens."));
        f.append(new StringItem("More", "If you can read this, form scrolling works."));

        f.addCommand(okCommand);
        f.addCommand(resetCommand);
        f.addCommand(exitCommand);
        f.setCommandListener(this);
        return f;
    }

    public void commandAction(Command command, Displayable displayable) {
        if (command == exitCommand) {
            notifyDestroyed();
            return;
        }

        if (command == resetCommand) {
            nameField.setString("Vector");
            notesField.setString("Short editable text");
            volumeGauge.setValue(5);
            progressGauge.setValue(35);
            statusItem.setText("Reset complete.");
            return;
        }

        if (command == okCommand) {
            int next = progressGauge.getValue() + 15;
            if (next > progressGauge.getMaxValue()) {
                next = 0;
            }
            progressGauge.setValue(next);
            statusItem.setText("Hello, " + nameField.getString() + ". Progress: " + next + "%.");

            Alert alert = new Alert("OK", "Demo controls updated.", null, AlertType.INFO);
            alert.setTimeout(1200);
            display.setCurrent(alert, form);
        }
    }
}
