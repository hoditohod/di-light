#include <iostream>
#include "di.h"

//Three classes with dependencies: A->B->C
class C {
public:
    void run() { std::cout << "World!\n"; }
    static auto factory() { return new C; }
};

class B {
public:
    C& c;
    void run() { std::cout << "DI "; c.run(); }
    static auto factory(C& c) { return new B{c}; }
};

class A {
public:
    B& b;
    void run() { std::cout << "Hello "; b.run(); }
    static auto factory(B& b) { return new A{b}; }
};

int main(void)
{
    di::Context ctx;
    ctx.get<A>().run(); //prints: Hello DI World!
}
