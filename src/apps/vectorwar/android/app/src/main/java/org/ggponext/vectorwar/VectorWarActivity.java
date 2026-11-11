package org.ggponext.vectorwar;

import org.libsdl.app.SDLActivity;

/** Runs the shared SDL2 Vector War frontend (libmain.so). */
public class VectorWarActivity extends SDLActivity {
    /** Space-separated command line, as on desktop: "7000 2 local 192.168.1.20:7001". */
    public static final String EXTRA_ARGS = "args";

    @Override
    protected String[] getLibraries() {
        // GGPO is loaded explicitly: API 21-22 do not resolve a library's
        // dependencies from the app's own native library directory.
        return new String[] { "SDL2", "GGPO", "main" };
    }

    @Override
    protected String[] getArguments() {
        String args = getIntent().getStringExtra(EXTRA_ARGS);
        if (args == null || args.trim().isEmpty()) {
            return new String[0];
        }
        return args.trim().split("\\s+");
    }
}
