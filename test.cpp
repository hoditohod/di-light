#include <iostream>

//#define HAS_TR2
#include "di.h"


/********************************************************************/
/* Test set 1: transitive dependencies: A depends on B depends on C */
/********************************************************************/
struct T1_C {
    std::string run() { return "C"; }
    static auto factory() { return new T1_C; }
};

struct T1_B {
    T1_C& c;
    std::string run() { return "B" + c.run(); }
    static auto factory(T1_C& c) { return new T1_B{c}; }
};

struct T1_A {
    T1_B& b;
    std::string run() { return "A" + b.run(); }
    static auto factory(T1_B& b) { return new T1_A{b}; }
};



/************************************************************/
/* Test set 2: transitive dependencies (reverse name order) */
/************************************************************/
struct T2_A {
    std::string run() { return "A"; }
    static auto factory() { return new T2_A; }
};

struct T2_B {
    T2_A& a;
    std::string run() { return "B" + a.run(); }
    static auto factory(T2_A& a) { return new T2_B{a}; }
};

struct T2_C {
    T2_B& b;
    std::string run() { return "C" + b.run(); }
    static auto factory(T2_B& b) { return new T2_C{b}; }
};



/************************************/
/* Test set 3: const ref dependency */
/************************************/
struct T3_B {
    std::string run() const { return "B"; }
    static auto factory() { return new T3_B; }
};

struct T3_A {
    const T3_B& b;
    std::string run() { return "A" + b.run(); }
    static auto factory(const T3_B& b) { return new T3_A{b}; }
};



/**************************************/
/* Test set 4: polymorphic mock class */
/**************************************/
struct T4_B {
    virtual ~T4_B() = default;
    virtual std::string run() { return "B"; }
    static auto factory() { return new T4_B; }
};

struct T4_B_mock : public T4_B{
#ifndef HAS_TR2
    typedef T4_B base;
#endif
    std::string run() override { return "Bmock"; }
    static auto factory() { return new T4_B_mock; }
};

struct T4_A {
    T4_B& b;
    std::string run() { return "A" + b.run(); }
    static auto factory(T4_B& b) { return new T4_A{b}; }
};



/*************************************/
/* Test set 5: polymorphic hierarchy */
/*************************************/
struct T5 {
    virtual ~T5() = default;
    static auto factory() { return new T5; }
};

struct T5_d : public T5{
#ifndef HAS_TR2
    typedef T5 base;
#endif
    static auto factory() { return new T5_d; }
};

struct T5_dd : public T5_d {
#ifndef HAS_TR2
    typedef T5_d base;
#endif
    static auto factory() { return new T5_dd; }
};



/********************************************/
/* Test set 6: class without factory method */
/********************************************/
struct T6 {
    std::string run() { return "A"; }
};

auto T6_factory() { return new T6; }



/****************************************************************************/
/* Test set 8: class without factory method, but other member named factory */
/****************************************************************************/
struct T8 {
    std::string run() { return "A"; }
    int factory;    // must not cause compile error
};


/*
 * - class with external factory
 * - class with factory member variable
 * - (factory signature error)
 * - nullptr instance
 * - multiple instance
 * - multiple factory
 * - cyclic dependency
 * - class without factory but instance added
 * - dependency on Context
 * - getNew without factory, but with instance
 */
int main()
{
#if 1
    { /* Test1: transitive dependencies */
        di::Context ctx;
        std::string result = ctx.get<T1_A>().run();
        std::cout << "Test1 ok: " << (result=="ABC") << "; result: " << result << std::endl;
    }

    { /* Test2: transitive dependencies rev order*/
        di::Context ctx;
        std::string result = ctx.get<T2_C>().run();
        std::cout << "Test2 ok: " << (result=="CBA") << "; result: " << result << std::endl;
    }

    { /* Test3: const ref dependency */
        di::Context ctx;
        std::string result = ctx.get<T3_A>().run();
        std::cout << "Test3 ok: " << (result=="AB") << "; result: " << result << std::endl;
    }

    { /* Test4: polymorphic mock class hierarchy - nonmock picked up by default */
        di::Context ctx;
        std::string result = ctx.get<T4_A>().run();
        std::cout << "Test4 ok: " << (result=="AB") << "; result: " << result << std::endl;
    }

    { /* Test5: polymorphic mock class - mock can be injected */
        di::ContextTmpl<T4_B_mock> ctx; //prefer derived mock over base
        std::string result = ctx.get<T4_A>().run();
        std::cout << "Test5 ok: " << (result=="ABmock") << "; result: " << result << std::endl;
    }
#endif
    { /* Test6: polymorphic mock class - both base and derived type can be requested, both are the same instance */
        di::ContextTmpl<T5_dd> ctx;   //all base types are in context, but only 1 derived instance
        T5&    a = ctx.get<T5>();
        T5_d&  b = ctx.get<T5_d>();
        T5_dd& c = ctx.get<T5_dd>();
        std::cout << "Test6 ok: " << ((&a == &b) && (&a == &c)) << "; result: " << &a << " " << &b << " " << &c << std::endl;
    }

    { /* Test7: class without factory method (3rd party) - compiles, but throws when used (no factory registered) */
        di::Context ctx;
        std::string result;
        try {
            ctx.get<T6>();
        } catch (std::runtime_error& e) {
            result = e.what();
        }
        std::cout << "Test7 ok: " << (result!="") << "; result: " << result << std::endl;
    }


    return 0;
}

