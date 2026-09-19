/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
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
