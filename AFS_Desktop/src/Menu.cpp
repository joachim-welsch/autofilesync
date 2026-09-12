#include "Menu.h"
#include "id.h"
#include "utilwx.h"
#include "TextResources.h"

using utilwx::tul;

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
    newSubMenu->Append(tul(id::File_new_Synchronization), mtxt::File_new_Synchronization);
    newSubMenu->Append(tul(id::File_new_SshConnection), mtxt::File_new_SshConnection);
    fileMenu->AppendSubMenu(newSubMenu, mtxt::File_new);
    
    fileMenu->Append(tul(id::File_open), mtxt::File_open);
    fileMenu->Append(tul(id::File_save), mtxt::File_save);
    fileMenu->Append(tul(id::File_save_as), mtxt::File_save_as);
    fileMenu->Append(tul(id::Quit), mtxt::Quit, "Dieses Programm beenden");
    return fileMenu;
}

wxMenu* Menu::createHelpMenu()
{
    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(tul(id::About), mtxt::About, sltxt::About);
    helpMenu->Append(tul(id::Help), mtxt::Help, sltxt::Help);
    return helpMenu;
}

wxMenu* Menu::createEditMenu()
{
    editMenu_ = new wxMenu;
    editMenu_->Append(tul(id::EditSync), mtxt::EditSynchronisation);
    editMenu_->Append(tul(id::DeleteSync), mtxt::DeleteSynchronisation);
    editMenu_->Append(tul(id::EditSshAccounts), mtxt::EditSshAccounts);
    editMenu_->Append(tul(id::EditSettings), mtxt::EditSettings);
    editMenu_->AppendSeparator();
    editMenu_->Append(tul(id::EditLogging), mtxt::EditLogging);
    return editMenu_;
}

wxMenu* Menu::createViewMenu()
{
    viewMenu_ = new wxMenu;
    viewMenu_->AppendRadioItem(tul(id::View_List), "&Liste", "Liste");
    viewMenu_->AppendRadioItem(tul(id::View_Tree), "&Baum", "Baum");
    viewMenu_->AppendRadioItem(tul(id::View_Grid), "&Tabelle", "Tabelle");
    viewMenu_->AppendSeparator();
    viewMenu_->Append(tul(id::View_Log), "&Log-Datei anzeigen");
    viewMenu_->AppendSeparator();
    viewMenu_->Append(tul(id::View_Background), "&Hintergrund");
    return viewMenu_;
}
 
wxMenu* Menu::createSyncMenu()
{
    syncMenu_ = new wxMenu;
    syncMenu_->Append(tul(id::StartSync), "Start");
    syncMenu_->Append(tul(id::StopSync), "Stopp");
	syncMenu_->Append(tul(id::SyncOnce), "Einmal synchronisieren");
    return syncMenu_;
}

wxMenu* Menu::createBackgroundServiceMenu()
{
    backgroundServiceMenu_ = new wxMenu;
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceState), L"Zustand: unbekannt");  // [Menu.md#background-service-state-entry]
    backgroundServiceMenu_->Enable(tul(id::BackgroundServiceState), false);
    backgroundServiceMenu_->AppendSeparator();
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceStart), "Start", "Hintergrunddienst starten");
    backgroundServiceMenu_->Append(tul(id::BackgroundServiceStop), "Stop", "Hintergrunddienst stoppen");
    return backgroundServiceMenu_;
}

wxMenu* Menu::createAssistantMenu()
{
    wxMenu* assistantMenu = new wxMenu;
    assistantMenu->Append(tul(id::Chat), mtxt::Chat, "Mit dem Assistenten chatten");
    return assistantMenu;
}

void Menu::create()
{
    menuBar_ = new wxMenuBar();
    menuBar_->Append(createFileMenu(), "&Datei");
    menuBar_->Append(createEditMenu(), "&Bearbeiten");
    menuBar_->Append(createSyncMenu(), "&Synchronisation");
    menuBar_->Append(createBackgroundServiceMenu(), "&Hintergrunddienst");
    menuBar_->Append(createViewMenu(), "&Ansicht");
    menuBar_->Append(createAssistantMenu(), "&Assistent");
    menuBar_->Append(createHelpMenu(), "&Hilfe");
}

Menu::~Menu()
{
    editMenu_ = nullptr;  // menuBar_ is deleted by the framework
}

