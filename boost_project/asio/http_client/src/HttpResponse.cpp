#include "HttpResponse.h"
#include <algorithm>
#include <string>

void HttpResponse::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::SetHeaders(const std::map<std::string, std::string>& headers) {
    headers_ = headers;
}

std::string HttpResponse::GetHeader(const std::string& key) const {
    auto it = headers_.find(key);
    if (it != headers_.end()) {
        return it->second;
    }

    // Try case-insensitive search
    for (const auto& [k, v] : headers_) {
        std::string k_lower = k;
        std::string key_lower = key;
        std::transform(k_lower.begin(), k_lower.end(), k_lower.begin(), ::tolower);
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
        if (k_lower == key_lower) {
            return v;
        }
    }

    return "";
}
