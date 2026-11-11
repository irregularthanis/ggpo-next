package org.ggponext.vectorwar;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Collects the same arguments as the desktop command line and starts the game:
 * local UDP port, player count, then one entry per player ("local" or
 * IP:PORT), optionally followed by spectator IP:PORT entries.
 */
public class LauncherActivity extends Activity {
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (16 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);

        final EditText port = field(layout, "Local UDP port", "7000", true);
        final EditText players = field(layout, "Number of players", "2", true);
        final EditText entries = field(layout,
                "Players in order: local or IP:PORT (spectators may follow)",
                "local 192.168.1.20:7001", false);

        Button start = new Button(this);
        start.setText("Start");
        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                String args = port.getText().toString().trim() + " "
                        + players.getText().toString().trim() + " "
                        + entries.getText().toString().trim();
                Intent intent = new Intent(LauncherActivity.this, VectorWarActivity.class);
                intent.putExtra(VectorWarActivity.EXTRA_ARGS, args);
                startActivity(intent);
            }
        });
        layout.addView(start);
        setContentView(layout);
    }

    private EditText field(LinearLayout layout, String label, String value, boolean number) {
        TextView text = new TextView(this);
        text.setText(label);
        layout.addView(text);
        EditText edit = new EditText(this);
        edit.setText(value);
        if (number) {
            edit.setInputType(InputType.TYPE_CLASS_NUMBER);
        }
        layout.addView(edit);
        return edit;
    }
}
