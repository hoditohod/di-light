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

struct T4_A2 {
    T4_A2(shared_ptr<di::Context> ctx) { ctx->inject(b); }

    shared_ptr<T4_B> b;
    std::string run() { return "A" + b->run(); }
};




/*************************************/
/* Test set 5: polymorphic hierarchy */
/*************************************/
struct T5 {
    virtual ~T5() { destructionMark += "T5"; }
};

struct T5_d : public T5{
#ifndef HAS_TR2
    using base = T5;
#endif
    virtual ~T5_d() { destructionMark += "T5_d"; }
};

struct T5_dd : public T5_d {
#ifndef HAS_TR2
    using base = T5_d;
#endif
    virtual ~T5_dd() { destructionMark += "T5_dd"; }
};



/***********************************/
/* Test set 6: abstract base class */
/***********************************/

struct T6 {
    virtual std::string run() = 0;
};


struct T6_d : public T6{
#ifndef HAS_TR2
    using base = T6;
#endif
    std::string run() override { return "A"; }
};


/**********************************/
/* Test set 7: cyclic dependency */
/**********************************/
struct T7_B;

struct T7_A {
    T7_A(shared_ptr<T7_B> b) : b(b) {}
    shared_ptr<T7_B> b;
    using dependencies = std::tuple<T7_B>;
};

struct T7_B {
    T7_B(shared_ptr<T7_A> a) : a(a) {}
    shared_ptr<T7_A> a;
    using dependencies = std::tuple<T7_A>;
};



/**********************************/
/* Test set 8: scope declarations */
/**********************************/

struct T8_A {
    // implicitly singleton
};

struct T8_B {
    using singleton = std::true_type;
};

struct T8_B_d : public T8_B { // derived class with can override scope
    using singleton = std::false_type;
};

struct T8_C {
    using singleton = std::false_type;
};

struct T8_C_d : public T8_C {   // derived class inherits base scope
};


/*********************************************/
/* Test set 9: unneccesary deps declarations */
/*********************************************/

struct T9_A{
    using dependencies = std::tuple<>;
};

struct T9_B{
    T9_B(shared_ptr<di::Context>) {}

    using dependencies = std::tuple<di::Context>;
};


/*****************************************/
/* Test set 10: depending on di::Context */
/*****************************************/

struct T10 {
    T10( shared_ptr<di::Context> ctx) : ctx(ctx) {}
    ~T10() { destructionMark += "T10"; }

    shared_ptr<di::Context> ctx;
};





/*************************************************/
/* Test set Z: classes that cause static asserts */
/*************************************************/

struct Z1 {
    using singleton = int; //int is not a valid type for singleton
};

struct Z2 {
    Z2(int) {} //no known construction, can't be registered
};

//auto z1 = di::Context::create<Z1>();
//auto z2 = di::ContextReg<Z2>::create<Z2>();



// with 'using dependencies'
int test_transitive1()
{
    destructionMark.clear();
    TINYTEST_STR_EQUAL( "ABDC", di::Context::create<T1_A>()->run().c_str() );
    TINYTEST_STR_EQUAL( "ACBD", destructionMark.c_str() );
    return 1;
}

// with 'inject()'
int test_transitive1i()
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
int test_constDep1i()
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

// polymorphic mock class - mock can be injected
int test_poly2()
{
    TINYTEST_STR_EQUAL( "ABmock", di::ContextReg<T4_B_mock>::create<T4_A>()->run().c_str() );
    return 1;
}

// polymorphic mock class - mock can be injected (using inject)
int test_poly2i()
{
    TINYTEST_STR_EQUAL( "ABmock", di::ContextReg<T4_B_mock>::create<T4_A2>()->run().c_str() );
    return 1;
}



// polymorphic classes - both base and derived reference type can be requested, both are the same instance (singleton scope)
int test_poly3()
{
    auto ctx = di::ContextReg<T5_dd>::create<di::Context>();  //all base types are in context, but only 1 derived instance
    auto a = ctx->get<T5>();
    auto b = ctx->get<T5_d>();
    auto c = ctx->get<T5_dd>();
    TINYTEST_ASSERT( ((a.get() == b.get()) && (a.get() == c.get())) );
    return 1;
}


// polymorphic classes - destructors are correctly called
int test_poly4()
{
    destructionMark.clear();
    {
        di::ContextReg<T5_dd>::create<T5>(); // all base types are in context, but only 1 derived instance
    }
    TINYTEST_STR_EQUAL( "T5_ddT5_dT5", destructionMark.c_str() );   // derived and base destructors called
    return 1;
}


// polymorphic classes - abstract base
int test_poly5()
{
    TINYTEST_STR_EQUAL( "A", di::ContextReg<T6_d>::create<T6>()->run().c_str() );
    return 1;
}


// cyclic dependencies are detected runtime
int test_cyclic1()
{
    try {
        di::Context::create<T7_A>();
    } catch (std::runtime_error& e) {
        //std::cout << e.what() << std::endl;
        return 1;
    }
    TINYTEST_ASSERT( false );
}


// classes without singleton declaration are implicitly singleton
int test_scope1()
{
    auto ctx = di::Context::create<di::Context>();
    auto a = ctx->get<T8_A>();
    auto b = ctx->get<T8_A>();
    TINYTEST_ASSERT( a.get() == b.get() );
    return 1;
}

// classes can be explicitly declared singleton
int test_scope2()
{
    auto ctx = di::Context::create<di::Context>();
    auto a = ctx->get<T8_B>();
    auto b = ctx->get<T8_B>();
    TINYTEST_ASSERT( a.get() == b.get() );
    return 1;
}

// classes can be explicitly declared prototype
int test_scope3()
{
    auto ctx = di::Context::create<di::Context>();
    auto a = ctx->get<T8_C>();
    auto b = ctx->get<T8_C>();
    TINYTEST_ASSERT( a.get() != b.get() );
    return 1;
}

// derived class inherits base scope if it has no scope declaration (does not fall back to default)
int test_scope4()
{
    auto ctx = di::Context::create<di::Context>();
    auto a = ctx->get<T8_C_d>();
    auto b = ctx->get<T8_C_d>();
    TINYTEST_ASSERT( a.get() != b.get() );
    return 1;
}

// derived class can override bas scope
int test_scope5()
{
    auto ctx = di::Context::create<di::Context>();
    auto a = ctx->get<T8_B_d>();
    auto b = ctx->get<T8_B_d>();
    TINYTEST_ASSERT( a.get() != b.get() );
    return 1;
}

// unnecessary deps declaration doesn't cause build error
int test_unnecessary1()
{
    di::Context::create<T9_A>();
    di::Context::create<T9_B>();
    return 1;
}

// depending on the Context doesn't end up as a circular dependency, and objects are destructed correctly
int test_dependOnContext()
{
    destructionMark.clear();
    di::Context::create<T10>();
    TINYTEST_STR_EQUAL( "T10", destructionMark.c_str() );
    return 1;
}



TINYTEST_START_SUITE(DI_light);
    TINYTEST_ADD_TEST(test_transitive1);
    TINYTEST_ADD_TEST(test_transitive1i);

    TINYTEST_ADD_TEST(test_constDep1);
    TINYTEST_ADD_TEST(test_constDep1i);

    TINYTEST_ADD_TEST(test_poly1);
    TINYTEST_ADD_TEST(test_poly2);
    TINYTEST_ADD_TEST(test_poly2i);
    TINYTEST_ADD_TEST(test_poly3);
    TINYTEST_ADD_TEST(test_poly4);
    TINYTEST_ADD_TEST(test_poly5);

    TINYTEST_ADD_TEST(test_cyclic1);

    TINYTEST_ADD_TEST(test_scope1);
    TINYTEST_ADD_TEST(test_scope2);
    TINYTEST_ADD_TEST(test_scope3);
    TINYTEST_ADD_TEST(test_scope4);
    TINYTEST_ADD_TEST(test_scope5);

    TINYTEST_ADD_TEST(test_unnecessary1);

    TINYTEST_ADD_TEST(test_dependOnContext);


TINYTEST_END_SUITE();


TINYTEST_MAIN_SINGLE_SUITE(DI_light);

