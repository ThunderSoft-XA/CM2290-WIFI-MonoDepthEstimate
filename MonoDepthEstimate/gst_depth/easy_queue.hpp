#ifndef EASY_QUEUE_HPP
#define EASY_QUEUE_HPP

#include <iostream>
#include<mutex>
#include<condition_variable>
#include<boost/circular_buffer.hpp>
#include<optional>

template<typename T>
class Queue
{
public:
	Queue(size_t capacity) :q_{ capacity } {}

	void push(T& val) // block
	{
		std::unique_lock<std::mutex> lk( mtx_ );
		not_full_.wait(lk, [this] {return !q_.full(); });
		assert(!q_.full());
		q_.push_back(std::forward<T>(val));
		not_empty_.notify_one();
	}

	bool try_push(T& val) //non-block
	{
		std::lock_guard<std::mutex> lk(mtx_ );
		if (q_.full())return false;
		q_.push_back(std::forward<T>(val));
		not_empty_.notify_one();
		return true;
	}

	T pop() // block
	{
		std::unique_lock<std::mutex> lk( mtx_ );
		not_empty_.wait(lk, [this] {return !q_.empty(); });
		asert(!q_.empty());
		T ret{ std::move_if_noexcept(q_.front()) };
		q_.pop_front();
		not_full_.notify_one();
		return ret;

	}
	T try_pop() // non-block
	{
		std::lock_guard<std::mutex> lk(mtx_ );
		if (q_.empty())return {};
		T ret{ std::move_if_noexcept(q_.front()) };
		q_.pop_front();
		not_full_.notify_one();
		return ret;
	}

    int size() {
        return (int)q_.size();
    }

    bool empty() {
        return q_.empty();
    }

private:
	boost::circular_buffer<T>q_;
	std::mutex mtx_;
	std::condition_variable not_full_;
	std::condition_variable not_empty_;
};

#endif //EASY_QUEUE_HPP