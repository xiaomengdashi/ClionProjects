/**
 * @file ConsistentHashBalancer.h
 * @brief 一致性哈希负载均衡算法实现
 * @details 使用虚拟节点技术实现一致性哈希，保证节点增减时的负载均衡
 */

#ifndef CONSISTENTHASHBALANCER_H
#define CONSISTENTHASHBALANCER_H

#include "LoadBalancer.h"
#include <map>
#include <functional>
#include <sstream>

/**
 * @class ConsistentHashBalancer
 * @brief 一致性哈希负载均衡器
 * @details 使用一致性哈希环和虚拟节点技术，实现请求的均匀分布
 */
class ConsistentHashBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     * @param virtual_nodes_per_server 每个物理服务器的虚拟节点数，默认150
     */
    explicit ConsistentHashBalancer(int virtual_nodes_per_server = 150)
        : virtual_nodes_per_server_(virtual_nodes_per_server) {
        if (virtual_nodes_per_server_ <= 0) {
            virtual_nodes_per_server_ = 150;
        }
    }

    /**
     * @brief 选择服务器
     * @param key 用于哈希的键值
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string& key = "") override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty() || hash_ring_.empty()) {
            return nullptr;
        }

        // 如果没有提供key，生成一个随机key
        std::string hash_key = key.empty() ? generateRandomKey() : key;

        // 计算key的哈希值
        size_t hash = hash_function_(hash_key);

        // 在哈希环上查找第一个大于等于hash的节点
        auto it = hash_ring_.lower_bound(hash);

        // 如果没找到，说明hash值比所有节点都大，选择第一个节点（环形结构）
        if (it == hash_ring_.end()) {
            it = hash_ring_.begin();
        }

        // 从当前位置开始查找可用的服务器
        auto start_it = it;
        do {
            auto server = it->second;
            if (server->isAlive()) {
                server->addConnection();
                return server;
            }

            // 移动到下一个节点
            ++it;
            if (it == hash_ring_.end()) {
                it = hash_ring_.begin();
            }
        } while (it != start_it);  // 遍历整个环

        // 如果所有服务器都不可用，返回nullptr
        return nullptr;
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Consistent Hash";
    }

    /**
     * @brief 设置每个服务器的虚拟节点数
     * @param count 虚拟节点数量
     */
    void setVirtualNodesCount(int count) {
        if (count > 0) {
            virtual_nodes_per_server_ = count;
            rebuildHashRing();
        }
    }

    /**
     * @brief 获取哈希环的大小（虚拟节点总数）
     * @return 哈希环中的节点数
     */
    size_t getHashRingSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return hash_ring_.size();
    }

protected:
    /**
     * @brief 服务器添加后更新哈希环
     * @param server 新添加的服务器
     */
    void onServerAdded(std::shared_ptr<Server> server) override {
        addServerToHashRing(server);
    }

    /**
     * @brief 服务器移除后更新哈希环
     * @param server 被移除的服务器
     */
    void onServerRemoved(std::shared_ptr<Server> server) override {
        removeServerFromHashRing(server);
    }

    /**
     * @brief 重置哈希环
     */
    void onReset() override {
        hash_ring_.clear();
        rebuildHashRing();
    }

private:
    /**
     * @brief 将服务器添加到哈希环
     * @param server 要添加的服务器
     */
    void addServerToHashRing(std::shared_ptr<Server> server) {
        // 根据服务器权重调整虚拟节点数
        int virtual_nodes = virtual_nodes_per_server_ * server->getWeight();

        for (int i = 0; i < virtual_nodes; ++i) {
            // 为每个虚拟节点生成唯一的键
            std::string virtual_key = generateVirtualKey(server->getId(), i);
            size_t hash = hash_function_(virtual_key);

            // 添加到哈希环
            hash_ring_[hash] = server;
        }
    }

    /**
     * @brief 从哈希环移除服务器
     * @param server 要移除的服务器
     */
    void removeServerFromHashRing(std::shared_ptr<Server> server) {
        // 移除该服务器的所有虚拟节点
        auto it = hash_ring_.begin();
        while (it != hash_ring_.end()) {
            if (it->second->getId() == server->getId()) {
                it = hash_ring_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief 重建整个哈希环
     */
    void rebuildHashRing() {
        hash_ring_.clear();
        for (const auto& server : servers_) {
            addServerToHashRing(server);
        }
    }

    /**
     * @brief 生成虚拟节点的键
     * @param server_id 服务器ID
     * @param index 虚拟节点索引
     * @return 虚拟节点键
     */
    std::string generateVirtualKey(const std::string& server_id, int index) {
        std::stringstream ss;
        ss << server_id << "#VN" << index;
        return ss.str();
    }

    /**
     * @brief 生成随机键
     * @return 随机生成的键
     */
    std::string generateRandomKey() {
        static std::atomic<uint64_t> counter(0);
        std::stringstream ss;
        ss << "random_key_" << counter.fetch_add(1) << "_"
           << std::chrono::steady_clock::now().time_since_epoch().count();
        return ss.str();
    }

    /**
     * @brief MurmurHash3算法实现
     * @param key 输入键
     * @param seed 哈希种子
     * @return 哈希值
     */
    uint32_t murmur3_32(const char* key, size_t len, uint32_t seed = 0) {
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;
        const uint32_t r1 = 15;
        const uint32_t r2 = 13;
        const uint32_t m = 5;
        const uint32_t n = 0xe6546b64;

        uint32_t hash = seed;
        const int nblocks = len / 4;
        const uint32_t* blocks = (const uint32_t*)key;

        for (int i = 0; i < nblocks; i++) {
            uint32_t k = blocks[i];
            k *= c1;
            k = (k << r1) | (k >> (32 - r1));
            k *= c2;

            hash ^= k;
            hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
        }

        const uint8_t* tail = (const uint8_t*)(key + nblocks * 4);
        uint32_t k1 = 0;

        switch (len & 3) {
            case 3: k1 ^= tail[2] << 16; [[fallthrough]];
            case 2: k1 ^= tail[1] << 8; [[fallthrough]];
            case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << r1) | (k1 >> (32 - r1));
                k1 *= c2;
                hash ^= k1;
        }

        hash ^= len;
        hash ^= (hash >> 16);
        hash *= 0x85ebca6b;
        hash ^= (hash >> 13);
        hash *= 0xc2b2ae35;
        hash ^= (hash >> 16);

        return hash;
    }

private:
    int virtual_nodes_per_server_;                          // 每个服务器的虚拟节点数
    std::map<size_t, std::shared_ptr<Server>> hash_ring_;   // 哈希环（有序map）

    // 使用MurmurHash作为哈希函数
    std::function<size_t(const std::string&)> hash_function_ =
        [this](const std::string& key) -> size_t {
            return murmur3_32(key.c_str(), key.length(), 0x9747b28c);
        };
};

#endif // CONSISTENTHASHBALANCER_H