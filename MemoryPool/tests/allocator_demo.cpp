#include "../utils/PoolAllocator.hpp"
#include "../core/VariableMemoryPool.hpp"
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <memory>


int main() {
    // 1. 创建一个内存池实例
    // 这里我们使用VariableMemoryPool，因为它更通用，可以处理不同大小的分配
    // 假设我们预分配1MB的内存给这个池子
    auto my_global_pool = std::make_shared<VariableMemoryPool>(1024 * 1024); 

    // 2. 创建一个使用该内存池的STL分配器
    // PoolAllocator<T> 的模板参数T是你容器中存储的元素类型
    // 构造函数传入你创建的内存池实例
    PoolAllocator<int, VariableMemoryPool> int_allocator(*my_global_pool);
    PoolAllocator<std::string, VariableMemoryPool> string_allocator(*my_global_pool);

    // 3. 声明STL容器时，指定使用你的自定义分配器
    // std::vector<元素类型, 分配器类型> 构造函数传入分配器实例
    std::vector<int, PoolAllocator<int, VariableMemoryPool>> my_int_vector(int_allocator);

    // std::map<Key类型, Value类型, 比较器类型, 分配器类型>
    std::map<std::string, int, std::less<std::string>, PoolAllocator<std::pair<const std::string, int>, VariableMemoryPool>> my_string_map(string_allocator);

    // 4. 像使用普通STL容器一样操作它们！
    // 所有的内存分配和释放都将自动通过你的内存池进行，无需手动干预！

    my_int_vector.push_back(10);
    my_int_vector.push_back(20);
    my_int_vector.emplace_back(30);

    for(auto it: my_int_vector) {
        std::cout << it << " ";
    }
    std::cout <<std::endl;

    my_string_map["hello"] = 1;
    my_string_map["world"] = 2;
    my_string_map.insert({"cpp", 3});

    for(auto it : my_string_map) {
        std::cout << it.first << ": " << it.second << " ";
    }
    std::cout << std::endl;

    // 当容器销毁时，它所占用的内存也会自动归还给内存池
    // 无需手动deallocate
    return 0;
}