//
// Created by Kolane on 2025/1/17.
//
#include <string>
#include <cstdarg>
#include <cwchar>
#include <cctype>
#include <cstdint>
#include <vector>


#pragma once


namespace util {

    /*
     * std::string format
     */
    template<typename = void>
    std::string formatv(const char * format, va_list args)
    {
        std::string s;

        if (format && *format) {
            va_list args_copy;

            va_copy(args_copy, args);
            int len = std::vsnprintf(nullptr, 0, format, args_copy);

            if (len > 0) {
                s.resize(len);
                va_copy(args_copy, args);
                std::vsnprintf((char*)s.data(), format, args_copy);
            }
        }

        return s;
    }

    /*
     * std::wstring format
     */
    template<typename = void>
    std::wstring formatv(const wchar_t * format, va_list args)
    {
        std::wstring s;

        if (format && *format) {
            va_list args_copy;

            while (true) {
                s.resize(s.capacity());

                va_copy(args_copy, args);

                int len = std::vswprintf(nullptr, 0, format, args_copy);
                if (len == -1) {
                    s.reserve(s.capacity() * 2);
                } else {
                    s.resize(len);
                    break;
                }
            }

            return s;
        }
    }

    /*
     * std::string format
     */
    template<typename = void>
    std::string format(const char * format, ...) {
        std::string s;

        if (format && *format) {
            va_list args;
            va_start(args, format);

            s = formatv(format, args);

            va_end(args);
        }

        return s;
    }

    /*
     * std::wstring format
     */
    template<typename = void>
    std::wstring format(const wchar_t * format, ...) {
        std::wstring s;

        if (format && *format) {
            va_list args;
            va_start(args, format);

            s = formatv(format, args);

            va_end(args);
        }

        return s;
    }

    template<
            class CharT,
            class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>
    >
    std::basic_string<CharT, Traits, Allocator> &trim_all(
            std::basic_string<CharT, Traits, Allocator> &s) {
        if (s.empty())
            return s;

        using size_type = typename std::basic_string<CharT, Traits, Allocator>::size_type;
        for (size_type i = s.size() - 1; i != size_type(-1); i--) {
            if (std::isspace(static_cast<unsigned char>(s[i]))) {
                s.erase(i, 1);
            }
        }
        return s;
    }


    template<
            class CharT,
            class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>
    >
    std::basic_string<CharT, Traits, Allocator> &trim_left(
            std::basic_string<CharT, Traits, Allocator> &s) {
        if (s.empty())
            return s;

        using size_type = typename std::basic_string<CharT, Traits, Allocator>::size_type;
        size_type pos = 0;
        for (; pos < s.size(); ++pos) {
            if (!std::isspace(static_cast<unsigned char>(s[pos]))) {
                break;
            }
        }

        s.erase(0, pos);
        return s;
    }

    template<
            class CharT,
            class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>
    >
    std::basic_string<CharT, Traits, Allocator> &trim_right(
            std::basic_string<CharT, Traits, Allocator> &s) {
        if (s.empty())
            return s;

        using size_type = typename std::basic_string<CharT, Traits, Allocator>::size_type;
        size_type pos = s.size() - 1;
        for (; pos != size_type(-1); pos--) {
            if (!std::isspace(static_cast<unsigned char>(s[pos]))) {
                break;
            }
        }

        s.erase(pos + 1);
        return s;
    }

    template<
            class CharT,
            class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>
    >
    std::basic_string<CharT, Traits, Allocator> &trim_both(
            std::basic_string<CharT, Traits, Allocator> &s) {
        trim_left(s);
        trim_right(s);

        return s;
    }

    template<class CharT, class Traits = std::char_traits<CharT>>
    std::basic_string<CharT, Traits> &trim_left(
            std::basic_string_view<CharT, Traits> &s) {
        if (s.empty())
            return s;

        using size_type = typename std::basic_string<CharT, Traits>::size_type;
        size_type pos = 0;
        for (; pos != s.size(); ++pos) {
            if (!std::isspace(static_cast<unsigned char>(s[pos])))
                break;
        }
        s.remove_prefix(pos);
        return s;
    }


    template<class CharT, class Traits = std::char_traits<CharT>>
    std::basic_string<CharT, Traits> &trim_right(
            std::basic_string_view<CharT, Traits> &s) {
        if (s.empty())
            return s;

        using size_type = typename std::basic_string<CharT, Traits>::size_type;
        size_type pos = s.size() - 1;
        for (; pos != size_type(-1); pos--) {
            if (!std::isspace(static_cast<unsigned char>(s[pos])))
                break;
        }
        s.remove_prefix(s.size() - pos - 1);
        return s;
    }

    template<class CharT, class Traits = std::char_traits<CharT>>
    std::basic_string<CharT, Traits> &trim_both(
            std::basic_string_view<CharT, Traits> &s) {
        trim_left(s);
        trim_right(s);
        return s;
    }

    template<class String, class Delimiter>
    inline std::vector<String> split(const String &s, const Delimiter &delim = " ") {
        using size_type = typename String::size_type;
        std::vector<String> result;
        size_type last_pos = s.find_first_not(delim, 0);
        size_type pos = s.find_first_of(delim, last_pos);
        while (String::npos != pos || String::npos != last_pos) {
            result.push_back(s.substr(last_pos, pos - last_pos));
            last_pos = s.find_first_not(delim, pos);
            pos = s.find_first_of(delim, last_pos);
        }

        return result;
    }


    template<class String, class OldStr, class NewStr>
    inline String replace(const String &s, const OldStr &old_str, const NewStr &new_str) {
        using size_type = typename String::size_type;
        using old_str_type = std::remove_reference<std::remove_cv_t<OldStr>>;
        using new_str_type = std::remove_reference<std::remove_cv_t<NewStr>>;
        using old_raw_type = std::remove_pointer_t<std::remove_all_extents_t<old_str_type>>;
        using new_raw_type = std::remove_pointer_t<std::remove_all_extents_t<new_str_type>>;

        size_type old_str_size = 0;
        size_type new_str_size = 0;

        if constexpr (std::is_same_v<old_raw_type, char>) {
            if constexpr (std::is_pointer_v<old_str_type > || std::is_array_v<old_str_type>) {
                old_str_size = std::basic_string_view<old_raw_type>{ old_str }.size();
            } else {
                old_str_size = std::basic_string_view<old_raw_type>{ old_str }.size();
            }
        } else {
            if constexpr (std::is_pointer_v<old_str_type > || std::is_array_v<old_str_type>) {
                old_str_size = std::basic_string_view<old_raw_type>{ old_str }.size();
            } else {
                old_str_size = std::basic_string_view<old_raw_type>{ old_str }.size();
            }
        }

        if constexpr (std::is_trivial_v<new_raw_type>) {
            if constexpr (std::is_pointer_v<new_str_type > || std::is_array_v<new_str_type>) {
                new_str_size = std::basic_string_view<new_raw_type>{ new_str }.size();
            } else {
                new_str_size = std::basic_string_view<new_raw_type>{ new_str }.size();
            }
        } else {
            if constexpr (std::is_pointer_v<new_str_type > || std::is_array_v<new_str_type>) {
                new_str_size = std::basic_string_view<new_raw_type>{ new_str }.size();
            } else {
                new_str_size = std::basic_string_view<new_raw_type>{ new_str }.size();
            }
        }

        for (size_type pos = 0; pos!= String::npos; pos += new_str_size) {
            pos = s.find(old_str, pos);
            if (String::npos!= pos) {
                if constexpr (std::is_pointer_v<new_str_type > || std::is_array_v<new_str_type>) {
                    s.replace(pos, old_str_size, new_str);
                } else {
                    s.replace(pos, old_str_size, new_str_size, new_str);
                }
            } else {
                break;
            }
        }
        return s;
    }

    template<class String1, class String2>
    inline std::size_t ifind(const String1 &src, const String2 &dest, typename String1::size_type pos = 0) {
        using str2_type = std::remove_reference_t<std::remove_cv_t<String2>>;
        using raw2_type = std::remove_pointer_t<std::remove_all_extents_t<str2_type>>;

        if (pos > src.size() || dest.empty()) {
            return String1::npos;
        }

        for (auto OnterIt = std::next(src.begin(), pos); OnterIt != src.end(); ++OnterIt) {
            auto InnerIt = OnterIt;
            auto SubStrIt = dest.begin();
            for (; InnerIt != src.end() && SubStrIt != dest.end(); ++InnerIt, ++SubStrIt) {
                if (std::toupper(*InnerIt) != std::toupper(*SubStrIt)) {
                    break;
                }
            }
            if (SubStrIt == dest.end()) {
                return std::distance(src.begin(), OnterIt);
            }
        }
        return String1::npos;
    }


    template<typename = void >
        inline bool iequals(std::string_view lhs, std::string_view rhs) {
         auto len = lhs.size();
         if (len != rhs.size()) {
             return false;
         }
         for (auto i = 0; i < len; ++i) {
             if (std::toupper(lhs[i]) != std::toupper(rhs[i])) {
                 return false;
             }
         }
         return true;
    }















}























