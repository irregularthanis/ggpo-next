// The Android launcher's matchmaking arguments match the desktop start
// screens (tests/vectorwar/start_form_contract_test.cpp). Plain Java:
//   javac -d OUT StartArgs.java StartArgsTest.java && java -cp OUT StartArgsTest
import org.ggponext.vectorwar.StartArgs;

public class StartArgsTest {
    private static int failures = 0;

    private static void expect(String server, String players, String lobby, String port, String expected) {
        String actual;
        try {
            actual = StartArgs.matchmaking(server, players, lobby, port);
        } catch (StartArgs.Invalid invalid) {
            actual = "error: " + invalid.getMessage();
        }
        if (!actual.equals(expected)) {
            System.err.println("FAIL: got \"" + actual + "\", expected \"" + expected + "\"");
            failures++;
        }
    }

    public static void main(String[] args) {
        expect("vw.example.org:7100", "4", "", "0", "--matchmaking vw.example.org:7100 4 0");
        expect(" vw.example.org:7100 ", "4", "fri3nds", "7000", "--matchmaking vw.example.org:7100 4 FRI3NDS 7000");
        expect("vw.example.org", "4", "two words", "7000", "error: Lobby code is up to 8 letters or digits");
        expect("vw.example.org", "5", "", "7000", "error: Players must be 2, 3 or 4");
        expect("  ", "2", "", "7000", "error: Enter a matchmaking server");
        expect("vw.example.org", "2", "", "70000", "error: Local port must be 0 to 65535");
        if (failures > 0) {
            System.err.println("android-start-args: " + failures + " failure(s)");
            System.exit(1);
        }
        System.out.println("android-start-args: matchmaking arguments pass");
    }
}
