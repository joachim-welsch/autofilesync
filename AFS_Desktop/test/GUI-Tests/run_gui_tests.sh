#!/bin/bash
# Linux: runs the GUI tests on a private X display. wxUIActionSimulator moves the
# real pointer and grabs the real keyboard, which would disturb the developer's
# session, and the tests must also run without any display attached.
# macOS: there is no equivalent - the events go through the window server of the
# logged-in session, so the tests take mouse and keyboard over while they run.
set -e
BINARY="${1:-$(dirname "$0")/build/GuiTests}"
shift || true
# The chat tests (ChatGuiTest, SlowGuiTest) need the local language model and
# take minutes each; they run only when the caller asks for them with a filter
# of their own, see GuiTest.md#ChatGuiTest.
case " $* " in
    *--gtest_filter*) ;;
    *) set -- "$@" "--gtest_filter=-ChatGuiTest.*:SlowGuiTest.*" ;;
esac
if [ ! -x "$BINARY" ]; then
    echo "Testprogramm nicht gefunden: $BINARY" >&2
    exit 1
fi
# caffeinate keeps the display awake: the synthetic input does not count as user
# activity, and a display that sleeps mid-run swallows every click and keystroke.
if [ "$(uname)" = "Darwin" ]; then
    echo "GUI-Tests laufen auf dem aktiven Desktop - bitte Maus und Tastatur nicht benutzen."
    exec caffeinate -dimsu "$BINARY" "$@"
fi
# On a Wayland desktop, GTK prefers the real Wayland session over $DISPLAY
# whenever WAYLAND_DISPLAY is still set - the app would then render on the
# developer's real screen, invisible to and untouched by wxUIActionSimulator's
# X11 events on the private display, so every typed character would silently
# go nowhere. GDK_BACKEND=x11 forces it onto the display xvfb-run sets up.
exec env GDK_BACKEND=x11 xvfb-run -a -s "-screen 0 1280x1024x24" "$BINARY" "$@"
