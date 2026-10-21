#!/bin/bash
# Nimmt die Demo fuer das Einfuehrungsvideo auf: afs_demo fuehrt AutoFileSync
# auf einem privaten X-Server vor, ffmpeg zeichnet den Bildschirm auf, und
# neben dem Video entsteht <video>.cursor.json mit den Mausbewegungen im
# Recordly-Format. Das Video laesst sich in Recordly importieren; Cursor,
# Auto-Zoom und Rahmen kommen dort hinzu.
#
#   scripts/record_demo.sh [--watch] [--out DIR] [--size WxH] [--lang de|en|fr|es|it|pl] [Optionen fuer afs_demo]
#
#   --watch   zeigt den privaten X-Server als Fenster (Xephyr) statt unsichtbar (Xvfb)
#   --out     Zielverzeichnis fuer Video, Cursor-Datei und Szenenmarken (Standard: demo-recording)
#   --size    Bildschirmgroesse der Aufnahme (Standard 1920x1080)
#   --lang    Sprache der Oberflaeche (Standard de). Bei en heissen Ordner und
#             Synchronisation Pictures und Photos, die Frage an den Assistenten ist
#             englisch, und die Dateien heissen autofilesync-demo-en-<Zeit>.*;
#             bei fr entsprechend Images und Photos, franzoesische Frage,
#             autofilesync-demo-fr-<Zeit>.*; bei es Imágenes und Fotos, spanische
#             Frage, autofilesync-demo-es-<Zeit>.*; bei it Immagini und Foto,
#             italienische Frage, autofilesync-demo-it-<Zeit>.*; bei pl Obrazy und
#             Fotografie, polnische Frage, autofilesync-demo-pl-<Zeit>.*
#
# Beispiel: scripts/record_demo.sh --watch --scenes create,preview,start --cycle-seconds 5
set -e
cd "$(dirname "$0")/.."

OUT=demo-recording
SIZE=1920x1080
WATCH=0
LANG_CODE=de
DEMO_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --watch) WATCH=1; shift ;;
        --out) OUT="$2"; shift 2 ;;
        --size) SIZE="$2"; shift 2 ;;
        --lang) LANG_CODE="$2"; shift 2 ;;
        *) DEMO_ARGS+=("$1"); shift ;;
    esac
done
STAMP_PREFIX=autofilesync-demo
# Voreinstellungen fuer die fremdsprachigen Aufnahmen; eigene Optionen dahinter ueberschreiben sie.
# Die Frage muss ohne Akzente auskommen: die Eingabesimulation tippt nur ASCII-Zeichen.
case "$LANG_CODE" in
    en) DEMO_ARGS=(--source-name Pictures --sync-name Photos --question "What happens when I unplug the drive?" "${DEMO_ARGS[@]}")
        STAMP_PREFIX=autofilesync-demo-en ;;
    fr) DEMO_ARGS=(--source-name Images --sync-name Photos --question "Que se passe-t-il si je retire le disque dur ?" --assistant-wait 180 "${DEMO_ARGS[@]}")
        STAMP_PREFIX=autofilesync-demo-fr ;;
    es) DEMO_ARGS=(--source-name Imágenes --sync-name Fotos --question "Dime si pierdo datos cuando desconecto el disco duro." --assistant-wait 180 "${DEMO_ARGS[@]}")
        STAMP_PREFIX=autofilesync-demo-es ;;
    it) DEMO_ARGS=(--source-name Immagini --sync-name Foto --question "Cosa succede se scollego il disco esterno?" --assistant-wait 180 "${DEMO_ARGS[@]}")
        STAMP_PREFIX=autofilesync-demo-it ;;
    pl) DEMO_ARGS=(--source-name Obrazy --sync-name Fotografie --question "Co robi AutoFileSync, gdy dysk USB zniknie?" --assistant-wait 180 "${DEMO_ARGS[@]}")
        STAMP_PREFIX=autofilesync-demo-pl ;;
esac

for tool in ffmpeg Xvfb; do
    command -v "$tool" >/dev/null || { echo "$tool fehlt (sudo apt install ffmpeg xvfb)" >&2; exit 1; }
done
[ "$WATCH" = 1 ] && ! command -v Xephyr >/dev/null && { echo "Xephyr fehlt (sudo apt install xserver-xephyr)" >&2; exit 1; }

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAFS_BUILD_DEMO=ON >/dev/null
cmake --build build --target afs_demo -j"$(nproc)"

DISPLAY_NUMBER=:97
mkdir -p "$OUT"
STAMP=$(date +%Y%m%d-%H%M%S)
VIDEO="$OUT/$STAMP_PREFIX-$STAMP.mp4"
MARKERS="$OUT/$STAMP_PREFIX-$STAMP.markers.txt"
TRIGGER=$(mktemp -u "${TMPDIR:-/tmp}/afs-demo-start.XXXXXX")

# ffmpeg endet nach SIGINT mit Exit-Code 255, obwohl die Datei vollstaendig
# ist; unter set -e darf das wait den Status deshalb nicht weiterreichen.
stop_ffmpeg() {
    [ -n "$FFMPEG_PID" ] || return 0
    kill -INT "$FFMPEG_PID" 2>/dev/null || true
    wait "$FFMPEG_PID" 2>/dev/null || true
    FFMPEG_PID=
}
# Erst auf das Ende jedes Clients warten, dann den X-Server beenden: ein noch
# verbundener Client (Fensterverwalter, ffmpeg) stirbt sonst mit "XIO: fatal IO error".
stop_background() {
    local pid=$1
    [ -n "$pid" ] || return 0
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}
cleanup() {
    stop_ffmpeg
    stop_background "$WM_PID"
    stop_background "$X_PID"
    rm -f "$TRIGGER"
}
trap cleanup EXIT

if [ "$WATCH" = 1 ]; then
    Xephyr "$DISPLAY_NUMBER" -screen "$SIZE" -dpi 110 -title "AutoFileSync Demo" -no-host-grab &
else
    Xvfb "$DISPLAY_NUMBER" -screen 0 "${SIZE}x24" -dpi 110 &
fi
X_PID=$!
for _ in $(seq 1 50); do xdpyinfo -display "$DISPLAY_NUMBER" >/dev/null 2>&1 && break; sleep 0.1; done
export DISPLAY="$DISPLAY_NUMBER" GDK_BACKEND=x11 LANGUAGE="$LANG_CODE"
command -v xrdb >/dev/null && echo "Xft.dpi: 110" | xrdb -merge
# Ein Fensterverwalter liefert Titelleisten und Schatten; ohne einen laeuft die
# Demo trotzdem, die Fenster sind dann rahmenlos.
for wm in openbox xfwm4 marco metacity; do
    if command -v "$wm" >/dev/null; then "$wm" & WM_PID=$!; sleep 0.5; break; fi
done
[ -z "$WM_PID" ] && echo "Hinweis: kein Fensterverwalter gefunden (z.B. sudo apt install openbox), Fenster ohne Titelleiste." >&2

ffmpeg -hide_banner -loglevel error -f x11grab -draw_mouse 0 -framerate 30 -video_size "$SIZE" -i "$DISPLAY_NUMBER" \
       -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p -movflags +frag_keyframe+empty_moov "$VIDEO" &
FFMPEG_PID=$!
sleep 1.5
touch "$TRIGGER"
set +e
build/afs_demo --wait-for-file "$TRIGGER" --telemetry "$VIDEO.cursor.json" --markers "$MARKERS" "${DEMO_ARGS[@]}"
RESULT=$?
set -e
sleep 1
stop_ffmpeg
echo
echo "Video:        $VIDEO"
echo "Cursor-Daten: $VIDEO.cursor.json"
echo "Szenenmarken: $MARKERS"
exit $RESULT
