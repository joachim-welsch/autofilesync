# Änderungen

## 0.4.0 – 2026-10-22

- Handys lassen sich mit AutoFileSync verbinden: „Datei → Handy verbinden…“
  zeigt einen QR-Code, die Android-App scannt ihn, und das Handy erscheint
  unter „Bearbeiten → Handys…“. PC und Handy prüfen sich danach gegenseitig
  über Zertifikate; der Code ist zehn Minuten und nur einmal gültig. Die App
  (`AFS_Android/`) zeigt vorerst nur den Stand der Verbindung; Synchronisationen
  mit dem Handy folgen in einer späteren Version (`doc/ANDROID-CONCEPT.md`).
- Synchronisation mit dem Handy: Ein verbundenes Handy steht im Dialog einer
  Synchronisation als Ort zur Wahl („Handy…“, mit den üblichen Ordnern wie
  DCIM/Camera), und nach dem Verbinden bietet eine Seite Vorlagen an: Fotos
  auf dem PC sichern, einen Ordner gleich halten, Musik aufs Handy. Der
  Durchlauf läuft, wenn die App sich meldet („Jetzt synchronisieren“); sie
  öffnet dafür kurz einen Dateiserver mit Einmalpasswort, dessen Schlüssel
  beim Verbinden festgelegt wurde. SSH-Verbindungen kennen jetzt einen Port
  (`port` in der Konfiguration, Standard 22). Übertragungen über SSH schicken
  mehrere Anfragen gleichzeitig und sind im WLAN um ein Vielfaches schneller.
- Das Handy synchronisiert im Hintergrund: alle 30 Minuten, kurz nach neuen
  Fotos und auf Knopfdruck, wahlweise nur im WLAN und nur beim Laden, mit
  Fortschritt in der Benachrichtigung. Neue Dateien erscheinen danach gleich in
  Galerie und Musik-App. Hat der PC eine neue Adresse bekommen, findet das
  Handy ihn selbst wieder (UDP-Port 52381 muss in der Firewall offen sein).
- Der Hintergrunddienst nimmt Handys an, sobald eines verbunden ist, auch ohne
  angemeldeten Benutzer; „Handy verbinden…“ funktioniert auch, während er läuft.
- Aufs Handy kopierte Dateien entstehen zuerst unter einem vorläufigen Namen und
  sind erst vollständig sichtbar; ein abgebrochener Abgleich hinterlässt keine
  halben Fotos oder Lieder mehr. SSH-Verbindungen haben im Dialog ein Feld für
  den Port. Die Android-App gibt es in allen sieben Sprachen, die Hilfe hat
  eine Seite zum Handy. Der Windows-Installer gibt den Handy-Port in der
  Firewall frei (nur im lokalen Netz), das Debian-Paket bringt ein ufw-Profil
  mit (`sudo ufw allow AutoFileSync`).
- Zwei-Wege-Synchronisation merkt sich nach jedem Durchlauf den Stand beider
  Seiten (Abgleichszustand, eine Datei je Synchronisation im Unterordner
  `state` neben der Konfiguration, gemeinsam für Programm und
  Hintergrunddienst). Damit wird eine auf einer Seite gelöschte Datei auf der
  anderen ebenfalls gelöscht, statt von dort zurückzukommen; das gilt auch für
  ganze Verzeichnisse, und ein nur im Ziel vorhandener Ordner wird in die
  Quelle übernommen. Eine seit der Löschung bearbeitete Datei gewinnt und
  wird zurückkopiert, mit Warnung im Protokoll. Auf beiden Seiten bearbeitete
  Dateien werden nicht überschrieben, sondern als Fehler gemeldet. Welche
  Seite sich geändert hat, entscheidet der Vergleich mit dem letzten Stand,
  nicht mehr allein die Änderungszeit. Der erste Durchlauf nach dem Update
  kennt noch keinen Stand und verhält sich wie bisher.
- Umbenannte oder verschobene Dateien werden auf der anderen Seite ebenfalls
  umbenannt statt neu übertragen und unter dem alten Namen gelöscht, auch
  über SSH. Für Einweg-Synchronisationen gibt es dafür die neue Option
  „Umbenennen und Verschieben erkennen“ (`trackMoves` in der Konfiguration),
  die zusammen mit „Gelöschte Dateien im Ziel ebenfalls löschen“ wirkt.
- Die Vorschau zeigt Löschungen in der Quelle (rot) und Umbenennungen mit
  altem und neuem Namen; Löschungen werden am Ende eines Durchlaufs
  ausgeführt.
- Im Dialog einer Synchronisation erklärt ein Hinweis bei eingeschalteter
  Zwei-Wege-Synchronisation, dass Löschungen und Umbenennungen übernommen
  werden. Die Hilfe beschreibt die Zwei-Wege-Synchronisation im Einzelnen,
  in allen sieben Sprachen.
- Der Assistent weiß, wie viele Dateien der Abgleichszustand einer
  Zwei-Wege-Synchronisation kennt.
- Wird eine Synchronisation gelöscht, verschwindet ihr Abgleichszustand mit.
- Änderungen werden sofort übernommen: Bei fortlaufender Synchronisation und
  im Hintergrunddienst beobachtet AutoFileSync die lokalen Verzeichnisse einer
  Synchronisation (inotify unter Linux, FSEvents unter macOS,
  ReadDirectoryChangesW unter Windows) und startet den Durchlauf, sobald eine
  Änderung zwei Sekunden Ruhe hatte, statt bis zum nächsten Takt zu warten.
  Ohne Änderung läuft alle zehn Minuten ein vollständiger Durchlauf als
  Sicherheitsnetz. Neue Option „Änderungen sofort übernehmen“ (`watchChanges`),
  für neue Synchronisationen voreingestellt; bestehende behalten ihren Takt.
  Verzeichnisse über SSH und Netzlaufwerke ohne Benachrichtigung bleiben beim
  Takt.
- SSH: Ein Durchlauf meldet sich je Konto einmal an und nutzt die Verbindung
  für alle Dateien, statt für jedes Auflisten, Prüfen, Kopieren und Löschen eine
  eigene Sitzung aufzubauen. Läufe über SSH sind damit um ein Vielfaches
  schneller, und Server, die viele Anmeldungen als Angriff werten, bleiben
  ruhig. Unter Windows (libssh2) bleibt es vorerst bei einer Sitzung je
  Vorgang.
- Konflikte lösen sich selbst: Wurde eine Datei seit dem letzten Abgleich auf
  beiden Seiten geändert, gewinnt die zuletzt geänderte Fassung (bei gleicher
  Zeit die Quelle), und die andere bleibt auf beiden Seiten als Konfliktkopie
  mit Datum und Rechnername erhalten, etwa `Bericht (Konflikt 2026-10-20,
  laptop).docx`. Protokoll und die Meldung nach dem Lauf nennen die Kopie. Die
  neue Option „Bei Konflikten beide Fassungen behalten“ (Vorgabe ein,
  `conflictCopies`) schaltet auf das bisherige Verhalten zurück: nichts
  überschreiben, Fehler melden.
- Schutz vor Massenlöschung: Würde ein Durchlauf mehr als die Hälfte der
  bekannten Dateien einer Synchronisation löschen (mindestens zehn), etwa weil
  das Ziel geleert oder der Datenträger ausgetauscht wurde, verweigert er alle
  Löschungen, meldet das im Protokoll, im Sicherungsnachweis und nach „Einmal
  synchronisieren“ und lässt beide Seiten unverändert. Die Vorschau zeigt die
  Löschungen weiterhin; „Jetzt synchronisieren“ aus der Vorschau führt sie als
  ausdrückliche Bestätigung aus. Der Hintergrunddienst verweigert sie, bis das
  geschehen ist.

## 0.3.0 – 2026-10-18

- macOS: Ob ein Datenträger eingehängt ist, wird über die Liste der
  eingehängten Dateisysteme des Systems geprüft; bisher las das Programm dafür
  `/proc/mounts`, das es unter macOS nicht gibt, und hielt jeden Datenträger
  für nicht vorhanden.
- Einträge der Hilfe mit Umlauten oder Akzenten erscheinen im
  Inhaltsverzeichnis und im Index; bisher standen dort leere Zeilen.
- Ist Quelle oder Ziel nicht erreichbar, etwa eine nicht angeschlossene
  externe Festplatte, ist das kein Fehler mehr, sondern eine Warnung für diese
  Synchronisation. Die neue Option „Warnung bei nicht vorhandenem Laufwerk oder
  SSH-Verbindung“ (Vorgabe ein, `warnIfUnreachable` in der Konfiguration)
  entscheidet: Ein, steht die Warnung im Log, in der Meldung nach „Einmal
  synchronisieren“ und im Sicherungsnachweis; aus, bleibt nur ein Info-Eintrag
  im Log – für eine Sicherungsplatte, die nur ab und zu angeschlossen ist.
- Synchronisation → Vorschau… zeigt, was ein Lauf tun würde, ohne etwas zu
  verändern: jeder geplante Schritt (kopieren, verschieben, zurückkopieren,
  im Ziel löschen) mit Synchronisation, Datei und Größe, Löschungen rot. Was
  nicht verglichen werden konnte, weil Quelle oder Ziel fehlt, nennt die
  Vorschau ebenfalls. „Jetzt synchronisieren“ startet von dort einen einzelnen
  Lauf. Auch der Assistent kann die Vorschau als Befehl vorschlagen.
- Beim Kopieren auf einen SSH-Server behält die Datei die Rechte der Quelle
  (Lesen, Schreiben, Ausführen für Besitzer, Gruppe und andere), so wie es beim
  Kopieren vom Server schon der Fall war. Bisher kam sie dort nur für den
  SSH-Benutzer lesbar an, ein Skript verlor sein Ausführungsrecht.
- Der SSH-Verzeichnisdialog zeigt die Änderungszeit der Einträge wie überall
  sonst als Ortszeit auf die Sekunde genau; bisher stand dort die Weltzeit mit
  neun Nachkommastellen.
- Kann die alte Zieldatei vor dem Kopieren nicht beiseitegelegt werden, etwa in
  einem schreibgeschützten Zielverzeichnis, zählt das jetzt als Fehler des
  Laufs und erscheint im Sicherungsnachweis; bisher stand es nur im Log.
- Im Dialog einer Synchronisation sperren sich Optionen gegenseitig, die
  zusammen keinen Sinn ergeben: „Quelle nach dem Kopieren löschen“ gegen
  Zwei-Wege-Synchronisation und „Gelöschte Dateien im Ziel ebenfalls löschen“,
  Zwei-Wege-Synchronisation gegen „Kopieren, auch wenn Zieldatei neuer ist“,
  und Letzteres braucht den Vergleich nach Zeit oder Größe. Nach jedem Klick
  wird der Zustand aller Optionen neu geprüft; die Hilfe nennt die Sperren.
- Vater-Sohn-Prinzip: Die aufbewahrten Generationen (`name.bak.1`, `name.bak.2`, …)
  im Ziel werden nicht mehr als Nur-im-Ziel-Dateien behandelt. Bisher löschte
  „Löschen im Ziel“ sie im selben Durchlauf wieder, und die Zwei-Wege-
  Synchronisation kopierte sie in die Quelle zurück.
- Abbrechen während eines Uploads über SSH entfernt die halb übertragene Datei
  auf dem Server; bisher blieb sie dort liegen.
- Die Hilfe gibt es in allen Sprachen der Oberfläche: Deutsch, Englisch,
  Spanisch, Französisch, Italienisch, Polnisch und Ukrainisch. Sie erscheint in
  der Sprache, die das Programm anzeigt, und wechselt mit, wenn die Sprache in
  den globalen Einstellungen umgestellt wird. Auch der Assistent liest die Hilfe
  in dieser Sprache.
- Assistent: Er kann auch Befehle vorschlagen – einmal synchronisieren,
  Synchronisation starten oder stoppen, Hintergrunddienst starten oder stoppen,
  Log-Datei anzeigen. Der Knopf trägt den Namen aus dem Menü und führt den
  Menübefehl aus; ist er gerade nicht möglich, sagt das Programm warum.
- Assistent: Er weiß, wann jede Synchronisation zuletzt lief, ob Quelle und
  Ziel dabei erreichbar waren und welcher Fehler zuerst auftrat, außerdem ob
  das Programm gerade synchronisiert und ob der Hintergrunddienst läuft. So
  kann er Fragen wie „Warum ist mein Ziel nicht aktuell?“ begründen.
- Assistent: Fragen wie „Wie stelle ich die Sprache um?“ beantwortet er mit
  dem passenden Abschnitt der Hilfe und nennt die Hilfeseite.
- Assistent: Ein langes Gespräch lässt seine ältesten Runden weg, statt jede
  weitere Frage mit „Unterhaltung zu lang“ abzulehnen, bis das Programm neu
  gestartet wird. **Neues Gespräch** im Chatfenster beginnt von vorn.
- Assistent: Fragen nach dem Protokoll werden in jeder Sprache erkannt
  („journal“, „registro“, „dziennik“, „журнал“ …), nicht nur auf Deutsch und
  Englisch; nur dann bekommt er den Auszug aus der Logdatei.
- Der Datumsvergleich hat eine einstellbare Toleranz (Reiter Optionen, Vorgabe
  2 Sekunden). Bisher galt eine Datei auf einem NTFS-, exFAT- oder
  FAT-Datenträger bei jedem Durchlauf als geändert, weil das Ziel die
  Änderungszeit gröber speichert als die Quelle, und wurde immer wieder
  kopiert.
- Der Fortschrittsbalken bewegt sich auch innerhalb einer Datei: Er zeigt die
  fertigen Dateien plus den schon kopierten Anteil der laufenden Datei, auch
  über SSH. Abbrechen wirkt mitten in einer großen Datei und lässt keine halbe
  Datei im Ziel zurück. Während „Einmal synchronisieren“ die Dateien zählt,
  zeigt der Mauszeiger die Wartesanduhr.
- Die Oberfläche folgt den Plattformregeln enger: Tastenkürzel für Neu,
  Öffnen, Speichern, Speichern als, Einstellungen, Beenden und Hilfe;
  Menüpunkte, die einen Dialog öffnen, enden mit „…“; Bestätigungen fragen
  mit „Löschen“, „Überschreiben“ bzw. „Speichern“/„Nicht speichern“ statt
  Ja/Nein, und Abbrechen ist die Vorgabe. Unter macOS stehen „Einstellungen…“
  und „Info“ im Programm-Menü, das Fenster zeigt ungespeicherte Änderungen im
  Schließen-Knopf, und das Info-Fenster ist das native.
- Dialoge nach den Plattformregeln: Feldbeschriftungen enden mit Doppelpunkt,
  die Knöpfe „...“ und „ssh“ heißen „Auswählen…“ und „SSH…“, der Log-Level ist
  ein Aufklappmenü, die globalen Einstellungen lassen sich abbrechen, und jede
  Meldung trägt ihr Symbol und erscheint vor dem richtigen Fenster. Der Hinweis
  auf den minimierten Start und auf ein fehlendes Hilfebuch kommt als
  Systembenachrichtigung statt als Dialog, der sich von selbst schließt.
- Die Werkzeugleiste beginnt mit „Neue Synchronisation“. Im Menü Datei stehen
  „Neue Synchronisation…“ und „Neue SSH-Verbindung…“ direkt. Im Dialog einer
  Synchronisation bilden die Vergleichskriterien eine eigene Gruppe. Dialoge
  und Tabelle skalieren mit der Bildschirmauflösung, die Tabellenköpfe nutzen
  die Systemschrift, und die Wurzel der Baumansicht heißt „Dieser Computer“
  statt „PC“.
- Konfiguration und Sicherungsnachweise stehen lesbar in der Datei: Zeichen wie
  kyrillische Buchstaben erscheinen als sie selbst statt als `\u0423…`, und
  Emoji in Namen gehen nicht mehr verloren.
- Der Benutzer, unter dem der Hintergrunddienst läuft, darf ihn ohne
  Administratorpasswort starten und stoppen: Das Debian-Paket legt dafür eine
  polkit-Regel an, der Windows-Installer gibt dem Dienstkonto die Rechte am
  Dienst.
- Die Konfiguration wird als UTF-8 geschrieben. Unter Linux und macOS ließ sie
  sich bisher nicht speichern, sobald Name oder Pfad einer Synchronisation
  einen Umlaut enthielt.
- Sicherungsnachweise werden als UTF-8 geschrieben. Bisher entstand kein
  Nachweis, wenn Name oder Pfad einer Synchronisation einen Umlaut enthielt,
  und in Französisch und Polnisch gar keiner.
- Die globalen Einstellungen haben eine Sprachauswahl. Die gewählte Sprache
  gilt sofort, steht in der Konfiguration und gilt bei jedem Start, auch für
  den Hintergrunddienst; ohne Auswahl gilt die Sprache des Betriebssystems.
- Ein Zahnrad rechts in der Werkzeugleiste öffnet die globalen Einstellungen.
- Übersetzungen ins Spanische, Französische, Italienische, Polnische und
  Ukrainische (`po/es.po`, `po/fr.po`, `po/it.po`, `po/pl.po`, `po/uk.po`);
  sie gelten, wenn die Umgebung die Sprache einstellt, z.B. `LANGUAGE=pl`.
- In einer Sprache ohne eigene Übersetzung (z.B. Schwedisch) erscheinen die
  Meldungen auf Englisch statt als technische Schlüssel wie
  `source_dir_not_reachable`.

## 0.2.0 – 2026-10-09

- macOS: Der Hintergrunddienst steckt im Programmbündel und damit im
  Disk-Image; Start im Menü Hintergrunddienst schreibt den LaunchAgent selbst,
  Stop entfernt ihn wieder. Bisher musste er aus den Quellen gebaut und von
  Hand eingerichtet werden und blieb bei Updates auf dem alten Stand.
- Windows: Der Hintergrunddienst wird mit dem Programm gebaut und vom
  Installationspaket als Dienst eingerichtet, beim Entfernen gestoppt und mit
  entfernt; eine Seite des Installationsprogramms fragt nach dem Konto, unter
  dem er laufen soll. Zuvor lief auf Windows ein alter, von Hand registrierter
  Stand, dem die Kennzeichnung der Logzeilen (G/D) und die übrigen
  Verbesserungen des Dienstes fehlten.
- Der Hintergrunddienst übernimmt eine gespeicherte Konfiguration von selbst,
  statt bis zum nächsten Neustart mit dem alten Stand zu arbeiten.
- Jede Änderung der Konfiguration wird sofort gespeichert; Datei → Speichern
  bleibt nur zum Nachholen eines fehlgeschlagenen Schreibens anwählbar.
- Der Fenstertitel zeigt "(nicht gespeichert)", solange eine Änderung nicht in
  der Datei steht; der Start des Hintergrunddienstes fragt dann vorher, ob
  gespeichert werden soll (Ja/Nein/Abbrechen).
- Das Hauptfenster wird nicht mehr bei jedem Start breiter; Größe und Position
  werden auf den Bildschirm beschnitten, auch wenn der zuletzt benutzte
  Monitor fehlt.
- Windows: Das Kürzen der gemeinsamen Logdatei beim Start scheitert nicht mehr
  mit "Zugriff verweigert", während der Hintergrunddienst sie offen hält.
- Bauen: Die Skripte liegen in `scripts/`, die Dokumente in `doc/`; der
  CMake-Build packt das Hilfebuch `help.htb` selbst.

## 0.1.0 – 2026-09-24

- Alle Texte der Oberfläche und des Synchronisationskerns kommen aus
  Sprachkatalogen (Deutsch, Englisch); die Sprache folgt der Umgebung, z.B.
  `LANGUAGE=en`.
- Lizenz: GNU GPL v3 oder neuer (`LICENSING.md`), Lizenz-Header in allen
  Quelldateien, Hinweise zu Komponenten Dritter, Lizenzseite im
  Windows-Installationsprogramm.
- Meldungen eines Einzellaufs erscheinen nach den Filtern Fehler/Warnung/Info
  in einer Zusammenfassung.
- Die Statuszeile zeigt durchgehend, was die Synchronisation tut.
- 94 GUI-Tests decken jede Hilfeseite ab; dabei behoben: Stopp ohne aktiven
  Lauf, wirkungsloses Abbrechen im Fortschrittsdialog, doppelte Fehlerbox beim
  fehlenden Editor, Hilfe-Knopf der Werkzeugleiste, Stillstand durch ein
  schlafendes Netzlaufwerk.
