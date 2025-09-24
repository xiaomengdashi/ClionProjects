#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace stdrpc {

/**
 * 序列化器类
 * 负责将各种数据类型序列化为字节流，以及从字节流反序列化回原始数据
 * 使用小端字节序进行数据编码
 */
class Serializer {
private:
    std::vector<uint8_t> buffer_;  // 内部缓冲区，存储序列化后的字节流
    size_t read_pos_ = 0;          // 当前读取位置，用于反序列化

public:
    /**
     * 构造函数
     */
    Serializer() = default;

    /**
     * 从字节流构造序列化器（用于反序列化）
     * @param data 字节流数据
     * @param size 数据大小
     */
    Serializer(const uint8_t* data, size_t size)
        : buffer_(data, data + size), read_pos_(0) {}

    /**
     * 获取序列化后的数据
     * @return 字节流数据
     */
    const std::vector<uint8_t>& getData() const { return buffer_; }

    /**
     * 获取数据大小
     * @return 数据大小（字节）
     */
    size_t getSize() const { return buffer_.size(); }

    /**
     * 清空缓冲区
     */
    void clear() {
        buffer_.clear();
        read_pos_ = 0;
    }

    /**
     * 重置读取位置
     */
    void resetReadPos() { read_pos_ = 0; }

    // ============= 序列化方法 =============

    /**
     * 序列化基本数据类型（POD类型）
     * @param value 要序列化的值
     */
    template<typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    write(const T& value) {
        // 为基本类型预留空间
        size_t old_size = buffer_.size();
        buffer_.resize(old_size + sizeof(T));
        // 直接拷贝字节
        std::memcpy(&buffer_[old_size], &value, sizeof(T));
    }

    /**
     * 序列化字符串
     * @param str 要序列化的字符串
     */
    void write(const std::string& str) {
        // 先写入字符串长度
        uint32_t size = static_cast<uint32_t>(str.size());
        write(size);
        // 再写入字符串内容
        if (size > 0) {
            size_t old_size = buffer_.size();
            buffer_.resize(old_size + size);
            std::memcpy(&buffer_[old_size], str.data(), size);
        }
    }

    /**
     * 序列化C字符串
     * @param str C风格字符串
     */
    void write(const char* str) {
        write(std::string(str));
    }

    /**
     * 序列化字节数组
     * @param data 数据指针
     * @param size 数据大小
     */
    void writeRaw(const void* data, size_t size) {
        size_t old_size = buffer_.size();
        buffer_.resize(old_size + size);
        std::memcpy(&buffer_[old_size], data, size);
    }

    /**
     * 序列化vector容器
     * @param vec 要序列化的vector
     */
    template<typename T>
    void write(const std::vector<T>& vec) {
        // 写入元素个数
        uint32_t size = static_cast<uint32_t>(vec.size());
        write(size);
        // 写入每个元素
        for (const auto& item : vec) {
            write(item);
        }
    }

    // ============= 反序列化方法 =============

    /**
     * 反序列化基本数据类型
     * @return 反序列化后的值
     */
    template<typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, T>::type
    read() {
        // 检查是否有足够的数据
        if (read_pos_ + sizeof(T) > buffer_.size()) {
            throw std::runtime_error("序列化器：读取越界");
        }
        T value;
        std::memcpy(&value, &buffer_[read_pos_], sizeof(T));
        read_pos_ += sizeof(T);
        return value;
    }

    /**
     * 反序列化字符串
     * @return 反序列化后的字符串
     */
    std::string readString() {
        // 先读取字符串长度
        uint32_t size = read<uint32_t>();
        // 检查是否有足够的数据
        if (read_pos_ + size > buffer_.size()) {
            throw std::runtime_error("序列化器：字符串读取越界");
        }
        // 读取字符串内容
        std::string str(reinterpret_cast<const char*>(&buffer_[read_pos_]), size);
        read_pos_ += size;
        return str;
    }

    /**
     * 读取原始字节数据
     * @param data 输出缓冲区
     * @param size 要读取的大小
     */
    void readRaw(void* data, size_t size) {
        if (read_pos_ + size > buffer_.size()) {
            throw std::runtime_error("序列化器：原始数据读取越界");
        }
        std::memcpy(data, &buffer_[read_pos_], size);
        read_pos_ += size;
    }

    /**
     * 反序列化vector容器
     * @return 反序列化后的vector
     */
    template<typename T>
    std::vector<T> readVector() {
        // 读取元素个数
        uint32_t size = read<uint32_t>();
        std::vector<T> vec;
        vec.reserve(size);
        // 读取每个元素
        for (uint32_t i = 0; i < size; ++i) {
            if constexpr (std::is_same_v<T, std::string>) {
                vec.push_back(readString());
            } else {
                vec.push_back(read<T>());
            }
        }
        return vec;
    }

    /**
     * 检查是否还有数据可读
     * @return 如果有数据返回true，否则返回false
     */
    bool hasData() const {
        return read_pos_ < buffer_.size();
    }

    /**
     * 获取剩余可读数据大小
     * @return 剩余字节数
     */
    size_t remainingBytes() const {
        return buffer_.size() - read_pos_;
    }
};

} // namespace stdrpc

#endif // SERIALIZER_HPP