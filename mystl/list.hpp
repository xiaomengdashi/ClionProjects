#pragma once


#include <list>
#include <vector>
#include <set>


inline void list_test()
{
    int ar[5] = {1,2,3,4,5};
    std::list<int> list;

    list.insert(list.begin(),ar,ar+5);
    list.insert(list.begin(), 10);
    list.insert(list.end(), 20);
    auto it = list.begin();
    std::cout << list.size() << std::endl;
    std::cout << list.empty() << std::endl;
    std::cout << *++list.begin() << std::endl;
    list.assign(10,100);

    std::cout << "===================" << std::endl;
    while(it !=list.end()) {
        std::cout << *it << std::endl;
        ++it;
    }
}


