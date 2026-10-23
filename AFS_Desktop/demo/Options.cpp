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
#include "Options.h"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <sstream>

namespace demo {
namespace {

const std::vector<std::string> INTRO_SCENES = {"create", "preview", "start", "background", "options", "ssh", "assistant", "finale"};
const std::vector<std::string> TOUR_SCENES = {"tour_window", "tour_toolbar", "tour_status", "tour_views", "tour_active", "tour_edit",
    "tour_delete", "finale"};  // [Options.md#sandbox]
const std::vector<std::string> SCENES = [] {
    std::vector<std::string> all = INTRO_SCENES;
    for (const std::string& scene : TOUR_SCENES)
        if (std::find(all.begin(), all.end(), scene) == all.end())
            all.push_back(scene);
    return all;
}();

void usage()
{
    std::cout <<
        "afs_demo - fuehrt AutoFileSync wie im Sprecherskript vor, fuer eine Bildschirmaufnahme\n\n"
        "  --scenes a,b,c        Szenen in dieser Reihenfolge (Standard: alle der Sandbox):\n"
        "                        photos: create preview start background options ssh assistant finale\n"
        "                        tour:   tour_window tour_toolbar tour_status tour_views tour_active\n"
        "                                tour_edit tour_delete finale\n"
        "  --sandbox NAME        photos (Einfuehrungsvideo, Standard) oder tour (Folge 1: drei fertige\n"
        "                        Synchronisationen fuer den Rundgang durchs Hauptfenster)\n"
        "  --root DIR            Sandbox mit Bildern, Konfiguration und Protokoll (Standard: <temp>/autofilesync-demo)\n"
        "  --target DIR          Zielverzeichnis der Synchronisation (Standard: <root>/USB-Backup)\n"
        "  --telemetry FILE      Cursor-Telemetrie im Recordly-Format (<video>.cursor.json)\n"
        "  --markers FILE        Szenenmarken mit Zeitstempel fuer den Schnitt\n"
        "  --start-delay S       Wartezeit vor der ersten Szene (Standard 3)\n"
        "  --wait-for-file FILE  stattdessen warten, bis die Datei existiert\n"
        "  --wait-stdin          stattdessen auf Eingabetaste im Terminal warten\n"
        "  --window WxH          Groesse des Hauptfensters (Standard 1280x800)\n"
        "  --speed F             Tempo der Maus- und Tastatureingaben, 2 = doppelt so schnell\n"
        "  --cycle-seconds N     Synchronisationszyklus sichtbar auf N Sekunden setzen (0 = Voreinstellung lassen)\n"
        "  --assistant-wait S    hoechstens S Sekunden auf die Antwort des Assistenten warten (Standard 90)\n"
        "  --question TEXT       Frage an den Assistenten\n"
        "  --source-name NAME    Name des Bilderordners unter <root> (Standard: Bilder)\n"
        "  --sync-name NAME      Name der angelegten Synchronisation (Standard: Fotos)\n"
        "  --keep-sandbox        Sandbox nach dem Lauf stehen lassen\n";
}

std::vector<std::string> split(const std::string& list)
{
    std::vector<std::string> parts;
    std::stringstream stream(list);
    for (std::string part; std::getline(stream, part, ',');)
        if (!part.empty())
            parts.push_back(part);
    return parts;
}

bool parse_int(const std::string& text, int& value)
{
    return std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc();
}

bool parse_size(const std::string& text, int& width, int& height)
{
    const auto x = text.find('x');
    return x != std::string::npos && parse_int(text.substr(0, x), width) && parse_int(text.substr(x + 1), height);
}

}

const std::vector<std::string>& all_scenes()
{
    return SCENES;
}

const std::vector<std::string>& default_scenes(const std::string& sandbox)  // [Options.md#sandbox]
{
    return sandbox == "tour" ? TOUR_SCENES : INTRO_SCENES;
}

std::optional<Options> parse_options(int argc, char** argv)  // [Options.md#parse_options]
{
    Options options;
    bool scenes_given = false;
    options.root = std::filesystem::temp_directory_path() / "autofilesync-demo";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        bool ok = true;
        if (arg == "--help" || arg == "-h") { usage(); return std::nullopt; }
        else if (arg == "--scenes") { options.scenes = split(value()); scenes_given = true; }
        else if (arg == "--sandbox") { options.sandbox = value(); ok = options.sandbox == "photos" || options.sandbox == "tour"; }
        else if (arg == "--root") options.root = value();
        else if (arg == "--target") options.target = value();
        else if (arg == "--telemetry") options.telemetry_file = value();
        else if (arg == "--markers") options.markers_file = value();
        else if (arg == "--start-delay") ok = parse_int(value(), options.start_delay_seconds);
        else if (arg == "--wait-for-file") options.trigger_file = value();
        else if (arg == "--wait-stdin") options.wait_for_stdin = true;
        else if (arg == "--window") ok = parse_size(value(), options.window_width, options.window_height);
        else if (arg == "--speed") { try { options.speed = std::stod(value()); } catch (...) { ok = false; } }
        else if (arg == "--cycle-seconds") ok = parse_int(value(), options.cycle_seconds);
        else if (arg == "--assistant-wait") ok = parse_int(value(), options.assistant_wait_seconds);
        else if (arg == "--question") options.question = std::filesystem::path(value()).wstring();  // [Options.md#question-via-path]
        else if (arg == "--source-name") options.source_name = value();  // [Options.md#names]
        else if (arg == "--sync-name") options.sync_name = std::filesystem::path(value()).wstring();
        else if (arg == "--keep-sandbox") options.keep_sandbox = true;
        else ok = false;
        if (!ok) {
            std::cerr << "Unbekannte oder unvollstaendige Option: " << arg << "\n";
            usage();
            return std::nullopt;
        }
    }
    if (!scenes_given)
        options.scenes = default_scenes(options.sandbox);
    for (const std::string& scene : options.scenes)
        if (std::find(SCENES.begin(), SCENES.end(), scene) == SCENES.end()) {
            std::cerr << "Unbekannte Szene: " << scene << "\n";
            return std::nullopt;
        }
    if (options.target.empty())
        options.target = options.root / "USB-Backup";
    if (options.speed <= 0)
        options.speed = 1.0;
    return options;
}

}
