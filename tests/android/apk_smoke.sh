#!/bin/sh
# Install the Vector War APK and run its frontend smoke on the connected device:
# the SDL window and renderer must start and log the success line.
set -eu
apk="$1"
adb install -r "$apk"
adb logcat -c
# Do not use `am start -W`: API 21 can wait forever for SDLActivity to become
# idle even after the frontend has started. The log marker below is the actual
# bounded readiness contract on every tested API level.
adb shell am start -n org.ggponext.vectorwar/.VectorWarActivity --es args --frontend-smoke
for second in $(seq 1 60); do
    if adb logcat -d | grep -q "Vector War SDL frontend smoke: OK"; then
        echo "Vector War APK frontend smoke passed (API $(adb shell getprop ro.build.version.sdk | tr -d '\r'))"
        exit 0
    fi
    sleep 1
done
echo "frontend smoke line not found; recent log:" >&2
adb logcat -d | grep -iE "SDL|vectorwar|AndroidRuntime|DEBUG" | tail -60 >&2
exit 1
