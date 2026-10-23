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
#include "Scenes.h"
#include "DemoSandbox.h"
#include "Pacing.h"
#include "Recording.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "SyncActions.h"
#include "UserActions.h"
#include "ViewActions.h"
#include "ChatDialog.h"
#include "GridCtrl.h"
#include "Config.h"
#include "ListCtrl.h"
#include "MainWindow.h"
#include "ModelLocation.h"
#include "SyncItem.h"
#include "SyncItemDialog.h"
#include "TextResources.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/dirdlg.h>
#include <wx/statusbr.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/toolbar.h>
#include <filesystem>
#include <iostream>
#include <map>

namespace fs = std::filesystem;
using namespace AFS_Core;
using utilwx::tul;
using guitest::find;
using guitest::pump;
using guitest::wait_until;
using guitest::ModalDialogWatcher;

namespace demo {
namespace {

constexpr int COPY_TIMEOUT_MILLISECONDS = 120000;
constexpr int DIALOG_TIMEOUT_MILLISECONDS = 4000;
constexpr int CYCLE_GRACE_MILLISECONDS = 15000;
constexpr int MENU_TIMEOUT_MILLISECONDS = 1500;
constexpr int MENU_FILE = 0, MENU_SYNC = 2, MENU_BACKGROUND_SERVICE = 3, MENU_VIEW = 4, MENU_ASSISTANT = 5;  // [Scenes.md#menu-by-keyboard]

wxString tr(const char* msgid)
{
    return translation::translate(msgid);
}

void mark(const wxString& label)
{
    Recording::instance().mark(label);
}

void report(const char* what)
{
    std::cerr << "Hinweis: " << what << std::endl;
}

std::size_t files_below(const fs::path& dir)
{
    std::error_code ec;
    std::size_t count = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec))
            ++count;
    return count;
}

bool wait_shown(wxDialog& dialog)
{
    return wait_until([&dialog] { return dialog.IsShownOnScreen(); }, DIALOG_TIMEOUT_MILLISECONDS);
}

bool click_tool(MainWindow& window, int tool_id)  // [Scenes.md#click_tool]
{
    wxToolBar* toolbar = window.GetToolBar();
    const std::optional<wxRect> rect = toolbar != nullptr ? tool_rect(*toolbar, tool_id) : std::nullopt;
    if (!rect) {
        report("Werkzeug nicht in der Werkzeugleiste gefunden, Befehl wird direkt ausgeloest.");
        return guitest::activate_toolbar_tool(window, tool_id);
    }
    const wxPoint center(rect->x + rect->width / 2, rect->y + rect->height / 2);
    glide_to(toolbar->ClientToScreen(center));
    dwell(SHORT_DWELL);
    return guitest::click_on(*toolbar, center);
}

class MenuTracker {  // [Scenes.md#menu-opened-before-next-key]
public:
    explicit MenuTracker(MainWindow& window): window_(window)
    {
        window_.Bind(wxEVT_MENU_OPEN, &MenuTracker::on_open, this);
    }
    ~MenuTracker()
    {
        window_.Unbind(wxEVT_MENU_OPEN, &MenuTracker::on_open, this);
    }
    bool press_until_opened(int key_code)
    {
        const int before = opened_;
        return press_key(key_code) && wait_until([this, before] { return opened_ > before; }, MENU_TIMEOUT_MILLISECONDS);
    }
private:
    void on_open(wxMenuEvent& event)
    {
        ++opened_;
        event.Skip();
    }
    MainWindow& window_;
    int opened_ = 0;
};

bool open_menu(MainWindow& window, int menu_index)  // [Scenes.md#menu-by-keyboard]
{
    MenuTracker tracker(window);
    if (!tracker.press_until_opened(WXK_F10))
        return false;
    for (int i = 0; i < menu_index; ++i)
        if (!tracker.press_until_opened(WXK_RIGHT))
            return false;
    return true;
}

int downs_to(MainWindow& window, int menu_index, int menu_item_id)  // [Scenes.md#downs-to-entry]
{
    const wxMenuBar* bar = window.GetMenuBar();
    if (bar == nullptr || menu_index >= static_cast<int>(bar->GetMenuCount()))
        return -1;
    int downs = 0;
    for (const wxMenuItem* item : bar->GetMenu(static_cast<size_t>(menu_index))->GetMenuItems()) {
        if (item->IsSeparator() || !item->IsEnabled())
            continue;
        ++downs;
        if (item->GetId() == menu_item_id)
            return downs;
    }
    return -1;
}

bool choose_menu_entry(MainWindow& window, int menu_index, int menu_item_id)
{
    const int downs = downs_to(window, menu_index, menu_item_id);
    if (downs < 0)
        return false;
    for (int i = 0; i < downs; ++i)
        if (!press_key(WXK_DOWN))
            return false;
    dwell(SHORT_DWELL);
    return press_key(WXK_RETURN);
}

void close_menu()
{
    press_key(WXK_ESCAPE);
    press_key(WXK_ESCAPE);
}

bool run_menu_item(MainWindow& window, int menu_index, int menu_item_id)  // [Scenes.md#menu-fallback]
{
    const int dialogs_before = ModalDialogWatcher::instance().dialogs_entered();
    if (open_menu(window, menu_index) && choose_menu_entry(window, menu_index, menu_item_id)
        && wait_until([dialogs_before] { return ModalDialogWatcher::instance().dialogs_entered() != dialogs_before; }, DIALOG_TIMEOUT_MILLISECONDS))
        return true;
    close_menu();
    report("Das Menue liess sich nicht per Tastatur bedienen, der Befehl wird direkt ausgeloest.");
    guitest::activate_menu_item(window, menu_item_id);
    pump();
    return true;
}

void pick_directory(const fs::path& directory)  // [Scenes.md#pick_directory]
{
    ModalDialogWatcher::instance().expect(tr("choose_dir_title"), [directory](wxDialog& dialog) {
        auto* dir_dialog = dynamic_cast<wxDirDialog*>(&dialog);
        if (dir_dialog == nullptr || !wait_shown(dialog))
            return;
        dwell(SHORT_DWELL);
        dir_dialog->SetPath(directory.string());
        dwell(DWELL);
        dir_dialog->wxDirDialogBase::SetPath(directory.string());
        dialog.EndModal(wxID_OK);
    });
}

bool hover_tab(wxDialog& dialog, const char* title_msgid, bool click)
{
    wxNotebook* notebook = notebook_of(dialog);
    if (notebook == nullptr)
        return false;
    for (size_t page = 0; page < notebook->GetPageCount(); ++page) {
        if (notebook->GetPageText(page) != tr(title_msgid))
            continue;
        const std::optional<wxRect> rect = tab_rect(*notebook, page);
        if (!rect)
            return false;
        const wxPoint center = notebook->ClientToScreen(wxPoint(rect->x + rect->width / 2, rect->y + rect->height / 2));
        glide_to(center);
        return !click || guitest::click_on(*notebook, wxPoint(rect->x + rect->width / 2, rect->y + rect->height / 2));
    }
    return false;
}

void set_cycle(wxDialog& dialog, int seconds)  // [Scenes.md#cycle]
{
    if (!hover_tab(dialog, "tab_options", true))
        return;
    dwell(SHORT_DWELL);
    glide_to(find(dialog, ID_SYNC_CYCLE_SPIN));
    guitest::set_spin_value(find(dialog, ID_SYNC_CYCLE_SPIN), seconds);
    dwell(SHORT_DWELL);
    hover_tab(dialog, "tab_general", true);
}

void fill_new_sync(wxDialog& dialog, const Options& options)  // [Scenes.md#fill_new_sync]
{
    if (!wait_shown(dialog))
        return;
    dwell(DWELL);
    type_slowly(find(dialog, ID_NAME_BOX), wxString(options.sync_name));
    dwell(SHORT_DWELL);
    glide_to(find(dialog, ID_TYPE_CHOICE));
    guitest::select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Tree));  // [Scenes.md#type-choice]
    dwell(SHORT_DWELL);
    pick_directory(DemoSandbox::source_dir());
    glide_click(find(dialog, ID_BTN_PICK_SOURCE_DIR));
    dwell(SHORT_DWELL);
    pick_directory(DemoSandbox::target_dir());
    glide_click(find(dialog, ID_BTN_PICK_TARGET_DIR));
    dwell(DWELL);
    if (options.cycle_seconds > 0)
        set_cycle(dialog, options.cycle_seconds);
    else
        hover_tab(dialog, "tab_options", false);
    dwell(DWELL);
    glide_click(find(dialog, wxID_OK));
    wait_until([&dialog] { return !dialog.IsShown(); }, DIALOG_TIMEOUT_MILLISECONDS);
}

long row_of(ListCtrl& list, const wxString& name)
{
    for (long row = 0; row < list.GetItemCount(); ++row)
        if (list.GetItemText(row) == name)
            return row;
    return -1;
}

bool scene_create(MainWindow& window, const Options& options)  // [Scenes.md#scenes]
{
    mark("Szene 3: Neue Synchronisation anlegen");
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [&options](wxDialog& dialog) { fill_new_sync(dialog, options); });
    click_tool(window, tul(id::File_new_Synchronization));
    pump();
    const bool created = Config::instance().find_by_name(options.sync_name).has_value();
    mark(created ? "Synchronisation steht in der Liste" : "FEHLER: Synchronisation wurde nicht angelegt");
    dwell(DWELL);
    return created;
}

bool scene_preview(MainWindow& window, const Options&)
{
    mark("Szene 3: Vorschau");
    ModalDialogWatcher::instance().expect(tr("preview_title"), [](wxDialog& dialog) {
        if (!wait_shown(dialog))
            return;
        dwell(LONG_DWELL);
        glide_click(find(dialog, wxID_CANCEL));
    });
    run_menu_item(window, MENU_SYNC, tul(id::Preview));
    const bool finished = guitest::wait_until_sync_finished(window);
    dwell(SHORT_DWELL);
    return finished;
}

bool scene_start(MainWindow& window, const Options& options)
{
    mark("Szene 3: Start");
    click_tool(window, tul(id::StartSync));
    const std::size_t expected = files_below(DemoSandbox::source_dir());
    const bool copied = wait_until([expected] { return files_below(DemoSandbox::target_dir()) >= expected; }, COPY_TIMEOUT_MILLISECONDS);
    mark(copied ? "Erster Lauf fertig, alle Fotos auf dem Ziel" : "FEHLER: erster Lauf nicht fertig");
    dwell(DWELL);
    glide_to(center_of(window));
    mark("Neues Foto kommt in die Quelle (Schnitt: Warten auf den Zyklus)");
    const fs::path arrived = DemoSandbox::target_dir() / "2026" / DemoSandbox::new_photo().filename();
    fs::create_directories(DemoSandbox::source_dir() / "2026");
    fs::copy_file(DemoSandbox::new_photo(), DemoSandbox::source_dir() / "2026" / DemoSandbox::new_photo().filename());
    const int cycle = (options.cycle_seconds > 0 ? options.cycle_seconds : 60) * 1000;  // [Scenes.md#cycle]
    const bool synced = wait_until([&arrived] { return fs::exists(arrived); }, cycle + CYCLE_GRACE_MILLISECONDS);
    mark(synced ? "Neues Foto ist auf dem Ziel angekommen" : "FEHLER: neues Foto nicht angekommen");
    dwell(DWELL);
    return copied && synced;
}

bool scene_background(MainWindow& window, const Options& options)
{
    mark("Szene 4: Fenster schliessen, Programm laeuft weiter");
    glide_to(wxPoint(window.GetScreenPosition().x + window.GetSize().x - 20, window.GetScreenPosition().y - 15));
    dwell(SHORT_DWELL);
    press_key(WXK_F4, wxMOD_ALT);  // [Scenes.md#close-to-tray]
    if (!wait_until([&window] { return !window.IsShown(); }, 1500))
        window.Close();
    dwell(LONG_DWELL);
    window.Show();
    window.Raise();
    dwell(DWELL);
    mark("Szene 4: Ziel nicht erreichbar (Festplatte abgezogen)");
    const fs::path offline = DemoSandbox::target_dir().string() + ".offline";
    fs::rename(DemoSandbox::target_dir(), offline);
    dwell(options.cycle_seconds > 0 ? (options.cycle_seconds + 3) * 1000 : LONG_DWELL);
    fs::rename(offline, DemoSandbox::target_dir());
    mark("Szene 4: Festplatte wieder da");
    dwell(DWELL);
    mark("Szene 4: Foto in der Quelle geloescht, Ziel behaelt es");
    fs::remove(DemoSandbox::photo_to_delete());
    dwell(options.cycle_seconds > 0 ? (options.cycle_seconds + 3) * 1000 : LONG_DWELL);
    mark("Szene 4: Menue Hintergrunddienst zeigen");
    open_menu(window, MENU_BACKGROUND_SERVICE);
    dwell(LONG_DWELL);
    close_menu();
    dwell(SHORT_DWELL);
    return true;
}

bool scene_options(MainWindow& window, const Options& options)
{
    mark("Szene 5: Optionen zeigen");
    auto* list = dynamic_cast<ListCtrl*>(find(window, tul(id::ListCtrl)));
    const long row = list != nullptr ? row_of(*list, wxString(options.sync_name)) : -1;
    if (row < 0)
        return false;
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), [](wxDialog& dialog) {
        if (!wait_shown(dialog))
            return;
        dwell(DWELL);
        hover_tab(dialog, "tab_options", true);
        dwell(LONG_DWELL);
        hover_tab(dialog, "tab_exclusions", true);
        dwell(DWELL);
        glide_click(find(dialog, wxID_CANCEL));
    });
    wxRect rect;
    list->GetItemRect(row, rect);
    glide_to(list->ClientToScreen(wxPoint(rect.x + rect.width / 2, rect.y + rect.height / 2)));
    guitest::click_on(*list, row, true);
    pump();
    dwell(SHORT_DWELL);
    return true;
}

bool scene_ssh(MainWindow& window, const Options&)
{
    mark("Szene 5: Neue SSH-Verbindung zeigen");
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), [](wxDialog& dialog) {
        if (!wait_shown(dialog))
            return;
        dwell(LONG_DWELL);
        glide_click(find(dialog, wxID_CANCEL));
    });
    run_menu_item(window, MENU_FILE, tul(id::File_new_SshConnection));
    dwell(SHORT_DWELL);
    return true;
}

bool scene_assistant(MainWindow& window, const Options& options)  // [Scenes.md#assistant]
{
    mark("Szene 5: Assistent");
    if (!(open_menu(window, MENU_ASSISTANT) && choose_menu_entry(window, MENU_ASSISTANT, tul(id::Chat))
            && wait_until([] { return wxWindow::FindWindowById(ID_CHAT_INPUT) != nullptr; }, DIALOG_TIMEOUT_MILLISECONDS))) {
        close_menu();
        guitest::activate_menu_item(window, tul(id::Chat));
    }
    wxWindow* input = wxWindow::FindWindowById(ID_CHAT_INPUT);
    if (!wait_until([input] { return input != nullptr && input->IsShownOnScreen(); }, DIALOG_TIMEOUT_MILLISECONDS))
        return false;
    dwell(DWELL);
    type_slowly(input, wxString(options.question));
    dwell(SHORT_DWELL);
    glide_click(wxWindow::FindWindowById(ID_CHAT_SEND));
    const bool model_available = !model_location::find().empty();
    if (model_available) {
        wxWindow* transcript = wxWindow::FindWindowById(ID_CHAT_TRANSCRIPT);
        wxWindow* send = wxWindow::FindWindowById(ID_CHAT_SEND);
        const wxString speaker = tr("chat_assistant") + L":";
        const bool answered = wait_until([transcript, send, speaker] {  // [Scenes.md#assistant]
            return guitest::value_of(transcript).Contains(speaker) && send != nullptr && send->IsEnabled();
        }, options.assistant_wait_seconds * 1000);
        mark(answered ? "Assistent hat vollstaendig geantwortet" : "Assistent hat nicht rechtzeitig geantwortet");
        dwell(LONG_DWELL + DWELL);
    } else {
        mark("Kein Sprachmodell gefunden, Antwort entfaellt");
        dwell(DWELL);
    }
    wxWindow* chat = wxGetTopLevelParent(input);
    press_key(WXK_ESCAPE);
    if (!wait_until([chat] { return !chat->IsShown(); }, 1500))
        chat->Hide();
    dwell(SHORT_DWELL);
    return true;
}

bool scene_finale(MainWindow& window, const Options&)
{
    mark("Szene 6: Abschlussbild");
    const wxPoint origin = window.GetScreenPosition();
    const wxSize size = window.GetSize();
    glide_to(wxPoint(origin.x + size.x / 2, origin.y + size.y - 40), GLIDE_MILLISECONDS * 2);
    dwell(LONG_DWELL + DWELL);
    return true;
}


ListCtrl* list_of(MainWindow& window)
{
    return dynamic_cast<ListCtrl*>(find(window, tul(id::ListCtrl)));
}

bool glide_to_row(ListCtrl& list, long row)
{
    wxRect rect;
    if (row < 0 || !list.GetItemRect(row, rect))
        return false;
    return glide_to(list.ClientToScreen(wxPoint(rect.x + rect.width / 3, rect.y + rect.height / 2)));
}

bool hover_tool(MainWindow& window, int tool_id, int milliseconds)  // [Scenes.md#hover_tool]
{
    wxToolBar* toolbar = window.GetToolBar();
    const std::optional<wxRect> rect = toolbar != nullptr ? tool_rect(*toolbar, tool_id) : std::nullopt;
    if (!rect)
        return false;
    glide_to(toolbar->ClientToScreen(wxPoint(rect->x + rect->width / 2, rect->y + rect->height / 2)));
    dwell(milliseconds);
    return true;
}

template <typename Shown>
bool choose_view(MainWindow& window, id view, Shown shown)  // [Scenes.md#choose_view]
{
    if (!(open_menu(window, MENU_VIEW) && choose_menu_entry(window, MENU_VIEW, tul(view)) && wait_until(shown, MENU_TIMEOUT_MILLISECONDS))) {
        close_menu();
        report("Das Menue Ansicht liess sich nicht per Tastatur bedienen, die Ansicht wird direkt umgeschaltet.");
        guitest::show_view(window, view);
    }
    pump();
    return shown();
}

bool scene_tour_window(MainWindow& window, const Options&)
{
    mark("Folge 1: Hauptfenster mit drei Synchronisationen");
    ListCtrl* list = list_of(window);
    if (list == nullptr || list->GetItemCount() < 3)
        return false;
    for (long row = 0; row < list->GetItemCount(); ++row) {
        mark("Zeile " + list->GetItemText(row));
        glide_to_row(*list, row);
        dwell(DWELL);
    }
    guitest::click_on(*list, row_of(*list, L"Fotos"));
    dwell(SHORT_DWELL);
    return true;
}

bool scene_tour_toolbar(MainWindow& window, const Options&)
{
    const std::pair<id, const char*> tools[] = {
        {id::File_new_Synchronization, "Neue Synchronisation"}, {id::EditSync, "Bearbeiten"}, {id::File_save, "Speichern"},
        {id::DeleteSync, "Loeschen"}, {id::StartSync, "Start"}, {id::Help, "Hilfe"}, {id::EditSettings, "Einstellungen"}};
    bool all = true;
    for (const auto& [tool, label] : tools) {
        mark(wxString("Werkzeug: ") + label);
        all = hover_tool(window, tul(tool), DWELL) && all;
    }
    return all;
}

bool scene_tour_status(MainWindow& window, const Options&)
{
    mark("Statuszeile");
    wxStatusBar* bar = window.GetStatusBar();
    wxRect field;
    if (bar == nullptr || !bar->GetFieldRect(bar->GetFieldsCount() - 1, field))
        return false;
    glide_to(bar->ClientToScreen(wxPoint(field.x + field.width / 3, field.y + field.height / 2)));
    dwell(LONG_DWELL);
    return true;
}

bool scene_tour_views(MainWindow& window, const Options&)
{
    mark("Ansicht Baum");
    const bool tree = choose_view(window, id::View_Tree, [&window] { return guitest::source_tree(window) != nullptr; });
    for (wxTreeCtrl* side : {guitest::source_tree(window), guitest::target_tree(window)})  // [Scenes.md#expanded-trees]
        if (side != nullptr)
            side->ExpandAll();
    glide_to(center_of(window));
    dwell(LONG_DWELL);
    mark("Ansicht Tabelle");
    const bool grid = choose_view(window, id::View_Grid, [&window] { return guitest::grid_view(window) != nullptr; });
    glide_to(center_of(window));
    dwell(LONG_DWELL);
    return tree && grid;
}

bool scene_tour_active(MainWindow& window, const Options&)  // [Scenes.md#tour_active]
{
    GridCtrl* grid = guitest::grid_view(window);
    int row = -1;
    for (int r = 0; grid != nullptr && r < grid->GetNumberRows(); ++r)
        if (grid->GetRowLabelValue(r) == L"Dokumente")
            row = r;
    if (row < 0)
        return false;
    mark("Haekchen Aktiv bei Dokumente wegnehmen");
    const wxRect cell = grid->CellToRect(row, 0);
    int x = 0, y = 0;
    grid->CalcScrolledPosition(cell.x + cell.width / 2, cell.y + cell.height / 2, &x, &y);
    wxWindow* cells = grid->GetGridWindow();
    grid->SetGridCursor(row, 0);  // [Scenes.md#tour_active]
    glide_to(cells->ClientToScreen(wxPoint(x, y)));
    dwell(SHORT_DWELL);
    const auto inactive = [] { const auto item = Config::instance().find_by_name(L"Dokumente"); return item && !item->enabled(); };
    guitest::click_on(*cells, wxPoint(x, y));
    if (!wait_until(inactive, SHORT_DWELL)) {
        report("Klick auf das Haekchen hat nicht umgeschaltet, der Wert wird direkt gesetzt.");
        guitest::change_grid_cell(*grid, row, 0, wxEmptyString);
    }
    mark(inactive() ? "Dokumente ist inaktiv" : "FEHLER: Dokumente ist noch aktiv");
    dwell(LONG_DWELL);
    return inactive();
}

bool scene_tour_edit(MainWindow& window, const Options&)
{
    mark("Ansicht Liste");
    choose_view(window, id::View_List, [&window] { return list_of(window) != nullptr; });
    dwell(SHORT_DWELL);
    ListCtrl* list = list_of(window);
    const long row = list != nullptr ? row_of(*list, L"Fotos") : -1;
    if (row < 0)
        return false;
    mark("Doppelklick auf Fotos");
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), [](wxDialog& dialog) {
        if (!wait_shown(dialog))
            return;
        dwell(LONG_DWELL + DWELL);
        glide_click(find(dialog, wxID_CANCEL));
    });
    glide_to_row(*list, row);
    dwell(SHORT_DWELL);
    guitest::click_on(*list, row, true);
    pump();
    mark("Dialog geschlossen");
    dwell(SHORT_DWELL);
    return true;
}

bool scene_tour_delete(MainWindow& window, const Options&)  // [Scenes.md#tour_delete]
{
    ListCtrl* list = list_of(window);
    const long row = list != nullptr ? row_of(*list, L"Haushaltsbuch") : -1;
    if (row < 0)
        return false;
    mark("Haushaltsbuch auswaehlen");
    glide_to_row(*list, row);
    guitest::click_on(*list, row);
    dwell(DWELL);
    mark("Loeschen-Symbol");
    ModalDialogWatcher::instance().expect(tr("confirm_delete_title"), [](wxDialog& dialog) {
        dwell(LONG_DWELL);
        if (!click_message_button(dialog, wxID_YES))
            report("Knopf der Sicherheitsabfrage nicht gefunden, die Abfrage wird direkt bestaetigt.");
    });
    click_tool(window, tul(id::DeleteSync));
    pump();
    const bool deleted = !Config::instance().find_by_name(L"Haushaltsbuch").has_value();
    mark(deleted ? "Haushaltsbuch geloescht" : "FEHLER: Haushaltsbuch nicht geloescht");
    glide_to(center_of(window));
    dwell(DWELL);
    return deleted;
}

using Scene = bool (*)(MainWindow&, const Options&);

const std::map<std::string, Scene> SCENES = {
    {"create", scene_create}, {"preview", scene_preview}, {"start", scene_start}, {"background", scene_background},
    {"options", scene_options}, {"ssh", scene_ssh}, {"assistant", scene_assistant}, {"finale", scene_finale},
    {"tour_window", scene_tour_window}, {"tour_toolbar", scene_tour_toolbar}, {"tour_status", scene_tour_status},
    {"tour_views", scene_tour_views}, {"tour_active", scene_tour_active}, {"tour_edit", scene_tour_edit},
    {"tour_delete", scene_tour_delete},
};

}

bool run_scene(const std::string& name, MainWindow& window, const Options& options)
{
    const auto scene = SCENES.find(name);
    if (scene == SCENES.end())
        return false;
    ModalDialogWatcher::instance().reset();
    const bool ok = scene->second(window, options);
    for (const wxString& unexpected : ModalDialogWatcher::instance().unexpected_dialogs())
        std::cerr << "Szene " << name << ": unerwarteter Dialog abgebrochen: " << unexpected << std::endl;
    return ok;
}

}
