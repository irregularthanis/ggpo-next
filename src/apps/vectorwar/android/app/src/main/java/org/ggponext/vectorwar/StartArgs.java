package org.ggponext.vectorwar;

/**
 * The launcher's matchmaking fields as command-line arguments, with the same
 * rules and messages as the desktop start screens (vw_start_form.cpp):
 * "--matchmaking SERVER PLAYERS [LOBBY] LOCAL_PORT". Plain Java so it is
 * tested without a device (tests/android/StartArgsTest.java).
 */
public final class StartArgs {
    private StartArgs() {}

    /** Thrown with the message the launcher shows under the fields. */
    public static final class Invalid extends Exception {
        public Invalid(String message) { super(message); }
    }

    public static String matchmaking(String server, String players, String lobby, String localPort)
            throws Invalid {
        server = server.trim();
        players = players.trim();
        lobby = lobby.trim().toUpperCase(java.util.Locale.ROOT);
        localPort = localPort.trim();
        if (server.isEmpty() || server.contains(" ")) throw new Invalid("Enter a matchmaking server");
        if (!isNumber(players, 2, 4)) throw new Invalid("Players must be 2, 3 or 4");
        if (!lobby.matches("[A-Z0-9]{0,8}")) throw new Invalid("Lobby code is up to 8 letters or digits");
        if (!isNumber(localPort, 0, 65535)) throw new Invalid("Local port must be 0 to 65535");
        StringBuilder args = new StringBuilder("--matchmaking ").append(server).append(' ').append(players);
        if (!lobby.isEmpty()) args.append(' ').append(lobby);
        return args.append(' ').append(localPort).toString();
    }

    private static boolean isNumber(String text, int low, int high) {
        if (!text.matches("[0-9]{1,5}")) return false;
        int value = Integer.parseInt(text);
        return value >= low && value <= high;
    }
}
