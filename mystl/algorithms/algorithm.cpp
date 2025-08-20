#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>

void _display(std::random_access_iterator_tag) {
    std::cout << "Random Access Iterator" << std::endl;
}

void _display(std::input_iterator_tag)
{
    std::cout << "Input Iterator" << std::endl;
}

void _display(std::output_iterator_tag)
{
    std::cout << "Output Iterator" << std::endl;
}

void _display(std::forward_iterator_tag)
{
    std::cout << "Forward Iterator" << std::endl;
}

void _display(std::bidirectional_iterator_tag)
{
    std::cout << "Bidirectional Iterator" << std::endl;
}

template <typename T>
void display(T itr)
{
    typename std::iterator_traits<T>::iterator_category cat;
    _display(cat);
}

void algorithm_test()
{
    std::cout << "=======algorithm_test========" << std::endl;
    using namespace std;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);
    vec.push_back(6);
    vec.push_back(7);

    std::cout << "Vector contents:" << std::endl;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    std::cout << "Iterator type: ";
    display(std::vector<int>::iterator());

    std::cout << "Sum of elements: " << std::accumulate(vec.begin(), vec.end(), 0) << std::endl;

    std::cout << "Using for_each:" << std::endl;
    std::for_each(vec.begin(), vec.end(), [](int i)
      {
            std::cout << i << " ";
      });
    std::cout << std::endl;

    std::cout << "Count of elements divisible by 3: " << std::count_if(vec.begin(), vec.end(), [](int i) { return i % 3 == 0; }) << std::endl;
    
    std::sort(vec.begin(), vec.end());
    std::cout << "After sort: ";
    for (int i : vec) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
    
    std::reverse(vec.begin(), vec.end());
    std::cout << "After reverse: ";
    for (int i : vec) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
}

int main() {
    algorithm_test();
    return 0;
}