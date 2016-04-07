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

#include <utility>

#define INPLACE

/* TODO:
 * - check base type with enable_if in declareBaseTypes
 * - check for std::tuple in dependencies typedef
 * - get rid of the copied-in C++14 header code
 * - derivedType to be CtxItem* instead of std::type_index?
 * - don't die if context is explicitly listed as sole dependency
 */


namespace di {

namespace detail {
    // A few SFINAE traits to check if various typedefs are present in a class

    template<typename T>
    class has_singleton_typedef
    {
        template<typename C> static char test(typename C::singleton*);
        template<typename C> static int test(...);
    public:
        static const bool value = sizeof(test<T>(nullptr)) == sizeof(char);
    };

    template<typename T>
    class has_dependencies_typedef
    {
        template<typename C> static char test(typename C::dependencies*);
        template<typename C> static int test(...);
    public:
        static const bool value = sizeof(test<T>(nullptr)) == sizeof(char);
    };

    template<typename T>
    class has_base_typedef
    {
        template<typename C> static char test(typename C::base*);
        template<typename C> static int test(...);
    public:
        static const bool value = sizeof(test<T>(nullptr)) == sizeof(char);
    };



    // Index sequence used for tuple type extraction
    template <std::size_t...Is> struct index_sequence {};

    template <std::size_t N, std::size_t...Is>
    struct build : public build<N - 1, N - 1, Is...> {};

    template <std::size_t...Is>
    struct build<0, Is...> {
        using type = index_sequence<Is...>;
    };

    template <std::size_t N>
    using make_index_sequence = typename build<N>::type;

} //endof namespace detail



class Context : public std::enable_shared_from_this<Context>
{
    // A single item in the context
    struct CtxItem
    {
        using weak_ptr_t = std::weak_ptr<char>;

#ifdef INPLACE
        std::aligned_storage<sizeof(weak_ptr_t), alignof(weak_ptr_t)>::type storage;
#else
        void* ptr{nullptr};
#endif

        bool cyclicMarker{false};                                       // flag used to detect circular dependencies
        bool singleton{false};                                          // flag for object scope
        std::function<void(void*)> factory;                             // factory fn. to create a new object instance
        void (*deleter)(void*){nullptr};                                // delete fn. (calls proper destructor)
        std::type_index derivedType{std::type_index(typeid(void))};     // a derived type (eg. implementation of an interface)

        void dump() const
        {
            std::cout << std::boolalpha << "mark: " << cyclicMarker << ", factory: " << (bool)factory << ", del: " << deleter << ", desc: " << derivedType.name() << std::endl;
        }

        // non-copyable, non-moveable
        CtxItem() = default;
        CtxItem(const CtxItem& rhs) = delete;
        CtxItem& operator=(const CtxItem& rhs) = delete;
        CtxItem(CtxItem&& rhs) = delete;
        CtxItem& operator=(CtxItem&& rhs) = delete;

        ~CtxItem()
        {
            if (deleter != nullptr)
#ifdef INPLACE
                deleter(&storage);
#else
                deleter(ptr);
#endif
        }

        bool isUnknownType()
        {
            // has no factory or derived type
            return !factory && derivedType == std::type_index(typeid(void));
        }

        bool useDerivedType()
        {
            // has no factory, but derived type present
            return !factory && derivedType != std::type_index(typeid(void));
        }

        bool hasInstance()
        {
            // a deleter function pointer is present -> there's an instance
            return deleter != nullptr;
        }
    };


    // The object storage (std::type_index removes const/volatile qualifiers)
    std::map<std::type_index, CtxItem> items;



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
    template<typename T, typename T::base* = nullptr>   // TODO: rewrite with enable_if
    void declareBaseTypes(std::type_index& derivedType)
    {
        items[ std::type_index(typeid( typename T::base )) ].derivedType = derivedType;
        declareBaseTypes<typename T::base>( derivedType );
    }

    template <typename T>
    void declareBaseTypes(...) { }
#endif

    template <typename T>
    inline void declareBaseTypesDispatch(std::type_index& instanceTypeIdx)
    {
#ifdef HAS_TR2
        declareBaseTypes< typename std::tr2::bases<T>::type >( instanceTypeIdx );
#else
        declareBaseTypes<T>( instanceTypeIdx );
#endif
    }

public:
    Context()
    {
        std::cout << "context constructor\n";
    }


    template<typename T, typename std::enable_if< detail::has_singleton_typedef<T>::value >::type* = nullptr >
    constexpr bool isSingletonScope()
    {
        // below assert is not very effective... too many error messages by the compiler
        static_assert( std::is_same<typename T::singleton, std::true_type>::value || std::is_same<typename T::singleton, std::false_type>::value,
                       "The singleton type alias must either refer to std::true_type or std::false_type!");

        return T::singleton::value;
    }

    template<typename T, typename std::enable_if< !detail::has_singleton_typedef<T>::value >::type* = nullptr >
    constexpr bool isSingletonScope()
    {
        return true;
    }

    template<typename T, bool Strict>
    inline void registerClass_priv()
    {
        auto instanceTypeIdx = std::type_index(typeid(T));
        declareBaseTypesDispatch<T>( instanceTypeIdx );

        CtxItem& item = items[ instanceTypeIdx ];

        if (item.factory)
            throw std::runtime_error(std::string("Factory already registed for type: ") + typeid(T).name());

        createFactory<T, Strict>(item);

        item.singleton = isSingletonScope<T>();
    }


    // Case1: T is default constructible
    template<typename T, bool Strict, typename std::enable_if< std::is_default_constructible<T>::value &&
                                                               !detail::has_dependencies_typedef<T>::value >::type* = nullptr >
    void createFactory(CtxItem& item)
    {
        //std::cout << "Default constructing: " << typeid(T).name() << std::endl;
        item.factory = [this](void* sh_ptr){
            *(static_cast<std::shared_ptr<T>*>(sh_ptr)) = std::make_shared<T>();
            std::cout << "factory type: " << typeid(T).name() << std::endl;
        };
    }

    // Case2: T is constructible with std::shared_ptr<Context>
    template<typename T, bool Strict, typename std::enable_if< std::is_constructible<T, std::shared_ptr<Context>>::value &&
                                                               !detail::has_dependencies_typedef<T>::value >::type* = nullptr >
    void createFactory(CtxItem& item)
    {
        //std::cout << "constructing with context: " << typeid(T).name() << std::endl;
        item.factory = [this](void* sh_ptr){
            *(static_cast<std::shared_ptr<T>*>(sh_ptr)) = std::make_shared<T>(get<Context>());
            std::cout << "factory type: " << typeid(T).name() << std::endl;
        };
    }


    // TODO: it might be better to check that T::dependencies is a std::tuple instead of simply checking its existance
    // Case3: T has a dependencies typedef listing its constructor arguments
    template<typename T, bool Strict, typename std::enable_if< detail::has_dependencies_typedef<T>::value >::type* = nullptr >
    void createFactory(CtxItem& item)
    {
        constexpr auto size = std::tuple_size<typename T::dependencies>::value;
        createFactoryWithDependencies<T, typename T::dependencies>(item, detail::make_index_sequence<size>());
    }


    // Case4: No known constuction method for T
    template<typename T, bool Strict, typename std::enable_if< !detail::has_dependencies_typedef<T>::value &&
                                                               !std::is_constructible<T, std::shared_ptr<Context>>::value &&
                                                               !std::is_default_constructible<T>::value >::type* = nullptr >
    void createFactory(CtxItem&)
    {
        static_assert( !Strict, "Don't know how to instantiate type, you cannot explicitly register such type!");
        throw std::runtime_error(std::string("Don't know how to instantiate '") + typeid(T).name() + "' or any of its derived types!");
    }

    // helper method for Case3
    template<typename T, typename Deps, std::size_t... index>
    void createFactoryWithDependencies(CtxItem& item, detail::index_sequence<index...>)
    {
        //std::cout << "constructing with dependnecies: " << typeid(T).name() << std::endl;
        item.factory = [this](void* sh_ptr){
            *(static_cast<std::shared_ptr<T>*>(sh_ptr)) = std::make_shared<T>( get< typename std::remove_reference<decltype(std::get<index>(std::declval<Deps>()))>::type >()... );
            std::cout << "factory type: " << typeid(T).name() << std::endl;
        };
        item.singleton = isSingletonScope<T>();
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
    template <typename T>
    std::shared_ptr<T> get()    //return std::shared_ptr<T_noncv> ???
    {
        using T_noncv = typename std::remove_cv<T>::type;
        CtxItem& item = items[ std::type_index(typeid(T_noncv)) ];

        // add a factory
        if (item.isUnknownType())
            registerClass_priv<T_noncv, false>();  //Strict: false -> failure to register a class (no way to construct it) is not a complie-time error (eg. abstract interface)

        CtxItem& effectiveItem = item.useDerivedType() ? items[ item.derivedType ] : item;

        if (effectiveItem.hasInstance())
        {
#ifdef INPLACE
            auto sharedPtr = reinterpret_cast< std::weak_ptr<T_noncv>* >(&effectiveItem.storage) -> lock();   //there's no nicer way...
#else
            auto sharedPtr = static_cast< std::weak_ptr<T_noncv>* >(effectiveItem.ptr) -> lock();
#endif

            if (!sharedPtr)
                throw std::runtime_error(std::string("Object was deleted by client code for type: ") + typeid(T_noncv).name());

            return sharedPtr;
        }

        // create an instance
       if (effectiveItem.cyclicMarker)
           throw std::runtime_error(std::string("Cyclic dependecy while instantiating type: ") + typeid(T).name());

       std::shared_ptr<T_noncv> sharedPtr;
       std::cout << "caller type: " << typeid(T_noncv).name() << std::endl;

       effectiveItem.cyclicMarker = true;
       effectiveItem.factory(&sharedPtr);
       effectiveItem.cyclicMarker = false;

       if ( !effectiveItem.singleton )
           return sharedPtr;

#ifdef INPLACE
       // placement new, explicit destruction
       new(&effectiveItem.storage) std::weak_ptr<T_noncv>(sharedPtr);
       effectiveItem.deleter = [](void* ptr) {
           //std::cout << "deleter " << typeid(T_noncv).name() << std::endl;
           static_cast< std::weak_ptr<T_noncv>* >(ptr) -> ~weak_ptr();
       };
#else
       effectiveItem.ptr = new std::weak_ptr<T>(std::forward(sharedPtr);
       effectiveItem.deleter = [](void* ptr) {
           //std::cout << "deleter " << typeid(T_noncv).name() << std::endl;
           delete static_cast< std::weak_ptr<T_noncv>* >(ptr);
       };
#endif
       return sharedPtr;
    }
    // explicit specialization of get<Context> is after class


    template <typename... Ts>
    void inject(std::shared_ptr<Ts>&... ts)
    {
        // use empty vararg lambda to provide arglist for parameter pack expansion
        [](...){}( (bool)(ts = get<Ts>())... ); //nice...
    }

    // Context is not a singleton! Every call to create() will result in a new Context which is destructed right
    // after the call unless some of the created objects depend on it.
    template <typename T>
    static std::shared_ptr<T> create()
    {
        auto ctx = std::make_shared<Context>();
        return ctx->get<T>();
    }

    // Variadic template to register a list of classes
    template <typename T1, typename T2, typename... Ts>
    void registerClass()
    {
        registerClass_priv<typename std::remove_cv<T1>::type, true>();
        registerClass<T2, Ts...>();
    }

    template <typename T>
    void registerClass()
    {
        registerClass_priv<typename std::remove_cv<T>::type, true>();
    }
};

// specialization for Context::get<Context>()
template<>
std::shared_ptr<Context> Context::get()
{
    std::cout << "get Context\n";
    return shared_from_this();
}


// Convenience class to register classes in a single call
template<typename... Ts>
class ContextReg
{
public:
    template <typename T>
    static std::shared_ptr<T> create()
    {
        auto ctx = std::make_shared<Context>();

        ctx->registerClass<Ts...>();
        return ctx->get<T>();
    }
};


} //end of namespace di

#endif
