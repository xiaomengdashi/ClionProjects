#include <iostream>
#include <string>
#include "singleton.hpp"



struct Student {
    unsigned int id;
    std::string name;

    unsigned int get_id() const {
        return id;
    }
};

#define the_student util::Singleton<Student>::instance()

int main() {


    std::cout << the_student.get_id() << std::endl;
    the_student.id = 1;
    std::cout << the_student.get_id() << std::endl;
    std::cout << the_student.name << std::endl;
    the_student.name = "张三";
    std::cout << the_student.name << std::endl;



    int a = 1;
    float b = a;

    double c = 33.5e39;

    b = c;

    return 0;
}