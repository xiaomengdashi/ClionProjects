//
// Created by Kolane on 2025/3/12.
//

#ifndef BOOST_DEMO_URLDECODE_H
#define BOOST_DEMO_URLDECODE_H

#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

class UrlParser {
public:
    explicit UrlParser(const std::string& url) {
        parse(url);
    }

    // 获取路径部分（不含查询参数）
    const std::string& path() const { return path_; }

    // 获取查询参数映射表
    const std::unordered_map<std::string, std::string>& params() const { return params_; }

    // 获取指定参数值（带默认值）
    std::string get_param(const std::string& key, const std::string& default_val = "") const {
        auto it = params_.find(key);
        return it != params_.end() ? it->second : default_val;
    }

    // URL解码静态方法
    static std::string decode(const std::string& str) {
        std::ostringstream decoded;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                int hex_val;
                std::istringstream hex_stream(str.substr(i + 1, 2));
                if (hex_stream >> std::hex >> hex_val) {
                    decoded << static_cast<char>(hex_val);
                    i += 2;
                } else {
                    decoded << str[i];
                }
            } else if (str[i] == '+') {
                decoded << ' ';
            } else {
                decoded << str[i];
            }
        }
        return decoded.str();
    }

private:
    void parse(const std::string& url) {
        // 分离路径和查询参数
        size_t query_start = url.find('?');
        path_ = url.substr(0, query_start);

        if (query_start != std::string::npos) {
            parse_query(url.substr(query_start + 1));
        }

        // 解码路径
        path_ = decode(path_);
    }

    void parse_query(const std::string& query) {
        std::istringstream query_stream(query);
        std::string pair;

        while (std::getline(query_stream, pair, '&')) {
            size_t delim_pos = pair.find('=');
            if (delim_pos == std::string::npos) continue;

            std::string key = decode(pair.substr(0, delim_pos));
            std::string value = decode(pair.substr(delim_pos + 1));

            // 转换为小写键（可选）
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            params_[key] = value;
        }
    }

    std::string path_;
    std::unordered_map<std::string, std::string> params_;
};


#endif //BOOST_DEMO_URLDECODE_H
