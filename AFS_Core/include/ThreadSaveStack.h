#pragma once

#include <exception>
#include <memory>
#include <mutex>
#include <stack>

struct emptyStack : std::exception {
	//const char* what() const throw();
};

template<typename T>
class ThreadSaveStack {
	std::stack<T> data_;
	mutable std::mutex m_;
public:
	ThreadSaveStack(){}
	ThreadSaveStack(const ThreadSaveStack& other) {
		std::lock_guard<std::mutex> lock(other.m_);
		data_ = other.data_;
	}
	ThreadSaveStack& operator=(const ThreadSaveStack&) = delete;
	void push(T value) {
		std::lock_guard<std::mutex> lock(m_);
		data_.push(std::move(value));
	}
	std::shared_ptr<T> pop() {
		std::lock_guard<std::mutex> lock(m_);
		if (data_.empty()) throw emptyStack();
		std::shared_ptr<T> const res(std::make_shared<T>(data_.top()));
		data_.pop();
		return res;
	}
	void pop(T& value) {
		std::lock_guard<std::mutex> lock(m_);
		if (data_.empty()) throw emptyStack();
		value = data_.top();
		data_.pop();
	}
	bool empty() const {
		std::lock_guard<std::mutex> lock(m_);
		return data_.empty();
	}
};
