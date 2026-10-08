# Änderungen

## Unveröffentlicht

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
