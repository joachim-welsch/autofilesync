# Konzept: AutoFileSync für Android

Stand: 21.10.2026 · Status: Entwurf

## 1. Ziel

Das Handy wird für AutoFileSync ein weiterer Ort, so wie ein USB-Datenträger
oder ein NAS über SSH. Fotos landen automatisch auf dem PC, Ordner bleiben auf
beiden Seiten gleich, Musik kommt vom PC aufs Handy.

Die Bedienung findet in der ersten Version **vollständig in der PC-Anwendung**
statt. Auf dem Handy wird nur einmal ein QR-Code gescannt und eine Berechtigung
erteilt; danach zeigt die App nur noch den Zustand an.

Leitlinien (aus CLAUDE.md, in dieser Reihenfolge):

1. Robustheit: Ein gelöschtes Foto auf dem Handy darf nie die Sicherung auf dem
   PC löschen, außer der Benutzer hat das ausdrücklich so eingestellt.
2. Intuitive Bedienung: Scannen, fertig. Keine IP-Adressen, Ports, Passwörter
   oder Schlüssel, die der Benutzer sehen oder abtippen muss.
3. Benutzerfreundlichkeit: Vorlagen für die häufigen Fälle statt leerer
   Formulare.
4. Geschwindigkeit.

## 2. Grundentscheidung: der PC synchronisiert, das Handy stellt bereit

| Variante | Vorteile | Nachteile |
|---|---|---|
| **A: Synchronisationskern nur auf dem PC, das Handy stellt seine Ordner über SFTP bereit** (empfohlen) | AFS_Core bleibt unverändert der einzige Kern: Zwei-Wege-Zustand, Konfliktkopien, Schutz vor Massenlöschung, Vorschau, Protokoll und Sicherungsnachweis gelten sofort auch fürs Handy. Die App bleibt klein (Kotlin). Die Konfiguration liegt an einer Stelle. | Es wird nur synchronisiert, wenn der PC an ist (für PC ↔ Handy ohnehin so). |
| B: AFS_Core per NDK auf dem Handy, das Handy synchronisiert selbst | Handy ↔ NAS auch ohne PC möglich | Zweite Plattform für den Kern, Speicherzugriff über Android-URIs statt Pfade, Konfiguration muss zwischen Geräten abgeglichen werden. Viel Aufwand für v1. |
| C: Das Handy als dauerhafter Server, der PC verbindet sich jederzeit | Einfachster Ablauf auf dem PC | Android beendet dauerhafte Hintergrunddienste. Seit Android 15 sind Vordergrunddienste vom Typ `dataSync` auf 6 Stunden pro Tag begrenzt. Nicht robust. |

**Entscheidung: Variante A, kombiniert mit „Das Handy meldet sich“.** Das Handy
hält keinen dauerhaften Server offen. Es meldet sich beim PC, wenn es etwas zu
tun gibt oder der Zeitplan es vorsieht. Nur für die Dauer eines Durchlaufs
öffnet es einen SFTP-Server, gegen den der PC mit dem vorhandenen SSH-Code
synchronisiert. Variante B bleibt als spätere Erweiterung möglich (Abschnitt 11).

## 3. Ablauf aus Sicht des Benutzers

### 3.1 Erstes Verbinden

```
PC (AutoFileSync)                          Handy
─────────────────                          ─────
Menü/Werkzeugleiste: „Handy verbinden…“
Dialog zeigt QR-Code und:
 „1. Installiere AutoFileSync auf dem Handy
     (der Code führt dorthin)
  2. Scanne den Code mit der App
     oder der Kamera“
                                           Kamera-App oder AutoFileSync-App scannt
                                           → App öffnet sich (App-Link)
                                           „Mit PC ‚Arbeitszimmer‘ verbinden?“ [Verbinden]
Dialog: „Pixel 8 verbunden ✓“              „Verbunden ✓“
                                           „AutoFileSync braucht Zugriff auf deine
                                            Dateien, damit der PC sie sichern kann.“
                                           [Zugriff erlauben] → Systemeinstellung
Dialog, Seite 2: „Was soll passieren?“
 ◉ Fotos und Videos auf dem PC sichern
 ○ Einen Ordner auf beiden Geräten gleich halten
 ○ Musik vom PC aufs Handy
 ○ Selbst einrichten
[Weiter] → Zielordner auf dem PC wählen
         → Vorschau (vorhandener PreviewDialog)
         → [Fertig]
                                           Benachrichtigung: „1.204 Fotos werden
                                           gesichert…“ → „Gesichert ✓“
```

Der QR-Code ist eine normale HTTPS-Adresse
(`https://welsch-software.com/pair#…`). Damit funktioniert jede Kamera-App:

- App installiert: Android öffnet über einen App-Link direkt die App.
- App nicht installiert: Die Webseite zeigt „AutoFileSync für Android
  installieren“ mit Links zu F-Droid und zur APK (Google Play erst später, siehe
  6.2). Nach der Installation
  scannt der Benutzer den Code erneut.
- Die Kopplungsdaten stehen hinter dem `#`. Dieser Teil wird nie an den
  Webserver übertragen.

### 3.2 Im Alltag

- Auf dem PC erscheint das Handy in der Ortsauswahl für Quelle und Ziel neben
  „Ordner“ und „SSH“ als eigener Eintrag mit Handysymbol und Gerätenamen. Ein
  Eintrag mit dem Handy ist ein ganz normaler Synchronisationseintrag: Er wird
  genauso bearbeitet, in der Vorschau angezeigt und protokolliert.
- Die Handy-App zeigt nur:
  - eine Statuskarte: „Verbunden mit Arbeitszimmer · zuletzt vor 4 Min. ·
    alles aktuell“
  - die Liste der Einträge, ohne Bearbeitung, mit Richtungspfeil
  - die Schaltfläche **Jetzt synchronisieren**
  - den Hinweis „Einstellungen änderst du in AutoFileSync auf dem PC.“
  - zwei Schalter: „Nur im WLAN“ (an) und „Nur beim Laden“ (aus)
- Fehler stehen in verständlichen Worten in der App, zum Beispiel „PC nicht
  erreichbar: Ist er eingeschaltet und im selben WLAN?“

### 3.3 Ordner auf dem Handy auswählen

Solange die App im Vordergrund ist, läuft ihr SFTP-Server. Der PC kann dann mit
dem vorhandenen `SshDirDialog` in den Ordnern des Handys blättern. Ist die App
nicht offen, zeigt der PC: „Öffne AutoFileSync auf dem Handy, um seine Ordner
zu sehen.“ Für die Vorlagen ist das nicht nötig: Ihre Ordner (`DCIM/Camera`,
`Pictures/Screenshots`, `Music`, `Download`) sind bekannt.

## 4. Kopplung (Pairing)

### 4.1 Inhalt des QR-Codes

```
https://welsch-software.com/pair#v=1&n=Arbeitszimmer&a=192.168.178.20,fd00::20&p=52381&f=<SHA-256 des PC-Zertifikats>&t=<Einmal-Token, 128 Bit>&e=<Ablaufzeit>
```

- `a`/`p`: Adressen und Port des Kopplungsdienstes auf dem PC
- `f`: Fingerabdruck des selbst signierten TLS-Zertifikats des PCs. Das Handy
  vertraut genau diesem Zertifikat. Damit gibt es kein „Trust on first use“
  und keinen Angriffspunkt für einen Mann in der Mitte.
- `t`: Einmal-Token, 10 Minuten gültig, nach Gebrauch verbraucht. Er beweist
  dem PC, dass das Handy den Bildschirm gesehen hat.

### 4.2 Ablauf

1. Das Handy baut eine TLS-1.3-Verbindung zum PC auf und prüft dabei den
   Fingerabdruck `f`.
2. Das Handy sendet den Token, seinen Gerätenamen, sein Client-Zertifikat und
   den öffentlichen Schlüssel seines SFTP-Servers (Ed25519, beim ersten
   App-Start erzeugt).
3. Der PC prüft den Token und legt einen **Geräteeintrag** an: Geräte-ID, Name,
   Fingerabdruck des Client-Zertifikats und SFTP-Hostschlüssel. Danach meldet
   er „gekoppelt“.
4. Alle späteren Verbindungen verwenden gegenseitiges TLS mit den gespeicherten
   Zertifikaten. Ein Token wird nicht mehr gebraucht.

Entkoppeln geht von beiden Seiten: Der PC löscht den Geräteeintrag und lehnt
das Zertifikat danach ab; das Handy löscht die PC-Daten. Die
Synchronisationseinträge, die das Gerät nutzen, bleiben erhalten und melden
„Gerät nicht gekoppelt“. So geht keine Konfiguration verloren.

## 5. Protokoll zwischen Handy und PC

Zwei Kanäle mit getrennten Aufgaben:

**Steuerkanal** (TLS 1.3 mit gegenseitiger Prüfung, Port 52381, JSON-Zeilen):

| Richtung | Nachricht | Zweck |
|---|---|---|
| Handy → PC | `hello {device, app_version, addresses, sftp_port, sftp_password, reason}` | Das Handy meldet sich. `reason`: Zeitplan, Änderung, Benutzer oder WLAN verbunden. |
| PC → Handy | `items [...]` | Name und Richtung der Einträge des Geräts für die Anzeige in der App |
| PC → Handy | `serve` oder `idle` | Ob ein Durchlauf ansteht. Läuft gerade eine Synchronisation in der GUI, antwortet der PC mit `idle` und dem Grund (SyncArbiter). |
| PC → Handy | `progress {file, done, total}` | Fortschritt für die Benachrichtigung |
| PC → Handy | `done {copied, errors, first_error, changed_paths}` | Ergebnis. `changed_paths` braucht das Handy für den Medienscan (6.4). |

**Datenkanal** (SFTP): Für einen Durchlauf öffnet das Handy seinen SFTP-Server.
Der PC verbindet sich mit dem vorhandenen `SftpConnection`/`SyncFileSsh`. Die
Anmeldung erfolgt mit einem **Einmalpasswort**, das das Handy im `hello` über
den gesicherten Steuerkanal schickt. Dafür reicht die vorhandene
Passwort-Anmeldung in `SshAccount`, eine Schlüsselanmeldung ist für v1 nicht
nötig. Den Hostschlüssel prüft der PC gegen den bei der Kopplung gespeicherten
Schlüssel, nicht gegen `known_hosts`. Grund: Die IP-Adresse des Handys ändert
sich, und `known_hosts` ordnet Schlüssel Adressen zu.

Warum SFTP statt eines eigenen Übertragungsprotokolls: Der Kern kennt genau zwei
Ortsarten (`SyncPathType::Filesystem`, `Ssh`), und alle Sonderfälle von SSH
sind schon gelöst: kurze Schreibvorgänge, abgebrochene Sitzungen, Zeitstempel
übernehmen, Ordner anlegen und löschen. Ein Handy ist intern ein SSH-Ort, dessen
Adresse, Port und Passwort bei jedem `hello` aktualisiert werden.

**Adresse finden:** Der QR-Code enthält die Adressen beim Koppeln. Ändern sie
sich später (DHCP), findet das Handy den PC über mDNS/DNS-SD
(`_autofilesync._tcp`, TXT-Eintrag mit Fingerabdruck) mit Androids
`NsdManager`. Gesucht wird nach Fingerabdruck, nicht nach Name.

## 6. Android-App

### 6.1 Technik

- Kotlin, Jetpack Compose, minSdk 26 (Android 8), targetSdk aktuell
- QR-Scan: CameraX und ZXing (Apache 2.0), ohne Google Play Services, damit
  die App auf F-Droid veröffentlicht werden kann
- SFTP-Server: Apache MINA SSHD (Apache 2.0), auf Android verbreitet
- TLS: Plattform-TLS mit eigenem TrustManager, der nur den gespeicherten
  Fingerabdruck akzeptiert
- Lizenz: GPLv3 wie AFS_Core. Apache 2.0 ist mit GPLv3 vereinbar.
- Sprachen: dieselben 7 wie die PC-Anwendung

### 6.2 Speicherzugriff

AFS_Core und SFTP arbeiten mit Pfaden. Deshalb nutzt v1 die Berechtigung
**„Zugriff auf alle Dateien“** (`MANAGE_EXTERNAL_STORAGE`). Der SFTP-Server
bekommt `/storage/emulated/0` als Wurzel.

- **Entschieden:** v1 erscheint nur über F-Droid und als APK zum Herunterladen
  (welsch-software.com). Dort gibt es für diese Berechtigung keine
  Einschränkung, und die Entwicklung hängt nicht von einer Play-Prüfung ab.
- Google Play folgt später. Dort muss die Berechtigung begründet werden.
  Synchronisations- und Sicherungs-Apps (z. B. Syncthing-Fork, FolderSync)
  bekommen sie, eine Ablehnung ist aber möglich (Risiko R2).
- Ausweichlösung, falls Play ablehnt: Der SFTP-Server bildet nur die Ordner ab,
  die der Benutzer über das Storage Access Framework freigegeben hat (virtuelles
  Dateisystem über `DocumentFile`). Mehr Aufwand, und bei SAF sind Zeitstempel
  nur eingeschränkt setzbar.

### 6.3 Hintergrund

| Auslöser | Umsetzung |
|---|---|
| Zeitplan | `WorkManager`, periodisch (mindestens 15 Min., Standard 30 Min.), Bedingung: WLAN ohne Volumenbegrenzung (Schalter „Nur im WLAN“), optional „Nur beim Laden“ |
| Neues Foto | `WorkManager` mit Content-URI-Trigger auf `MediaStore` (Images/Video), 1 Min. Wartezeit zum Bündeln |
| WLAN verbunden | Netzwerkbedingung des Workers |
| Benutzer | Schaltfläche **Jetzt synchronisieren** (expedited work) |

Ein Durchlauf läuft in einem `CoroutineWorker`. Dieser wird über
`setForeground()` zum Vordergrunddienst vom Typ `dataSync`, mit
Fortschrittsbenachrichtigung. Er öffnet den SFTP-Server, sendet `hello`, wartet
auf `done` und schließt den Server wieder. Die Grenze von 6 Stunden pro Tag für
`dataSync` (Android 15) reicht dafür. Beim Aufräumen nach einer langen ersten
Sicherung meldet die App das verständlich und setzt beim nächsten Mal fort.

Änderungen auf dem PC kann das Handy erst sehen, wenn es sich meldet (ohne
Push-Dienst). Der PC kann das Handy nicht wecken. Das ist für v1 bewusst so:
spätestens nach einem Zeitplanintervall, sofort beim Öffnen der App.

### 6.4 Android-Besonderheiten

- **Medienscan:** Dateien, die der PC aufs Handy kopiert (Musik), sieht die
  Galerie- oder Musik-App erst nach `MediaScannerConnection.scanFile()`. Die App
  scannt die `changed_paths` aus `done`.
- **Zeitstempel:** Der PC setzt die Änderungszeit über SFTP-`setstat`. Unter
  Android 13 funktioniert das im gemeinsamen Speicher sekundengenau, in allen
  geprüften Ordnern, für Dateien und Ordner, auch nach dem Umbenennen
  (Abschnitt 13). Android 14/15 ist noch offen (R1).
- **Berechtigungen:** Der gemeinsame Speicher kennt keine POSIX-Rechte. `chmod`
  meldet Erfolg, ändert aber nichts. MINA meldet unverändert 0660 (gemessen).
  Der Server der App muss deshalb feste Werte melden (Dateien 0644, Ordner
  0755), sonst übernimmt der PC 0660 für jede Datei vom Handy
  (`copy-from-ssh-keeps-the-permissions`).
- **Akku:** Kein Durchlauf unter 15 % Akku ohne Ladegerät. Ein laufender
  Durchlauf wird sauber beendet; der Kern behandelt den Abbruch wie eine
  abgebrochene Sitzung.

## 7. Änderungen an der PC-Anwendung

### 7.1 AFS_Core (neue Module, jeweils mit `doc/*.md`)

| Modul | Aufgabe |
|---|---|
| `Device` | Geräteeintrag: ID, Name, Zertifikats-Fingerabdruck, SFTP-Hostschlüssel, zuletzt bekannte Adresse, Port, Einmalpasswort (nur im Speicher) |
| `DeviceRegistry` | Gekoppelte Geräte in der Konfiguration (`devices` in `autofilesync.json`, Lesen mit Standardwerten wie bei anderen neuen Schlüsseln) |
| `DevicePairing` | Token erzeugen und prüfen, QR-Inhalt bauen |
| `DeviceLink` | TLS-Steuerkanal: `hello` annehmen, Durchlauf für die Einträge des Geräts anstoßen, `progress`/`done` senden. Nutzt OpenSSL (schon vorhanden für `PasswordCrypto`/`SmtpMailer`). |
| `DeviceAnnounce` | mDNS-Ankündigung (Avahi/Bonjour/Windows DNS-SD) |

Anpassungen an bestehendem Code:

- `SyncPathType` bekommt **keinen** neuen Wert. Ein Handy-Ort ist ein SSH-Ort,
  dessen `SshAccount` auf ein Gerät verweist (`device_id`). Konfiguration und
  Oberfläche zeigen „Handy: Pixel 8“ statt Benutzer@Host.
- `SshAccount` bekommt einen **Port** (heute fest 22). Das Handy kann ohne Root
  nicht auf 22 lauschen.
- `SshHostKeyVerification`: Prüfung gegen einen erwarteten Schlüssel aus dem
  Geräteeintrag, als Alternative zu `known_hosts`.
- `Synchronize`/`SyncControler`: ein Durchlauf, der nur die Einträge eines
  Geräts umfasst und von außen (`hello`) angestoßen wird. Einträge mit Handy
  werden vom normalen Zeittakt übersprungen: Ist das Handy nicht da, ist das der
  Normalfall und keine Warnung (vgl. `warn_if_unreachable_`).
- **Mehrere SFTP-Anfragen gleichzeitig** in `SyncFileSsh` (Voraussetzung für
  Phase 2, siehe Abschnitt 13): libssh mit `sftp_aio_begin_read/write` (ab
  0.11), libssh2 mit größeren Puffern pro Aufruf, damit es selbst mehrere
  Anfragen unterwegs hält. Davon profitieren auch NAS und Server über SSH.
- Schreiben aufs Handy über temporären Namen plus Umbenennen (prüfen, ob
  `SyncFileSsh` das heute schon tut, siehe `remote-target-stays-writable`). Eine
  abgebrochene Übertragung darf auf dem Handy keine halbe Datei unter dem
  richtigen Namen hinterlassen.
- QR-Erzeugung: `qrcodegen` von Nayuki (MIT, eine Datei) unter `third-party/`

### 7.2 Wer lauscht?

Genau ein Prozess betreibt den Kopplungsdienst und den Steuerkanal:
bevorzugt `backgroundsync`, sonst die GUI, solange sie läuft. Die Absprache
übernimmt eine Sperre nach dem Muster von `SyncArbiter`. Läuft nur die GUI,
zeigt der Kopplungsdialog: „Damit das Handy auch synchronisiert, wenn
AutoFileSync geschlossen ist, richte den Hintergrunddienst ein.“ Daneben steht
eine Schaltfläche, die das direkt erledigt (`BackgroundService`).

Firewall: Der Windows-Installer legt eine Regel für das Programm an (nicht für
den Port, siehe Abschnitt 12). Unter Linux
zeigt der Kopplungsdialog einen Hinweis, wenn innerhalb von 10 Minuten keine
Verbindung ankommt („Eine Firewall blockiert vielleicht Port 52381“).

### 7.3 AFS_Desktop

- **Dialog „Handy verbinden…“** (Menü, Werkzeugleiste, Assistent beim ersten
  Start): QR-Code groß, darunter die zwei Schritte, Live-Status (wartet →
  verbunden), danach Seite mit den Vorlagen.
- **Vorlagen** erzeugen normale Synchronisationseinträge mit sicheren
  Voreinstellungen:

  | Vorlage | Richtung | Wichtige Einstellungen |
  |---|---|---|
  | Fotos und Videos sichern | Handy → PC | `DCIM/Camera`, `Pictures/Screenshots`; `delete_in_target` aus (ein auf dem Handy gelöschtes Foto bleibt auf dem PC); Ziel `Bilder/<Gerätename>` |
  | Ordner gleich halten | beidseitig | `two_way_sync`, `conflict_copies`, `track_moves`, Papierkorb |
  | Musik aufs Handy | PC → Handy | `Music`; Löschen im Ziel an, damit das Handy nicht volläuft |

- Ortsauswahl in `SyncItemDialog`: dritte Möglichkeit „Handy“ mit Liste der
  gekoppelten Geräte
- Baum und Ansicht: Handysymbol und Status „zuletzt gesehen vor …“
- Einstellungen: Liste der gekoppelten Geräte mit „Umbenennen“ und „Entkoppeln“
- Hilfe und Änderungsprotokoll, Übersetzungen (po-Einträge von Hand, siehe
  bekannte Fallstricke)

## 8. Sicherheit

- Kein offener Dienst ohne Authentifizierung: Der Steuerkanal nimmt außer
  während eines laufenden Kopplungsdialogs nur bekannte Client-Zertifikate an.
  Ohne offenen Dialog lehnt der PC jede Kopplung ab.
- Der Token ist einmalig, läuft nach 10 Minuten ab, und nach 5 Fehlversuchen
  wird der Dialog geschlossen.
- Der SFTP-Server des Handys läuft nur während eines Durchlaufs oder solange
  die App offen ist. Er nimmt nur das Einmalpasswort an, und nur von der Adresse
  des PCs.
- Gespeicherte Geheimnisse:
  - PC: über `PasswordCrypto`
  - Handy: Android Keystore
- Nur im lokalen Netz: v1 bietet keinen Zugriff aus dem Internet und keinen
  Relay-Server.

## 9. Robustheit: Sonderfälle

| Fall | Verhalten |
|---|---|
| Handy während des Durchlaufs aus dem WLAN | Die Sitzung bricht ab. Der Kern meldet den Fehler, `forget_unseen` wird nicht aufgerufen (unvollständiger Durchlauf). Der nächste Durchlauf setzt fort. |
| Handy-Speicher voll | Fehler „Auf dem Handy ist kein Platz mehr“ in App und PC-Protokoll. Der Durchlauf bricht ab. |
| Benutzer löscht 2.000 Fotos auf dem Handy | Vorlage „Fotos sichern“: Auf dem PC passiert nichts. Bei einem Zwei-Wege-Eintrag greift der vorhandene Schutz vor Massenlöschung. |
| GUI synchronisiert gerade | Der PC antwortet `idle` (SyncArbiter). Das Handy versucht es nach 2 Minuten erneut. |
| PC aus | `hello` schlägt fehl. Die App zeigt „PC nicht erreichbar“, aber erst nach 24 Stunden ohne Erfolg als Benachrichtigung, damit der Benutzer nicht ständig gestört wird. |
| Zwei Handys | Jedes Gerät hat eigene Einträge und eigenen Zustand. Die Durchläufe laufen nacheinander. |
| Handy zurückgesetzt oder neu | Neu koppeln. Der PC bietet an, die Einträge des alten Geräts zu übernehmen. |

## 10. Tests

- **Integrationstests (AFS_Core):** Kopplung, Steuerkanal und Durchlauf gegen
  ein simuliertes Handy in C++. Dazu ein TLS-Client und als SFTP-Server der
  vorhandene lokale SSH-Server mit Passwortanmeldung, mit Ports und
  Verzeichnissen im Temp-Verzeichnis, die nach dem Test entfernt werden.
  Abgedeckt: Token abgelaufen, schon verbraucht oder falsch; fremdes Zertifikat;
  geänderter Hostschlüssel; Abbruch mitten im Durchlauf.
- **GUI-Tests:** Dialog „Handy verbinden“, Vorlagen erzeugen erwartete
  Einträge. Konfiguration hinterher wie vorher (Geräteeintrag und Einträge
  entfernen).
- **Android:** Unit-Tests für Protokoll und Worker; instrumentierte Tests auf
  Emulator (API 26, 30, 34, 35) für SFTP-Server, Zeitstempel und Medienscan.
- **Ende-zu-Ende:** echtes Gerät gegen den PC, manuell nach Checkliste, später
  in der Demo-Aufnahme (`afs_demo`) als eigene Szene

## 11. Phasen

| Phase | Inhalt | Ergebnis |
|---|---|---|
| 0 – Spikes ✓ | MINA-SFTP mit „Alle Dateien“; `setstat`/mtime im gemeinsamen Speicher; Durchsatz im WLAN; libssh (Linux) und libssh2 (Windows) gegen MINA. Erledigt am 21.10.2026 auf Android 13, Android 14/15 steht noch aus. | Ergebnisse in Abschnitt 13, Testprojekt `AFS_Android/spike/` |
| 1 – Kopplung ✓ | `DevicePairing`, Geräteliste in `Config`, `DeviceLink` (`pair`, `hello`), QR-Dialog, Handy-Liste; App: Scannen, Koppeln, Statuskarte. Umgesetzt am 21.10.2026, Abweichungen in Abschnitt 14. | Handy erscheint in AutoFileSync als gekoppelt |
| 2 – Synchronisieren ✓ | parallele SFTP-Anfragen im Kern, `serve`/`done`, Port und Hostschlüssel in SSH, Ortsauswahl „Handy“, Vorlagen. Umgesetzt am 21.10.2026, Abweichungen in Abschnitt 15. | Fotos sichern per **Jetzt synchronisieren** |
| 3 – Hintergrund | WorkManager-Auslöser, Vordergrundbenachrichtigung, mDNS, Medienscan, Daemon als Kopplungsdienst | Läuft ohne Zutun |
| 4 – Fertigstellen | 7 Sprachen, Hilfe, Tests, F-Droid-Metadaten, signierte APK zum Herunterladen, App-Link (`assetlinks.json` auf welsch-software.com) | Veröffentlichung über F-Droid und APK |

Spätere Erweiterungen, nicht in v1:

- Bearbeiten von Einträgen in der App
- mehrere PCs pro Handy (das Datenmodell sieht das schon vor)
- AFS_Core per NDK auf dem Handy für Handy ↔ NAS ohne PC (Variante B)
- Zugriff von unterwegs über Relay
- „Platz schaffen“: gesicherte Fotos auf dem Handy löschen
- Google Play (Begründung für `MANAGE_EXTERNAL_STORAGE`, notfalls SAF-Variante)

## 12. Risiken und offene Fragen

- **R1 Zeitstempel:** Unter Android 13 geklärt (funktioniert). Android 14/15
  sowie andere Hersteller sind noch zu prüfen, sobald ein Gerät verfügbar ist
  (Emulator-Abbild reicht; die Test-App läuft unverändert). Falls dort nicht:
  für Handy-Orte nach Größe plus Zustand vergleichen.
- **R2 Google Play:** Wird `MANAGE_EXTERNAL_STORAGE` genehmigt? Betrifft v1
  nicht mehr (nur F-Droid und APK). Falls Play später ablehnt:
  SAF-Ausweichlösung (6.2) nur für die Play-Version.
- **R3 Windows:** Funktional geklärt: libssh2 beherrscht alle Aufrufe des
  Kerns gegen MINA (unter Linux geprüft, dieselbe Bibliothek). Umbenennen auf
  eine vorhandene Datei lehnt MINA bei libssh2 ab, das nutzt der Kern aber nie
  (SyncFileSsh.md#ssh_rename). Offen bleibt nur die Geschwindigkeit
  (Abschnitt 13).
- **R5 Durchsatz:** Ohne parallele Anfragen schafft der Kern über WLAN nur etwa
  1 MiB/s. Die erste Sicherung von 4 GB Fotos würde über eine Stunde dauern.
  Gegenmaßnahme siehe 7.1, gemessen 5-fach schneller.
- **R4 Netze mit Client-Isolation** (Gäste-WLAN, manche Mesh-Systeme):
  PC und Handy sehen sich nicht. Die App erkennt das (mDNS ohne Antwort, TCP
  abgewiesen) und sagt es in einem Satz.
- **Entschieden:** Port 52381 fest, mit automatischem Ausweichen auf einen
  freien Port, wenn 52381 belegt ist. Der tatsächliche Port wird im
  Geräteeintrag gemerkt und über den QR-Code und mDNS weitergegeben. Die
  Windows-Firewallregel gilt für das Programm, nicht für den Port, damit sie
  auch beim Ausweichen greift.
- **Entschieden:** v1 nur über F-Droid und APK (6.2).

## 13. Ergebnisse Phase 0 (21.10.2026)

Testgerät: OnePlus DN2103, Android 13 (API 33). Gegenseite: Linux-PC im selben
WLAN, libssh 0.11.3 und libssh2 1.11.1. Server: Apache MINA SSHD 2.14.0 in der
Test-App `AFS_Android/spike/`, Wurzel `/storage/emulated/0`, Berechtigung
„Zugriff auf alle Dateien“. Das Prüfprogramm `AFS_Android/spike/pc-probe/`
führt dieselbe Aufruffolge aus wie `SyncFileSsh`: 64-KiB-Blöcke, `setstat` für
Zeit und Rechte, `rename`, `readdir`, `mkdir`, `rmdir`, `unlink`.

**Funktion:** Beide Bibliotheken bestehen alle Aufrufe des Kerns. Der Inhalt
nach Hin- und Rückweg ist byte-gleich. Die mtime ist über SFTP sekundengenau
setzbar und bleibt nach `chmod` erhalten. Direkt auf dem Handy gelingt
`setLastModified` in `/`, `DCIM`, `Download`, `Music` und `Pictures`, auch für
Ordner und nach dem Umbenennen.

**Rechte:** `chmod` meldet Erfolg und ändert nichts. Gemeldet wird immer 0660
(siehe 6.4).

**Durchsatz (32 MiB, MiB/s):**

| Variante | USB (adb forward) hoch / runter | WLAN hoch / runter |
|---|---|---|
| libssh wie im Kern (ein Block nach dem anderen) | 1,2 / 0,9 | 0,8–1,5 / 0,9 |
| libssh mit 16 Anfragen gleichzeitig (`sftp_aio_*`) | – | 3,4–5,2 / 7,2–8,0 |
| libssh2 wie im Kern | 8,3 / 13,9 | 1,1 / 3,2–3,5 |

Die Antwortzeit des Handys im WLAN schwankt stark (Ping 3–110 ms, im Mittel
18–55 ms). Bei einem Block pro Hin- und Rückweg begrenzt das den Kern auf etwa
1 MiB/s. Mehrere Anfragen gleichzeitig sind deshalb Voraussetzung für Phase 2
(7.1). Eine WLAN-Sperre (`WIFI_MODE_FULL_LOW_LATENCY`) hat die Werte nicht
messbar verbessert. Sie kann trotzdem während eines Durchlaufs gehalten werden,
damit das Handy das WLAN nicht abschaltet.

**Sonstiges:**
- MINA läuft auf Android ohne Anpassungen. Einzige Ausnahme ist das
  Home-Verzeichnis (`PathUtils.setUserHomeFolderResolver`).
- Der Standard-Hostschlüssel ist ECDSA P-521. Die App soll Ed25519 erzeugen.
- OnePlus (ColorOS) erlaubt kein Setzen von App-Ops über adb. Die Freigabe
  „Alle Dateien“ muss auch für Tests von Hand erteilt werden.

## 14. Stand nach Phase 1 (21.10.2026)

Umgesetzt wie in Abschnitt 4 und 5 beschrieben, mit diesen Abweichungen:

- **Geräteliste** liegt in `Config` (`Devices` in `autofilesync.json`) statt in einem
  eigenen Modul `DeviceRegistry`, nach dem Muster der SSH-Konten.
- **Adresse, Port und Zeitpunkt der letzten Meldung** eines Handys werden nicht
  gespeichert, sondern nur im Speicher des laufenden Programms gehalten
  (`DevicePresence`): Eine Konfiguration, die sich bei jedem `hello` ändert,
  würde beim Beenden nach dem Speichern fragen.
- **Nur IPv4** im QR-Code und beim Lauschen. IPv6 im Heimnetz braucht eine Scope-ID,
  die das Handy nicht kennt.
- **Hostschlüssel des Handys** ist ECDSA P-256 statt Ed25519: Java auf Android 13 hat
  kein Ed25519 ohne Zusatzbibliothek, und libssh/libssh2 akzeptieren P-256.
- **Wer lauscht:** nur das GUI-Programm. Es öffnet den Port beim Start, wenn ein Handy
  gekoppelt ist, und im Dialog „Handy verbinden“. Ohne gekoppeltes Handy ist kein
  Port offen. Der Hintergrunddienst kommt in Phase 3; die Rolle ist bereits über eine
  Sperrdatei vergeben (`DeviceLink.md#role`).
- **App-Link:** Der QR-Code öffnet die App über die Kamera-App erst, wenn
  `https://welsch-software.com/.well-known/assetlinks.json` den Signaturschlüssel der
  App nennt (Phase 4). Bis dahin scannt die App selbst.
- **Sprachen der App:** Englisch und Deutsch; die übrigen fünf in Phase 4.
- **Tests:** 16 Integrationstests (`DeviceTests.cpp`, mit dem simulierten Handy
  `FakePhone`) und 3 GUI-Tests (`PairDeviceTest.cpp`).
- **Ende-zu-Ende geprüft** am 21.10.2026 mit dem OnePlus DN2103 gegen den PC-Teil
  ohne Oberfläche: Kopplung und `hello` kommen an, die Statuskarte zeigt „Verbunden
  mit Laptop · Alles aktuell“. Zwei Erkenntnisse daraus:
  - Der Schlüssel im Android-Keystore braucht `DIGEST_NONE` neben SHA-256, sonst
    kann Conscrypt den TLS-Handshake nicht signieren (Fehlerbild: „Operation not
    supported on transport endpoint“). Der TLS-Socket muss direkt von der
    `SSLSocketFactory` kommen, nicht über einen bestehenden Socket gelegt werden.
  - Unter Linux blockierte `ufw` den Port 52381; `ufw allow 52381/tcp` behebt es.
    Der Dialog zeigt den Firewall-Hinweis nach einer Minute (7.2). Für Phase 4 ist zu
    prüfen, ob das Debian-Paket eine ufw-Anwendungsdefinition
    (`/etc/ufw/applications.d/autofilesync`) mitliefern soll.

## 15. Stand nach Phase 2 (21.10.2026)

Umgesetzt wie in Abschnitt 5 und 7 beschrieben, mit diesen Abweichungen:

- **Handy als SSH-Konto:** Ein Handy ist für den Kern ein SSH-Konto mit der Geräte-ID als
  Name (`Config.md#device-accounts`); `SyncPathType` blieb unverändert. Host, Port,
  Benutzer und Einmalpasswort setzt der Steuerkanal nur für die Dauer des Durchlaufs.
  Die Oberfläche zeigt statt der ID den Namen mit Handy-Symbol (`PhoneDirDialog.md#display_path`).
- **Durchlauf:** `hello` mit `serving` und SFTP-Zugangsdaten → `items`, `serve`,
  `progress` je Eintrag, `done` mit Fehlerzahl. Läuft eine fortlaufende Synchronisation
  im GUI, übernimmt sie die Handy-Einträge beim nächsten Takt; sonst legt der Steuerkanal
  einen Einzel-Durchlauf nur für dieses Handy an (`DeviceLink.md#serve`,
  `Synchronize.md#device-items`). Handy-Einträge laufen ausschließlich, während das Handy
  seinen Server anbietet; ein abwesendes Handy ist kein Fehler.
- **Kein Ordner-Browsing auf dem Handy:** Der Dialog „Ordner auf dem Handy“ bietet die
  üblichen Android-Ordner an oder nimmt einen Pfad entgegen. Live-Browsing bräuchte den
  Server, der nur während eines Durchlaufs läuft.
- **Parallele SFTP-Anfragen** nur mit libssh (Linux, macOS; `sftp_aio_*`, libssh ≥ 0.11).
  libssh2 unter Windows sendet weiter eine Anfrage nach der anderen.
- **Atomares Schreiben** auf dem Handy (temporärer Name + Umbenennen) ist nicht umgesetzt;
  eine abgebrochene Übertragung hinterlässt eine unvollständige Datei, die der nächste
  Durchlauf ersetzt. Ebenso offen: Medienscan nach dem Durchlauf (`changedPaths`), feste
  Rechte 0644/0755 im Server der App, Port-Feld im SSH-Konten-Dialog.
- **App:** Der SFTP-Server (Apache MINA SSHD) läuft nur während eines Durchlaufs, mit
  neuem Passwort je Durchlauf. Der Durchlauf läuft im Vordergrund der App; Hintergrund,
  Zeitplan und Benachrichtigungen kommen in Phase 3. Keystore-Schlüssel brauchen
  `DIGEST_NONE` (Abschnitt 14).
- **Tests:** 20 Gerätetests (darunter der `serve`-Durchlauf gegen den lokalen SSH-Server
  und ein abgewiesener falscher Hostschlüssel), 99 SSH-Tests mit den parallelen Anfragen,
  5 GUI-Tests (Kopplung, Handy-Liste, Vorlagen).
