# AFS_Android

Die Android-App von AutoFileSync (GPLv3), Konzept in `doc/ANDROID-CONCEPT.md`.

- `app/`: die App. Kotlin, Jetpack Compose, minSdk 26.
  - `link/`: Kopplung und Steuerkanal zum PC (`PairingOffer` liest den QR-Code,
    `DeviceIdentity` ist das Client-Zertifikat aus dem Android-Keystore,
    `SshHostKey` der Schlüssel des späteren SFTP-Servers, `PcLink` die
    TLS-Verbindung mit Fingerabdruck-Pinning, `PcStore` das Gespeicherte).
  - `ui/`: Bildschirme (`AppUi`) und QR-Scanner (`QrScanner`, CameraX + ZXing).
  - `AppState`: Zustand und Aktionen hinter den Knöpfen.
- `spike/`: der Prototyp aus Phase 0 (SFTP-Server, Zeitstempel-Test), eigenständiges
  Gradle-Projekt.

## Bauen und installieren

```sh
cd AFS_Android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Braucht das Android SDK (Plattform 34, build-tools 34) in `local.properties`
(`sdk.dir=…`) und Java 17 oder neuer.

## Ablauf

1. Auf dem PC in AutoFileSync: Datei → Handy verbinden…
2. In der App: „QR-Code scannen“, Code abfotografieren, „Verbinden“.
3. Die Statuskarte zeigt den PC, die letzte Meldung und die Synchronisationen,
   die den PC betreffen (ab Phase 2).

Ohne `assetlinks.json` auf welsch-software.com öffnet die Kamera-App den Code
noch nicht direkt in der App (Phase 4); der Scanner in der App funktioniert
unabhängig davon.
