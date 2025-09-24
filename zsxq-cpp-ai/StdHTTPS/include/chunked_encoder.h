/**
 * @file chunked_encoder.h
 * @brief HTTP chunked传输编码处理器头文件
 * @details 实现HTTP/1.1 chunked传输编码的编码和解码功能
 */

#ifndef STDHTTPS_CHUNKED_ENCODER_H
#define STDHTTPS_CHUNKED_ENCODER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace stdhttps {

/**
 * @brief Chunked编码器
 * @details 将数据编码为HTTP chunked格式，支持流式编码
 */
class ChunkedEncoder {
public:
    /**
     * @brief 构造函数
     */
    ChunkedEncoder() = default;
    
    /**
     * @brief 析构函数
     */
    ~ChunkedEncoder() = default;

    /**
     * @brief 编码数据块
     * @param data 原始数据
     * @param size 数据大小
     * @return 编码后的chunk数据
     */
    std::string encode_chunk(const void* data, size_t size);
    
    /**
     * @brief 编码数据块
     * @param data 原始数据字符串
     * @return 编码后的chunk数据
     */
    std::string encode_chunk(const std::string& data);
    
    /**
     * @brief 生成最后的chunk（大小为0的chunk）
     * @param trailer_headers 可选的trailer头部
     * @return 最后的chunk数据
     */
    std::string encode_final_chunk(const std::vector<std::pair<std::string, std::string>>& trailer_headers = {});
    
    /**
     * @brief 编码完整的消息体为chunked格式
     * @param body 完整的消息体
     * @param chunk_size 每个chunk的大小（默认8KB）
     * @return 编码后的chunked数据
     */
    std::string encode_body(const std::string& body, size_t chunk_size = 8192);

    /**
     * @brief 重置编码器状态
     */
    void reset();

private:
    /**
     * @brief 将数字转换为十六进制字符串
     * @param value 数值
     * @return 十六进制字符串
     */
    std::string to_hex(size_t value) const;
};

/**
 * @brief Chunk数据结构
 */
struct ChunkData {
    std::string data;           // chunk数据内容
    size_t size;               // chunk大小
    bool is_final;             // 是否为最后一个chunk
    std::vector<std::pair<std::string, std::string>> trailer_headers; // trailer头部
    
    ChunkData(const std::string& chunk_data = "", size_t chunk_size = 0, bool final = false)
        : data(chunk_data), size(chunk_size), is_final(final) {}
};

/**
 * @brief Chunked解码器状态枚举
 */
enum class ChunkedDecodeState {
    CHUNK_SIZE,     // 读取chunk大小
    CHUNK_DATA,     // 读取chunk数据
    CHUNK_TRAILER,  // 读取trailer头部
    COMPLETE,       // 解码完成
    ERROR           // 解码错误
};

/**
 * @brief Chunked解码器
 * @details 解码HTTP chunked格式数据，支持流式解码
 */
class ChunkedDecoder {
public:
    /**
     * @brief Chunk回调函数类型
     * @param chunk_data 解码出的chunk数据
     */
    using ChunkCallback = std::function<void(const ChunkData&)>;

    /**
     * @brief 构造函数
     */
    ChunkedDecoder();
    
    /**
     * @brief 析构函数
     */
    ~ChunkedDecoder() = default;

    /**
     * @brief 解码数据
     * @param data 输入数据
     * @param size 数据大小
     * @return 消费的字节数，-1表示错误
     */
    int decode(const char* data, size_t size);
    
    /**
     * @brief 解码数据
     * @param data 输入数据字符串
     * @return 消费的字节数，-1表示错误
     */
    int decode(const std::string& data);
    
    /**
     * @brief 获取解码状态
     */
    ChunkedDecodeState get_state() const { return state_; }
    
    /**
     * @brief 是否解码完成
     */
    bool is_complete() const { return state_ == ChunkedDecodeState::COMPLETE; }
    
    /**
     * @brief 是否有解码错误
     */
    bool has_error() const { return state_ == ChunkedDecodeState::ERROR; }
    
    /**
     * @brief 获取错误信息
     */
    const std::string& get_error() const { return error_message_; }
    
    /**
     * @brief 设置chunk回调函数
     * @param callback 回调函数
     */
    void set_chunk_callback(ChunkCallback callback);
    
    /**
     * @brief 获取完整的解码数据
     * @return 解码后的完整数据
     */
    const std::string& get_decoded_data() const { return decoded_data_; }
    
    /**
     * @brief 获取trailer头部
     * @return trailer头部列表
     */
    const std::vector<std::pair<std::string, std::string>>& get_trailer_headers() const { 
        return trailer_headers_; 
    }
    
    /**
     * @brief 重置解码器状态
     */
    void reset();

private:
    // 状态机处理方法
    int decode_chunk_size_state(const char* data, size_t size, size_t& consumed);
    int decode_chunk_data_state(const char* data, size_t size, size_t& consumed);
    int decode_chunk_trailer_state(const char* data, size_t size, size_t& consumed);
    
    // 辅助方法
    void set_error(const std::string& message);
    size_t find_line_end(const char* data, size_t size, size_t offset = 0) const;
    size_t parse_hex_size(const std::string& hex_str) const;
    void parse_trailer_line(const std::string& line);
    void notify_chunk(const ChunkData& chunk);

private:
    ChunkedDecodeState state_;      // 解码状态
    std::string buffer_;            // 解码缓冲区
    std::string decoded_data_;      // 完整的解码数据
    std::string error_message_;     // 错误信息
    
    size_t current_chunk_size_;     // 当前chunk的大小
    size_t current_chunk_read_;     // 当前chunk已读取的字节数
    
    std::vector<std::pair<std::string, std::string>> trailer_headers_; // trailer头部
    ChunkCallback chunk_callback_;  // chunk回调函数
};

/**
 * @brief Chunked工具类
 * @details 提供chunked编码/解码的便捷方法
 */
class ChunkedUtils {
public:
    /**
     * @brief 将完整数据编码为chunked格式
     * @param data 原始数据
     * @param chunk_size 每个chunk的大小
     * @return 编码后的chunked数据
     */
    static std::string encode(const std::string& data, size_t chunk_size = 8192);
    
    /**
     * @brief 解码完整的chunked数据
     * @param chunked_data chunked格式数据
     * @param decoded_data 输出解码后的数据
     * @param trailer_headers 输出trailer头部
     * @return 是否解码成功
     */
    static bool decode(const std::string& chunked_data, 
                      std::string& decoded_data,
                      std::vector<std::pair<std::string, std::string>>& trailer_headers);
    
    /**
     * @brief 解码完整的chunked数据（简化版本）
     * @param chunked_data chunked格式数据
     * @param decoded_data 输出解码后的数据
     * @return 是否解码成功
     */
    static bool decode(const std::string& chunked_data, std::string& decoded_data);
    
    /**
     * @brief 验证chunked数据格式是否正确
     * @param chunked_data chunked格式数据
     * @return 是否格式正确
     */
    static bool validate(const std::string& chunked_data);
    
    /**
     * @brief 计算chunked编码后的大小
     * @param original_size 原始数据大小
     * @param chunk_size 每个chunk的大小
     * @return 编码后的大小（近似值）
     */
    static size_t calculate_encoded_size(size_t original_size, size_t chunk_size = 8192);

private:
    ChunkedUtils() = delete; // 工具类，不允许实例化
};

/**
 * @brief 流式Chunked编码器
 * @details 支持流式编码，适用于大文件或实时数据传输
 */
class StreamChunkedEncoder {
public:
    /**
     * @brief 数据回调函数类型
     * @param encoded_data 编码后的数据
     */
    using DataCallback = std::function<void(const std::string&)>;

    /**
     * @brief 构造函数
     * @param callback 数据输出回调
     * @param chunk_size 默认chunk大小
     */
    StreamChunkedEncoder(DataCallback callback, size_t chunk_size = 8192);
    
    /**
     * @brief 析构函数
     */
    ~StreamChunkedEncoder();

    /**
     * @brief 写入数据
     * @param data 数据
     * @param size 数据大小
     * @return 是否写入成功
     */
    bool write(const void* data, size_t size);
    
    /**
     * @brief 写入数据
     * @param data 数据字符串
     * @return 是否写入成功
     */
    bool write(const std::string& data);
    
    /**
     * @brief 结束编码
     * @param trailer_headers 可选的trailer头部
     * @return 是否成功
     */
    bool finish(const std::vector<std::pair<std::string, std::string>>& trailer_headers = {});
    
    /**
     * @brief 是否已结束
     */
    bool is_finished() const { return finished_; }
    
    /**
     * @brief 设置chunk大小
     * @param chunk_size chunk大小
     */
    void set_chunk_size(size_t chunk_size);
    
    /**
     * @brief 获取当前chunk大小
     */
    size_t get_chunk_size() const { return chunk_size_; }

private:
    void flush_buffer();
    
private:
    DataCallback callback_;         // 数据输出回调
    size_t chunk_size_;            // chunk大小
    std::string buffer_;           // 内部缓冲区
    bool finished_;                // 是否已结束
    ChunkedEncoder encoder_;       // 内部编码器
};

} // namespace stdhttps

#endif // STDHTTPS_CHUNKED_ENCODER_H