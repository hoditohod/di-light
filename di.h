/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gyorgy Szekely
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DI_H
#define DI_H

#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#ifdef HAS_TR2
#include <tr2/type_traits>
#endif

#define INPLACE


namespace di {

class Context : public std::enable_shared_from_this<Context>
{
    // A single item in the context
    struct CtxItem
    {
        using shared_ptr_t = std::shared_ptr<char>;

#ifdef INPLACE
        std::aligned_storage<sizeof(shared_ptr_t), alignof(shared_ptr_t)>::type storage;
#else
        void* ptr = nullptr;
#endif

        bool marker = false;                                            // flag used to detect circular dependencies
        bool singleton = false;
        std::function<void(void)> factory;                              // factory fn. to create a new object instance
        void (*deleter)(void*) = nullptr;                               // delete fn. (calls proper destructor)
        std::type_index derivedType = std::type_index(typeid(void));    // a derived type (eg. implementation of an interface)

        void dump() const
        {
            std::cout << std::boolalpha << "mark: " << marker << ", factory: " << (bool)factory << ", del: " << deleter << ", desc: " << derivedType.name() << std::endl;
        }

        // non-copyable, non-moveable
        CtxItem() = default;
        CtxItem(const CtxItem& rhs) = delete;
        CtxItem& operator=(const CtxItem& rhs) = delete;
        CtxItem(CtxItem&& rhs) = delete;
        CtxItem& operator=(CtxItem&& rhs) = delete;

        ~CtxItem()
        {
            std::cout << " ctxitem destructor!\n";
            if (deleter != nullptr)
#ifdef INPLACE
                deleter(&storage);
#else
                deleter(ptr);
#endif
        }
    };



    // Factory signature
    template <class InstanceType, class... Args>
    using FactoryFunction = std::shared_ptr<InstanceType>(*)(std::shared_ptr<Args>...);



    // The object storage
    std::map<std::type_index, CtxItem> items;
    std::weak_ptr<Context> self;



#ifdef HAS_TR2
    // Recursively iterate over all bases
    template <typename T, typename std::enable_if< !T::empty::value >::type* = nullptr >
    void declareBaseTypes(std::type_index& derivedType)
    {
        items[ std::type_index(typeid( typename T::first::type )) ].derivedType = derivedType;
        declareBaseTypes<typename T::rest::type>( derivedType );
    }

    template <typename T, typename std::enable_if< T::empty::value >::type* = nullptr >
    void declareBaseTypes(std::type_index&) { }
#else
    template<typename T, typename T::base* = nullptr>
    void declareBaseTypes(std::type_index& derivedType)
    {
        items[ std::type_index(typeid( typename T::base )) ].derivedType = derivedType;
        declareBaseTypes<typename T::base>( derivedType );
    }

    template <typename T>
    void declareBaseTypes(...) { }
#endif



    // Add factory method automatically if present in class (singleton)
    template <typename T, typename std::enable_if< std::is_function<decltype(T::singletonFactory)>::value >::type* = nullptr>
    void addClassAuto(void*) // argument only used to disambiguate from vararg version
    {
        addFactoryPriv(T::singletonFactory, true);
    }

    // Add factory method automatically if present in class (prototype)
    template <typename T, typename std::enable_if< std::is_function<decltype(T::prototypeFactory)>::value >::type* = nullptr>
    void addClassAuto(void*)
    {
        addFactoryPriv(T::prototypeFactory, false);
    }

    template<typename T>
    void addClassAuto(...)
    {
        // Depending on a class that does not provide *factory is not an error at compile time,
        // but you must make sure to register a factory before actually wanting an instance of that class.
        throw std::runtime_error(std::string("Class '") + typeid(T).name() + "' has no factory in context!");
    }



    // Add a factory function to context
    template <class InstanceType, class... Args>
    void addFactoryPriv(FactoryFunction<InstanceType, Args...> factoryFunction, bool singleton)
    {
        auto instanceTypeIdx = std::type_index(typeid(InstanceType));

#ifdef HAS_TR2
        declareBaseTypes< typename std::tr2::bases<InstanceType>::type >( instanceTypeIdx );
#else
        declareBaseTypes<InstanceType>( instanceTypeIdx );
#endif

        CtxItem& item = items[ instanceTypeIdx ];

        if (item.factory)
            throw std::runtime_error(std::string("Factory already registed for type: ") + typeid(InstanceType).name());

        item.factory = [factoryFunction, this](){ addInstance(factoryFunction( get_m<Args>()... )); };
        item.singleton = singleton;
    }

    template <typename T>
    void addFactoryPriv(T, bool)
    {
        // Use a dummy is_void type trait to force GCC to display instantiation type in error message
        static_assert( std::is_void<T>::value, "Factory has incorrect signature! Should be: std::shared_ptr<Foo> Foo::[singleton/prototype]Factory(std::shared_ptr<Bar> bar, ...); ");
    }



    // Add an already instantiated object to the context (T is the return type of the *Factory)
    template <typename T>
    void addInstance(std::shared_ptr<T> sharedInstance)
    {
        if (!sharedInstance)
            throw std::runtime_error(std::string("Trying to add empty shared_ptr for type: ") + typeid(T).name());

        CtxItem& item = items[ std::type_index(typeid(T)) ];

        if (item.deleter != nullptr)
            throw std::runtime_error(std::string("Instance already in Context for type: ") + typeid(T).name());

#ifdef INPLACE
        // placement new, explicit destruction
        new(&item.storage) std::shared_ptr<T>(sharedInstance);
        item.deleter = [](void* ptr) {
            std::cout << "deleter " << typeid(T).name() << std::endl;
            static_cast< std::shared_ptr<T>* >(ptr) -> ~shared_ptr();
        };
#else
        item.ptr = new std::shared_ptr<T>(std::forward(sharedInstance);
        item.deleter = [](void* ptr) {
            std::cout << "deleter " << typeid(T).name() << std::endl;
            delete static_cast< std::shared_ptr<T>* >(ptr);
        };
#endif
    }

    Context()
    {
        std::cout << "context constructor\n";
    }

public:

    ~Context()
    {
        std::cout << "context destructor\n";
    }

    void dump(const std::string& msg)
    {
        std::cout << msg << std::endl;
        for (auto it = items.cbegin(); it != items.cend(); it++)
        {
            std::cout << it->first.name() << " - ";
            it->second.dump();
        }
    }

    // Get an instance from the context, runs factories recursively to satisfy all dependencies
    template <class T>
    std::shared_ptr<T> get_m()
    {
        auto it = items.find( std::type_index(typeid(T)) );

        if (it == items.end())
        {
            addClassAuto<T>(nullptr);
            it = items.find( std::type_index(typeid(T)) );
        }
        else
        {
            CtxItem& item = it->second;

            // fallback to derived type (no instance or factory, but a derived type is registered)
            if ( !item.factory && (item.derivedType != std::type_index(typeid(void))) )
                it = items.find(item.derivedType);
        }

        CtxItem& item = it->second;


         if (item.deleter == nullptr)
        {
            if (item.marker)
                throw std::runtime_error(std::string("Cyclic dependecy while instantiating type: ") + typeid(T).name());

            item.marker = true;
            item.factory();
            item.marker = false;
        }
#ifdef INPLACE
        return *(reinterpret_cast< std::shared_ptr<T>* >(&item.storage));   //there's no nicer way...
#else
        return *(static_cast< std::shared_ptr<T>* >(item.ptr));
#endif
    }

    // explicit specialization for Context is after class

    template <class T>
    static std::shared_ptr<T> get_s()
    {
        struct ConstructibleContext : public Context {};

        auto ctx = std::make_shared<ConstructibleContext>();
        return ctx->get_m<T>();
    }


#if 0

    // Variadic template to add a list of free standing factory functions
    template <typename T1, typename T2, typename... Ts>
    void addFactory(T1 t1, T2 t2, Ts... ts)
    {
        addFactoryPriv(t1);
        addFactory(t2, ts...);
    }

    template <typename T>
    void addFactory(T t)
    {
        addFactoryPriv(t);
    }



    // Variadic template to add a list of classes with factory methods
    template <typename InstanceType1, typename InstanceType2, typename... ITs>
    void addClass()
    {
        addFactoryPriv(InstanceType1::factory);
        addClass<InstanceType2, ITs...>();
    }

    template <typename InstanceTypeLast>
    void addClass()
    {
        addFactoryPriv(InstanceTypeLast::factory);
    }
#endif
};

// specialization for Context::get<Context>()
template<>
std::shared_ptr<Context> Context::get_m()
{
    std::cout << "get Context\n";
    return shared_from_this();
}

#if 0
// Convenience class to add classes and free standing factories in a single construtor call
template<typename... Ts>
class ContextTmpl : public Context
{
public:
    template<typename... T2s>
    ContextTmpl(T2s... t2s)
    {
        addClass<Ts...>();
        addFactory(t2s...);
    }

    ContextTmpl()
    {
           addClass<Ts...>();
    }
};

template<>
class ContextTmpl<> : public Context
{
public:
    template<typename... T2s>
    ContextTmpl(T2s... t2s)
    {
        addFactory(t2s...);
    }

    ContextTmpl() = default;
};
#endif

} //end of namespace di

#endif
