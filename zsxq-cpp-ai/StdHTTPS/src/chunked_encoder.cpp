/**
 * @file chunked_encoder.cpp
 * @brief HTTP chunked传输编码处理器实现
 * @details 实现chunked编码和解码的具体功能
 */

#include "chunked_encoder.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace stdhttps {

// ChunkedEncoder实现
std::string ChunkedEncoder::encode_chunk(const void* data, size_t size) {
    if (size == 0) {
        return encode_final_chunk();
    }
    
    std::ostringstream oss;
    // 写入chunk大小（十六进制）
    oss << to_hex(size) << "\r\n";
    // 写入chunk数据
    oss.write(static_cast<const char*>(data), size);
    // 写入chunk结束标记
    oss << "\r\n";
    
    return oss.str();
}

std::string ChunkedEncoder::encode_chunk(const std::string& data) {
    return encode_chunk(data.data(), data.size());
}

std::string ChunkedEncoder::encode_final_chunk(
    const std::vector<std::pair<std::string, std::string>>& trailer_headers) {
    
    std::ostringstream oss;
    // 写入最后一个chunk（大小为0）
    oss << "0\r\n";
    
    // 写入trailer头部
    for (const auto& header : trailer_headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // 写入结束标记
    oss << "\r\n";
    
    return oss.str();
}

std::string ChunkedEncoder::encode_body(const std::string& body, size_t chunk_size) {
    if (body.empty()) {
        return encode_final_chunk();
    }
    
    std::ostringstream oss;
    size_t offset = 0;
    
    while (offset < body.size()) {
        size_t current_chunk_size = std::min(chunk_size, body.size() - offset);
        std::string chunk_data = body.substr(offset, current_chunk_size);
        oss << encode_chunk(chunk_data);
        offset += current_chunk_size;
    }
    
    // 添加最后的chunk
    oss << encode_final_chunk();
    
    return oss.str();
}

void ChunkedEncoder::reset() {
    // 当前实现中编码器是无状态的，所以重置操作为空
}

std::string ChunkedEncoder::to_hex(size_t value) const {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << value;
    return oss.str();
}

// ChunkedDecoder实现
ChunkedDecoder::ChunkedDecoder()
    : state_(ChunkedDecodeState::CHUNK_SIZE)
    , current_chunk_size_(0)
    , current_chunk_read_(0) {
}

int ChunkedDecoder::decode(const char* data, size_t size) {
    if (state_ == ChunkedDecodeState::ERROR || state_ == ChunkedDecodeState::COMPLETE) {
        return 0;
    }
    
    size_t total_consumed = 0;
    
    while (total_consumed < size && 
           state_ != ChunkedDecodeState::COMPLETE && 
           state_ != ChunkedDecodeState::ERROR) {
        
        size_t consumed = 0;
        int result = 0;
        
        switch (state_) {
            case ChunkedDecodeState::CHUNK_SIZE:
                result = decode_chunk_size_state(data + total_consumed, 
                                               size - total_consumed, consumed);
                break;
                
            case ChunkedDecodeState::CHUNK_DATA:
                result = decode_chunk_data_state(data + total_consumed,
                                               size - total_consumed, consumed);
                break;
                
            case ChunkedDecodeState::CHUNK_TRAILER:
                result = decode_chunk_trailer_state(data + total_consumed,
                                                  size - total_consumed, consumed);
                break;
                
            default:
                set_error("未知的解码状态");
                return -1;
        }
        
        if (result < 0) {
            return -1;
        }
        
        total_consumed += consumed;
        
        // 避免无限循环
        if (consumed == 0 && result == 0) {
            break;
        }
    }
    
    return static_cast<int>(total_consumed);
}

int ChunkedDecoder::decode(const std::string& data) {
    return decode(data.data(), data.size());
}

void ChunkedDecoder::set_chunk_callback(ChunkCallback callback) {
    chunk_callback_ = callback;
}

void ChunkedDecoder::reset() {
    state_ = ChunkedDecodeState::CHUNK_SIZE;
    buffer_.clear();
    decoded_data_.clear();
    error_message_.clear();
    current_chunk_size_ = 0;
    current_chunk_read_ = 0;
    trailer_headers_.clear();
}

int ChunkedDecoder::decode_chunk_size_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        // 还没有找到完整的行，将数据添加到缓冲区
        buffer_.append(data, size);
        consumed = size;
        
        if (buffer_.size() > 1024) { // chunk大小行不应该太长
            set_error("chunk大小行过长");
            return -1;
        }
        return 0;
    }
    
    // 找到完整的行
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2; // +2 for CRLF
    
    // 去除行尾的CRLF
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    buffer_.clear();
    
    // 解析chunk大小（可能包含扩展信息，用分号分隔）
    size_t semicolon_pos = line.find(';');
    std::string size_str = line.substr(0, semicolon_pos);
    
    // 去除前后空格
    size_str.erase(0, size_str.find_first_not_of(" \t"));
    size_str.erase(size_str.find_last_not_of(" \t") + 1);
    
    try {
        current_chunk_size_ = parse_hex_size(size_str);
    } catch (const std::exception&) {
        set_error("chunk大小格式错误: " + size_str);
        return -1;
    }
    
    current_chunk_read_ = 0;
    
    if (current_chunk_size_ == 0) {
        // 最后一个chunk，接下来读取trailer
        state_ = ChunkedDecodeState::CHUNK_TRAILER;
    } else {
        // 读取chunk数据
        state_ = ChunkedDecodeState::CHUNK_DATA;
    }
    
    return 1;
}

int ChunkedDecoder::decode_chunk_data_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    
    // 计算需要读取的字节数
    size_t bytes_needed = current_chunk_size_ - current_chunk_read_;
    size_t bytes_to_read = std::min(size, bytes_needed);
    
    // 读取chunk数据
    std::string chunk_data(data, bytes_to_read);
    decoded_data_.append(chunk_data);
    current_chunk_read_ += bytes_to_read;
    consumed = bytes_to_read;
    
    // 检查是否读完当前chunk
    if (current_chunk_read_ >= current_chunk_size_) {
        // 通知chunk完成
        ChunkData chunk(chunk_data, current_chunk_size_, false);
        notify_chunk(chunk);
        
        // 需要跳过chunk后面的CRLF
        if (size > bytes_to_read) {
            if (size >= bytes_to_read + 2 &&
                data[bytes_to_read] == '\r' && data[bytes_to_read + 1] == '\n') {
                consumed += 2;
                state_ = ChunkedDecodeState::CHUNK_SIZE;
            } else if (size == bytes_to_read + 1) {
                // 可能CRLF被分割了，暂存到buffer
                if (data[bytes_to_read] == '\r') {
                    buffer_ = "\r";
                    consumed += 1;
                    state_ = ChunkedDecodeState::CHUNK_SIZE;
                } else {
                    set_error("chunk数据后缺少CRLF");
                    return -1;
                }
            } else {
                set_error("chunk数据后缺少CRLF");
                return -1;
            }
        } else {
            // 等待更多数据读取CRLF
            state_ = ChunkedDecodeState::CHUNK_SIZE;
        }
    }
    
    return consumed > 0 ? 1 : 0;
}

int ChunkedDecoder::decode_chunk_trailer_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        buffer_.append(data, size);
        consumed = size;
        
        if (buffer_.size() > 8192) { // trailer不应该太大
            set_error("trailer过大");
            return -1;
        }
        return 0;
    }
    
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2;
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    buffer_.clear();
    
    if (line.empty()) {
        // trailer结束，解码完成
        state_ = ChunkedDecodeState::COMPLETE;
        
        // 通知最后一个chunk
        ChunkData final_chunk("", 0, true);
        final_chunk.trailer_headers = trailer_headers_;
        notify_chunk(final_chunk);
    } else {
        // 解析trailer行
        parse_trailer_line(line);
    }
    
    return 1;
}

void ChunkedDecoder::set_error(const std::string& message) {
    state_ = ChunkedDecodeState::ERROR;
    error_message_ = message;
}

size_t ChunkedDecoder::find_line_end(const char* data, size_t size, size_t offset) const {
    for (size_t i = offset; i < size - 1; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::string::npos;
}

size_t ChunkedDecoder::parse_hex_size(const std::string& hex_str) const {
    if (hex_str.empty()) {
        throw std::invalid_argument("空的十六进制字符串");
    }
    
    size_t result = 0;
    for (char c : hex_str) {
        result *= 16;
        if (c >= '0' && c <= '9') {
            result += c - '0';
        } else if (c >= 'A' && c <= 'F') {
            result += c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            result += c - 'a' + 10;
        } else {
            throw std::invalid_argument("无效的十六进制字符: " + std::string(1, c));
        }
    }
    
    return result;
}

void ChunkedDecoder::parse_trailer_line(const std::string& line) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string name = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // 去除前后空格
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        trailer_headers_.emplace_back(name, value);
    }
}

void ChunkedDecoder::notify_chunk(const ChunkData& chunk) {
    if (chunk_callback_) {
        chunk_callback_(chunk);
    }
}

// ChunkedUtils实现
std::string ChunkedUtils::encode(const std::string& data, size_t chunk_size) {
    ChunkedEncoder encoder;
    return encoder.encode_body(data, chunk_size);
}

bool ChunkedUtils::decode(const std::string& chunked_data, 
                         std::string& decoded_data,
                         std::vector<std::pair<std::string, std::string>>& trailer_headers) {
    ChunkedDecoder decoder;
    int result = decoder.decode(chunked_data);
    
    if (result < 0 || decoder.has_error()) {
        return false;
    }
    
    if (!decoder.is_complete()) {
        return false;
    }
    
    decoded_data = decoder.get_decoded_data();
    trailer_headers = decoder.get_trailer_headers();
    return true;
}

bool ChunkedUtils::decode(const std::string& chunked_data, std::string& decoded_data) {
    std::vector<std::pair<std::string, std::string>> trailer_headers;
    return decode(chunked_data, decoded_data, trailer_headers);
}

bool ChunkedUtils::validate(const std::string& chunked_data) {
    std::string decoded;
    return decode(chunked_data, decoded);
}

size_t ChunkedUtils::calculate_encoded_size(size_t original_size, size_t chunk_size) {
    if (original_size == 0) {
        return 5; // "0\r\n\r\n"
    }
    
    size_t num_chunks = (original_size + chunk_size - 1) / chunk_size;
    size_t overhead_per_chunk = 10; // 估计值：十六进制大小 + 2个CRLF
    size_t total_overhead = num_chunks * overhead_per_chunk + 5; // +5 for final chunk
    
    return original_size + total_overhead;
}

// StreamChunkedEncoder实现
StreamChunkedEncoder::StreamChunkedEncoder(DataCallback callback, size_t chunk_size)
    : callback_(callback), chunk_size_(chunk_size), finished_(false) {
}

StreamChunkedEncoder::~StreamChunkedEncoder() {
    if (!finished_) {
        // 自动结束编码
        finish();
    }
}

bool StreamChunkedEncoder::write(const void* data, size_t size) {
    if (finished_) {
        return false;
    }
    
    buffer_.append(static_cast<const char*>(data), size);
    
    // 如果缓冲区达到chunk大小，则刷新
    while (buffer_.size() >= chunk_size_) {
        std::string chunk_data = buffer_.substr(0, chunk_size_);
        buffer_.erase(0, chunk_size_);
        
        std::string encoded = encoder_.encode_chunk(chunk_data);
        callback_(encoded);
    }
    
    return true;
}

bool StreamChunkedEncoder::write(const std::string& data) {
    return write(data.data(), data.size());
}

bool StreamChunkedEncoder::finish(const std::vector<std::pair<std::string, std::string>>& trailer_headers) {
    if (finished_) {
        return false;
    }
    
    // 刷新剩余缓冲区
    flush_buffer();
    
    // 发送最后的chunk
    std::string final_chunk = encoder_.encode_final_chunk(trailer_headers);
    callback_(final_chunk);
    
    finished_ = true;
    return true;
}

void StreamChunkedEncoder::set_chunk_size(size_t chunk_size) {
    if (!finished_ && chunk_size > 0) {
        // 如果正在改变chunk大小，先刷新当前缓冲区
        if (!buffer_.empty()) {
            flush_buffer();
        }
        chunk_size_ = chunk_size;
    }
}

void StreamChunkedEncoder::flush_buffer() {
    if (!buffer_.empty()) {
        std::string encoded = encoder_.encode_chunk(buffer_);
        callback_(encoded);
        buffer_.clear();
    }
}

} // namespace stdhttps