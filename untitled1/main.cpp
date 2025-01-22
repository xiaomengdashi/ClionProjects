#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "myshared_ptr.hpp"
#include "MyVector.hpp"


using std::chrono::system_clock;

// 使用 typedef 定义函数指针类型
typedef int(*fncPtr)(int, int);

int add(int a, int b) {
    return a + b;
}


void clock_test() {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    auto s = start.time_since_epoch();

    std::cout << "s:" << s.count() << std::endl;
    std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
    end.time_since_epoch();
    std::chrono::duration<double> elapsed_seconds = end - start;

    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
    system_clock::time_point epoch;

    std::chrono::hours h(10 *20);

    system_clock::time_point tp = epoch + h;


    std::chrono::time_point<system_clock, system_clock::duration> a = system_clock::now();

    std::cout << "a:" << a.time_since_epoch().count()<< std::endl;

    // 获取当前时间，并格式化成YY-MM-DD HH:MM:SS 格式

    std::time_t now = std::chrono::system_clock::to_time_t(a);

     auto b = std::ctime(&now);
    std::cout << "b:" << b << std::endl;



}

void duration_cast_test() {
    std::chrono::seconds sec(3600);
    std::chrono::hours ms = std::chrono::duration_cast<std::chrono::hours>(sec);
    std::cout << "ms:" << ms.count() << std::endl;
}

template <typename T>
using time_point = std::chrono::time_point<std::chrono::system_clock, T>;

void time_point_cast_test() {


    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock, std::chrono::hours> tp2 = std::chrono::time_point_cast<std::chrono::hours>(tp);
    std::cout << "tp2:" << tp2.time_since_epoch().count() << std::endl;
}


void thread_test() {
   auto s =  std::this_thread::get_id();
    std::cout << "thread id: " << s << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));

    std::this_thread::yield();

    std::thread t([=](int a, int b) {
        std::cout << s << std::endl;
        std::cout << "id: " <<std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));

    }, 1, 2);

    std::cout << t.joinable() << std::endl;
    t.join();
    std::cout << t.joinable();

    std::cout << std::thread::hardware_concurrency() << std::endl;



}

class A {
public:
    A() {
        std::cout << "A()" << std::endl;
        m = new int(1);
    }

    A(int* ptr) {
        std::cout << "A(int)" << std::endl;
        m = ptr;
    }

    A(const A& other) {
        std::cout << "A(const A&)" << std::endl;
        m = new int(*other.m);
    }

    A(A&& other) noexcept {
        std::cout << "A(A&&)" << std::endl;
        m = other.m;
        other.m = nullptr;
    }

    A& operator=(const A& other) {
        std::cout << "operator=" << std::endl;
        if (this != &other) {
            delete m;
            m = new int(*other.m);
        }
        return *this;
    }

    A& operator=(A&& other) noexcept {
        std::cout << "operator=" << std::endl;
        if (this!= &other) {
            delete m;
            m = other.m;
            other.m = nullptr;
        }
        return *this;
    }

    ~A() {
        std::cout << "~A()" << std::endl;
        delete m;
    }

    int get() {
        return *m;
    }


private:
    int *m{};
};


class MyClass {
public:
    // 使用 typedef 定义类型别名
    typedef std::vector<int> IntVector;

    // 使用类型别名作为成员变量的类型
    IntVector myVector;

    // 使用类型别名作为函数参数的类型
    void processVector(IntVector& vec) {
        // 处理向量的代码
    }
};


void test_func() {
    fncPtr fun = add; // 将函数 add 的地址赋给 funcPtr
    int result = fun(2, 3); // 通过 funcPtr 调用 add 函数
    std::cout << "Result: " << result << std::endl;
}

void test_A() {
    A a(new int(1));
    A b(a);
    A c(std::move(a));
    A d = A();
    A e = A(new int(1));
    A f = A(new int(1));
    d = f;
}

void tese_def() {
    MyClass::IntVector vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
}

void test_myshared_ptr()
{
    std::cout << "=======test_myshared_ptr========" << std::endl;
    shared_ptr<int> p1(new int(10));
    std::cout << *p1.get() << std::endl;
    std::cout<< p1.use_count() <<std::endl;

    shared_ptr<int> p2 = p1;
    shared_ptr<int> p3;
    p3 = p2;
    shared_ptr<int> p4 = std::move(p3);
    std::cout<< p4.use_count() <<std::endl;

    shared_ptr<int> p5(new int(100));
    p5 = std::move(p4);
    std::cout<< p5.use_count() <<std::endl;

    shared_ptr<std::string> s1(new  std::string("hell0000000000000000o"));
    std::cout << s1->size() << std::endl;

}


void test_vector() {
    MyVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);

    std::cout << vec.size() << std::endl;
    std::cout << vec.capacity() << std::endl;

    for (size_t i = 0; i < vec.size(); i++) {
        std::cout << vec[i] << std::endl;
    }

    for (auto it = vec.begin(); it != vec.end(); it++) {
        std::cout << *it << std::endl;
    }

    vec.pop_back();
    std::cout << "Size after pop_back: " << vec.size() << std::endl;

}

int main() {
//    test_func();
//    test_A();
//
//    clock_test();
//    duration_cast_test();
//    time_point_cast_test();
//    thread_test();
//
//    tese_def();



  //  test_myshared_ptr();

    test_vector();

    return 0;
}
