## Installing Vector War on macOS

`ggpo-<version>-vectorwar-macos.dmg` runs on macOS 11 and later, on Apple Silicon and Intel Macs. Open the disk image and drag **Vector War** to **Applications**.

The app is not signed by an Apple Developer account, so macOS blocks it the first time. To open it once; later launches work normally:

- **macOS 15 Sequoia and later:** double-click Vector War and dismiss the warning. Then open **System Settings → Privacy & Security**, scroll to Security, click **Open Anyway** next to Vector War, and confirm.
- **macOS 11 to 14:** in Applications, **right-click** (or Control-click) Vector War, choose **Open**, then **Open** again in the dialog.

When macOS asks whether Vector War may accept incoming network connections, choose **Allow** for direct or hole-punched play. Relay play works either way.

The macOS SDK (`ggpo-<version>-macos-universal.tar.gz`) needs none of this; the application that embeds GGPO signs it.

Verify downloads against `SHA256SUMS`.
