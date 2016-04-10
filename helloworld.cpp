#include <iostream>
#include "di.h"

/*************************************************************************/
/* Set1: A depends on B&C, B depends on C (no dependency on the library) */
/*************************************************************************/
struct C { };

struct B {
    B( std::shared_ptr<C> c) : c(c) {}

    std::shared_ptr<C> c;

    using dependencies = std::tuple<C>;
};

struct A {
    A( std::shared_ptr<B> b, std::shared_ptr<C> c) : b(b), c(c) {}

    std::shared_ptr<B> b;
    std::shared_ptr<C> c;

    void hello() { std::cout << "Hello Set1!\n"; }

    using dependencies = std::tuple<B, C>;
};


/************************************************************************/
/* Set2: A depends on B&C, B depends on C (+ dependency on di::Context) */
/************************************************************************/
struct C2 { };

struct B2 {
    B2( std::shared_ptr<di::Context> ctx) { ctx->inject(c); }

    std::shared_ptr<C2> c;
};

struct A2 {
    A2( std::shared_ptr<di::Context> ctx) { ctx->inject(b,c); }

    std::shared_ptr<B2> b;
    std::shared_ptr<C2> c;

    void hello() { std::cout << "Hello Set2!\n"; }
};



int main(void)
{
    // Instantiation without di-light (Set1 only):
    auto c = std::make_shared<C>();
    auto b = std::make_shared<B>(c);
    auto a = std::make_shared<A>(b, c);
    a->hello();

    // Set1 with di-light:
    di::Context::create<A>()->hello();

    // Set2 with di-light:
    di::Context::create<A2>()->hello();
}

