# Änderungen

## 0.01.0001 – in Arbeit

- Dual-Licensing: GNU GPL v3 oder kommerzielle Lizenz (`LICENSING.md`), Lizenz-
  Header in allen Quelldateien, Hinweise zu Komponenten Dritter, Lizenzseite im
  Windows-Installationsprogramm.
- Lizenzdatei `autofilesync.lic` mit Ed25519-Signatur; die Statuszeile nennt
  beim Start den Lizenznehmer.
- Meldungen eines Einzellaufs erscheinen nach den Filtern Fehler/Warnung/Info
  in einer Zusammenfassung.
- Die Statuszeile zeigt durchgehend, was die Synchronisation tut.
- 97 GUI-Tests decken jede Hilfeseite ab; dabei behoben: Stopp ohne aktiven
  Lauf, wirkungsloses Abbrechen im Fortschrittsdialog, doppelte Fehlerbox beim
  fehlenden Editor, Hilfe-Knopf der Werkzeugleiste, Stillstand durch ein
  schlafendes Netzlaufwerk.
