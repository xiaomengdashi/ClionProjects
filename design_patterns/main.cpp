//
// Created by Kolane Steve on 25-2-21.
//


#include <iostream>

#include "strategy.hpp"
#include "singleton.hpp"
#include "factory_pattern.hpp"
#include "prototype.hpp"
#include "test.hpp"

using namespace std;

int main()
{
    singleton_test();
    strategy_test();
    factory_test();
    prototype_test();

    return 0;
}


