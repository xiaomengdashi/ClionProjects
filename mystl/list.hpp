#pragma once


#include <list>
#include <vector>
#include <set>


inline void list_test()
{
  std::list<int> list;
  std::list<int>::iterator it;
}

inline void vector_test()
{
  std::vector<int> vector;
  std::vector<int>::iterator it;
  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3);
  vector.push_back(4);
  vector.push_back(5);
  for (it = vector.begin(); it != vector.end(); ++it)
  {
    std::cout << *it << std::endl;
  }
}


