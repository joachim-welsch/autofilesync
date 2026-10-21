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
#include "QrCode.h"
#include <wx/dcmemory.h>
#include "qrcodegen/qrcodegen.hpp"

wxBitmap qr_bitmap(const std::string& text, int pixels)  // [QrCode.md#qr_bitmap]
{
    constexpr int QUIET_ZONE = 4;  // modules of white border the QR standard asks for
    const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = code.getSize() + 2 * QUIET_ZONE;
    const int scale = std::max(1, pixels / modules);
    const int side = modules * scale;
    wxBitmap bitmap(side, side, 24);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    for (int y = 0; y < code.getSize(); ++y)
        for (int x = 0; x < code.getSize(); ++x)
            if (code.getModule(x, y))
                dc.DrawRectangle((x + QUIET_ZONE) * scale, (y + QUIET_ZONE) * scale, scale, scale);
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}
