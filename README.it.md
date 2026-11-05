# AutoFileSync

[English](README.md) | [Deutsch](README.de.md) | [Español](README.es.md) | [Français](README.fr.md) | **Italiano** | [Polski](README.pl.md) | [Українська](README.uk.md)

AutoFileSync mantiene allineati i file di due posizioni: un singolo file, una
cartella o un intero albero di cartelle. Ogni posizione può essere una
cartella del proprio computer, un'unità di rete, un'unità USB, un computer
raggiungibile via SSH o uno smartphone Android. Configura, avvia, dimentica.

AutoFileSync è software libero sotto la GNU General Public License,
versione 3 o successiva, e funziona su Windows, macOS e Linux.

Sito web: <https://welsch-software.com>
Codice sorgente e segnalazione errori: <https://github.com/joachim-welsch/autofilesync>

## Funzioni

- Sincronizzazione di file, cartelle e alberi di cartelle, delimitata da
  caratteri jolly.
- Confronto per data, dimensione o contenuto; spostare invece di copiare;
  gestione configurabile dei file eliminati e dei file più recenti nella
  destinazione.
- Sincronizzazione bidirezionale che ricorda l'ultimo stato: eliminazioni e
  rinomine vengono riportate, e i file modificati su entrambi i lati vengono
  conservati come copie di conflitto.
- Un'anteprima che mostra cosa farebbe un'esecuzione prima che cambi qualcosa.
- Non si perde nulla: le generazioni conservano le versioni precedenti, le
  eliminazioni finiscono nel cestino, un'unità scollegata non viene mai
  interpretata come «tutto eliminato», e le eliminazioni di massa vengono
  eseguite solo dopo la conferma nell'anteprima.
- Origine o destinazione via SSH su un NAS, un server o un computer della rete
  domestica; le password vengono salvate cifrate.
- Gli smartphone Android si associano con un codice QR; i modelli salvano le
  foto sul PC, mantengono allineata una cartella o portano la musica sullo
  smartphone. L'app (`AFS_Android/`) sincronizza in background, a scelta solo
  in Wi-Fi e durante la ricarica.
- Le modifiche vengono applicate subito: le cartelle locali vengono
  sorvegliate invece di essere controllate solo a intervalli fissi.
- Un servizio in background che sincronizza con la stessa configurazione,
  anche senza utenti connessi.
- Un assistente integrato che risponde alle domande sul programma basandosi
  sulla guida, spiega perché una destinazione non è aggiornata e propone i
  comandi adatti.
- Un registro con livello selezionabile, notifica via e-mail in caso di errori
  gravi e un rapporto di backup in formato JSON.
- Interfaccia e guida in inglese, tedesco, spagnolo, francese, italiano,
  polacco e ucraino; la lingua si sceglie nelle impostazioni.

## Installazione

Pacchetti pronti per Windows, macOS e Linux sono disponibili su
<https://welsch-software.com>. Per compilarlo da sé, vedere `doc/BUILD.md`;
l'installazione completa, servizio in background incluso, è descritta in
`doc/INSTALL-Linux.md` e `doc/INSTALL-macOS.md` (questi documenti sono in
tedesco).

In breve per Linux:

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libssl-dev imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Struttura

| Cartella | Contenuto |
|---|---|
| `AFS_Core/` | Motore di sincronizzazione come libreria statica, senza interfaccia |
| `AFS_Desktop/` | Programma con interfaccia wxWidgets, guida e modelli di installer |
| `backgroundsync/` | Servizio in background per Linux, macOS e Windows |
| `AFS_Android/` | App Android (Kotlin), istruzioni di compilazione in `AFS_Android/README.md` |
| `po/` | Cataloghi delle lingue |
| `third-party/` | Testi di licenza delle librerie utilizzate |

Test: `scripts/run_Integrationtests.sh` per il motore,
`scripts/run_GUI_Tests.sh` per l'interfaccia; dettagli in `doc/BUILD.md`.

## Contribuire

Segnalazioni di errori e richieste vanno inviate come issue a
<https://github.com/joachim-welsch/autofilesync/issues>; le modifiche arrivano
come pull request. I contributi sono rilasciati sotto la stessa licenza del
progetto (`doc/LICENSING.md`). Lo stile del codice e la procedura sono
descritti in `doc/CONTRIBUTING.md`.

## Sostegno

AutoFileSync è e resterà gratuito. Chi desidera sostenerne lo sviluppo può
fare una donazione via PayPal a `joachim.welsch@posteo.de`. Estensioni e
adattamenti su misura vengono realizzati volentieri a pagamento; le richieste
vanno inviate a `contact@welsch-software.com`.

## Licenza

GNU General Public License v3 o successiva, vedere `LICENSE` e
`doc/LICENSING.md`. Copyright © 2026 Joachim Welsch. I componenti di terze
parti e le relative licenze sono elencati in `doc/THIRD-PARTY-NOTICES.md`.
