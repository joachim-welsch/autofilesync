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
#pragma once

#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/stopwatch.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace demo {

class Recording: public wxEvtHandler {  // [Recording.md#Recording]
public:
    static Recording& instance();
    void start(const std::filesystem::path& markers_file, const std::filesystem::path& telemetry_file);
    void mark(const wxString& label);  // [Recording.md#mark]
    void finish();  // [Recording.md#finish]
    long elapsed_milliseconds() const { return clock_.Time(); }
private:
    struct Sample { long time_ms; double cx; double cy; const char* interaction; };
    void on_timer(wxTimerEvent&);
    void sample(const wxPoint& screen_point, const char* interaction);
    void write_telemetry() const;
    wxStopWatch clock_;
    wxTimer timer_;
    wxRect screen_;
    std::ofstream markers_;
    std::filesystem::path telemetry_file_;
    std::vector<Sample> samples_;
};

}
