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
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace AFS_Core {

enum class PlannedAction { CopyToTarget, MoveToTarget, CopyToSource, DeleteInTarget, DeleteInSource, RenameInTarget, RenameInSource,
                           ConflictCopyToTarget, ConflictCopyToSource };  // [SyncPlan.md#PlannedAction]
constexpr std::size_t PLANNED_ACTIONS = 9;

inline bool file_in_target(PlannedAction action)  // the file the step handles lives in the target
{
    return action == PlannedAction::DeleteInTarget || action == PlannedAction::CopyToSource || action == PlannedAction::RenameInTarget
        || action == PlannedAction::ConflictCopyToSource;
}

inline bool conflict_copy(PlannedAction action)  // both versions survive, see [SyncPlan.md#PlannedAction]
{
    return action == PlannedAction::ConflictCopyToTarget || action == PlannedAction::ConflictCopyToSource;
}

inline bool winner_in_target(PlannedAction action)  // the file that is copied lives in the target
{
    return action == PlannedAction::CopyToSource || action == PlannedAction::ConflictCopyToSource;
}

inline bool rename(PlannedAction action)  // the file keeps its content and only changes its name, see [SyncPlan.md#PlannedAction]
{
    return action == PlannedAction::RenameInTarget || action == PlannedAction::RenameInSource;
}

inline bool deletion(PlannedAction action)  // the steps that lose data, see [SyncPlan.md#PlannedAction]
{
    return action == PlannedAction::DeleteInTarget || action == PlannedAction::DeleteInSource;
}

struct PlannedStep {
    std::wstring item;
    PlannedAction action;
    std::wstring source;
    std::wstring target;
    uint64_t bytes;
};

class SyncPlan {  // [SyncPlan.md#SyncPlan]
    std::vector<PlannedStep> steps_;
    std::array<std::size_t, PLANNED_ACTIONS> counts_{};
    uint64_t bytes_ = 0;
public:
    static constexpr std::size_t MAX_STEPS = 10000;  // [SyncPlan.md#MAX_STEPS]
    void add(PlannedStep step);
    const std::vector<PlannedStep>& steps() const { return steps_; }
    std::size_t count(PlannedAction action) const { return counts_[static_cast<std::size_t>(action)]; }
    std::size_t count() const;
    std::size_t deletions() const { return count(PlannedAction::DeleteInTarget) + count(PlannedAction::DeleteInSource); }
    std::size_t not_listed() const { return count() - steps_.size(); }
    uint64_t bytes() const { return bytes_; }  // volume of the copies, deletions move nothing
    bool empty() const { return count() == 0; }
};

struct UnreachableItem {  // [SyncPlan.md#UnreachableItem]
    std::wstring name;
    std::wstring message;
    bool warn;
};

class SyncPlanGuard {  // [SyncPlan.md#SyncPlanGuard]
    SyncPlan* previous_;
public:
    explicit SyncPlanGuard(SyncPlan* plan);
    ~SyncPlanGuard();
    SyncPlanGuard(const SyncPlanGuard&) = delete;
    SyncPlanGuard& operator=(const SyncPlanGuard&) = delete;
};

bool planning();  // [SyncPlan.md#planning]
void plan_step(PlannedStep step);

} // namespace AFS_Core
