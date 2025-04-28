#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include "singleton.hpp"
#include "count_down_latch.hpp"



struct Student {
    unsigned int id;
    std::string name;

    unsigned int get_id() const {
        return id;
    }
};

#define the_student util::Singleton<Student>::GetInstance()

void worker(util::CountDownLatch& latch, int id) {
    std::cout << "Worker " << id << " is starting" << std::endl;
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Worker " << id << " is done" << std::endl;
    latch.CountDown();
}

int main() {
    std::cout << the_student->get_id() << std::endl;
    the_student->id = 1;
    std::cout << the_student->get_id() << std::endl;
    std::cout << the_student->name << std::endl;
    the_student->name = "张三";
    std::cout << the_student->name << std::endl;



    int num_workers = 5;
    util::CountDownLatch latch(num_workers);

    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back(worker, std::ref(latch), i);
    }

    std::cout << "Main thread is waiting for workers to finish" << std::endl;
    latch.Await();
    std::cout << "All workers have finished" << std::endl;

    for (auto& worker : workers) {
        worker.join();
    }


    return 0;
}