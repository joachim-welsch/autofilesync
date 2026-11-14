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
#include "Menu.h"
#include "id.h"
#include "utilwx.h"
#include "Translation.h"

using utilwx::tul;
using AFS_Core::translation::translate;

namespace {
wxString with_shortcut(const wxString& label, const char* shortcut)  // [Menu.md#shortcuts]
{
    return label + "\t" + shortcut;
}

#ifdef __WXOSX__
constexpr const char* HELP_SHORTCUT = "Ctrl+?";
#else
constexpr const char* HELP_SHORTCUT = "F1";
#endif
}

wxMenuBar*  Menu::menu_bar()
{
	return menu_bar_;
}

wxMenu* Menu::edit_menu()
{
	return edit_menu_;
}

wxMenu* Menu::view_menu()
{
	return view_menu_;
}

wxMenu* Menu::sync_menu()
{
	return sync_menu_;
}

wxMenu* Menu::background_service_menu()
{
	return background_service_menu_;
}

wxMenu* Menu::create_file_menu()
{
    wxMenu* file_menu = new wxMenu;  // [Menu.md#structure]
    file_menu->Append(tul(id::File_new_Synchronization), with_shortcut(translate("menu_new_synchronisation"), "Ctrl+N"));
    file_menu->Append(tul(id::File_new_SshConnection), translate("menu_new_ssh_connection"));
    file_menu->Append(tul(id::File_new_WebDavConnection), translate("menu_new_webdav_connection"));
    file_menu->Append(tul(id::File_connect_phone), translate("menu_connect_phone"));  // [Menu.md#phone-entries]
    file_menu->AppendSeparator();
    file_menu->Append(tul(id::File_open), with_shortcut(translate("menu_open"), "Ctrl+O"));
    file_menu->Append(tul(id::File_save), with_shortcut(translate("menu_save"), "Ctrl+S"));
    file_menu->Append(tul(id::File_save_as), with_shortcut(translate("menu_save_as"), "Ctrl+Shift+S"));
    file_menu->AppendSeparator();
    file_menu->Append(tul(id::Quit), with_shortcut(translate("menu_quit"), "Ctrl+Q"), translate("menu_quit_help"));
    return file_menu;
}

wxMenu* Menu::create_help_menu()
{
    wxMenu* help_menu = new wxMenu;  // [Menu.md#structure]
    help_menu->Append(tul(id::Help), with_shortcut(translate("menu_help"), HELP_SHORTCUT), translate("menu_help"));
    help_menu->AppendSeparator();
    help_menu->Append(tul(id::About), translate("menu_about"), translate("menu_about"));
    return help_menu;
}

wxMenu* Menu::create_assistant_menu()
{
    wxMenu* assistant_menu = new wxMenu;  // [Menu.md#structure]
    assistant_menu->Append(tul(id::Chat), translate("menu_chat"), translate("menu_chat_help"));
    return assistant_menu;
}

wxMenu* Menu::create_edit_menu()
{
    edit_menu_ = new wxMenu;
    edit_menu_->Append(tul(id::EditSync), translate("edit_synchronisation"));
    edit_menu_->Append(tul(id::DeleteSync), translate("delete_synchronisation"));
    edit_menu_->Append(tul(id::EditSshAccounts), translate("menu_ssh_connections"));
    edit_menu_->Append(tul(id::EditDevices), translate("menu_devices"));  // [Menu.md#phone-entries]
    edit_menu_->Append(tul(id::EditSettings), with_shortcut(translate("menu_settings"), "Ctrl+,"));
    edit_menu_->AppendSeparator();
    edit_menu_->Append(tul(id::EditLogging), translate("menu_logging_settings"));
    return edit_menu_;
}

wxMenu* Menu::create_view_menu()
{
    view_menu_ = new wxMenu;
    view_menu_->AppendRadioItem(tul(id::View_List), translate("menu_view_list"), translate("menu_view_list_help"));
    view_menu_->AppendRadioItem(tul(id::View_Tree), translate("menu_view_tree"), translate("menu_view_tree_help"));
    view_menu_->AppendRadioItem(tul(id::View_Grid), translate("menu_view_grid"), translate("menu_view_grid_help"));
    view_menu_->AppendSeparator();
    view_menu_->Append(tul(id::View_Log), translate("menu_view_log"));
    view_menu_->AppendSeparator();
    view_menu_->Append(tul(id::View_Background), translate("menu_view_background"));
    return view_menu_;
}
 
wxMenu* Menu::create_sync_menu()
{
    sync_menu_ = new wxMenu;
    sync_menu_->Append(tul(id::StartSync), translate("menu_start"));
    sync_menu_->Append(tul(id::StopSync), translate("menu_stop"));
    sync_menu_->Append(tul(id::SyncOnce), translate("menu_sync_once"));
    sync_menu_->Append(tul(id::Preview), translate("menu_preview"), translate("menu_preview_help"));
    sync_menu_->AppendSeparator();
    sync_menu_->Append(tul(id::RestoreFiles), translate("menu_restore_files"), translate("menu_restore_files_help"));  // [Menu.md#restore-entry]
    return sync_menu_;
}

wxMenu* Menu::create_background_service_menu()
{
    background_service_menu_ = new wxMenu;
    background_service_menu_->Append(tul(id::BackgroundServiceState), translate("bs_state_unknown"));  // [Menu.md#background-service-state-entry]
    background_service_menu_->Enable(tul(id::BackgroundServiceState), false);
    background_service_menu_->AppendSeparator();
    background_service_menu_->Append(tul(id::BackgroundServiceStart), translate("menu_start"), translate("menu_bs_start_help"));
    background_service_menu_->Append(tul(id::BackgroundServiceStop), translate("menu_stop"), translate("menu_bs_stop_help"));
    return background_service_menu_;
}

void Menu::create()
{
    menu_bar_ = new wxMenuBar();
    menu_bar_->Append(create_file_menu(), translate("menu_file"));
    menu_bar_->Append(create_edit_menu(), translate("menu_edit"));
    menu_bar_->Append(create_sync_menu(), translate("menu_synchronisation"));
    menu_bar_->Append(create_background_service_menu(), translate("menu_background_service"));
    menu_bar_->Append(create_view_menu(), translate("menu_view"));
    menu_bar_->Append(create_assistant_menu(), translate("menu_assistant"));
    menu_bar_->Append(create_help_menu(), translate("menu_help_menu"));
}

Menu::~Menu()
{
    edit_menu_ = nullptr;  // menu_bar_ is deleted by the framework
}

