#pragma once
#include <functional>

template<typename T>
class EasyLogic {
public:
    T expression_result;
    std::function<void()> func;

    EasyLogic(const T& t) : expression_result(t) {}

    EasyLogic& execute(std::function<void()> f) {
        func = std::move(f);
        return *this;
    }

    EasyLogic& if_equal(const T& value) {
        if (func) {
            if (expression_result == value) {
                func();
            }
        }
        return *this;
    }

    EasyLogic& if_not_equal(const T& value) {
        if (func) {
            if (expression_result != value) {
                func();
            }
        }
        return *this;
    }

    EasyLogic& throw_if_equal(const T& value, std::string message = "error") {
        if (expression_result == value) {
            auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
            std::cerr << ec.message();
            throw std::system_error(ec, message);
        }
        return *this;
    }

    EasyLogic& throw_if_not_equal(const T& value, std::string message = "error") {
        if (expression_result != value) {
            auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
            std::cerr << ec.message();
            throw std::system_error(ec, message);
        }
        return *this;
    }
    
    T result() noexcept {
        return expression_result;
    }
};

template<typename T>
EasyLogic<T> convert_error(T t) {
    return EasyLogic<T>(t);
}

template<typename T>
class ConvertReturn {
public:
    T m_input_result;
    int m_ready_return_value = -1;
    int m_current_return_value = -1;
    bool m_return_value_is_set = false;

    ConvertReturn(const T& t) : m_input_result(t) {}

    ConvertReturn& to(int value)
    {
        m_ready_return_value = value;
        return *this;
    }

    //template <typename Condition>
    ConvertReturn& if_meet_condition(std::function<bool()> condition)
    {
        if(m_return_value_is_set){
            return *this;
        }

        if(condition()){
            m_return_value_is_set = true;
            m_current_return_value = m_ready_return_value;
        }

	    return *this;
    }

	ConvertReturn& if_meet_condition(std::function<bool(T)> condition)
	{
        if(m_return_value_is_set){
            return *this;
        }

        if(condition(m_input_result)){
            m_return_value_is_set = true;
            m_current_return_value = m_ready_return_value;
        }

	    return *this;
	}

    operator int() const
    {
        if(m_return_value_is_set){
            return m_current_return_value;
        }
        else{
            return m_ready_return_value;
        }

    }

    T result() noexcept {
        return m_input_result;
    }

    int value() const{
        if(m_return_value_is_set){
            return m_current_return_value;
        }
        else{
            return m_ready_return_value;
        }
    }
};

template<typename T>
ConvertReturn<T> convert_return(T t) {
    return ConvertReturn<T>(t);
}