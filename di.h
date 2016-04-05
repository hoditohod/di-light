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


namespace compat {
    //
    // std::index_sequence is C++14, the below block is taken from gcc's <utility> header to provide support on C++11 compilers
    //


    // Stores a tuple of indices.  Used by tuple and pair, and by bind() to
    // extract the elements in a tuple.
    template<size_t... _Indexes>
      struct _Index_tuple
      {
        typedef _Index_tuple<_Indexes..., sizeof...(_Indexes)> __next;
      };

    // Builds an _Index_tuple<0, 1, 2, ..., _Num-1>.
    template<size_t _Num>
      struct _Build_index_tuple
      {
        typedef typename _Build_index_tuple<_Num - 1>::__type::__next __type;
      };

    template<>
      struct _Build_index_tuple<0>
      {
        typedef _Index_tuple<> __type;
      };


    /// Class template integer_sequence
    template<typename _Tp, _Tp... _Idx>
      struct integer_sequence
      {
        typedef _Tp value_type;
        static constexpr size_t size() { return sizeof...(_Idx); }
      };

    template<typename _Tp, _Tp _Num,
             typename _ISeq = typename _Build_index_tuple<_Num>::__type>
      struct _Make_integer_sequence;

    template<typename _Tp, _Tp _Num,  size_t... _Idx>
      struct _Make_integer_sequence<_Tp, _Num, _Index_tuple<_Idx...>>
      {
        static_assert( _Num >= 0,
                       "Cannot make integer sequence of negative length" );

        typedef integer_sequence<_Tp, static_cast<_Tp>(_Idx)...> __type;
      };

    /// Alias template make_integer_sequence
    template<typename _Tp, _Tp _Num>
      using make_integer_sequence
        = typename _Make_integer_sequence<_Tp, _Num>::__type;

    /// Alias template index_sequence
    template<size_t... _Idx>
      using index_sequence = integer_sequence<size_t, _Idx...>;

    /// Alias template make_index_sequence
    template<size_t _Num>
      using make_index_sequence = make_integer_sequence<size_t, _Num>;

    /// Alias template index_sequence_for
    template<typename... _Types>
      using index_sequence_for = make_index_sequence<sizeof...(_Types)>;
} //endof namespace compat



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
        void* ptr = nullptr;
#endif

        bool marker = false;                                            // flag used to detect circular dependencies
        bool singleton = false;
        std::function<void(void*)> factory;                              // factory fn. to create a new object instance
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


    Context()
    {
        std::cout << "context constructor\n";
    }


    template<typename T, typename std::enable_if< detail::has_singleton_typedef<T>::value >::type* = nullptr >
    constexpr bool isSingletonScope()
    {
        return T::singleton::value;
    }

    template<typename T, typename std::enable_if< !detail::has_singleton_typedef<T>::value >::type* = nullptr >
    constexpr bool isSingletonScope()
    {
        return true;
    }

    template<typename T, bool Strict>
    inline void registerClass()
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
    template<typename T, bool Strict, typename std::enable_if< std::is_default_constructible<T>::value && !detail::has_dependencies_typedef<T>::value >::type* = nullptr >
    void createFactory(CtxItem& item)
    {
        //std::cout << "Default constructing: " << typeid(T).name() << std::endl;
        item.factory = [this](void* sh_ptr){
            *(static_cast<std::shared_ptr<T>*>(sh_ptr)) = std::make_shared<T>();
            std::cout << "factory type: " << typeid(T).name() << std::endl;
        };
    }

    // Case2: T is constructible with std::shared_ptr<Context>
    template<typename T, bool Strict, typename std::enable_if< std::is_constructible<T, std::shared_ptr<Context>>::value >::type* = nullptr >
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
        createFactoryWithDependencies<T, typename T::dependencies>(item, compat::make_index_sequence<size>());
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
    void createFactoryWithDependencies(CtxItem& item, compat::index_sequence<index...>)
    {
        //std::cout << "constructing with dependnecies: " << typeid(T).name() << std::endl;
        item.factory = [this](void* sh_ptr){
            *(static_cast<std::shared_ptr<T>*>(sh_ptr)) = std::make_shared<T>( get< typename std::remove_reference<decltype(std::get<index>(std::declval<Deps>()))>::type >()... );
            std::cout << "factory type: " << typeid(T).name() << std::endl;
        };
        item.singleton = isSingletonScope<T>();
    }

    // pivate!
    template <typename... Ts>
    void pass(Ts... ) {}


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

        if (item.isUnknownType())
            // add a factory
            registerClass<T_noncv, false>();  //Strict: false -> failure to register a class (no way to construct it) is not a complie-time error (eg. abstract interface)

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
       if (effectiveItem.marker)
           throw std::runtime_error(std::string("Cyclic dependecy while instantiating type: ") + typeid(T).name());

       std::shared_ptr<T_noncv> sharedPtr;
       std::cout << "caller type: " << typeid(T_noncv).name() << std::endl;

       effectiveItem.marker = true;
       effectiveItem.factory(&sharedPtr);
       effectiveItem.marker = false;

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
           std::cout << "deleter " << typeid(T_noncv).name() << std::endl;
           delete static_cast< std::weak_ptr<T_noncv>* >(ptr);
       };
#endif
       return sharedPtr;
    }

    template <typename... Ts>
    void inject(std::shared_ptr<Ts>&... ts)
    {
        pass( ts = get<Ts>()... );
    }

    // explicit specialization of get<Context> is after class

    // Context is not a singleton! Every call to create() will result in a new Context which is destructed right
    // after the call unless some of the created objects depend on it.
    template <class T>
    static std::shared_ptr<T> create()
    {
        struct ConstructibleContext : public Context {};

        auto ctx = std::make_shared<ConstructibleContext>();
        return ctx->get<T>();
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
std::shared_ptr<Context> Context::get()
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
