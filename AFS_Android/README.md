# AFS_Android

Die Android-App von AutoFileSync (GPLv3), Konzept in `doc/ANDROID-CONCEPT.md`.

- `app/`: die App. Kotlin, Jetpack Compose, minSdk 26.
  - `link/`: Kopplung und Steuerkanal zum PC (`PairingOffer` liest den QR-Code,
    `DeviceIdentity` ist das Client-Zertifikat aus dem Android-Keystore,
    `SshHostKey` der Schlüssel des späteren SFTP-Servers, `PcLink` die
    TLS-Verbindung mit Fingerabdruck-Pinning, `PcStore` das Gespeicherte).
  - `ui/`: Bildschirme (`AppUi`) und QR-Scanner (`QrScanner`, CameraX + ZXing).
  - `work/`: Durchläufe im Hintergrund (`SyncWorker` über WorkManager, `Scheduler` für
    Zeitplan, neue Fotos und den Knopf, `Pass` als gemeinsamer Ablauf, `Notifications`).
  - `link/Discovery`: findet den PC per UDP-Rundruf, wenn sich seine Adresse geändert hat.
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

Mit `keystore.properties` wird auch der Debug-Build mit dem Release-Schlüssel signiert,
sodass Debug- und Release-APK sich gegenseitig ohne Deinstallieren ersetzen; ohne sie
gilt der übliche Debug-Schlüssel.

## Ablauf

1. Auf dem PC in AutoFileSync: Datei → Handy verbinden…
2. In der App: „QR-Code scannen“, Code abfotografieren, „Verbinden“.
3. Die Statuskarte zeigt den PC, die letzte Meldung und die Synchronisationen,
   die den PC betreffen (ab Phase 2).

Ohne `assetlinks.json` auf welsch-software.com öffnet die Kamera-App den Code
noch nicht direkt in der App (Phase 4); der Scanner in der App funktioniert
unabhängig davon.

## Release

- Einmalig: `tools/make-release-key.sh` erzeugt den Signaturschlüssel (Standard:
  `~/.android/autofilesync-release.jks`) und `keystore.properties` (beides nie ins Repo).
  Schlüssel und Passwort sichern: ohne sie lässt sich kein Update mehr installieren.
- `scripts/makeAndroidInstaller.sh` (im Wurzelverzeichnis) baut die signierte APK, prüft
  die Signatur und legt `build/autofilesync-<versionName>.apk` mit `.sha256` ab;
  `--install` installiert sie zusätzlich per adb. Die Version steht in `versionName` /
  `versionCode` in `app/build.gradle.kts`.
- `./gradlew assembleRelease` allein baut die signierte APK, ohne `keystore.properties` eine
  unsignierte (so baut F-Droid selbst).
- `tools/assetlinks.sh` gibt die `assetlinks.json` für
  `https://welsch-software.com/.well-known/assetlinks.json` aus; erst damit öffnet die
  Kamera-App den QR-Code direkt in der App.
- F-Droid-Metadaten: `fastlane/metadata/android/<Sprache>/`, Änderungen je Version in
  `changelogs/<versionCode>.txt`.
