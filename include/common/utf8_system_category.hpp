#pragma once

#include <boost/locale.hpp>
#include <system_error>

//using boost::locale::conv::utf_to_utf;
using boost::locale::conv::to_utf;

//std::error_category const& gai_category() {
//    static struct final : std::error_category {
//        const char* name() const noexcept override {
//            return "getaddrinfo";
//        }
//
//        std::string message(int err) const override {
//            return utf_to_utf<char>(gai_strerrorW(err));
//        }
//    } instance;
//    return instance;
//}

inline std::error_category const& utf8_system_category() {
	static struct final : std::_System_error_category {
		std::string message(int err) const override {
			return to_utf<char>(_System_error_category::message(err), "GBK");
		}
	} instance;
	return instance;
}

//class utf8_system_category : public std::error_category {
//public:
//    const char* name() const noexcept override {
//        return "utf8_system_category";
//    }
//
//    std::string message(int err) const override {
//        // 获取系统错误消息（ANSI 编码）
//        LPSTR buffer = nullptr;
//        DWORD size = FormatMessageA(
//            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
//            (LPSTR)&buffer, 0, NULL);
//        if (size == 0) {
//            return "Unknown error";
//        }
//        std::string ansi_message(buffer);
//        LocalFree(buffer);
//
//        // 使用 Boost.Locale 进行编码转换，从系统默认编码转换为 UTF-8
//        try {
//            return to_utf<char>(ansi_message, "GBK");
//        }
//        catch (const boost::locale::conv::conversion_error&) {
//            return "Encoding conversion error";
//        }
//    }
//};
//
//inline const std::error_category& utf8_system_category() {
//    static utf8_system_category instance;
//    return instance;
//}
