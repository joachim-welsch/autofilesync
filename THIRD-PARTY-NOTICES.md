# Hinweise zu Komponenten Dritter

AutoFileSync verwendet die folgenden Bibliotheken und Werkzeuge. Ihre
Lizenztexte liegen im Verzeichnis `third-party/` und werden mit jedem Paket
ausgeliefert. Die Lizenzen sind mit der GPL von AutoFileSync verträglich
(siehe `LICENSING.md`).

| Komponente | Verwendung | Lizenz | Text |
|---|---|---|---|
| wxWidgets 3.3 | Oberfläche (statisch unter Windows, Systembibliothek unter Linux, im Bündel unter macOS) | wxWindows Library Licence 3.1 (LGPL 2 mit Ausnahme) | `wxWidgets-LICENSE.txt`, `wxWidgets-LGPL-2.txt` |
| Boost (property_tree, algorithm) | Konfiguration, Zeichenketten | Boost Software License 1.0 | `Boost-LICENSE_1_0.txt` |
| OpenSSL 3 | Verschlüsselung der Passwörter, Prüfung der Lizenzdatei, TLS für E-Mail | Apache License 2.0 | `OpenSSL-LICENSE.txt` |
| libssh2 (Windows) | SSH/SFTP | BSD-3-Clause | `libssh2-LICENSE.txt` |
| libssh (Linux, macOS) | SSH/SFTP, als Systembibliothek bzw. im Bündel | LGPL 2.1 | Text der LGPL 2.1 in `gettext-libintl-LGPL-2.1.txt`, Quelle: <https://www.libssh.org> |
| zlib | Kompression in libssh2 | zlib License | `zlib-LICENSE.txt` |
| GNU gettext (libintl) | Übersetzungen | LGPL 2.1 | `gettext-libintl-LGPL-2.1.txt` |
| GNU libiconv | Zeichensatzumwandlung für gettext | LGPL 2.1 | `libiconv-LICENSE.txt` |
| llama.cpp / ggml | Chat-Assistent (lokales Sprachmodell) | MIT | `llama.cpp-LICENSE.txt`, `ggml-LICENSE.txt` |
| Google Test | nur Tests, nicht ausgeliefert | BSD-3-Clause | – |

## LGPL-Bibliotheken

libintl, libiconv und libssh stehen unter der GNU Lesser General Public
License 2.1. Sie werden als eigene dynamische Bibliotheken ausgeliefert
(`intl-8.dll`, `iconv-2.dll` unter Windows; Systembibliotheken unter Linux;
Frameworks im macOS-Bündel), sodass sie durch geänderte Fassungen ersetzt
werden können. Der Quelltext dieser Bibliotheken ist bei den jeweiligen
Projekten erhältlich: <https://www.gnu.org/software/gettext/>,
<https://www.gnu.org/software/libiconv/>, <https://www.libssh.org>.

## Sprachmodell des Chat-Assistenten

Das Sprachmodell ist nicht Teil des Pakets; `setup_llama_windows` und
`setup_llama_macos` laden es auf Wunsch von Hugging Face:

| Modell | Lizenz |
|---|---|
| Qwen2.5-Coder-7B-Instruct, Qwen2.5-Coder-14B-Instruct | Apache License 2.0 |
| Qwen2.5-Coder-3B-Instruct | Qwen Research License – nur für Forschung und nicht-kommerzielle Nutzung |

Die Setup-Skripte nennen die Lizenz des gewählten Modells vor dem
Herunterladen. Für die kommerzielle Nutzung des Chat-Assistenten ist ein
Modell unter Apache License 2.0 zu wählen (`-Size 7b` bzw. `--size 7b` oder
größer).
