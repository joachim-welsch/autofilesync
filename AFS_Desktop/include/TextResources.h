/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once

namespace txt {
	constexpr wchar_t AddSynchronisation[] = L"Synchronisation hinzufügen";
	constexpr wchar_t AddSshConnection[] = L"SSH-Verbindung hinzufügen";
	constexpr wchar_t EditSshConnection[] = L"SSH-Verbindung bearbeiten";
	constexpr wchar_t SshAccounts[] = L"SSH-Verbindungen";
	constexpr wchar_t EditSettings[] = L"Globale Einstellungen";
	constexpr wchar_t EditLogging[] = L"Logging-Einstellungen";
	constexpr wchar_t EditSynchronisation[] = L"Synchronisation ändern";
	constexpr wchar_t DeleteSynchronisation[] = L"Synchronisation löschen";
	constexpr wchar_t Chat[] = L"Assistent";
}

namespace tbtxt { // ToolBar text
	constexpr wchar_t Help[] = L"Hilfe";
	constexpr wchar_t AddSynchronisation[] = L"Synchronisation hinzufügen";
	constexpr wchar_t EditSettings[] = L"Global Einstellungen";
	constexpr wchar_t EditSynchronisation[] = L"Synchronisation editieren";
	constexpr wchar_t SaveFile[] = L"Speichern";
	constexpr wchar_t DeleteSynchronisation[] = L"Synchronisation löschen";
	constexpr wchar_t StartSync[] = L"Synchronisation starten";
	constexpr wchar_t StopSync[] = L"Synchronisation stoppen";
}

namespace mtxt { // Menue text
	constexpr wchar_t File_new[] = L"&Neu";
	constexpr wchar_t File_new_SshConnection[] = L"SSH-Verbindung";
	constexpr wchar_t File_new_Synchronization[] = L"Synchronisation";
    constexpr wchar_t File_open[] = L"&Öffnen";
    constexpr wchar_t File_save[] = L"&Speichern";
    constexpr wchar_t File_save_as[] = L"Speichern &als";
    constexpr wchar_t Quit[] = L"Beenden\tAlt-X";
    constexpr wchar_t About[] = L"Info über AutoFileSync\tF1";
	constexpr wchar_t Help[] = L"Hilfe anzeigen";
	constexpr wchar_t EditSettings[] = L"Globale Einstellungen";
	constexpr wchar_t EditSshAccounts[] = L"SSH-Verbindungen...";
	constexpr wchar_t EditSynchronisation[] = L"Synchronisation editieren";
	constexpr wchar_t DeleteSynchronisation[] = L"Synchronisation löschen";
	constexpr wchar_t EditLogging[] = L"Logging Einstellungen";
	constexpr wchar_t Chat[] = L"&Chat...";
}

namespace sltxt { // Status line text
    constexpr wchar_t About[] = L"Info über AutoFileSync\tF1";
	constexpr wchar_t Help[] = L"Hilfe anzeigen";
}
