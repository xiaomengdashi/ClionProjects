#include <iostream>
#include <vector>
#include <string>

// 使用 typedef 定义函数指针类型
typedef int(*fncPtr)(int, int);

int add(int a, int b) {
    return a + b;
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

class B {
public:
    B() {
        std::cout << "B()" << std::endl;
    }
    B(const B& other) {
        std::cout << "B(const B&)" << std::endl;
    }
    B(B&& other) noexcept {
        std::cout << "B(B&&)" << std::endl;
    }
    ~B() {
        std::cout << "~B()" << std::endl;
    }

    void print() {
        std::cout << "print()" << std::endl;
    }
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
    A* g = new A;
    delete g;
    A* h = new A();
    delete h;
}

void test_B() {
    B *b = nullptr;
    std::cout <<"BBBBBBBBBB"<< std::endl;
    std::cout << &b << std::endl;
    std::cout << sizeof(*b)<< std::endl;
    b->print();
}

void test_def() {
    MyClass::IntVector vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
}

int main() {
    std::cout << "=======Class Tests=======" << std::endl;
    
    std::cout << "\n--- Function Pointer Test ---" << std::endl;
    test_func();
    
    std::cout << "\n--- Class A Test ---" << std::endl;
    test_A();
    
    std::cout << "\n--- Class B Test ---" << std::endl;
    test_B();
    
    std::cout << "\n--- Typedef Test ---" << std::endl;
    test_def();
    
    return 0;
}