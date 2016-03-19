#include <iostream>
#include "tinytest/tinytest.h"

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

int test_transitive1() // transitive dependencies
{
    di::Context ctx;
    TINYTEST_STR_EQUAL( "ABC", ctx.get<T1_A>().run().c_str() );
    return 1;
}

int test_constRef() // const ref dependency
{
    di::Context ctx;
    TINYTEST_STR_EQUAL( "AB", ctx.get<T3_A>().run().c_str() );
    return 1;
}

int test_poly1() // polymorphic mock class hierarchy - nonmock picked up by default
{
    di::Context ctx;
    TINYTEST_STR_EQUAL( "AB", ctx.get<T4_A>().run().c_str() );
    return 1;
}

int test_poly2() // polymorphic mock class - mock can be injected
{
    di::ContextTmpl<T4_B_mock> ctx; //prefer derived mock over base
    TINYTEST_STR_EQUAL( "ABmock", ctx.get<T4_A>().run().c_str() );
    return 1;
}

int test_poly3() // polymorphic classes - both base and derived reference type can be requested, both are the same instance
{
    di::ContextTmpl<T5_dd> ctx;   //all base types are in context, but only 1 derived instance
    T5&    a = ctx.get<T5>();
    T5_d&  b = ctx.get<T5_d>();
    T5_dd& c = ctx.get<T5_dd>();
    TINYTEST_ASSERT( ((&a == &b) && (&a == &c)) );
    return 1;
}

int test_factory1() // class without factory method (3rd party) - compiles, but throws when used (no factory registered)
{
    di::Context ctx;
    try {
        ctx.get<T6>();
    } catch (std::runtime_error& e) {
        return 1;
    }
    TINYTEST_ASSERT( false );
}




TINYTEST_START_SUITE(DI_light);
  TINYTEST_ADD_TEST(test_transitive1);
  TINYTEST_ADD_TEST(test_constRef);
  TINYTEST_ADD_TEST(test_poly1);
  TINYTEST_ADD_TEST(test_poly2);
  TINYTEST_ADD_TEST(test_poly3);
  TINYTEST_ADD_TEST(test_factory1);
TINYTEST_END_SUITE();



TINYTEST_MAIN_SINGLE_SUITE(DI_light);

