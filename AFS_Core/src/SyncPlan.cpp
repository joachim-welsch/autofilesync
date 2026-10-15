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
#include "SyncPlan.h"
#include <numeric>

namespace AFS_Core {

namespace {
    thread_local SyncPlan* current_plan = nullptr;
}

void SyncPlan::add(PlannedStep step)
{
    ++counts_[static_cast<std::size_t>(step.action)];
    if (step.action != PlannedAction::DeleteInTarget)
        bytes_ += step.bytes;
    if (steps_.size() < MAX_STEPS)  // [SyncPlan.md#MAX_STEPS]
        steps_.push_back(std::move(step));
}

std::size_t SyncPlan::count() const
{
    return std::accumulate(counts_.begin(), counts_.end(), std::size_t{0});
}

SyncPlanGuard::SyncPlanGuard(SyncPlan* plan)
    : previous_(current_plan)
{
    current_plan = plan;
}

SyncPlanGuard::~SyncPlanGuard()
{
    current_plan = previous_;
}

bool planning()
{
    return current_plan != nullptr;
}

void plan_step(PlannedStep step)
{
    if (current_plan != nullptr)
        current_plan->add(std::move(step));
}

} // namespace AFS_Core
