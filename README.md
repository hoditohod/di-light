# DI-light

DI-light (dee-light) is a single header dependency injection library for C++11.

Example:
```cpp
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

```
For more examples, check the unit tests! (test.cpp)

### Overview
DI-light supports constructor injection of reference or const reference dependencies. It perfomes lazy instantiation and lifetime management of objects, and tries to be as automatic and non-intrusive as possible.

#### Usage
Classes must be equipped with a static factory function that returns a new instance of the class and lists dependencies as arguments (ref or const ref). There's minimal dependeny on the library itself, the di.h header is only needed to be included at single place where classes are wired together. 

DI is performed with the help of a Context: it holds all information of the classes in the process. Classes with factories are automatically registered and instantiated by the Context (on demand) when they are requested by the user by or an other dependent class.

Instance pointers are stored in the Context and objects are destructed in correct order when the Context is destroyed.

#### Under the hood
Inside the Context there's a type map, which holds among others the factory function and instance pointer of the class. When a ctx.get&lt;SomeClass&gt;() is called, SomeClass is looked up in the typemap. If it's present and has the instance already created, then it's simply returned. If the map entry is present, but there's no instance, then the corresponding factory is invoked to create an instance recursively calling ctx.get() on all dependencies. If a type is not found in the typemap Context looks for a SomeClass::factory and registers it automatically if present.


#### Polymorphism support
Polymorphism makes things somewhat complicated. When a class depends on an abstract interface the Context won't be able to automatically satisfy this dependency since the interface doesn't have a factory. The user has to explicitly register an implementation (derived) class in the Context and indicate somehow that this derived class is to be used when the interface is required. There are 2 ways for this indication:
- if the compiler supports the TR2 type traits, then Context can automatically discover all base classes of the implementation class
- if there's no TR2 the user must explicitly state base-derived relationship with a typedef in the derived class (see unit test for examples!)

Note: TR2 type-traits proposal was rejected by the C++ standards comittee. It is supported by some compilers, but might be removed in the future.

##### Thanks
This lib was inspired by the post on [gpfault.net](http://gpfault.net/posts/dependency-injection-cpp.txt.html).

