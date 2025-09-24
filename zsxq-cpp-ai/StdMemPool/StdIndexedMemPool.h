/**
 * @file StdIndexedMemPool.h
 * @brief 一个高性能内存池实现
 * 
 * 内存池通过返回整数索引而不是直接返回指针，允许在使用较少位数的情况下管理大量元素。
 * 它还保证被回收的内存不会被返回给操作系统，使得即使在元素被回收后读取也是安全的。
 */

#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <type_traits>
#include <vector>

namespace std_mem_pool {

/**
 * @brief 处理CPU缓存本地性的工具类
 * 
 * 简化版的CacheLocality实现，用于决定如何在多线程环境中分配内存以减少缓存争用
 */
class CacheLocality {
public:
    /**
     * @brief 获取系统的CPU数量
     * @return 系统中的CPU数量
     */
    static size_t getNumCpus() {
        static const size_t numCpus = determineNumCpus();
        return numCpus;
    }

private:
    /**
     * @brief 确定系统中的CPU数量
     * @return 系统中的CPU数量
     */
    static size_t determineNumCpus() {
        long res = sysconf(_SC_NPROCESSORS_ONLN);
        if (res <= 0) {
            return 1; // 如果获取失败，假设只有一个CPU
        }
        return static_cast<size_t>(res);
    }
};

/**
 * @brief 访问分散器，用于分配线程到不同条带以减少缓存争用
 * 
 * 简化版的AccessSpreader实现，用于在多线程环境中分配线程到不同的内存区域
 */
class AccessSpreader {
public:
    /**
     * @brief 获取当前线程应该使用的条带索引
     * @param numStripes 条带总数
     * @return 当前线程应使用的条带索引
     */
    static size_t current(size_t numStripes) {
        static thread_local unsigned currentCpu = threadId() % 256;
        return currentCpu % numStripes;
    }

private:
    /**
     * @brief 获取当前线程ID
     * @return 线程ID
     */
    static unsigned threadId() {
        static std::atomic<unsigned> nextId{0};
        static thread_local unsigned id = nextId.fetch_add(1, std::memory_order_relaxed);
        return id;
    }
};

/**
 * @brief 内存池的特性类，控制对象的生命周期管理
 * 
 * 这个模板类定义了内存池中对象的初始化、清理和回收行为。
 * 可以通过特化这个模板来自定义对象的生命周期管理。
 * 
 * @tparam T 要管理的元素类型
 * @tparam EagerRecycleWhenTrivial 对于简单类型是否使用急切回收策略
 * @tparam EagerRecycleWhenNotTrivial 对于非简单类型是否使用急切回收策略
 */
template <
    typename T,
    bool EagerRecycleWhenTrivial = true,  // 修改为默认true，使得int类型也使用急切回收
    bool EagerRecycleWhenNotTrivial = true>
struct IndexedMemPoolTraits {
    /**
     * @brief 判断是否使用急切回收策略
     * @return 如果应该使用急切回收策略则返回true
     */
    static constexpr bool eagerRecycle() {
        return std::is_trivial<T>::value
            ? EagerRecycleWhenTrivial
            : EagerRecycleWhenNotTrivial;
    }

    /**
     * @brief 当元素第一次分配时初始化
     * @param ptr 指向要初始化的元素的指针
     */
    static void initialize(T* ptr) {
        if constexpr (!eagerRecycle()) {
            new (ptr) T();
        }
    }

    /**
     * @brief 当池销毁时清理元素
     * @param ptr 指向要清理的元素的指针
     */
    static void cleanup(T* ptr) {
        if constexpr (!eagerRecycle()) {
            ptr->~T();
        }
    }

    /**
     * @brief 当元素被分配时调用
     * @param ptr 指向被分配元素的指针
     * @param args 转发给元素构造函数的参数
     */
    template <typename... Args>
    static void onAllocate(T* ptr, Args&&... args) {
        static_assert(
            sizeof...(Args) == 0 || eagerRecycle(),
            "emplace-style allocation requires eager recycle, "
            "which is defaulted only for non-trivial types");
        if (eagerRecycle()) {
            new (ptr) T(std::forward<Args>(args)...);
        }
    }

    /**
     * @brief 当元素被回收时调用
     * @param ptr 指向被回收元素的指针
     */
    static void onRecycle(T* ptr) {
        if (eagerRecycle()) {
            ptr->~T();
        }
    }
};

/**
 * @brief 使用惰性回收策略的特性类
 * 
 * 在这种策略中，元素在第一次从池中分配时被默认构造，并在池销毁时被析构。
 * 
 * @tparam T 要管理的元素类型
 */
template <typename T>
using IndexedMemPoolTraitsLazyRecycle = IndexedMemPoolTraits<T, false, false>;

/**
 * @brief 使用急切回收策略的特性类
 * 
 * 在这种策略中，元素在从池中分配时被构造，在回收到池中时被析构。
 * 
 * @tparam T 要管理的元素类型
 */
template <typename T>
using IndexedMemPoolTraitsEagerRecycle = IndexedMemPoolTraits<T, true, true>;

/**
 * @brief 索引化内存池实现
 * 
 * IndexedMemPool动态分配并池化元素类型(T)，返回4字节整数索引，
 * 可以通过池的operator[]方法访问或获取实际元素的指针。
 * 
 * 关键特性：
 * 1. 即使元素被回收，仍然可以安全读取其内存（但需要验证读取的正确性）
 * 2. 使用索引而不是指针，使用更少的位管理大量元素
 * 3. 通过本地化列表减少线程间的争用
 * 4. 使用mmap预分配整个地址空间，但延迟元素构造
 * 
 * @tparam T 要管理的元素类型
 * @tparam NumLocalLists_ 本地列表数量，默认为32
 * @tparam LocalListLimit_ 本地列表大小限制，默认为200
 * @tparam Atom 原子操作的模板类型，默认为std::atomic
 * @tparam Traits 控制元素生命周期的特性类
 */
template <
    typename T,
    uint32_t NumLocalLists_ = 32,
    uint32_t LocalListLimit_ = 200,
    template <typename> class Atom = std::atomic,
    typename Traits = IndexedMemPoolTraits<T>>
class IndexedMemPool {
public:
    /**
     * @brief 管理的元素类型
     */
    typedef T value_type;

    /**
     * @brief 内存池回收器，用于UniquePtr
     * 
     * 这是一个自定义的Deleter，用于std::unique_ptr，当智能指针被销毁时自动回收内存池中的元素
     */
    struct Recycler {
        /**
         * @brief 指向池的指针
         */
        IndexedMemPool* pool;

        /**
         * @brief 构造函数
         * @param pool 指向池的指针
         */
        explicit Recycler(IndexedMemPool* pool) : pool(pool) {}

        /**
         * @brief 复制构造函数
         */
        Recycler(const Recycler&) = default;
        
        /**
         * @brief 赋值操作符
         */
        Recycler& operator=(const Recycler&) = default;

        /**
         * @brief 函数调用操作符，用于回收元素
         * @param elem 要回收的元素指针
         */
        void operator()(T* elem) const {
            pool->recycleIndex(pool->locateElem(elem));
        }
    };

    /**
     * @brief 智能指针类型，用于自动回收分配的元素
     */
    typedef std::unique_ptr<T, Recycler> UniquePtr;

    /**
     * @brief 禁止拷贝构造和赋值
     */
    IndexedMemPool(const IndexedMemPool&) = delete;
    IndexedMemPool& operator=(const IndexedMemPool&) = delete;

    /**
     * @brief 确保LocalListLimit在8位内
     */
    static_assert(LocalListLimit_ <= 255, "LocalListLimit must fit in 8 bits");
    
    /**
     * @brief 公开的常量
     */
    enum {
        NumLocalLists = NumLocalLists_,  ///< 本地列表数量
        LocalListLimit = LocalListLimit_, ///< 本地列表大小限制
    };

    /**
     * @brief 确保原子类型是无异常默认构造的
     */
    static_assert(
        std::is_nothrow_default_constructible<Atom<uint32_t>>::value,
        "Atom must be nothrow default constructible");

    /**
     * @brief 计算给定容量下可能的最大索引值
     * 
     * @param capacity 要求的容量
     * @return 最大可能的索引值
     */
    static constexpr uint32_t maxIndexForCapacity(uint32_t capacity) {
        // 索引std::numeric_limits<uint32_t>::max()保留用于isAllocated跟踪
        return uint32_t(std::min(
            uint64_t(capacity) + (NumLocalLists - 1) * LocalListLimit,
            uint64_t(std::numeric_limits<uint32_t>::max() - 1)));
    }

    /**
     * @brief 计算给定最大索引下的实际容量
     * 
     * @param maxIndex 最大索引值
     * @return 实际容量
     */
    static constexpr uint32_t capacityForMaxIndex(uint32_t maxIndex) {
        return maxIndex - (NumLocalLists - 1) * LocalListLimit;
    }

    /**
     * @brief 构造一个内存池，可以分配至少capacity个元素
     * 
     * 即使所有本地列表都满了，仍然可以分配capacity个元素
     * 
     * @param capacity 要求的容量
     */
    explicit IndexedMemPool(uint32_t capacity)
        : actualCapacity_(maxIndexForCapacity(capacity)),
          size_(0),
          globalHead_({0, 0}) {
        // 计算需要的内存大小
        const size_t needed = sizeof(Slot) * (actualCapacity_ + 1);
        // 页大小对齐
        size_t pagesize = size_t(sysconf(_SC_PAGESIZE));
        mmapLength_ = ((needed - 1) & ~(pagesize - 1)) + pagesize;
        assert(needed <= mmapLength_ && mmapLength_ < needed + pagesize);
        assert((mmapLength_ % pagesize) == 0);

        // 使用mmap分配内存
        slots_ = static_cast<Slot*>(mmap(
            nullptr,
            mmapLength_,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0));
        if (slots_ == MAP_FAILED) {
            assert(errno == ENOMEM);
            throw std::bad_alloc();
        }
    }

    /**
     * @brief 销毁内存池并清理所有元素
     */
    ~IndexedMemPool() {
        // 清理所有已分配的元素
        for (uint32_t i = maxAllocatedIndex(); i > 0; --i) {
            Traits::cleanup(slots_[i].elemPtr());
            slots_[i].localNext.~Atom<uint32_t>();
            slots_[i].globalNext.~Atom<uint32_t>();
        }
        // 释放内存
        munmap(slots_, mmapLength_);
    }

    /**
     * @brief 返回可同时分配的元素数量的下限
     * 
     * 由于本地列表的存在，实际上可能成功返回更多元素
     * 
     * @return 可同时分配的元素数量下限
     */
    uint32_t capacity() { return capacityForMaxIndex(actualCapacity_); }

    /**
     * @brief 返回池中曾经分配的最大索引
     * 
     * 包括已被回收的元素
     * 
     * @return 曾经分配的最大索引
     */
    uint32_t maxAllocatedIndex() const {
        // 取最小值，因为当size_ == actualCapacity_ - 1时的并发请求可能使size_ > actualCapacity_
        return std::min(uint32_t(size_), uint32_t(actualCapacity_));
    }

    /**
     * @brief 分配一个非零索引的槽位，如果使用急切回收模式则在那里构造一个T
     * 
     * 在槽位被标记为已分配前，会将指向元素的指针传递给Traits::onAllocate
     * 如果没有可用元素，则返回0
     * 
     * @param args 转发给元素构造函数的参数
     * @return 分配的元素索引，如果分配失败则为0
     */
    template <typename... Args>
    uint32_t allocIndex(Args&&... args) {
        auto idx = localPop(localHead());
        if (idx != 0) {
            Slot& s = slot(idx);
            Traits::onAllocate(s.elemPtr(), std::forward<Args>(args)...);
            markAllocated(s);
        }
        return idx;
    }

    /**
     * @brief 分配一个元素并返回指向它的智能指针
     * 
     * 如果有元素可用，返回一个std::unique_ptr，当它被回收时会将元素回收到池中
     * 如果没有可用元素，则返回空指针
     * 
     * @param args 转发给元素构造函数的参数
     * @return 指向分配元素的智能指针，如果分配失败则为nullptr
     */
    template <typename... Args>
    UniquePtr allocElem(Args&&... args) {
        auto idx = allocIndex(std::forward<Args>(args)...);
        T* ptr = idx == 0 ? nullptr : slot(idx).elemPtr();
        return UniquePtr(ptr, Recycler(this));
    }

    /**
     * @brief 回收之前由alloc()分配的元素
     * 
     * @param idx 要回收的元素索引
     */
    void recycleIndex(uint32_t idx) {
        assert(isAllocated(idx));
        localPush(localHead(), idx);
    }

    /**
     * @brief 访问池中由idx引用的元素
     * 
     * @param idx 元素索引
     * @return 对元素的引用
     */
    T& operator[](uint32_t idx) { return *(slot(idx).elemPtr()); }

    /**
     * @brief 访问池中由idx引用的元素（const版本）
     * 
     * @param idx 元素索引
     * @return 对元素的const引用
     */
    const T& operator[](uint32_t idx) const { return *(slot(idx).elemPtr()); }

    /**
     * @brief 定位元素的索引
     * 
     * 如果elem == &pool[idx]，则pool.locateElem(elem) == idx
     * 同时，pool.locateElem(nullptr) == 0
     * 
     * @param elem 元素指针
     * @return 元素索引
     */
    uint32_t locateElem(const T* elem) const {
        if (!elem) {
            return 0;
        }

        static_assert(std::is_standard_layout<Slot>::value, "offsetof needs POD");

        auto slot = reinterpret_cast<const Slot*>(
            reinterpret_cast<const char*>(elem) - offsetof(Slot, elemStorage));
        auto rv = uint32_t(slot - slots_);

        // 这个断言也测试rv是否在范围内
        assert(elem == &(*this)[rv]);
        return rv;
    }

    /**
     * @brief 检查索引是否已分配且未回收
     * 
     * @param idx 要检查的索引
     * @return 如果索引已分配且未回收则为true
     */
    bool isAllocated(uint32_t idx) const {
        return slot(idx).localNext.load(std::memory_order_acquire) == uint32_t(-1);
    }

private:
    /**
     * @brief 槽位结构，用于存储元素和链接信息
     */
    struct Slot {
        /**
         * @brief 元素存储空间
         */
        alignas(T) char elemStorage[sizeof(T)];
        
        /**
         * @brief 本地链表的下一个索引
         */
        Atom<uint32_t> localNext;
        
        /**
         * @brief 全局链表的下一个索引
         */
        Atom<uint32_t> globalNext;

        /**
         * @brief 构造函数
         */
        Slot() : localNext{}, globalNext{} {}
        
        /**
         * @brief 获取元素指针
         * @return 指向元素的指针
         */
        T* elemPtr() { 
            return std::launder(reinterpret_cast<T*>(&elemStorage)); 
        }
        
        /**
         * @brief 获取元素指针（const版本）
         * @return 指向元素的const指针
         */
        const T* elemPtr() const {
            return std::launder(reinterpret_cast<const T*>(&elemStorage));
        }
    };

    /**
     * @brief 带标签的指针结构，用于原子操作
     */
    struct TaggedPtr {
        /**
         * @brief 索引
         */
        uint32_t idx;

        /**
         * @brief 标签和大小
         * 
         * 大小存储在底部8位，标签存储在顶部24位
         */
        uint32_t tagAndSize;

        /**
         * @brief 常量定义
         */
        enum : uint32_t {
            SizeBits = 8,               ///< 大小字段的位数
            SizeMask = (1U << SizeBits) - 1, ///< 大小字段的掩码
            TagIncr = 1U << SizeBits,    ///< 标签增量
        };

        /**
         * @brief 获取大小
         * @return 大小值
         */
        uint32_t size() const { return tagAndSize & SizeMask; }

        /**
         * @brief 创建一个具有替换大小的新TaggedPtr
         * @param repl 替换的大小
         * @return 新的TaggedPtr
         */
        TaggedPtr withSize(uint32_t repl) const {
            assert(repl <= LocalListLimit);
            return TaggedPtr{idx, (tagAndSize & ~SizeMask) | repl};
        }

        /**
         * @brief 创建一个大小增加1的新TaggedPtr
         * @return 新的TaggedPtr
         */
        TaggedPtr withSizeIncr() const {
            assert(size() < LocalListLimit);
            return TaggedPtr{idx, tagAndSize + 1};
        }

        /**
         * @brief 创建一个大小减少1的新TaggedPtr
         * @return 新的TaggedPtr
         */
        TaggedPtr withSizeDecr() const {
            assert(size() > 0);
            return TaggedPtr{idx, tagAndSize - 1};
        }

        /**
         * @brief 创建一个具有替换索引的新TaggedPtr
         * @param repl 替换的索引
         * @return 新的TaggedPtr
         */
        TaggedPtr withIdx(uint32_t repl) const {
            return TaggedPtr{repl, tagAndSize + TagIncr};
        }

        /**
         * @brief 创建一个空的TaggedPtr
         * @return 新的空TaggedPtr
         */
        TaggedPtr withEmpty() const { return withIdx(0).withSize(0); }
    };

    /**
     * @brief 本地列表结构
     */
    struct alignas(64) LocalList {
        /**
         * @brief 列表头
         */
        Atom<TaggedPtr> head;

        /**
         * @brief 构造函数
         */
        LocalList() : head(TaggedPtr{0, 0}) {}
    };

    ///// 字段

    /**
     * @brief 从mmap分配的字节数，是机器页大小的倍数
     */
    size_t mmapLength_;

    /**
     * @brief 实际分配的槽位数量
     * 
     * 这将保证满足构造时请求的容量
     * 槽位编号为1..actualCapacity_（注意从1开始计数）
     * 并占用slots_[1..actualCapacity_]
     */
    uint32_t actualCapacity_;

    /**
     * @brief 记录已经构造的槽位数量
     * 
     * 为了允许使用原子++而不是CAS，我们允许这个值溢出
     * 实际构造的元素数量是min(actualCapacity_, size_)
     */
    Atom<uint32_t> size_;

    /**
     * @brief 原始存储，只有1..min(size_,actualCapacity_)（包含）实际构造
     * 
     * 注意slots_[0]不构造也不使用
     */
    alignas(64) Slot* slots_;

    /**
     * @brief 使用AccessSpreader找到你的列表
     * 
     * 我们使用条带而不是线程局部变量，以避免在线程启动或结束时需要增长或缩小
     * 这些是由localNext链接的列表的头
     */
    LocalList local_[NumLocalLists];

    /**
     * @brief 全局列表头
     * 
     * 这是由globalNext链接的节点列表的头，它们自己又各自是由localNext链接的列表的头
     */
    alignas(64) Atom<TaggedPtr> globalHead_;

    ///// 私有方法

    /**
     * @brief 获取槽位索引
     * @param idx 逻辑索引
     * @return 物理槽位索引
     */
    uint32_t slotIndex(uint32_t idx) const {
        assert(
            0 < idx && idx <= actualCapacity_ &&
            idx <= size_.load(std::memory_order_acquire));
        return idx;
    }

    /**
     * @brief 获取槽位引用
     * @param idx 逻辑索引
     * @return 槽位引用
     */
    Slot& slot(uint32_t idx) { return slots_[slotIndex(idx)]; }

    /**
     * @brief 获取槽位引用（const版本）
     * @param idx 逻辑索引
     * @return 槽位引用
     */
    const Slot& slot(uint32_t idx) const { return slots_[slotIndex(idx)]; }

    /**
     * @brief 将本地列表推入全局列表
     * 
     * localHead引用由localNext链接的完整列表
     * s应该引用slot(localHead)，作为微优化传入
     * 
     * @param s 槽位引用
     * @param localHead 本地列表头
     */
    void globalPush(Slot& s, uint32_t localHead) {
        while (true) {
            TaggedPtr gh = globalHead_.load(std::memory_order_acquire);
            s.globalNext.store(gh.idx, std::memory_order_relaxed);
            if (globalHead_.compare_exchange_strong(gh, gh.withIdx(localHead))) {
                // 成功
                return;
            }
        }
    }

    /**
     * @brief 将单个节点推入本地列表
     * 
     * idx引用单个节点
     * 
     * @param head 列表头
     * @param idx 节点索引
     */
    void localPush(Atom<TaggedPtr>& head, uint32_t idx) {
        Slot& s = slot(idx);
        TaggedPtr h = head.load(std::memory_order_acquire);
        bool recycled = false;
        while (true) {
            s.localNext.store(h.idx, std::memory_order_release);
            if (!recycled) {
                Traits::onRecycle(slot(idx).elemPtr());
                recycled = true;
            }

            if (h.size() == LocalListLimit) {
                // 推入将溢出本地列表，改为窃取它
                if (head.compare_exchange_strong(h, h.withEmpty())) {
                    // 窃取成功，将所有内容放入全局列表
                    globalPush(s, idx);
                    return;
                }
            } else {
                // 本地列表有空间
                if (head.compare_exchange_strong(h, h.withIdx(idx).withSizeIncr())) {
                    // 成功
                    return;
                }
            }
            // h被失败的CAS更新
        }
    }

    /**
     * @brief 从全局列表中弹出一个节点
     * @return 弹出的节点索引，如果为空则为0
     */
    uint32_t globalPop() {
        while (true) {
            TaggedPtr gh = globalHead_.load(std::memory_order_acquire);
            if (gh.idx == 0 ||
                globalHead_.compare_exchange_strong(
                    gh,
                    gh.withIdx(
                        slot(gh.idx).globalNext.load(std::memory_order_relaxed)))) {
                // 全局列表为空，或弹出成功
                return gh.idx;
            }
        }
    }

    /**
     * @brief 从本地列表中弹出一个节点
     * @param head 列表头
     * @return 弹出的节点索引，如果分配失败则为0
     */
    uint32_t localPop(Atom<TaggedPtr>& head) {
        while (true) {
            TaggedPtr h = head.load(std::memory_order_acquire);
            if (h.idx != 0) {
                // 本地列表非空，尝试弹出
                Slot& s = slot(h.idx);
                auto next = s.localNext.load(std::memory_order_relaxed);
                if (head.compare_exchange_strong(h, h.withIdx(next).withSizeDecr())) {
                    // 成功
                    return h.idx;
                }
                continue;
            }

            uint32_t idx = globalPop();
            if (idx == 0) {
                // 全局列表为空，分配并构造新槽位
                if (size_.load(std::memory_order_relaxed) >= actualCapacity_ ||
                    (idx = ++size_) > actualCapacity_) {
                    // 分配失败
                    return 0;
                }
                Slot& s = slot(idx);
                // Atom强制为无异常默认构造
                // 作为优化，使用默认初始化（无括号）而不是直接初始化（有括号）：
                // 这些位置在加载之前先存储
                new (&s.localNext) Atom<uint32_t>;
                new (&s.globalNext) Atom<uint32_t>;
                Traits::initialize(s.elemPtr());
                return idx;
            }

            Slot& s = slot(idx);
            auto next = s.localNext.load(std::memory_order_relaxed);
            if (head.compare_exchange_strong(
                    h, h.withIdx(next).withSize(LocalListLimit))) {
                // 全局列表移动到本地列表，为我们保留头
                return idx;
            }
            // 本地批量推入失败，将idx返回到全局列表并重试
            globalPush(s, idx);
        }
    }

    /**
     * @brief 获取当前线程的本地列表头
     * @return 本地列表头的引用
     */
    Atom<TaggedPtr>& localHead() {
        auto stripe = AccessSpreader::current(NumLocalLists);
        return local_[stripe].head;
    }

    /**
     * @brief 将槽位标记为已分配
     * @param slot 要标记的槽位
     */
    void markAllocated(Slot& slot) {
        slot.localNext.store(uint32_t(-1), std::memory_order_release);
    }

public:
    /**
     * @brief 槽位大小的常量
     */
    static constexpr std::size_t kSlotSize = sizeof(Slot);
};

} // namespace std_mem_pool 