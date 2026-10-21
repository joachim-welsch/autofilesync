# DemoMain

Module comment for DemoMain.cpp, the entry point of afs_demo.

afs_demo is built like the GUI test binary: the complete application compiled with _GUITEST,
so App.cpp leaves main() to this file, plus the input simulation of test/GUI-Tests. Instead
of tests it runs the scenes of the introduction video (Scenes.md) while a screen recorder
watches, and it writes the cursor positions and the scene marks the video editor needs
(Recording.md).

## own-options-not-for-wx

wxWidgets parses the command line in OnInit and would reject the demo's options as unknown, so
wxEntryStart only gets the program name; the options were taken out before (Options.md).

## wait_for_start

Three ways to start the scenes: a delay, a file the recording script touches once ffmpeg is
running (record_demo.sh), or the Enter key in the terminal for a recording the user starts by
hand in Recordly. All three pump events while they wait so the window stays responsive and is
drawn completely before the recording begins.

## asserts-to-stderr

A wxWidgets assertion opens a modal box that would freeze the scene in front of the camera
until someone clicks it; on stderr the line is still seen after the take.
