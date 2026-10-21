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
#include "QrCode.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;
using translation::translate;

namespace {
constexpr int TIMER_MILLISECONDS = 500;
constexpr int FIREWALL_HINT_AFTER_TICKS = 120;  // a minute without a phone: the usual cause is a firewall [PairDeviceDialog.md#firewall-hint]
constexpr int CODE_PIXELS = 280;
}

std::filesystem::path device_identity_dir()  // [PairDeviceDialog.md#device_identity_dir]
{
    return std::filesystem::path(Config::get_root_pathname()).parent_path();
}

bool start_device_link()  // [PairDeviceDialog.md#start_device_link]
{
    return DeviceLink::instance().start(device_identity_dir());
}

BEGIN_EVENT_TABLE(PairDeviceDialog, wxDialog)
    EVT_TIMER(ID_PAIRDEVICE_TIMER, PairDeviceDialog::on_timer)
    EVT_BUTTON(wxID_CLOSE, PairDeviceDialog::on_close_button)
END_EVENT_TABLE()

PairDeviceDialog::PairDeviceDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE), timer_(this, ID_PAIRDEVICE_TIMER)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    code_ = new wxStaticBitmap(this, wxID_ANY, wxBitmap(FromDIP(CODE_PIXELS), FromDIP(CODE_PIXELS)));
    main_sizer->Add(code_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
    steps_ = new wxStaticText(this, wxID_ANY, translate("pair_step_install") + "\n" + translate("pair_step_scan"));
    main_sizer->Add(steps_, 0, wxLEFT | wxRIGHT, 10);
    status_ = new wxStaticText(this, wxID_ANY, translate("pair_waiting"));
    status_->SetFont(status_->GetFont().Bold());
    main_sizer->Add(status_, 0, wxALL, 10);
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
    if (Config::instance().devices_snapshot().empty())
        link.stop();  // [PairDeviceDialog.md#link-lifetime]
}

void PairDeviceDialog::show_offer()  // [PairDeviceDialog.md#show_offer]
{
    if (!start_device_link()) {
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
        show_offer();  // [PairDeviceDialog.md#renewed-code]
        return;
    }
    if (++ticks_ == FIREWALL_HINT_AFTER_TICKS) {
        status_->SetLabel(translate("pair_waiting") + "\n" + translate("pair_firewall_hint", {std::to_wstring(link.port())}));
        Fit();
    }
}

void PairDeviceDialog::on_close_button(wxCommandEvent&)
{
    EndModal(wxID_CLOSE);
}
