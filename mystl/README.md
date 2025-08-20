# MySTL - 自定义STL实现

这是一个自定义的STL（Standard Template Library）实现项目，包含了常用的容器、内存管理、算法和并发工具。

## 项目结构

```
mystl/
├── README.md               # 项目说明文档
├── containers/             # 容器实现
│   ├── vector.cpp          # 动态数组实现
│   ├── vector_v2.cpp       # 动态数组实现（版本2）
│   ├── list.cpp            # 双向链表实现
│   ├── list_v2.cpp         # 双向链表实现（版本2）
│   ├── deque.cpp           # 双端队列实现
│   ├── deque_v2.cpp        # 双端队列实现（版本2）
│   ├── bst.cpp             # 二叉搜索树实现
│   └── dynamic_array.cpp   # 动态数组实现
├── memory/                 # 内存管理
│   ├── unique_ptr.cpp      # 智能指针unique_ptr实现
│   ├── shared_ptr.cpp      # 智能指针shared_ptr实现
│   ├── weak_ptr.cpp        # 智能指针weak_ptr实现
│   ├── allocator.cpp       # 内存分配器实现
│   └── memory_pool.cpp     # 内存池实现
├── algorithms/             # 算法实现
│   └── algorithm.cpp       # 通用算法实现
├── concurrency/            # 并发工具
│   ├── lock.cpp            # 锁实现
│   ├── mutex.cpp           # 互斥量实现
│   ├── task.cpp            # 任务调度实现
│   ├── thread_test.cpp     # 线程测试实现
│   └── threadsafe_stack.cpp # 线程安全栈实现
├── utils/                  # 工具类
│   ├── logger.cpp          # 日志工具实现
│   └── time_utils.cpp      # 时间工具实现
├── examples/               # 示例代码
│   └── class_tests.cpp     # 类测试示例
└── test_*                  # 编译生成的测试可执行文件
```

## 主要特性

### 容器 (Containers)
- **Vector**: 动态数组，支持随机访问和动态扩容
- **List**: 双向链表，支持高效的插入和删除操作
- **Deque**: 双端队列，支持两端的高效插入和删除
- **BST**: 二叉搜索树，支持有序数据的存储和查找
- **DynamicArray**: 另一种动态数组实现

### 内存管理 (Memory Management)
- **UniquePtr**: 独占所有权的智能指针，支持自定义删除器和数组特化
- **SharedPtr**: 共享所有权的智能指针，使用引用计数管理内存
- **WeakPtr**: 弱引用智能指针，解决循环引用问题
- **Allocator**: 自定义内存分配器
- **MemoryPool**: 内存池，提高内存分配效率

### 算法 (Algorithms)
- 通用算法实现，包括排序、查找等常用算法

### 并发工具 (Concurrency)
- **Lock**: 锁机制实现
- **Mutex**: 互斥量实现
- **Task**: 任务调度系统
- **ThreadSafeStack**: 线程安全的栈实现

### 工具类 (Utils)
- **Common**: 通用头文件和宏定义
- **Logger**: 日志记录工具

## 编译和运行

### 前提条件
- 支持C++20的编译器（GCC 10+, Clang 10+, MSVC 2019+）
- 本项目已通过C++20标准编译测试

### 编译步骤

本项目采用直接编译的方式，无需CMake。每个模块都可以独立编译：

```bash
# 编译容器模块
g++ -std=c++20 -o test_vector containers/vector.cpp
g++ -std=c++20 -o test_deque containers/deque.cpp
g++ -std=c++20 -o test_list containers/list.cpp

# 编译内存管理模块
g++ -std=c++20 -o test_shared_ptr memory/shared_ptr.cpp
g++ -std=c++20 -o test_unique_ptr memory/unique_ptr.cpp
g++ -std=c++20 -o test_memory_pool memory/memory_pool.cpp

# 编译并发模块（需要pthread支持）
g++ -std=c++20 -pthread -o test_thread_test concurrency/thread_test.cpp
g++ -std=c++20 -pthread -o test_threadsafe_stack concurrency/threadsafe_stack.cpp

# 编译算法和工具模块
g++ -std=c++20 -o test_algorithm algorithms/algorithm.cpp
g++ -std=c++20 -o test_logger utils/logger.cpp
```

### 运行测试

```bash
# 运行容器测试
./test_vector
./test_deque
./test_list

# 运行内存管理测试
./test_shared_ptr
./test_unique_ptr
./test_memory_pool

# 运行并发测试
./test_thread_test
./test_threadsafe_stack
```

## 使用说明

### 基本使用示例

每个模块都包含完整的实现和测试代码，可以直接编译运行：

```cpp
// Vector容器使用示例（来自vector.cpp）
MyVector<int> vec;
vec.push_back(1);
vec.push_back(2);
vec.push_back(3);
std::cout << "Vector size: " << vec.size() << std::endl;

// 智能指针使用示例（来自shared_ptr.cpp）
MySharedPtr<int> ptr1 = MyMakeShared<int>(42);
MySharedPtr<int> ptr2 = ptr1;
std::cout << "Reference count: " << ptr1.use_count() << std::endl;

// 线程安全栈使用示例（来自threadsafe_stack.cpp）
ThreadSafeStack<int> stack;
stack.push(10);
int value;
if (stack.try_pop(value)) {
    std::cout << "Popped value: " << value << std::endl;
}
```

## 项目特点

1. **C++20标准**: 全面采用C++20标准，利用最新的语言特性
2. **模块化设计**: 按功能模块分类，每个模块独立实现和测试
3. **完整实现**: 包含完整的容器、内存管理、并发和算法实现
4. **测试覆盖**: 每个模块都包含详细的测试代码
5. **线程安全**: 并发模块提供线程安全的数据结构实现
6. **内存管理**: 实现了完整的智能指针和内存池系统

## 编译测试结果

本项目已通过完整的C++20编译测试：

- ✅ 所有容器模块编译成功
- ✅ 所有内存管理模块编译成功  
- ✅ 所有并发模块编译成功
- ✅ 所有算法和工具模块编译成功
- ✅ 所有测试程序运行正常

注意：编译过程中可能出现一些关于未使用参数的警告，这些警告不影响程序的正常运行。

## 贡献指南

欢迎提交Issue和Pull Request来改进这个项目。在提交代码前，请确保：

1. 代码符合项目的命名规范
2. 添加适当的注释和文档
3. 通过所有测试用例
4. 遵循现有的代码风格

## 许可证

本项目采用MIT许可证，详情请参见LICENSE文件。