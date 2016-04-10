# DI-light

DI-light (dee-light) is a single header dependency injection library for C++11. It supports constructor injection of dependencies managed by std::shared_ptr's. Discovery and instantiation of the dependent objects is as automatic as possible. 

There are 2 usage scenarios:
- Classes get their dependencies in the constructor, and there's a type declaration listing the dependencies. Pros: classes only depend on the Standard C++ library thus freely reusable without the DI library. Cons: bit more boilerplate. (Example1 below.)

- Classes only depend on the di::Context class, and dependencies get injected inside the constructor. Pros: minimal code. Cons: classes depend on DI library. (Example2 below.)

The 2 methods can be freely mixed.

Example1: (classes don't depend on the library)
```cpp
#include <iostream>

struct C { };

struct B {
    B( std::shared_ptr<C> c) : c(c) {}
    std::shared_ptr<C> c;
    
    using dependencies = std::tuple<C>; //list the dependencies for the library
};

struct A {
    A( std::shared_ptr<B> b, std::shared_ptr<C> c) : b(b), c(c) {}
    std::shared_ptr<B> b;
    std::shared_ptr<C> c;

    void hello() { std::cout << "Hello Example!\n"; }

    using dependencies = std::tuple<B, C>; //list the dependencies for the library
};

#include "di.h"  //no dependency on the library up to this point!

int main(void) {
    di::Context::create<A>()->hello();
}
```

Example2 (classes depend on di::Context)
```cpp
#include <iostream>
#include "di.h"

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

int main(void) {
    di::Context::create<A2>()->hello();
}
```
For more examples, check the unit tests! (test.cpp)


#### Desription
The library contains 2 classes: di::Context and di::ContextReg, the later is only a convenience wrapper for the first. Both classes are instantiated with the create() static method which receives a single template parameter describing what to create and return a std::shared_ptr with that class. Dependencies a discovered with either the 'dependencies' typedef or with the inject() method (see above 2 examples), and instantiated/injected on demand automatically. The mechanism is recursive and copes with transitive dependencies in any depth. If no classes depend on the Context itself, the Context gets destructed at the end of the create() call, and the lifetimes of the dependent classes are managed by the std::shared_ptr's.


#### Scope
Classes by default have singleton scope, that is: there's only a single instance present in the Context. If class B and C depend on class A, both will be injected of the same A instance. It is possible to disable the singleton scope for a class with a using singleton = std::false_type type alias. In this case a new instance of the class will be created every time threre's a need for it.


#### Polymorphism support
Polymorphism makes things somewhat complicated. When a class depends on an abstract interface the Context won't be able to automatically satisfy this dependency since the interface is not direclty instantiable, and there's no way enumerate dierived classes in C++ (which could even be ambiguous there's more than one implementer). The user can explicitly register the derived class with the ContextReg<> class template. If the library encounters a dependency on the interface and there's a derived class already in the Context then it will be used to satisfy the dependency. ContextReg<> is variadic class template, allows the registration of multiple classes, in case of ambiguity the last one will be selected. Even with this manual registration the library need some way to discover base-derived relationship. There're 2 ways to indicate this:
- if the compiler supports the TR2 type traits, then Context can automatically discover all base classes of the implementation class
- if there's no TR2 the user must explicitly state base-derived relationship with a typedef / type alias in the derived class (using base = SomeClass; see unit test for examples!)

Note: TR2 type-traits proposal was rejected by the C++ standards comittee. It is supported by some compilers, but might be removed in the future.

#### Under the hood
Inside the Context there's a type map, which hold the following information for each type:
- factory / deleted function
- type-erased std::weak_ptr (if a singleton instance was created)
- base/derived descriptor, and some other flags


#### FAQ
No questions yet...

##### Thanks
This lib was originally inspired by the post on [gpfault.net](http://gpfault.net/posts/dependency-injection-cpp.txt.html).

