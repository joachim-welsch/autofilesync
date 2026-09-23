/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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

#include <exception>
#include <memory>
#include <mutex>
#include <stack>

namespace AFS_Core {

struct EmptyStack : std::exception {
	const char* what() const noexcept override { return "empty stack"; }
};

template<typename T>
class ThreadSaveStack {
	std::stack<T> data_;
	mutable std::mutex m_;
public:
	ThreadSaveStack() = default;
	ThreadSaveStack(const ThreadSaveStack& other)
	{
		std::lock_guard<std::mutex> lock(other.m_);
		data_ = other.data_;
	}
	ThreadSaveStack& operator=(const ThreadSaveStack&) = delete;
	void push(T value)
	{
		std::lock_guard<std::mutex> lock(m_);
		data_.push(std::move(value));
	}
	std::shared_ptr<T> pop()
	{
		std::lock_guard<std::mutex> lock(m_);
		if (data_.empty()) throw EmptyStack();
		std::shared_ptr<T> const res(std::make_shared<T>(data_.top()));
		data_.pop();
		return res;
	}
	void pop(T& value)
	{
		std::lock_guard<std::mutex> lock(m_);
		if (data_.empty()) throw EmptyStack();
		value = data_.top();
		data_.pop();
	}
	bool empty() const
	{
		std::lock_guard<std::mutex> lock(m_);
		return data_.empty();
	}
};

} // namespace AFS_Core
