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

wxMenuBar*  Menu::menuBar()
{
	return menuBar_;
}

wxMenu* Menu::editMenu()
{
	return editMenu_;
}

wxMenu* Menu::viewMenu()
{
	return viewMenu_;
}

wxMenu* Menu::syncMenu()
{
	return syncMenu_;
}

wxMenu* Menu::backgroundServiceMenu()
{
	return backgroundServiceMenu_;
}

wxMenu* Menu::createFileMenu()
{
    wxMenu* fileMenu = new wxMenu;
    
    wxMenu* newSubMenu = new wxMenu;
    newSubMenu->Append(tul(id::File_new_Synchronization), with_shortcut(translate("menu_new_synchronisation"), "Ctrl+N"));
    newSubMenu->Append(tul(id::File_new_SshConnection), translate("menu_new_ssh_connection"));
    fileMenu->AppendSubMenu(newSubMenu, translate("menu_new"));
    fileMenu->Append(tul(id::File_open), with_shortcut(translate("menu_open"), "Ctrl+O"));
    fileMenu->Append(tul(id::File_save), with_shortcut(translate("menu_save"), "Ctrl+S"));
    fileMenu->Append(tul(id::File_save_as), with_shortcut(translate("menu_save_as"), "Ctrl+Shift+S"));
    fileMenu->Append(tul(id::Quit), with_shortcut(translate("menu_quit"), "Ctrl+Q"), translate("menu_quit_help"));
    return fileMenu;
}

wxMenu* Menu::createHelpMenu()
{
    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(tul(id::About), translate("menu_about"), translate("menu_about"));
    helpMenu->Append(tul(id::Help), with_shortcut(translate("menu_help"), HELP_SHORTCUT), translate("menu_help"));
    return helpMenu;
}

wxMenu* Menu::createEditMenu()
{
    editMenu_ = new wxMenu;
    editMenu_->Append(tul(id::EditSync), translate("edit_synchronisation"));
    editMenu_->Append(tul(id::DeleteSync), translate("delete_synchronisation"));
    editMenu_->Append(tul(id::EditSshAccounts), translate("menu_ssh_connections"));
    editMenu_->Append(tul(id::EditSettings), with_shortcut(translate("menu_settings"), "Ctrl+,"));
    editMenu_->AppendSeparator();
    editMenu_->Append(tul(id::EditLogging), translate("menu_logging_settings"));
    return editMenu_;
}

wxMenu* Menu::createViewMenu()
{
    viewMenu_ = new wxMenu;
    viewMenu_->AppendRadioItem(tul(id::View_List), translate("menu_view_list"), translate("menu_view_list_help"));
    viewMenu_->AppendRadioItem(tul(id::View_Tree), translate("menu_view_tree"), translate("menu_view_tree_help"));
    viewMenu_->AppendRadioItem(tul(id::View_Grid), translate("menu_view_grid"), translate("menu_view_grid_help"));
    viewMenu_->AppendSeparator();
    viewMenu_->Append(tul(id::View_Log), translate("menu_view_log"));
    viewMenu_->AppendSeparator();
    viewMenu_->Append(tul(id::View_Background), translate("menu_view_background"));
    return viewMenu_;
}
 
wxMenu* Menu::createSyncMenu()
{
    syncMenu_ = new wxMenu;
    syncMenu_->Append(tul(id::StartSync), translate("menu_start"));
    syncMenu_->Append(tul(id::StopSync), translate("menu_stop"));
    syncMenu_->Append(tul(id::SyncOnce), translate("menu_sync_once"));
    return syncMenu_;
}

wxMenu* Menu::createBackgroundServiceMenu()
{
    backgroundServiceMenu_ = new wxMenu;
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceState), translate("bs_state_unknown"));  // [Menu.md#background-service-state-entry]
    backgroundServiceMenu_->Enable(tul(id::BackgroundServiceState), false);
    backgroundServiceMenu_->AppendSeparator();
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceStart), translate("menu_start"), translate("menu_bs_start_help"));
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceStop), translate("menu_stop"), translate("menu_bs_stop_help"));
    return backgroundServiceMenu_;
}

wxMenu* Menu::createAssistantMenu()
{
    wxMenu* assistantMenu = new wxMenu;
    assistantMenu->Append(tul(id::Chat), translate("menu_chat"), translate("menu_chat_help"));
    return assistantMenu;
}

void Menu::create()
{
    menuBar_ = new wxMenuBar();
    menuBar_->Append(createFileMenu(), translate("menu_file"));
    menuBar_->Append(createEditMenu(), translate("menu_edit"));
    menuBar_->Append(createSyncMenu(), translate("menu_synchronisation"));
    menuBar_->Append(createBackgroundServiceMenu(), translate("menu_background_service"));
    menuBar_->Append(createViewMenu(), translate("menu_view"));
    menuBar_->Append(createAssistantMenu(), translate("menu_assistant"));
    menuBar_->Append(createHelpMenu(), translate("menu_help_menu"));
}

Menu::~Menu()
{
    editMenu_ = nullptr;  // menuBar_ is deleted by the framework
}

