#include <vector>
#include <string>
#include <iostream>

using namespace std;

void vector_test()
{
    cout << "=======vector_test========" << endl;
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
    
    cout << "Vector operations completed successfully" << endl;
    cout << "v8 size: " << v8.size() << endl;
    
    v8.clear();
    cout << "v8 size after clear: " << v8.size() << endl;
}

int main() {
    vector_test();
    return 0;
}