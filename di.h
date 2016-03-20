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

namespace di {

class Context
{
    // A single item in the context
    struct CtxItem
    {
        void* instancePtr = nullptr;                                    // object instance pointer
        bool marker = false;                                            // flag used to detect circular dependencies
        std::function<void(void)> factory;                              // factory fn. to create a new object instance
        void (*deleter)(void*) = nullptr;                               // delete fn. (calls proper destructor)
        std::type_index derivedType = std::type_index(typeid(void));    // a derived type (eg. implementation of an interface)

        // non-copyable, non-moveable
        CtxItem() = default;
        CtxItem(const CtxItem& rhs) = delete;
        CtxItem& operator=(const CtxItem& rhs) = delete;
        CtxItem(CtxItem&& rhs) = delete;
        CtxItem& operator=(CtxItem&& rhs) = delete;

        void dump() const
        {
            std::cout << std::boolalpha << "inst: "<< instancePtr << ", mark: " << marker << ", factory: " << (bool)factory << ", del: " << deleter << ", desc: " << derivedType.name() << std::endl;
        }
    };



    // Factory signature
    template <class InstanceType, class... Args>
    using FactoryFunction = InstanceType*(*)(Args&...);



    // The object storage
    std::map<std::type_index, CtxItem> items;
    std::vector<CtxItem*> constructionOrder;



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



    // Add factory method automatically if present in class
    template <typename T, typename std::enable_if< std::is_function<decltype(T::factory)>::value >::type* = nullptr>
    void addClassAuto(void*) // argument only used to disambiguate from vararg version
    {
        addFactoryPriv(T::factory);
    }

    template<typename T>
    void addClassAuto(...)
    {
        throw std::runtime_error(std::string("Class '") + typeid(T).name() + "' has no factory in context!");
    }



    // Add a factory function to context
    template <class InstanceType, class... Args>
    void addFactoryPriv(FactoryFunction<InstanceType, Args...> factoryFunction)
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

        item.factory = [factoryFunction, this]()
        {
            addInstance(factoryFunction( get<Args>()... ), true);
        };
    }

    template <typename T>
    void addFactoryPriv(T)
    {
        // Use a dummy is_void type trait to force GCC to display instantiation type in error message
        static_assert( std::is_void<T>::value, "Factory has incorrect signature, should take (const) references and return a pointer! Examlpe: Foo* Foo::factory(Bar& bar); ");
    }



    // Gets a ContextItem, tries adding a class factory if type not found in map
    template <class T>
    CtxItem& getItem()
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
            if ( !item.instancePtr && !item.factory && (item.derivedType != std::type_index(typeid(void))) )
                it = items.find(item.derivedType);
        }

        return it->second;
    }



    // Add an already instantiated object to the context
    template <typename T>
    void addInstance(T* instance, bool takeOwnership = false)
    {
        if (instance == nullptr)
            throw std::runtime_error(std::string("Trying to add nullptr instance for type: ") + typeid(T).name());

        CtxItem& item = items[ std::type_index(typeid(T)) ];

        if (item.instancePtr != nullptr)
            throw std::runtime_error(std::string("Instance already in Context for type: ") + typeid(T).name());

        item.instancePtr = static_cast<void*>(instance);

        if (takeOwnership)
        {
            item.deleter = [](void* ptr) { delete( static_cast<T*>(ptr) ); };
            constructionOrder.push_back(&item);
        }
    }

public:
    Context()
    {
        addInstance(this);
    }

    ~Context()
    {
        for (auto it = constructionOrder.rbegin(); it != constructionOrder.rend(); it++)
            (**it).deleter((**it).instancePtr);
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
    T& get()
    {
        CtxItem& item = getItem<T>(); // may return derived type

        if (item.instancePtr == nullptr)
        {
            if (item.marker)
                throw std::runtime_error(std::string("Cyclic dependecy while instantiating type: ") + typeid(T).name());

            item.marker = true;
            item.factory();
            item.marker = false;
        }

        return *(static_cast<T*>(item.instancePtr));
    }



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
};

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


} //end of namespace DI

#endif
