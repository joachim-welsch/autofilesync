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

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace guitest {

using FileContents = std::vector<std::pair<std::filesystem::path, std::string>>;  // relative path -> content; a trailing separator makes a directory

class SandboxTree {  // [SandboxTree.md#SandboxTree]
public:
    SandboxTree(std::filesystem::path root, const FileContents& files = {});
    ~SandboxTree();
    SandboxTree(const SandboxTree&) = delete;
    SandboxTree& operator=(const SandboxTree&) = delete;
    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path path(const std::filesystem::path& relative) const { return root_ / relative; }
    void write(const std::filesystem::path& relative, const std::string& content) const;
    std::string read(const std::filesystem::path& relative) const;
    bool exists(const std::filesystem::path& relative) const;
    std::vector<std::string> entries() const;  // [SandboxTree.md#entries]
private:
    std::filesystem::path root_;
};

}
