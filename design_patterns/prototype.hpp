#pragma once

#include <iostream>
#include <memory>


using namespace std;


class Prototype
{
public:
    virtual ~Prototype() = default;
    virtual std::unique_ptr<Prototype> clone() const = 0;
    virtual void display() const = 0;
};

class ConcretePrototypeA : public Prototype
{
public:
    std::unique_ptr<Prototype> clone() const override
    {
        return std::make_unique<ConcretePrototypeA>(*this);
    }

    void display() const override
    {
        cout << "ConcretePrototypeA" << endl;
    }

    ~ConcretePrototypeA()
    {
        std::cout << "del ConcretePrototypeA" << std::endl;
    }
};

class ConcretePrototypeB : public Prototype
{
public:
    std::unique_ptr<Prototype> clone() const override
    {
        return std::make_unique<ConcretePrototypeB>(*this);
    }

    void display() const override
    {
        cout << "ConcretePrototypeB" << endl;
    }
    ~ConcretePrototypeB()
    {
        std::cout << "del ConcretePrototypeB" << std::endl;
    }
};

void prototype_test()
{
    cout << "=========prototype test==========" << endl;
    Prototype* prototypeA = new ConcretePrototypeA();
    Prototype* prototypeB = new ConcretePrototypeB();

    std::unique_ptr<Prototype> clonedPrototypeA = prototypeA->clone();
    std::unique_ptr<Prototype> clonedPrototypeB = prototypeB->clone();

    prototypeA->display();
    prototypeB->display();

    clonedPrototypeA->display();
    clonedPrototypeB->display();

    delete prototypeA;
    delete prototypeB;

    cout << "\n\n" << endl;
}
