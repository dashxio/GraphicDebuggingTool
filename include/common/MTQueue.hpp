#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <iterator>
#include <type_traits>

template <typename T>
class MTObj {
private:
	T m_value;
	std::mutex m_mtx;

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
	class Accessor {
		MTQueue& m_d;
		std::unique_lock<std::mutex> m_guard;

	public:
		Accessor(MTQueue& d) : m_d(d), m_guard(d.m_mtx) {}

		std::deque<T>& value(){
			return m_d.m_arr;
		}
	};

	Accessor getAccessor(){
		return {*this};
	}

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

    template<typename Iterable, 
		typename = std::enable_if_t<!std::is_same_v<decltype(std::begin(std::declval<Iterable>())), void>>>
    void push_many(Iterable&& vals) {
        std::unique_lock lck(m_mtx);
        std::copy(std::make_move_iterator(std::begin(vals)),
                  std::make_move_iterator(std::end(vals)),
                  std::back_inserter(m_arr));
        m_cv.notify_all();
    }

};