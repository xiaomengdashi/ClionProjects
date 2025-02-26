#include <vector>
#include <string>

using namespace std;

void vector_test()
{
    vector<int> v1;
    vector<int> v2(v1);
    vector<int> v3(10, 42);
    vector<int> v4{10};
    vector<int> v5(10);
    vector<string> v8(3, "hi");

    v8.emplace_back("hello");
    v8.insert(v8.begin(), "hello");
    v8.pop_back();
    v8.erase(v8.begin() + 1);
    v8.clear();
}