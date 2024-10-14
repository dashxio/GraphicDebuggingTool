#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <iterator>

template <typename T>
class MTObj {
private:
	std::mutex m_mtx;
	T m_value;

public:
	class Accessor {
		MTObj& m_o;
		std::unique_lock<std::mutex> m_guard;

	public:
		Accessor(MTObj& o) : m_o(o), m_guard(o.m_mtx) {}

		T& value(){
			return m_o.m_value;
		}
	};

	T value(){
		std::unique_lock lck(m_mtx);
		return m_value;
	}

	void setValue(const T& value){
		std::unique_lock lck(m_mtx);
		m_value = value;
	}

	Accessor getAccessor(){
		return {*this};
	}

};

template<class T>
class MTQueue {
	std::condition_variable m_cv;
	std::mutex m_mtx;
	std::deque<T> m_arr;

public:
	T pop() {
		std::unique_lock lck(m_mtx);
		m_cv.wait(lck, [this] {return !m_arr.empty(); });
		T ret = std::move(m_arr.front());
		m_arr.pop_front();
		return ret;
	}

	auto pop_hold() {
		std::unique_lock lck(m_mtx);
		m_cv.wait(lck, [this] {return !m_arr.empty(); });
		T ret = std::move(m_arr.front());
		m_arr.pop_front();
		return std::pair{std::move(ret), std::move(lck)};
	}

	void push(T val) {
		std::unique_lock lck(m_mtx);
		m_arr.push_back(std::move(val));
		m_cv.notify_one();
	}

	void push_many(std::initializer_list<T> vals) {
		std::unique_lock lck(m_mtx);
		std::copy(std::move_iterator(vals.begin()), std::move_iterator(vals.end()), std::back_insert_iterator(m_arr));
		m_cv.notify_all();
	}

};