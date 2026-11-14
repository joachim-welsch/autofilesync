# AutoFileSync

[English](README.md) | [Deutsch](README.de.md) | [Español](README.es.md) | **Français** | [Italiano](README.it.md) | [Polski](README.pl.md) | [Українська](README.uk.md)

AutoFileSync maintient à jour les fichiers de deux emplacements : un seul
fichier, un dossier ou toute une arborescence. Chaque emplacement peut être un
dossier de votre propre ordinateur, un lecteur réseau, une clé ou un disque
USB, un ordinateur accessible par SSH ou un téléphone Android. Configurer,
démarrer, oublier.

AutoFileSync est un logiciel libre sous la GNU General Public License,
version 3 ou ultérieure, et fonctionne sous Windows, macOS et Linux.

Site web : <https://welsch-software.com>
Code source et signalement des erreurs : <https://github.com/joachim-welsch/autofilesync>

## Fonctionnalités

- Synchronisation de fichiers, de dossiers et d'arborescences, délimitée par
  des caractères génériques.
- Comparaison par date, taille ou contenu ; déplacer au lieu de copier ;
  traitement réglable des fichiers supprimés et des fichiers plus récents dans
  la cible.
- Synchronisation bidirectionnelle qui retient le dernier état : les
  suppressions et les renommages sont reportés, et les fichiers modifiés des
  deux côtés sont conservés sous forme de copies de conflit.
- Un aperçu qui montre ce que ferait une exécution avant que quoi que ce soit
  ne change.
- Rien ne se perd : les générations conservent les versions antérieures, les
  suppressions vont à la corbeille, un lecteur débranché n'est jamais
  interprété comme « tout supprimé », et les suppressions massives ne sont
  exécutées qu'après confirmation dans l'aperçu.
- Source ou cible par SSH sur un NAS, un serveur ou un ordinateur du réseau
  domestique ; les mots de passe sont enregistrés chiffrés.
- Les téléphones Android sont associés par code QR ; des modèles sauvegardent
  les photos sur le PC, maintiennent un dossier synchronisé ou mettent de la
  musique sur le téléphone. L'application (`AFS_Android/`) synchronise en
  arrière-plan, au choix uniquement en Wi-Fi et pendant la charge.
- Les modifications sont reprises aussitôt : les dossiers locaux sont
  surveillés au lieu d'être seulement vérifiés à intervalles fixes.
- Un service en arrière-plan qui synchronise avec la même configuration, même
  sans utilisateur connecté.
- Un assistant intégré qui répond aux questions sur le programme à partir de
  l'aide, explique pourquoi une cible n'est pas à jour et propose les
  commandes adaptées.
- Un journal au niveau réglable, une notification par e-mail en cas d'erreur
  grave et un rapport de sauvegarde au format JSON.
- Interface et aide en anglais, allemand, espagnol, français, italien,
  polonais et ukrainien ; la langue se choisit dans les paramètres.

## Installation

Des paquets prêts à l'emploi pour Windows, macOS et Linux sont disponibles
sur <https://welsch-software.com>. Pour compiler soi-même, voir
`doc/BUILD.md` ; l'installation complète, service en arrière-plan compris,
est décrite dans `doc/INSTALL-Linux.md` et `doc/INSTALL-macOS.md` (ces
documents sont en allemand).

En bref sous Linux :

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libcurl4-openssl-dev libssl-dev \
                 imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Structure

| Dossier | Contenu |
|---|---|
| `AFS_Core/` | Moteur de synchronisation sous forme de bibliothèque statique, sans interface |
| `AFS_Desktop/` | Programme avec interface wxWidgets, aide et modèles d'installateur |
| `backgroundsync/` | Service en arrière-plan pour Linux, macOS et Windows |
| `AFS_Android/` | Application Android (Kotlin), instructions de compilation dans `AFS_Android/README.md` |
| `po/` | Catalogues de langues |
| `third-party/` | Textes de licence des bibliothèques utilisées |

Tests : `scripts/run_Integrationtests.sh` pour le moteur,
`scripts/run_GUI_Tests.sh` pour l'interface ; détails dans `doc/BUILD.md`.

## Contribuer

Les signalements d'erreurs et les suggestions s'envoient sous forme d'issue à
<https://github.com/joachim-welsch/autofilesync/issues> ; les modifications
arrivent sous forme de pull request. Les contributions sont placées sous la
même licence que le projet (`doc/LICENSING.md`). Le style de code et la
procédure sont décrits dans `doc/CONTRIBUTING.md`.

## Soutien

AutoFileSync est et restera gratuit. Pour soutenir son développement, vous
pouvez faire un don par PayPal à `joachim.welsch@posteo.de`. Les extensions et
adaptations sur mesure sont volontiers réalisées contre rémunération ; les
demandes sont à adresser à `contact@welsch-software.com`.

## Licence

GNU General Public License v3 ou ultérieure, voir `LICENSE` et
`doc/LICENSING.md`. Copyright © 2026 Joachim Welsch. Les composants tiers et
leurs licences figurent dans `doc/THIRD-PARTY-NOTICES.md`.
