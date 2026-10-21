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
#include "Recording.h"
#include "UserActions.h"
#include <wx/display.h>
#include <wx/utils.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace demo {
namespace {

constexpr int SAMPLE_INTERVAL_MILLISECONDS = 33;  // [Recording.md#telemetry-format]
constexpr int TELEMETRY_VERSION = 2;

std::string time_stamp(long milliseconds)
{
    std::ostringstream text;
    text << std::setfill('0') << std::setw(2) << milliseconds / 60000 << ':' << std::setw(2) << milliseconds / 1000 % 60
         << '.' << milliseconds / 100 % 10;
    return text.str();
}

}

Recording& Recording::instance()
{
    static Recording recording;
    return recording;
}

void Recording::start(const std::filesystem::path& markers_file, const std::filesystem::path& telemetry_file)
{
    screen_ = wxDisplay(0u).GetGeometry();
    telemetry_file_ = telemetry_file;
    if (!markers_file.empty())
        markers_.open(markers_file);
    clock_.Start();
    timer_.SetOwner(this);
    Bind(wxEVT_TIMER, &Recording::on_timer, this);
    guitest::set_click_observer([this](const wxPoint& point, bool double_click) {
        sample(point, double_click ? "double-click" : "click");
        sample(point, "mouseup");
    });
    if (!telemetry_file_.empty())
        timer_.Start(SAMPLE_INTERVAL_MILLISECONDS);
}

void Recording::mark(const wxString& label)  // [Recording.md#mark]
{
    const std::string line = time_stamp(clock_.Time()) + "  " + std::string(label.ToUTF8());
    std::cout << line << std::endl;
    if (markers_.is_open())
        markers_ << line << '\n' << std::flush;
}

void Recording::on_timer(wxTimerEvent&)
{
    sample(wxGetMousePosition(), "move");
}

void Recording::sample(const wxPoint& screen_point, const char* interaction)
{
    if (telemetry_file_.empty() || screen_.width <= 0 || screen_.height <= 0)
        return;
    const double cx = std::clamp((screen_point.x - screen_.x) / static_cast<double>(screen_.width), 0.0, 1.0);
    const double cy = std::clamp((screen_point.y - screen_.y) / static_cast<double>(screen_.height), 0.0, 1.0);
    samples_.push_back({clock_.Time(), cx, cy, interaction});
}

void Recording::finish()
{
    timer_.Stop();
    guitest::set_click_observer(nullptr);
    mark("Ende");
    if (!telemetry_file_.empty())
        write_telemetry();
}

void Recording::write_telemetry() const  // [Recording.md#telemetry-format]
{
    std::ofstream file(telemetry_file_);
    file << "{\n  \"version\": " << TELEMETRY_VERSION << ",\n  \"samples\": [";
    const char* separator = "\n";
    for (const Sample& sample : samples_) {
        file << separator << "    {\"timeMs\": " << sample.time_ms << ", \"cx\": " << std::setprecision(5) << sample.cx
             << ", \"cy\": " << sample.cy << ", \"interactionType\": \"" << sample.interaction << "\", \"cursorType\": \"arrow\"}";
        separator = ",\n";
    }
    file << "\n  ]\n}\n";
}

}
