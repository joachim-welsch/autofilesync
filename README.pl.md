# AutoFileSync

[English](README.md) | [Deutsch](README.de.md) | [Español](README.es.md) | [Français](README.fr.md) | [Italiano](README.it.md) | **Polski** | [Українська](README.uk.md)

AutoFileSync utrzymuje pliki w dwóch miejscach w tym samym stanie: pojedynczy
plik, folder albo całe drzewo folderów. Każde z miejsc może być folderem na
własnym komputerze, dyskiem sieciowym, nośnikiem USB, komputerem dostępnym
przez SSH lub telefonem z Androidem. Skonfiguruj, uruchom, zapomnij.

AutoFileSync jest wolnym oprogramowaniem na licencji GNU General Public
License w wersji 3 lub nowszej i działa w systemach Windows, macOS i Linux.

Strona internetowa: <https://welsch-software.com>
Kod źródłowy i zgłaszanie błędów: <https://github.com/joachim-welsch/autofilesync>

## Funkcje

- Synchronizacja plików, folderów i drzew folderów, zawężana za pomocą
  symboli wieloznacznych.
- Porównywanie według daty, rozmiaru lub zawartości; przenoszenie zamiast
  kopiowania; konfigurowalne postępowanie z usuniętymi plikami i nowszymi
  plikami w miejscu docelowym.
- Synchronizacja dwukierunkowa, która zapamiętuje ostatni stan: usunięcia i
  zmiany nazw są przenoszone, a pliki zmienione po obu stronach zostają
  zachowane jako kopie konfliktowe.
- Podgląd, który pokazuje, co zrobiłby przebieg, zanim cokolwiek się zmieni.
- Nic nie ginie: generacje przechowują starsze wersje, usunięte pliki trafiają
  do kosza, odłączony dysk nigdy nie jest traktowany jako „wszystko usunięte”,
  a masowe usuwanie następuje dopiero po potwierdzeniu w podglądzie.
- Źródło lub cel przez SSH na serwerze NAS, serwerze lub komputerze w sieci
  domowej; hasła są przechowywane w postaci zaszyfrowanej.
- Telefony z Androidem łączy się za pomocą kodu QR; szablony zapisują zdjęcia
  na komputerze, utrzymują folder w tym samym stanie lub przenoszą muzykę na
  telefon. Aplikacja (`AFS_Android/`) synchronizuje w tle, opcjonalnie tylko
  przez Wi-Fi i podczas ładowania.
- Zmiany są przejmowane od razu: lokalne foldery są obserwowane, a nie tylko
  sprawdzane w stałych odstępach czasu.
- Usługa w tle, która synchronizuje z tą samą konfiguracją, także gdy żaden
  użytkownik nie jest zalogowany.
- Wbudowany asystent, który odpowiada na pytania o program na podstawie
  pomocy, wyjaśnia, dlaczego cel nie jest aktualny, i proponuje odpowiednie
  polecenia.
- Dziennik z wybieranym poziomem, powiadomienie e-mail o poważnych błędach i
  raport kopii zapasowej w formacie JSON.
- Interfejs i pomoc w języku angielskim, niemieckim, hiszpańskim,
  francuskim, włoskim, polskim i ukraińskim; język wybiera się w
  ustawieniach.

## Instalacja

Gotowe pakiety dla systemów Windows, macOS i Linux są dostępne na stronie
<https://welsch-software.com>. Aby samodzielnie zbudować program, zobacz
`doc/BUILD.md`; pełna instalacja wraz z usługą w tle jest opisana w
`doc/INSTALL-Linux.md` i `doc/INSTALL-macOS.md` (te dokumenty są w języku
niemieckim).

Skrócona wersja dla Linuksa:

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libssl-dev imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Struktura

| Folder | Zawartość |
|---|---|
| `AFS_Core/` | Rdzeń synchronizacji jako biblioteka statyczna, bez interfejsu |
| `AFS_Desktop/` | Program z interfejsem wxWidgets, pomocą i szablonami instalatora |
| `backgroundsync/` | Usługa w tle dla systemów Linux, macOS i Windows |
| `AFS_Android/` | Aplikacja na Androida (Kotlin), instrukcja budowania w `AFS_Android/README.md` |
| `po/` | Katalogi językowe |
| `third-party/` | Teksty licencji używanych bibliotek |

Testy: `scripts/run_Integrationtests.sh` dla rdzenia,
`scripts/run_GUI_Tests.sh` dla interfejsu; szczegóły w `doc/BUILD.md`.

## Współpraca

Zgłoszenia błędów i propozycje należy przesyłać jako issue na
<https://github.com/joachim-welsch/autofilesync/issues>, a zmiany jako pull
request. Wkład jest objęty tą samą licencją co projekt (`doc/LICENSING.md`).
Styl kodu i sposób pracy opisuje `doc/CONTRIBUTING.md`.

## Wsparcie

AutoFileSync jest i pozostanie bezpłatny. Kto chce wesprzeć jego rozwój, może
przekazać darowiznę przez PayPal na adres `joachim.welsch@posteo.de`.
Rozszerzenia i dostosowania na zamówienie chętnie wykonujemy odpłatnie;
zapytania prosimy kierować na adres `contact@welsch-software.com`.

## Licencja

GNU General Public License v3 lub nowsza, zobacz `LICENSE` i
`doc/LICENSING.md`. Copyright © 2026 Joachim Welsch. Komponenty innych
autorów i ich licencje wymienia `doc/THIRD-PARTY-NOTICES.md`.
