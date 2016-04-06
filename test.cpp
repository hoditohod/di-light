#include <iostream>
#include "tinytest/tinytest.h"

//#define HAS_TR2
#include "di.h"

using std::shared_ptr;

std::string destructionMark;

/*************************************************************************/
/* Test set 1: transitive dependencies: A depends on B&C, B depends on D */
/*************************************************************************/
struct T1_D {
    std::string run() { return "D"; }
    ~T1_D() { destructionMark += "D";  }
};

struct T1_C {
    std::string run() { return "C"; }
    ~T1_C() { destructionMark += "C";  }
};

struct T1_B {
    T1_B( shared_ptr<T1_D> d) : d(d) {}
    ~T1_B() { destructionMark += "B"; }

    shared_ptr<T1_D> d;

    std::string run() { return "B" + d->run(); }

    using dependencies = std::tuple<T1_D>;
};

struct T1_A {
    T1_A( shared_ptr<T1_B> b, shared_ptr<T1_C> c) : b(b), c(c) {}
    ~T1_A() { destructionMark += "A"; }

    shared_ptr<T1_B> b;
    shared_ptr<T1_C> c;

    std::string run() { return "A" + b->run() + c->run(); }

    using dependencies = std::tuple<T1_B, T1_C>;
 };



/**************************************************************************************/
/* Test set 2: transitive dependencies: A depends on B&C, B depends on D (use inject) */
/**************************************************************************************/
struct T2_D {
    std::string run() { return "D"; }
    ~T2_D() { destructionMark += "D";  }
};

struct T2_C {
    std::string run() { return "C"; }
    ~T2_C() { destructionMark += "C";  }
};

struct T2_B {
    T2_B( shared_ptr<di::Context> ctx) { ctx->inject(d); }
    ~T2_B() { destructionMark += "B"; }

    shared_ptr<T2_D> d;

    std::string run() { return "B" + d->run(); }
};

struct T2_A {
    T2_A( shared_ptr<di::Context> ctx) { ctx->inject(b,c); }
    ~T2_A() { destructionMark += "A"; }

    shared_ptr<T2_B> b;
    shared_ptr<T2_C> c;

    std::string run() { return "A" + b->run() + c->run(); }
};



/********************************/
/* Test set 3: const dependency */
/********************************/
struct T3_B {
    std::string run() const { return "B"; }
};

struct T3_A {
    T3_A(shared_ptr<const T3_B> b) : b(b) {}

    shared_ptr<const T3_B> b;
    std::string run() { return "A" + b->run(); }

    using dependencies = std::tuple<T3_B>;
};

struct T3_A2 {
    T3_A2( shared_ptr<di::Context> ctx) { ctx->inject(b); }

    shared_ptr<const T3_B> b;
    std::string run() { return "A" + b->run(); }
};




/**************************************/
/* Test set 4: polymorphic mock class */
/**************************************/
struct T4_B {
    virtual ~T4_B() = default;
    virtual std::string run() { return "B"; }
};

struct T4_B_mock : public T4_B{
#ifndef HAS_TR2
    typedef T4_B base;
#endif
    std::string run() override { return "Bmock"; }
};

struct T4_A {
    T4_A( shared_ptr<T4_B> b) : b(b) {}

    shared_ptr<T4_B> b;

    std::string run() { return "A" + b->run(); }

    using dependencies = std::tuple<T4_B>;
};

#if 0


/*************************************/
/* Test set 5: polymorphic hierarchy */
/*************************************/
struct T5 {
    virtual ~T5() { destructionMark += "T5"; }
    static auto factory() { return new T5; }
};

struct T5_d : public T5{
#ifndef HAS_TR2
    typedef T5 base;
#endif
    virtual ~T5_d() { destructionMark += "T5_d"; }
    static auto factory() { return new T5_d; }
};

struct T5_dd : public T5_d {
#ifndef HAS_TR2
    typedef T5_d base;
#endif
    virtual ~T5_dd() { destructionMark += "T5_dd"; }
    static auto factory() { return new T5_dd; }
};

/* abstract base class */

/********************************************/
/* Test set 6: class without factory method */
/********************************************/
struct T6 {
    std::string run() { return "A"; }
};

auto T6_factory() { return new T6; }



/****************************************************************************/
/* Test set 7: class without factory method, but other member named factory */
/****************************************************************************/
struct T7 {
    std::string run() { return "A"; }
    int factory;    // must not cause compile error
};

auto T7_factory() { return new T7; }



/***************************************/
/* Test set 8: factory returns nullptr */
/***************************************/
struct T8 {
    static T8* factory() { return nullptr; }
};



/****************************************************/
/* Test set 9: multiple factories for the same type */
/****************************************************/
struct T9_A {
    static T9_A* factory() { return new T9_A; }
};

struct T9_B {
    static T9_A* factory() { return new T9_A; }
};



/**********************************/
/* Test set 10: cyclic dependency */
/**********************************/
struct T10_B;

struct T10_A {
    T10_B& b;
    static auto factory(T10_B& b) { return new T10_A{b}; }
};

struct T10_B {
    T10_A& a;
    static auto factory(T10_A& a) { return new T10_B{a}; }
};



/*****************************************/
/* Test set 11: class depends on Context */
/*****************************************/
struct T11 {
    di::Context& ctx;
    std::string run() { return "A"; }
    static auto factory(di::Context& ctx) { return new T11{ctx}; }
};
#endif








// with 'using dependencies'
int test_transitive1()
{
    destructionMark.clear();
    TINYTEST_STR_EQUAL( "ABDC", di::Context::create<T1_A>()->run().c_str() );
    TINYTEST_STR_EQUAL( "ACBD", destructionMark.c_str() );
    return 1;
}

// with 'inject()'
int test_transitive2()
{
    destructionMark.clear();
    TINYTEST_STR_EQUAL( "ABDC", di::Context::create<T2_A>()->run().c_str() );
    TINYTEST_STR_EQUAL( "ACBD", destructionMark.c_str() );
    return 1;
}

// const ref dependencies are supported
int test_constDep1()
{
    TINYTEST_STR_EQUAL( "AB", di::Context::create<T3_A>()->run().c_str() );
    return 1;
}

// const ref dependencies are supported, with inject()
int test_constDep2()
{
    TINYTEST_STR_EQUAL( "AB", di::Context::create<T3_A2>()->run().c_str() );
    return 1;
}



// polymorphic mock class hierarchy - nonmock picked up by default
int test_poly1()
{
    TINYTEST_STR_EQUAL( "AB", di::Context::create<T4_A>()->run().c_str() );
    return 1;
}

#if 0

// polymorphic mock class - mock can be injected
int test_poly2()
{
    di::ContextTmpl<T4_B_mock> ctx; //prefer derived mock over base
    TINYTEST_STR_EQUAL( "ABmock", ctx.get<T4_A>().run().c_str() );
    return 1;
}


// polymorphic classes - both base and derived reference type can be requested, both are the same instance
int test_poly3()
{
    di::ContextTmpl<T5_dd> ctx;   //all base types are in context, but only 1 derived instance
    T5&    a = ctx.get<T5>();
    T5_d&  b = ctx.get<T5_d>();
    T5_dd& c = ctx.get<T5_dd>();
    TINYTEST_ASSERT( ((&a == &b) && (&a == &c)) );
    return 1;
}


// polymorphic classes - destructors are correctly called
int test_poly4()
{
    destructionMark.clear();
    {
        di::ContextTmpl<T5_dd> ctx;   // all base types are in context, but only 1 derived instance
        ctx.get<T5>();                // returns derived instance
    }
    TINYTEST_STR_EQUAL( "T5_ddT5_dT5", destructionMark.c_str() );   // derived and base destructors called
    return 1;
}


// class without factory method (3rd party) - compiles, but throws when used (no factory registered)
int test_factory1()
{
    di::Context ctx;
    try {
        ctx.get<T6>();
    } catch (std::runtime_error& e) {
        //std::cout << e.what() << std::endl;
        return 1;
    }
    TINYTEST_ASSERT( false );
}


// class without factory method (3rd party) - self standing factory method can be registered
int test_factory2()
{
    di::ContextTmpl<> ctx(T6_factory);
    TINYTEST_STR_EQUAL( "A", ctx.get<T6>().run().c_str() );
    return 1;
}


// class without factory method (3rd party), but other member named factory compiles and ignored
int test_factory3()
{
    di::ContextTmpl<> ctx(T7_factory);
    TINYTEST_STR_EQUAL( "A", ctx.get<T7>().run().c_str() );
    return 1;
}


// multiple factories with the same return type are not allowed
int test_factory4()
{
    try {
        di::ContextTmpl<T9_A, T9_B> ctx;
    } catch (std::runtime_error& e) {
        //std::cout << e.what() << std::endl;
        return 1;
    }
    TINYTEST_ASSERT( false );
}


// nullptr instance can't be added to the context
int test_instance1()
{
    di::Context ctx;
    try {
        ctx.get<T8>();
    } catch (std::runtime_error& e) {
        //std::cout << e.what() << std::endl;
        return 1;
    }
    TINYTEST_ASSERT( false );
}


// cyclic dependencies are detected runtime
int test_cyclic()
{
    di::Context ctx;
    try {
        ctx.get<T10_A>();
    } catch (std::runtime_error& e) {
        //std::cout << e.what() << std::endl;
        return 1;
    }
    TINYTEST_ASSERT( false );
}


// a class may depend on the Context itself
int test_dependOnContext()
{
    di::Context ctx;
    TINYTEST_STR_EQUAL( "A", ctx.get<T11>().run().c_str() );
    return 1;
}

#endif



TINYTEST_START_SUITE(DI_light);
    TINYTEST_ADD_TEST(test_transitive1);
    TINYTEST_ADD_TEST(test_transitive2);

    TINYTEST_ADD_TEST(test_constDep1);
    TINYTEST_ADD_TEST(test_constDep2);

    TINYTEST_ADD_TEST(test_poly1);
//    TINYTEST_ADD_TEST(test_poly2);
//    TINYTEST_ADD_TEST(test_poly3);
//    TINYTEST_ADD_TEST(test_poly4);
//    TINYTEST_ADD_TEST(test_factory1);
//    TINYTEST_ADD_TEST(test_factory2);
//    TINYTEST_ADD_TEST(test_factory3);
//    TINYTEST_ADD_TEST(test_factory4);
//    TINYTEST_ADD_TEST(test_instance1);
//    TINYTEST_ADD_TEST(test_cyclic);
//    TINYTEST_ADD_TEST(test_dependOnContext);
TINYTEST_END_SUITE();


TINYTEST_MAIN_SINGLE_SUITE(DI_light);

