/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "DemoSandbox.h"
#include "Options.h"
#include "Pacing.h"
#include "Recording.h"
#include "Scenes.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "MainWindow.h"
#include <wx/app.h>
#include <wx/debug.h>
#include <wx/evtloop.h>
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <unistd.h>

namespace {

constexpr int PLACEMENT_MILLISECONDS = 800;

void log_assert(const wxString& file, int line, const wxString& function, const wxString& condition, const wxString& message)  // [DemoMain.md#asserts-to-stderr]
{
    std::cerr << "wx assert " << file << ':' << line << ' ' << function << ": " << condition << " - " << message << std::endl;
}

bool stdin_has_line()
{
    pollfd input{STDIN_FILENO, POLLIN, 0};
    return poll(&input, 1, 0) > 0;
}

void wait_for_start(const demo::Options& options)  // [DemoMain.md#wait_for_start]
{
    if (!options.trigger_file.empty()) {
        std::cout << "Warte auf " << options.trigger_file << " ..." << std::endl;
        guitest::wait_until([&options] { return std::filesystem::exists(options.trigger_file); }, 24 * 3600 * 1000);
    } else if (options.wait_for_stdin) {
        std::cout << "Aufnahme starten und dann hier die Eingabetaste druecken ..." << std::endl;
        guitest::wait_until(stdin_has_line, 24 * 3600 * 1000);
    } else {
        guitest::pump(options.start_delay_seconds * 1000);
    }
}

int run(const demo::Options& options)
{
    MainWindow* window = dynamic_cast<MainWindow*>(wxTheApp->GetTopWindow());
    if (window == nullptr) {
        std::cerr << "Kein Hauptfenster." << std::endl;
        return EXIT_FAILURE;
    }
    guitest::pump(PLACEMENT_MILLISECONDS);
    window->Raise();
    demo::glide_to(demo::center_of(*window), 1);
    wait_for_start(options);
    demo::Recording::instance().start(options.markers_file, options.telemetry_file);
    int failures = 0;
    for (const std::string& scene : options.scenes)
        if (!demo::run_scene(scene, *window, options))
            ++failures;
    demo::Recording::instance().finish();
    window->close();
    guitest::pump(300);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}

int main(int argc, char** argv)
{
    const std::optional<demo::Options> options = demo::parse_options(argc, argv);
    if (!options)
        return EXIT_FAILURE;
    demo::set_speed(options->speed);
    demo::DemoSandbox::create(*options);
    int wx_argc = 1;  // [DemoMain.md#own-options-not-for-wx]
    if (!wxEntryStart(wx_argc, argv))
        return EXIT_FAILURE;
    wxSetAssertHandler(log_assert);
    demo::DemoSandbox::isolate_preferences(*options);
    guitest::ModalDialogWatcher::instance();
    int result = EXIT_FAILURE;
    if (wxTheApp->CallOnInit()) {
        wxEventLoop main_loop;
        wxEventLoopActivator activate_main_loop(&main_loop);
        result = run(*options);
        wxTheApp->OnExit();
    }
    wxEntryCleanup();
    if (!options->keep_sandbox)
        demo::DemoSandbox::remove();
    return result;
}
