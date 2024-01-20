#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <string_view>
#include <concepts>
#include <sstream>
#include <vector>
#include <initializer_list>

template<typename C, typename F>
std::string format_list(C& list, F&& f) {
    std::ostringstream s;
    bool first = true;
    for(const auto& item : list) {
        if(!first) {
            s<<", ";
        }
        first = false;
        s<<f(item);
    }
    return std::move(s).str();
}

inline std::vector<std::string> split(std::string_view s, std::string_view delims) {
    std::vector<std::string> vec;
    size_t old_pos = 0;
    size_t pos = 0;
    while((pos = s.find_first_of(delims, old_pos)) != std::string::npos) {
        vec.emplace_back(s.substr(old_pos, pos - old_pos));
        old_pos = pos + 1;
    }
    vec.emplace_back(s.substr(old_pos));
    return vec;
}

inline constexpr const char * const ws = " \t\n\r\f\v";

inline std::string_view trim(const std::string_view s) {
    const size_t l = s.find_first_not_of(ws);
    const size_t r = s.find_last_not_of(ws) + 1;
    return s.substr(l, r - l);
}

inline std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false) {
    size_t i = 0;
    size_t j;
    std::string output;
    while((j = str.find('\n', i)) != std::string::npos) {
        if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
        output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
        i = j + 1;
    }
    if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
    output.insert(output.end(), str.begin() + i, str.end());
    return output;
}

// TODO: Not happy with this
// template<typename First, typename... Args>
// std::string join(First&& first, Args&&... args, char c) {
//     return first + (... + (c + args));
// }
inline std::string join(std::initializer_list<std::string> list, char c) {
    std::string result;
    bool first = true;
    for(const auto& item : list) {
        if(!first) {
            result += c;
        }
        first = false;
        result += item;
    }
    return result;
}

#endif
