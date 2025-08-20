#include <list>
#include <vector>
#include <set>
#include <iostream>

void list_test()
{
    std::cout << "=======list_test========" << std::endl;
    int ar[5] = {1,2,3,4,5};
    std::list<int> list;

    list.insert(list.begin(),ar,ar+5);
    list.insert(list.begin(), 10);
    list.insert(list.end(), 20);
    
    std::cout << "List size: " << list.size() << std::endl;
    std::cout << "List empty: " << list.empty() << std::endl;
    std::cout << "Second element: " << *++list.begin() << std::endl;
    
    std::cout << "List contents before assign:" << std::endl;
    for (auto it = list.begin(); it != list.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    list.assign(10,100);
    
    std::cout << "List contents after assign:" << std::endl;
    auto it = list.begin();
    while(it != list.end()) {
        std::cout << *it << " ";
        ++it;
    }
    std::cout << std::endl;
}

int main() {
    list_test();
    return 0;
}