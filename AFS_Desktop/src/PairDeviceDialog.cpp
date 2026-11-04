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
#include "PairDeviceDialog.h"
#include "Config.h"
#include "DeviceLink.h"
#include "FirewallHint.h"
#include "QrCode.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/clipbrd.h>

using namespace AFS_Core;
using translation::translate;

namespace {
constexpr int TIMER_MILLISECONDS = 500;
constexpr int FIREWALL_HINT_AFTER_TICKS = 30;  // [PairDeviceDialog.md#firewall-hint]
constexpr int CODE_PIXELS = 280;
}

std::filesystem::path device_identity_dir()  // [PairDeviceDialog.md#device_identity_dir]
{
    return DeviceLink::default_dir();
}

bool start_device_link()  // [PairDeviceDialog.md#start_device_link]
{
    return DeviceLink::instance().start(device_identity_dir());
}

BEGIN_EVENT_TABLE(PairDeviceDialog, wxDialog)
    EVT_TIMER(ID_PAIRDEVICE_TIMER, PairDeviceDialog::on_timer)
    EVT_BUTTON(wxID_CLOSE, PairDeviceDialog::on_close_button)
    EVT_BUTTON(ID_PAIRDEVICE_COPY_COMMAND, PairDeviceDialog::on_copy_command)
END_EVENT_TABLE()

PairDeviceDialog::PairDeviceDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE), timer_(this, ID_PAIRDEVICE_TIMER)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    code_ = new wxStaticBitmap(this, wxID_ANY, wxBitmap(FromDIP(CODE_PIXELS), FromDIP(CODE_PIXELS)));
    main_sizer->Add(code_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
    steps_ = new wxStaticText(this, wxID_ANY, translate("pair_step_install") + L"\n" + translate("pair_step_scan") + L"\n" + translate("pair_same_network"));  // [PairDeviceDialog.md#same-network]
    main_sizer->Add(steps_, 0, wxLEFT | wxRIGHT, 10);
    status_ = new wxStaticText(this, wxID_ANY, translate("pair_waiting"));
    status_->SetFont(status_->GetFont().Bold());
    main_sizer->Add(status_, 0, wxALL, 10);
    add_firewall_row(main_sizer);
    details_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    details_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    main_sizer->Add(details_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    main_sizer->Add(CreateStdDialogButtonSizer(wxCLOSE), 0, wxEXPAND | wxALL, 10);
    SetEscapeId(wxID_CLOSE);
    SetSizer(main_sizer);
    show_offer();
    Fit();
    Centre();
    timer_.Start(TIMER_MILLISECONDS);
}

PairDeviceDialog::~PairDeviceDialog()
{
    timer_.Stop();
    DeviceLink& link = DeviceLink::instance();
    link.pairing().end();  // [PairDeviceDialog.md#token-ends-with-the-dialog]
    if (Config::instance().devices_snapshot().empty() && link.running())
        link.stop();  // [PairDeviceDialog.md#link-lifetime]
}

void PairDeviceDialog::add_firewall_row(wxBoxSizer* main_sizer)  // [PairDeviceDialog.md#firewall-row]
{
    firewall_row_ = new wxBoxSizer(wxHORIZONTAL);
    firewall_command_ = new wxTextCtrl(this, ID_PAIRDEVICE_FIREWALL_COMMAND, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    firewall_row_->Add(firewall_command_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    firewall_row_->Add(new wxButton(this, ID_PAIRDEVICE_COPY_COMMAND, translate("pair_copy_command")), 0, wxALIGN_CENTER_VERTICAL);
    main_sizer->Add(firewall_row_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    main_sizer->Hide(firewall_row_);
}

void PairDeviceDialog::show_offer()  // [PairDeviceDialog.md#show_offer]
{
    if (!start_device_link() && !DeviceLink::instance().hosted_elsewhere()) {  // [PairDeviceDialog.md#hosted-elsewhere]
        status_->SetLabel(translate("pair_link_unavailable"));
        status_->Wrap(FromDIP(CODE_PIXELS + 20));
        code_->Hide();
        steps_->Hide();
        return;
    }
    const PairingOffer offer = DeviceLink::instance().offer();
    code_->SetBitmap(qr_bitmap(qr_content(offer), FromDIP(CODE_PIXELS)));
    wxString addresses;
    for (const auto& address : offer.addresses)
        addresses += (addresses.empty() ? "" : ", ") + wxString::FromUTF8(address);
    details_->SetLabel(translate("pair_details", {wxString::FromUTF8(offer.pc_name).ToStdWstring(), addresses.ToStdWstring(), std::to_wstring(offer.port)}));
    ticks_ = 0;
}

void PairDeviceDialog::on_timer(wxTimerEvent&)
{
    DeviceLink& link = DeviceLink::instance();
    if (const auto device = link.take_paired()) {
        paired_ = device;
        timer_.Stop();
        status_->SetLabel(translate("pair_done", {wxString::FromUTF8(device->name).ToStdWstring()}));
        code_->Hide();
        steps_->Hide();
        Fit();
        return;
    }
    if (!link.pairing().open()) {
        if (++closed_ticks_ >= 2)  // [PairDeviceDialog.md#renewed-code]
            show_offer();
        return;
    }
    closed_ticks_ = 0;
    if (++ticks_ == FIREWALL_HINT_AFTER_TICKS)
        show_firewall_hint(link.port());
}

void PairDeviceDialog::show_firewall_hint(uint16_t port)  // [PairDeviceDialog.md#firewall-hint]
{
    const bool ufw = ufw_blocks_incoming();
    const std::wstring hint = ufw ? translate("pair_ufw_hint", {std::to_wstring(port)}) : translate("pair_firewall_hint", {std::to_wstring(port)});
    status_->SetLabel(translate("pair_waiting") + L"\n" + hint);
    status_->Wrap(std::max(steps_->GetBestSize().GetWidth(), FromDIP(CODE_PIXELS)));
    if (ufw) {
        firewall_command_->SetValue(wxString::FromUTF8(ufw_allow_command(local_ipv4_networks(), port)));
        GetSizer()->Show(firewall_row_);
    }
    Layout();
    Fit();
}

void PairDeviceDialog::on_copy_command(wxCommandEvent&)
{
    if (!wxTheClipboard->Open())
        return;
    wxTheClipboard->SetData(new wxTextDataObject(firewall_command_->GetValue()));
    wxTheClipboard->Flush();  // keeps the command after the program ends [PairDeviceDialog.md#firewall-row]
    wxTheClipboard->Close();
}

void PairDeviceDialog::on_close_button(wxCommandEvent&)
{
    EndModal(wxID_CLOSE);
}
