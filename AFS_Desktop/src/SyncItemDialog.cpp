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
#include "SyncItemDialog.h"
#include <sstream>
#include <filesystem>
#include <optional>
#include <wx/sizer.h>
#include "wx/filedlg.h"
#include <wx/dirdlg.h>
#include <wx/choicdlg.h>
#include "SyncItem.h"
#include "Config.h"
#include "CloudFolders.h"
#include "Util.h"
#include "Logging.h"
#include "SshAccountSelectDialog.h"
#include "SshDirDialog.h"
#include "PhoneDirDialog.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

namespace fs = std::filesystem;
using util::wstring_to_string;
using util::string_to_wstring;
using std::wstring;

constexpr int MAX_TIME_TOLERANCE_SECONDS = 3600;  // [SyncItemDialog.md#tolerance-beside-time]

BEGIN_EVENT_TABLE(SyncItemDialog, wxDialog)
    EVT_UPDATE_UI(wxID_OK, SyncItemDialog::on_update_ok_button)
END_EVENT_TABLE()

SyncItemDialog::SyncItemDialog(SyncItem* sync_item, wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style ,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    wxPanel* tab1 = new wxPanel(notebook);
    wxBoxSizer* tab1_sizer = new wxBoxSizer(wxVERTICAL);
    tab1_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(720, 10)));

    wxBoxSizer* name_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* name_lable = new wxStaticText(tab1, wxID_STATIC, utilwx::field_label("name"));
    name_lable->SetMinSize(wxSize(FromDIP(60), name_lable->GetMinSize().y));
    name_sizer->Add(name_lable, 0, wxALIGN_CENTER_VERTICAL);
    name_box_ = new wxTextCtrl(tab1, ID_NAME_BOX);
    name_sizer->Add(name_box_, 1);
    tab1_sizer->Add(name_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* type_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* type_lable = new wxStaticText(tab1, wxID_STATIC, utilwx::field_label("type"));
    type_lable->SetMinSize(wxSize(FromDIP(60), type_lable->GetMinSize().y));
    type_sizer->Add(type_lable, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString types;
    types.push_back(sync_item_type_to_str(SyncItemType::File));
    types.push_back(sync_item_type_to_str(SyncItemType::Dir));
    types.push_back(sync_item_type_to_str(SyncItemType::Tree));

    type_choice_ = new wxChoice(tab1, ID_TYPE_CHOICE, wxDefaultPosition, wxDefaultSize, types);
    type_sizer->Add(type_choice_, 1);

    wxStaticText* wildcard_lable = new wxStaticText(tab1, wxID_STATIC, utilwx::field_label("wildcard"));
    wildcard_lable->SetMinSize(wxSize(FromDIP(75), wildcard_lable->GetMinSize().y));
    type_sizer->Add(wildcard_lable, wxSizerFlags().Border(wxLEFT).CenterVertical());

    wildcard_box_ = new wxTextCtrl(tab1, ID_WILDCARD_BOX);
    type_sizer->Add(wildcard_box_, wxSizerFlags().Border(wxLEFT));

    tab1_sizer->Add(type_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* source_dir_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* source_dir_lable = new wxStaticText(tab1, wxID_STATIC, utilwx::field_label("source"));
    source_dir_lable->SetMinSize(wxSize(FromDIP(60), source_dir_lable->GetMinSize().y));
    source_dir_sizer->Add(source_dir_lable, 0, wxALIGN_CENTER_VERTICAL);

    source_dir_box_ = new wxTextCtrl(tab1, ID_SOURCE_DIR_BOX);
    source_dir_sizer->Add(source_dir_box_, 1);
    wxButton* btn_pick_source_dir_text = new wxButton(tab1, ID_BTN_PICK_SOURCE_DIR, translation::translate("button_choose"));
    source_dir_sizer->Add(btn_pick_source_dir_text, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btn_ssh_source = new wxButton(tab1, ID_BTN_SSH_SOURCE, L"SSH\u2026");
    source_dir_sizer->Add(btn_ssh_source, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btn_phone_source = new wxButton(tab1, ID_BTN_PHONE_SOURCE, translation::translate("button_phone"));  // [SyncItemDialog.md#phone-buttons]
    source_dir_sizer->Add(btn_phone_source, wxSizerFlags().Border(wxLEFT, 2));

    tab1_sizer->Add(source_dir_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* target_dir_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* target_dir_lable = new wxStaticText(tab1, wxID_STATIC, utilwx::field_label("target"));
    target_dir_lable->SetMinSize(wxSize(FromDIP(60), target_dir_lable->GetMinSize().y));
    target_dir_sizer->Add(target_dir_lable, 0, wxALIGN_CENTER_VERTICAL);

    target_dir_box_ = new wxTextCtrl(tab1, ID_TARGET_DIR_BOX);
    target_dir_sizer->Add(target_dir_box_, 1);

    wxButton* btn_pick_target_dir_text = new wxButton(tab1, ID_BTN_PICK_TARGET_DIR, translation::translate("button_choose"));
    target_dir_sizer->Add(btn_pick_target_dir_text, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btn_ssh_target = new wxButton(tab1, ID_BTN_SSH_TARGET, L"SSH\u2026");
    target_dir_sizer->Add(btn_ssh_target, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btn_phone_target = new wxButton(tab1, ID_BTN_PHONE_TARGET, translation::translate("button_phone"));
    target_dir_sizer->Add(btn_phone_target, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btn_cloud_target = new wxButton(tab1, ID_BTN_CLOUD_TARGET, translation::translate("button_cloud"));  // [SyncItemDialog.md#cloud-button]
    target_dir_sizer->Add(btn_cloud_target, wxSizerFlags().Border(wxLEFT, 2));

    tab1_sizer->Add(target_dir_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    tab1->SetSizer(tab1_sizer);
    notebook->AddPage(tab1, translation::translate("tab_general"));

    notebook->AddPage(create_options_page(notebook), translation::translate("tab_options"));

    wxPanel* tab3 = new wxPanel(notebook);
    wxBoxSizer* tab3_sizer = new wxBoxSizer(wxVERTICAL);
    tab3_sizer->Add(new wxStaticText(tab3, wxID_STATIC, translation::translate("exclusions_hint")), 0, wxALL, 5);
    exclusions_box_ = new wxTextCtrl(tab3, ID_EXCLUSIONS_BOX, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    tab3_sizer->Add(exclusions_box_, 1, wxEXPAND | wxALL, 5);
    tab3->SetSizer(tab3_sizer);
    notebook->AddPage(tab3, translation::translate("tab_exclusions"));

    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    btn_pick_source_dir_text->Bind(wxEVT_BUTTON, &SyncItemDialog::on_pick_source_dir_text, this);
    btn_pick_target_dir_text->Bind(wxEVT_BUTTON, &SyncItemDialog::on_pick_target_dir_text, this);
    btn_ssh_source->Bind(wxEVT_BUTTON, &SyncItemDialog::on_ssh_source, this);
    btn_ssh_target->Bind(wxEVT_BUTTON, &SyncItemDialog::on_ssh_target, this);
    btn_phone_source->Bind(wxEVT_BUTTON, &SyncItemDialog::on_phone_source, this);
    btn_phone_target->Bind(wxEVT_BUTTON, &SyncItemDialog::on_phone_target, this);
    btn_cloud_target->Bind(wxEVT_BUTTON, &SyncItemDialog::on_cloud_target, this);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(300, 100)));
    Fit();
    Centre();
    if (sync_item != nullptr)
        set_data(*sync_item);
    event_bindings();
}

wxPanel* SyncItemDialog::create_options_page(wxNotebook* notebook)  // [SyncItemDialog.md#options-grouped]
{
    wxPanel* page = new wxPanel(notebook);
    wxBoxSizer* page_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* behaviour_sizer = new wxBoxSizer(wxVERTICAL);
    const auto add_option = [&](wxCheckBox*& check, wxWindowID id, const char* msgid) {
        check = new wxCheckBox(page, id, translation::translate(msgid));
        behaviour_sizer->Add(check, 0, wxBOTTOM, 5);
    };
    add_option(enabled_check_, ID_ENABLED_CHECK, "option_enabled");
    add_option(del_source_after_check_, ID_DEL_SOURCE_AFTER_CHECK, "option_delete_source_after_copy");
    add_option(two_way_sync_check_, ID_TWO_WAY_SYNC_CHECK, "option_two_way_sync");
    add_option(copy_when_target_newer_check_, ID_CPY_WHEN_TARGET_NEWER_CHECK, "option_copy_when_target_newer");
    add_option(father_son_principle_check_, ID_FATHER_SON_PRINCIPLE_CHECK, "option_father_son_principle");
    add_option(delete_in_target_check_, ID_DELETE_IN_TARGET_CHECK, "option_delete_in_target");
    add_option(backup_proof_check_, ID_BACKUP_PROOF_CHECK, "option_backup_proof");
    add_option(sync_empty_files_check_, ID_SYNC_EMPTY_FILES_CHECK, "option_sync_empty_files");
    add_option(warn_if_unreachable_check_, ID_WARN_IF_UNREACHABLE_CHECK, "option_warn_if_unreachable");
    add_option(track_moves_check_, ID_TRACK_MOVES_CHECK, "option_track_moves");
    add_option(conflict_copies_check_, ID_CONFLICT_COPIES_CHECK, "option_conflict_copies");
    add_option(watch_changes_check_, ID_WATCH_CHANGES_CHECK, "option_watch_changes");
    two_way_hint_ = new wxStaticText(page, ID_TWO_WAY_HINT, translation::translate("two_way_hint"));  // [SyncItemDialog.md#two-way-hint]
    two_way_hint_->Wrap(FromDIP(HINT_WRAP_WIDTH));
    behaviour_sizer->Add(two_way_hint_, 0, wxBOTTOM, 5);
    wxBoxSizer* sync_cycle_sizer = new wxBoxSizer(wxHORIZONTAL);
    sync_cycle_sizer->Add(new wxStaticText(page, wxID_STATIC, translation::translate("pause_seconds")), 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
    sync_cycle_spin_ = new wxSpinCtrl(page, ID_SYNC_CYCLE_SPIN, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 60, 1);
    sync_cycle_sizer->Add(sync_cycle_spin_);
    behaviour_sizer->Add(sync_cycle_sizer, 0, wxTOP, 5);
    page_sizer->Add(behaviour_sizer, 1, wxEXPAND | wxALL, 10);
    wxStaticBoxSizer* compare_sizer = new wxStaticBoxSizer(wxVERTICAL, page, translation::translate("compare_by"));
    wxStaticBox* box = compare_sizer->GetStaticBox();
    compare_date_check_ = new wxCheckBox(box, ID_COMPARE_DATE_CHECK, translation::translate("compare_time"));
    compare_size_check_ = new wxCheckBox(box, ID_COMPARE_SIZE_CHECK, translation::translate("size"));
    compare_content_check_ = new wxCheckBox(box, ID_COMPARE_CONTENT_CHECK, translation::translate("compare_content"));
    for (wxCheckBox* check : {compare_date_check_, compare_size_check_, compare_content_check_})
        compare_sizer->Add(check, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    wxBoxSizer* tolerance_sizer = new wxBoxSizer(wxHORIZONTAL);  // [SyncItemDialog.md#tolerance-beside-time]
    tolerance_sizer->Add(new wxStaticText(box, wxID_STATIC, translation::translate("time_tolerance_seconds")), 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
    time_tolerance_spin_ = new wxSpinCtrl(box, ID_TIME_TOLERANCE_SPIN, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, MAX_TIME_TOLERANCE_SECONDS, DEFAULT_TIME_TOLERANCE_SECONDS);
    tolerance_sizer->Add(time_tolerance_spin_);
    compare_sizer->Add(tolerance_sizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    page_sizer->Add(compare_sizer, 0, wxTOP | wxRIGHT | wxBOTTOM, 10);
    page->SetSizer(page_sizer);
    return page;
}

namespace {
void lock(wxCheckBox* check, bool locked)  // [SyncItemDialog.md#option-locks]
{
    if (locked)
        check->SetValue(false);
    check->Enable(!locked);
}
}

void SyncItemDialog::update_option_locks()  // [SyncItemDialog.md#option-locks]
{
    const bool move = del_source_after_check_->GetValue();
    lock(two_way_sync_check_, move);
    lock(delete_in_target_check_, move || act_sync_item_type(type_choice_) == SyncItemType::File);
    const bool two_way = two_way_sync_check_->GetValue();
    lock(copy_when_target_newer_check_, two_way || !(compare_date_check_->GetValue() || compare_size_check_->GetValue()));
    two_way_sync_check_->Enable(!move && !copy_when_target_newer_check_->GetValue());
    del_source_after_check_->Enable(!two_way && !delete_in_target_check_->GetValue());
    time_tolerance_spin_->Enable(compare_date_check_->GetValue());
    if (two_way)  // [SyncItemDialog.md#track-moves-implied]
        track_moves_check_->SetValue(true);
    track_moves_check_->Enable(!two_way && act_sync_item_type(type_choice_) != SyncItemType::File);
    conflict_copies_check_->Enable(two_way);  // [SyncItemDialog.md#conflict-copies-two-way-only]
    if (two_way_hint_->Show(two_way))  // [SyncItemDialog.md#two-way-hint]
        two_way_hint_->GetParent()->Layout();
}

void SyncItemDialog::event_bindings()
{
    for (wxCheckBox* check : {del_source_after_check_, two_way_sync_check_, copy_when_target_newer_check_, delete_in_target_check_,
                              compare_date_check_, compare_size_check_, compare_content_check_})
        check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { update_option_locks(); });
    type_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { update_option_locks(); });
}

SyncItemDialog::~SyncItemDialog()
{}

void SyncItemDialog::set_data(const SyncItem& si)
{
    wildcard_ = si.wildcard();
    exclusions_.Clear();
    for (const auto& pattern : si.exclusions()) {
        if (!exclusions_.empty())
            exclusions_ += "\n";
        exclusions_ += wxString(pattern);
    }
    source_ = si.source();
    target_ = si.target();
    name_ = si.name();
    type_ = si.item_type_str();
    enabled_ = si.enabled();
    del_source_after_ = si.del_aftercopy();
    compare_date_ = si.compare_items().time();
    time_tolerance_seconds_ = si.compare_items().time_tolerance_seconds();
    two_way_sync_ = si.two_way_sync();
    compare_size_ = si.compare_items().size();
    copy_when_target_newer_ = si.copy_when_target_newer();
    compare_content_ = si.compare_items().content();
    father_son_principle_ = si.father_son_principle();
    delete_in_target_ = si.delete_in_target();
    backup_proof_ = si.backup_proof();
    sync_empty_files_ = si.sync_empty_files();
    warn_if_unreachable_ = si.warn_if_unreachable();
    track_moves_ = si.track_moves();
    conflict_copies_ = si.conflict_copies();
    watch_changes_ = si.watch_changes();
    sync_cycle_ = si.sync_cycle();
    
}

void SyncItemDialog::save_data(SyncItem& si)
{
    log_info("SyncItemDialog::save_data called");
    si.name(name_.ToStdWstring());
    si.wildcard(wildcard_.ToStdWstring());
    {
        Exclusions exclusions;  // [SyncItemDialog.md#exclusions-one-per-line]
        std::wistringstream lines(exclusions_.ToStdWstring());
        std::wstring line;
        while (std::getline(lines, line)) {
            const auto first = line.find_first_not_of(L" \t\r");
            if (first == std::wstring::npos)
                continue;
            const auto last = line.find_last_not_of(L" \t\r");
            exclusions.push_back(line.substr(first, last - first + 1));
        }
        si.exclusions(exclusions);
    }
    si.source(source_.ToStdWstring());
    si.target(target_.ToStdWstring());
    si.sync_item_type(str_to_sync_item_type(type_.ToStdWstring()));
    si.enabled(enabled_);
    si.del_aftercopy(del_source_after_);
    CompareItems compare_items;
    compare_items.time(compare_date_);
    compare_items.time_tolerance_seconds(time_tolerance_seconds_);
    si.two_way_sync(two_way_sync_);
    compare_items.size(compare_size_);
    si.copy_when_target_newer(copy_when_target_newer_);
    compare_items.content(compare_content_);
    si.compare_items(compare_items);
    si.father_son_principle(father_son_principle_);
    si.delete_in_target(delete_in_target_);
    si.backup_proof(backup_proof_);
    si.sync_empty_files(sync_empty_files_);
    si.warn_if_unreachable(warn_if_unreachable_);
    si.track_moves(track_moves_);
    si.conflict_copies(conflict_copies_);
    si.watch_changes(watch_changes_);
    si.sync_cycle(sync_cycle_);
}

void SyncItemDialog::on_update_ok_button(wxUpdateUIEvent& event)
{
    event.Enable(false);
    if (!source_dir_box_->GetValue().empty() && !target_dir_box_->GetValue().empty()) {  // [SyncItemDialog.md#empty-wildcard-allowed]
        int selection = type_choice_->GetSelection();
        if (selection != wxNOT_FOUND && (compare_size_check_->GetValue() || compare_content_check_->GetValue() || compare_date_check_->GetValue()))
            event.Enable(true);
    }
}

bool SyncItemDialog::TransferDataToWindow()
{
    name_box_->SetValue(name_);
    wildcard_box_->SetValue(wildcard_);
    exclusions_box_->SetValue(exclusions_);
    source_dir_box_->SetValue(source_);
    target_dir_box_->SetValue(target_);
    int selection = type_choice_->FindString(type_);
    if (selection != wxNOT_FOUND)
        type_choice_->SetSelection(selection);
    enabled_check_->SetValue(enabled_);
    del_source_after_check_->SetValue(del_source_after_);
    compare_date_check_->SetValue(compare_date_);
    two_way_sync_check_->SetValue(two_way_sync_);
    compare_size_check_->SetValue(compare_size_);
    copy_when_target_newer_check_->SetValue(copy_when_target_newer_);
    compare_content_check_->SetValue(compare_content_);
    father_son_principle_check_->SetValue(father_son_principle_);
    delete_in_target_check_->SetValue(delete_in_target_);
    backup_proof_check_->SetValue(backup_proof_);
    sync_empty_files_check_->SetValue(sync_empty_files_);
    warn_if_unreachable_check_->SetValue(warn_if_unreachable_);
    track_moves_check_->SetValue(track_moves_);
    conflict_copies_check_->SetValue(conflict_copies_);
    watch_changes_check_->SetValue(watch_changes_);
    sync_cycle_spin_->SetValue(sync_cycle_);
    time_tolerance_spin_->SetValue(time_tolerance_seconds_);
    update_option_locks();
    return true;
}

bool SyncItemDialog::TransferDataFromWindow()
{
    name_ = name_box_->GetValue();
    wildcard_ = wildcard_box_->GetValue();
    exclusions_ = exclusions_box_->GetValue();
    source_ = source_dir_box_->GetValue();
    target_ = target_dir_box_->GetValue();
    int selection = type_choice_->GetSelection();
    if (selection != wxNOT_FOUND)
        type_ = type_choice_->GetString(selection);
    else
        type_ = "";
    enabled_ = enabled_check_->GetValue();
    del_source_after_ = del_source_after_check_->GetValue();
    compare_date_ = compare_date_check_->GetValue();
    two_way_sync_ = two_way_sync_check_->GetValue();
    compare_size_ = compare_size_check_->GetValue();
    copy_when_target_newer_ = copy_when_target_newer_check_->GetValue();
    compare_content_ = compare_content_check_->GetValue();
    father_son_principle_ = father_son_principle_check_->GetValue();
    delete_in_target_ = delete_in_target_check_->GetValue();
    backup_proof_ = backup_proof_check_->GetValue();
    sync_empty_files_ = sync_empty_files_check_->GetValue();
    warn_if_unreachable_ = warn_if_unreachable_check_->GetValue();
    track_moves_ = track_moves_check_->GetValue();
    conflict_copies_ = conflict_copies_check_->GetValue();
    watch_changes_ = watch_changes_check_->GetValue();
    sync_cycle_ = sync_cycle_spin_->GetValue();
    time_tolerance_seconds_ = static_cast<uint16_t>(time_tolerance_spin_->GetValue());
    return true;
}

wxString with_volumen_name(const wxString& path)
{
#ifdef _UNIX
    return path;  // [SyncItemDialog.md#no-volume-spec-on-unix]
#else
    wstring ws_path = path.ToStdWstring();
    fs::path fs_path;
    std::string path_str = wstring_to_string(ws_path);
	try {
        fs_path = fs::path(path_str);
	}
	catch (std::exception& e) {
		log_error(translation::translate("volume_name_error") + string_to_wstring(e.what()));
		return path;
	}
    if (fs_path.empty() || !fs_path.has_root_name())
		return path;
    wstring root_name = fs_path.root_name().wstring();
    wstring relative_path = ws_path.substr(root_name.size());
    TCHAR volume_name[MAX_PATH + 1] = { 0 };
    TCHAR file_system_name[MAX_PATH + 1] = { 0 };
    DWORD serial_number = 0;
    DWORD max_component_length = 0;
    DWORD file_system_flags = 0;
    if (GetVolumeInformation(
        root_name.c_str(),
        volume_name,
        ARRAYSIZE(volume_name),
        &serial_number,
        &max_component_length,
        &file_system_flags,
        file_system_name,
        ARRAYSIZE(file_system_name)
    )) {
        std::wstringstream os;
        os << volume_name << L" (" << root_name << L")" << relative_path;
        return os.str();
    }
    else {
        return path;
    }
#endif
}

wxString choose_file(SyncItemDialog* sync_item_dialog)
{
    wxFileDialog dialog
    (
        sync_item_dialog,
        translation::translate("choose_file_title"),
        wxEmptyString,
        wxEmptyString,
        translation::translate("all_files_wildcard", {wxASCII_STR(wxFileSelectorDefaultWildcardStr).ToStdWstring(), wxASCII_STR(wxFileSelectorDefaultWildcardStr).ToStdWstring()})
    );
    dialog.CentreOnParent();
    dialog.SetDirectory(wxGetHomeDir());
    if (dialog.ShowModal() == wxID_OK)
    {
        return with_volumen_name(dialog.GetPath());
    }
    return "";
}

wxString choose_dir(SyncItemDialog* sync_item_dialog, const wxString& title = translation::translate("choose_dir_title"), const wxString& start_dir = wxGetHomeDir())
{
    wxDirDialog dialog(sync_item_dialog, title, start_dir, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK)
    {
        return with_volumen_name(dialog.GetPath());
    }
    return "";
}

SyncItemType SyncItemDialog::act_sync_item_type(wxChoice* type_choice)
{
    auto selection = type_choice->GetSelection();
    wxString stype;
    if (selection != wxNOT_FOUND) {
        stype = type_choice->GetString(selection);
        return str_to_sync_item_type(stype.ToStdWstring());
    }
    else
        return SyncItemType::Undefined;
}

void SyncItemDialog::on_pick_source_dir_text(wxCommandEvent& WXUNUSED(event))
{
    SyncItemType sync_type = act_sync_item_type(type_choice_);
    wxString value;
    if (sync_type == SyncItemType::File)
        value = choose_file(this);
    else
        value = choose_dir(this);
    if (!value.empty())
        source_dir_box_->SetValue(value);
}

void SyncItemDialog::on_pick_target_dir_text(wxCommandEvent& WXUNUSED(event))
{
    wxString value = choose_dir(this);
    if (!value.empty())
        target_dir_box_->SetValue(value);
}

bool SyncItemDialog::Validate()
{
    auto name = name_box_->GetValue();
    if (name.IsEmpty()) {
        wxMessageBox(translation::translate("name_empty"), translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        return false;
    }
    if (name != name_ && Config::instance().find_by_name(name.ToStdWstring()) != std::nullopt) {  // [SyncItemDialog.md#name-compared-against-original]
        wxMessageBox(translation::translate("sync_exists", {name.ToStdWstring()}), translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        return false;
    }
    return true;
}

void SyncItemDialog::on_ssh_source(wxCommandEvent& event)
{
    handle_ssh_button(source_dir_box_);
}

void SyncItemDialog::on_ssh_target(wxCommandEvent& event)
{
    handle_ssh_button(target_dir_box_);
}

void SyncItemDialog::on_phone_source(wxCommandEvent&)
{
    handle_phone_button(source_dir_box_);
}

void SyncItemDialog::on_phone_target(wxCommandEvent&)
{
    handle_phone_button(target_dir_box_);
}

void SyncItemDialog::handle_phone_button(wxTextCtrl* target_box)  // [SyncItemDialog.md#phone-buttons]
{
    if (Config::instance().devices_snapshot().empty()) {
        wxMessageBox(translation::translate("no_devices_paired"), translation::translate("info"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    PhoneDirDialog dlg(this, wxID_ANY, txt::phone_dir(), target_box->GetValue());
    if (dlg.ShowModal() == wxID_OK)
        target_box->SetValue(dlg.path());
}

namespace {
std::optional<CloudFolder> choose_cloud_folder(wxWindow* parent)  // [SyncItemDialog.md#cloud-button]
{
    const CloudFolders folders = cloud_folders::detect();
    if (folders.empty()) {
        wxMessageBox(translation::translate("no_cloud_folder_found"), translation::translate("info"), wxOK | wxICON_INFORMATION, parent);
        return std::nullopt;
    }
    if (folders.size() == 1)
        return folders.front();
    wxArrayString choices;
    for (const CloudFolder& folder : folders)
        choices.push_back(folder.provider + L" \u2013 " + folder.path.wstring());
    wxSingleChoiceDialog dialog(parent, translation::translate("choose_cloud_folder"), txt::cloud_folder(), choices);
    if (dialog.ShowModal() != wxID_OK)
        return std::nullopt;
    return folders[dialog.GetSelection()];
}
}

void SyncItemDialog::on_cloud_target(wxCommandEvent&)  // [SyncItemDialog.md#cloud-button]
{
    const auto folder = choose_cloud_folder(this);
    if (!folder)
        return;
    const wxString chosen = choose_dir(this, translation::translate("choose_cloud_dir_title", {folder->provider}), folder->path.wstring());
    if (!chosen.empty())
        target_dir_box_->SetValue(chosen);
}

void SyncItemDialog::handle_ssh_button(wxTextCtrl* target_box)
{
    SshAccount account;
    wxString current_value = target_box->GetValue();
    
    int colon_pos = current_value.Find(':');  // does the value hold an SSH path of the form "sshname:/path"?
    if (colon_pos != wxNOT_FOUND && colon_pos > 0) {
        wxString ssh_name = current_value.Left(colon_pos);
        auto found = Config::instance().find_ssh_account_by_name(std::string(ssh_name.ToUTF8()));  // is this name a configured SSH account?
        if (found.has_value())
            account = found.value();
    }
    
    if (account.name().empty()) {  // no account found: let the user pick one
        SshAccountSelectDialog select_dlg(this);
        if (select_dlg.ShowModal() == wxID_OK) {
            auto selected_account = select_dlg.get_selected_account();
            if (selected_account.has_value()) {
                account = selected_account.value();
            } else {
                return;
            }
        } else {
            return;
        }
    }
    
    wxString initial_path = "/";  // keep the remote path that is already configured
    if (colon_pos != wxNOT_FOUND) {
        initial_path = current_value.Mid(colon_pos + 1);
        if (initial_path.empty()) {
            initial_path = "/";
        }
    }
    
    SshDirDialog dir_dlg(this, txt::select_ssh_dir(), account, std::string(initial_path.ToUTF8()));
    if (dir_dlg.ShowModal() == wxID_OK) {
        wxString selected_path = dir_dlg.get_path();
        wxString full_path = wxString::FromUTF8(account.name()) + ":" + selected_path;
        target_box->SetValue(full_path);
    }
}
