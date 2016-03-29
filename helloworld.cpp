#include <iostream>
#include "di.h"

//Three classes with dependencies: A->B->C
class C {
public:
    void run() { std::cout << "World!\n"; }
    static auto prototypeFactory() { return std::make_shared<C>(); }
    ~C() { std::cout << "C destructor\n"; }
};

class B {
public:
    B(std::shared_ptr<C> c) : c(c) {}
    std::shared_ptr<C> c;
    void run() { std::cout << "DI "; c->run(); }
    static auto singletonFactory(std::shared_ptr<C> c, std::shared_ptr<di::Context>) { return std::make_shared<B>(c); }
    ~B() { std::cout << "B destructor\n"; }
};

/*
class A {
public:
    B& b;
    void run() { std::cout << "Hello "; b.run(); }
    static auto factory(B& b) { return new A{b}; }
};
*/
int main(void)
{
    std::cout << "Sizeof: " << sizeof( std::shared_ptr<void> ) <<std::endl;

#if 0
    {
        di::Context ctx;
        ctx.get<B>()->run(); //prints: Hello DI World!
        std::cout << "end of scope!\n";
    }
    std::cout << "out of scope\n";
#endif

#if 0
    auto b = di::Context::get_s<B>();
    std::cout << "auto_b created\n";
    b->run();
#endif


    auto ctx = di::Context::get_s<di::Context>();
    auto c = ctx->get_m<C>();
    c->run();

#if 0
    auto b = B::factory(C::factory());
    b->run();
#endif
}

