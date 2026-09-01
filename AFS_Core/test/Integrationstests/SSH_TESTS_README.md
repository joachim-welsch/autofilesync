# SSH Tests für SshFileInspector

## Übersicht

Die SSH-Tests prüfen die Funktionalität der `SshFileInspector`-Klasse, die für die Abfrage von Datei-Informationen über SSH/SFTP zuständig ist.

## Test-Umgebung einrichten

### Voraussetzungen

1. **SSH-Server**: Ein laufender SSH-Server (z.B. OpenSSH)
2. **Testbenutzer**: Ein Benutzer mit SSH-Zugang
3. **libssh**: Die libssh-Bibliothek muss installiert sein

### Konfiguration

Die Sync-Tests für **Verzeichnisse und Bäume** (`SyncSshDirTest`, `SyncSshTreeTest`)
sind **aktiv**. Sie prüfen beim Start des Fixtures über `SSHTest::sshAvailable()`
(einmal pro Prozess), ob der Testserver erreichbar ist, und werden andernfalls
mit `GTEST_SKIP()` übersprungen statt zu scheitern. Wer den Testserver gemäß
dieser Anleitung einrichtet, bekommt sie ohne weiteres Zutun ausgeführt.

Die übrigen SSH-Tests (`SshFileInspectorTest`, `SyncSshFileTests`) sind weiterhin
**deaktiviert** (DISABLED_), da sie einen konfigurierten SSH-Server benötigen.

Um die Tests zu aktivieren:

1. **Öffnen Sie `SshTests.cpp`** und passen Sie die Konfiguration an:

```cpp
namespace SshTestConfig {
    const std::string SSH_HOST = "localhost";      // Ihr SSH-Server
    const std::string SSH_USER = "testuser";        // Ihr Testbenutzer
    const std::string SSH_PASSWORD = "testpassword"; // Ihr Passwort
    const std::string TEST_REMOTE_PATH = "/tmp/test_file.txt";
    const std::string TEST_REMOTE_DIR = "/tmp";
}
```

2. **Entfernen Sie das `DISABLED_` Präfix** von den Tests, die Sie ausführen möchten:

```cpp
// Vorher:
TEST_F(SshFileInspectorTest, DISABLED_ConstructorConnectsSuccessfully)

// Nachher:
TEST_F(SshFileInspectorTest, ConstructorConnectsSuccessfully)
```

### Lokalen SSH-Server für Tests einrichten (Linux)

```bash
# SSH-Server installieren (falls nicht vorhanden)
sudo apt-get install openssh-server

# SSH-Server starten
sudo systemctl start ssh

# Testbenutzer erstellen
sudo useradd -m -s /bin/bash testuser
echo "testuser:testpassword" | sudo chpasswd

# Testen
ssh testuser@localhost
```

## Verfügbare Tests

| Test | Beschreibung |
|------|-------------|
| `ConstructorConnectsSuccessfully` | Verbindungsaufbau mit korrekten Credentials |
| `ConstructorFailsWithWrongPassword` | Fehlschlag bei falschen Credentials |
| `ConstructorFailsWithInvalidHost` | Fehlschlag bei ungültigem Host |
| `InspectExistingFile` | Prüfung einer existierenden Datei |
| `InspectNonExistingFile` | Prüfung einer nicht existierenden Datei |
| `InspectMultipleFiles` | Mehrere Dateien mit derselben Session prüfen |
| `CheckFileSize` | Dateigröße korrekt ermitteln |
| `CheckTimestampFormat` | Zeitstempel-Format validieren |
| `InspectEmptyPath` | Verhalten bei leerem Pfad |
| `DestructorCleansUpProperly` | Ressourcen werden korrekt freigegeben |
| `TimestampConsistency` | Konsistenz zwischen raw_mtime und formatiertem Timestamp |
| `InspectDirectory` | Verhalten bei Verzeichnissen |
| `StressTestMultipleQueries` | Viele aufeinanderfolgende Abfragen |
| `InspectPathWithSpaces` | Pfade mit Leerzeichen |
| `InspectRelativePath` | Relative Pfade |

## Tests ausführen

### Alle Tests ausführen

```bash
cd ~/Entwicklung/autofilesync/build
./Integrationtests
```

### Nur SSH-Tests ausführen

```bash
./Integrationtests --gtest_filter=SshFileInspectorTest.*
```

### Einen spezifischen Test ausführen

```bash
./Integrationtests --gtest_filter=SshFileInspectorTest.InspectExistingFile
```

### Deaktivierte Tests auch ausführen

```bash
./Integrationtests --gtest_also_run_disabled_tests --gtest_filter=SshFileInspectorTest.*
```

## Troubleshooting

### "Verbindungsfehler: Connection refused"

- Prüfen Sie, ob der SSH-Server läuft: `sudo systemctl status ssh`
- Prüfen Sie die Firewall-Einstellungen
- Prüfen Sie den Hostnamen/IP in der Konfiguration

### "Authentifizierungsfehler"

- Prüfen Sie Benutzername und Passwort
- Prüfen Sie, ob Passwort-Authentifizierung erlaubt ist in `/etc/ssh/sshd_config`:
  ```
  PasswordAuthentication yes
  ```

### "cannot open source file" Fehler in IDE

Diese IntelliSense-Fehler können ignoriert werden, solange das Projekt erfolgreich kompiliert.

## Weitere Informationen

- **SshFileInspector.h**: Definition der Klasse und RemoteFileInfo-Struktur
- **SshFileInspector.cpp**: Implementation der SSH/SFTP-Funktionalität
- **libssh Dokumentation**: https://www.libssh.org/

## Sicherheitshinweis

⚠️ **Wichtig**: Diese Tests verwenden Passwort-Authentifizierung im Klartext. Verwenden Sie sie nur in sicheren Test-Umgebungen und niemals mit produktiven Credentials!

Für Produktiv-Systeme sollte SSH-Key-Authentifizierung verwendet werden.
