package org.ggponext.vectorwar;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The Android start screen. Direct play collects the same arguments as the
 * desktop command line: local UDP port, player count, then one entry per
 * player ("local" or IP:PORT), optionally followed by spectator IP:PORT
 * entries. Matchmaking passes "--matchmaking SERVER PLAYERS [LOBBY] LOCAL_PORT",
 * like the desktop start screens.
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

        heading(layout, "Direct");
        final EditText players = field(layout, "Number of players", "2", true);
        final EditText entries = field(layout,
                "Players in order: local or IP:PORT (spectators may follow)",
                "local 192.168.1.20:7001", false);
        Button start = new Button(this);
        start.setText("Connect");
        layout.addView(start);

        heading(layout, "Matchmaking: 2 to 4 players, share the lobby code");
        final EditText server = field(layout, "Server", getString(R.string.matchmaking_host), false);
        final EditText group = field(layout, "Players (2 to 4)", "2", true);
        final EditText lobby = field(layout, "Lobby code (optional)", "", false);
        lobby.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS);
        Button matchmaking = new Button(this);
        matchmaking.setText("Matchmaking");
        layout.addView(matchmaking);

        final TextView message = new TextView(this);
        message.setTextColor(0xFFFF6060);
        layout.addView(message);

        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                launch(port.getText().toString().trim() + " "
                        + players.getText().toString().trim() + " "
                        + entries.getText().toString().trim());
            }
        });
        matchmaking.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                try {
                    message.setText("");
                    launch(StartArgs.matchmaking(server.getText().toString(),
                            group.getText().toString(), lobby.getText().toString(),
                            port.getText().toString()));
                } catch (StartArgs.Invalid invalid) {
                    message.setText(invalid.getMessage());
                }
            }
        });

        ScrollView scroll = new ScrollView(this);
        scroll.addView(layout);
        setContentView(scroll);
    }

    private void launch(String args) {
        Intent intent = new Intent(this, VectorWarActivity.class);
        intent.putExtra(VectorWarActivity.EXTRA_ARGS, args);
        startActivity(intent);
    }

    private void heading(LinearLayout layout, String label) {
        TextView text = new TextView(this);
        text.setText(label);
        text.setTextSize(18);
        text.setPadding(0, (int) (12 * getResources().getDisplayMetrics().density), 0, 0);
        layout.addView(text);
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
