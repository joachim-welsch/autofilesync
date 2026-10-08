# Integrationtests – Visual Studio 2022

Zwei Wege, beide aus der mitgelieferten `CMakeLists.txt` abgeleitet.

## Variante A – natives MSBuild-Projekt (empfohlen, wenn CMake unter Windows wegfallen soll)

Dateien: `Integrationtests.sln`, `Integrationtests.vcxproj`, `Integrationtests.vcxproj.filters`, `vcpkg.json`

Alle vier Dateien in das Verzeichnis des Testprojekts legen (dort, wo `SyncFileTests.cpp` usw. liegen).

### Abhängigkeiten

```cmd
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
%USERPROFILE%\vcpkg\vcpkg integrate install
```

Danach löst VS das `vcpkg.json` beim Build automatisch auf (gtest, libssh2, openssl) und kopiert
die DLLs ins Ausgabeverzeichnis.

### Pfade

`$ENV{HOME}` gibt es unter Windows nicht. Das Projekt nutzt stattdessen:

| CMake | vcxproj |
|---|---|
| `HOME_DIR $ENV{HOME}` | `$(HomeDir)`, Default `$(USERPROFILE)` |
| `LIB_DIR` | `$(LibDir)`, Default `<Repo>\AFS_Core` (aus der Projektlage abgeleitet) |

Abweichende Ablage: entweder die beiden Properties oben im `.vcxproj` anpassen, als
Umgebungsvariablen setzen, oder `msbuild /p:LibDir=D:\src\autofilesync\AFS_Core` übergeben.

### Übersetzte Einstellungen

| CMake | MSVC |
|---|---|
| `CMAKE_CXX_STANDARD 20` | `/std:c++20` (`stdcpp20`) + `/permissive-` |
| `-g -O0 -Wall -Wextra` (Debug) | `/Od /Zi /W4 /RTC1 /MDd` |
| `-O3` (Release) | `/O2 /Ob3 /Oi /GL-frei, /MD` |
| `-D_UNIX` | **entfällt**, dafür `_WINDOWS`, `WIN32` |
| `-D_TEST` | `_TEST` |
| `LIB_SOURCE_DIR="${LIB_DIR}"` | `LIB_SOURCE_DIR="$(LibDirPosix)"` (Vorwärtsslashes, damit das String-Literal nicht an Escapes scheitert) |
| `ssh`, `OpenSSL::Crypto` | `libssh2.lib`, `libcrypto.lib` + `ws2_32/crypt32/bcrypt` |
| `add_test(...)` | Test Explorer über die Google-Test-Adapter-Integration von VS |

### Punkte, die beim ersten Build vermutlich anschlagen

1. **`_UNIX`-Zweige im Code.** Die Quellen enthalten `MountedDrives.cpp`,
   also existiert vermutlich schon ein Windows-Pfad. Falls der an einem anderen Makro hängt
   (`_WIN32`, `_MSC_VER`, `_WINDOWS`), das entsprechende Define in den `PreprocessorDefinitions`
   ergänzen.
2. **`pch.cpp`.** Die CMakeLists kompiliert die Datei nur mit, ohne echtes PCH-Setup – im
   `.vcxproj` ist `PrecompiledHeader` deshalb auf `NotUsing`. Wenn du echtes PCH willst:
   `pch.cpp` auf *Create* (`/Yc"pch.h"`), alle übrigen auf *Use* (`/Yu"pch.h"`), und die
   Fremdquellen aus `$(LibDir)` auf *NotUsing* lassen.
3. **`/W4` ist schärfer als `-Wall -Wextra`.** Für den ersten Durchlauf sind `4100` und `4996`
   bereits unterdrückt; ggf. weitere ergänzen statt den Warnlevel zu senken.
4. **libssh2-Crypto-Backend.** Der vcpkg-Port hat die Default-Features `openssl` und `zlib`,
   also genügt ein schlichtes `"libssh2"` im Manifest. Ein Feature namens `crypto-openssl`
   existiert dort nicht (das ist die Schreibweise des `libssh`-Ports) und lässt die
   Manifest-Auflösung mit Exit-Code 1 abbrechen. Falls du es explizit machen willst:
   `{ "name": "libssh2", "features": [ "openssl" ] }`.
5. **x86 ist nicht konfiguriert.** Nur `x64` – bei Bedarf die beiden `ProjectConfiguration`-
   Blöcke duplizieren.

## Variante B – CMake direkt in VS 2022

Dateien: `CMakeLists-crossplatform.txt` (in `CMakeLists.txt` umbenennen), `CMakePresets.json`

VS 2022 öffnet CMake-Ordner nativ. Vorteil: eine Buildbeschreibung für Linux und Windows,
kein Drift zwischen zwei Projektdateien.

```cmd
set VCPKG_ROOT=%USERPROFILE%\vcpkg
```

Dann in VS „Ordner öffnen“ → Preset *VS 2022 x64 Debug*. Die Datei hält den Linux-Build
unverändert (`-D_UNIX`, `-Wall -Wextra`, `-O3`) und schaltet unter MSVC auf `/W4 /permissive- /MP`.

Wenn du langfristig auf beiden Plattformen entwickelst, ist B die wartungsärmere Wahl;
A ist sinnvoll, wenn das Windows-Setup dauerhaft eigenständig bleiben soll.
