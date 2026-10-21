# Phase 0 spike

Throwaway prototype for doc/ANDROID-CONCEPT.md, phase 0. Results are in section 13 there.

- `app/`: Android app with an Apache MINA SFTP server (port 52382, user `afs`,
  password `afs-spike`, root `/storage/emulated/0`) and a modification time probe.
- `pc-probe/`: runs the SFTP calls of SyncFileSsh against it with libssh,
  libssh with pipelined requests, or libssh2.

```sh
./gradlew assembleDebug && adb install -r app/build/outputs/apk/debug/app-debug.apk
# grant "All files access" on the phone (button in the app)
adb shell am start -n com.welschsoftware.autofilesync.spike/.MainActivity --es cmd mtime   # or start|stop|lock|unlock
adb logcat -s AFS-SPIKE:I
g++ -std=c++20 -O2 -o afs_probe pc-probe/probe.cpp -lssh -lssh2
./afs_probe libssh|libssh-aio|libssh2 <phone-ip> 52382 afs-spike [MiB]
```

Both probes remove everything they create on the phone.
