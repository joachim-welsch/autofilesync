/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include "pch.h"
#include "CpuTopology.h"
#include <algorithm>
#include <thread>
#ifdef _WIN32
    #include <Windows.h>
    #include <vector>
#elif defined(_MACOS)
    #include <sys/sysctl.h>
#else
    #include <filesystem>
    #include <fstream>
    #include <set>
    #include <string>
    #include <system_error>
#endif

namespace AFS_Core {

namespace cpu_topology {

namespace {
#ifdef _WIN32
unsigned count_cores()
{
    DWORD size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
    std::vector<char> buffer(size);
    if (size == 0 || !GetLogicalProcessorInformationEx(RelationProcessorCore,
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &size))
        return 0;
    unsigned cores = 0;
    for (DWORD offset = 0; offset < size; ++cores)  // entries differ in size, one per core
        offset += reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset)->Size;
    return cores;
}
#elif defined(_MACOS)
unsigned count_cores()
{
    int cores = 0;
    std::size_t size = sizeof cores;
    return sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) == 0 && cores > 0 ? static_cast<unsigned>(cores) : 0;
}
#else
unsigned count_cores()
{
    namespace fs = std::filesystem;
    std::set<std::string> cores;  // [CpuTopology.md#sibling-lists]
    std::error_code ec;
    for (fs::directory_iterator it("/sys/devices/system/cpu", ec), end; !ec && it != end; it.increment(ec)) {
        std::ifstream siblings(it->path() / "topology" / "thread_siblings_list");
        std::string list;
        if (std::getline(siblings, list))
            cores.insert(list);
    }
    return static_cast<unsigned>(cores.size());
}
#endif
}

unsigned physical_cores()
{
    const unsigned logical = std::max(1u, std::thread::hardware_concurrency());
    const unsigned cores = count_cores();
    return cores == 0 ? logical : std::min(cores, logical);  // [CpuTopology.md#fallback]
}

}

} // namespace AFS_Core
