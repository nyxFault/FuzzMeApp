# FuzzMeApp

[![Android](https://img.shields.io/badge/Platform-Android-3DDC84?logo=android&logoColor=white)](https://developer.android.com/)
[![Language](https://img.shields.io/badge/Code-Java%20%2B%20JNI%20(C%2B%2B)-2F80ED)](#)
[![Gradle](https://img.shields.io/badge/Build-Gradle-02303A?logo=gradle&logoColor=white)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

An intentionally vulnerable Android app for JNI + fuzzing practice.

This app was built as a hands-on target for testing native crash discovery workflows on real devices.  
It is **not** meant for production use.

---

## What this app does

- Starts a native TCP listener on port `4444`
- Receives client data in JNI/native code (`libfuzzme.so`)
- Shows last received payload in the app UI
- Shows connection toast with source IP:port
- Intentionally crashes when payload starts with:
  - `FuzzMe@123`

---

## Why this exists

Most Android security demos stop at toy examples.  
This project is for people who actually want to:

- inspect JNI behavior
- test native attack surface from app traffic
- wire up fuzz harnesses against a real Android `.so`
- reproduce and triage crashes with concrete inputs

---

## Project layout

```text
FuzzMeApp/
├── app/
│   ├── src/main/java/com/example/fuzzmeapp/MainActivity.java
│   ├── src/main/cpp/native-lib.cpp
│   ├── src/main/cpp/CMakeLists.txt
│   ├── src/main/AndroidManifest.xml
│   └── src/main/res/
├── gradle/
├── build.gradle.kts
└── settings.gradle.kts
```

---

## Build

```bash
cd /path/to/FuzzMeApp
./gradlew :app:assembleDebug
```

Output APK:

```text
app/build/outputs/apk/debug/app-debug.apk
```

---

## Quick test

1. Install and open app
2. Tap **Start**
3. Send payload:

```bash
adb shell "echo -n hello | nc 127.0.0.1 4444"
```

Crash trigger test:

```bash
adb shell "echo -n FuzzMe@123 | nc 127.0.0.1 4444"
```

Expected: app process crashes (intentional behavior).

---

## Security note

This app intentionally contains crash logic for research and education.  
Do not expose it on untrusted networks. Do not ship this in production.

---

## License

MIT — see [LICENSE](./LICENSE)
