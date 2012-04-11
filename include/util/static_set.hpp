#ifndef UTIL___STATIC_SET__HPP
#define UTIL___STATIC_SET__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Mike DiCuccio, Eugene Vasilchenko
 *
 * File Description:
 *     CStaticArraySet<> -- template class to provide convenient access to
 *                          a statically-defined array, while making sure that
 *                          the order of the array meets sort criteria in
 *                          debug builds.
 *
 */

#include <util/error_codes.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_param.hpp>
#include <utility>
#include <typeinfo>
#include <algorithm>
#include <functional>


BEGIN_NCBI_SCOPE


///
/// Template structure SStaticPair is simlified replacement of STL pair<>
/// Main reason of introducing this structure is o allow static initialization
/// by { xxx } construct.
/// It's main use is for static const structures which do not need constructors
///
template<class FirstType, class SecondType>
struct SStaticPair
{
    typedef FirstType first_type;
    typedef SecondType second_type;

    first_type first;
    second_type second;
};


/// Parameter to control printing diagnostic message about conversion
/// of static array data from a different type.
/// Default value is "off".
NCBI_PARAM_DECL_EXPORT(NCBI_XUTIL_EXPORT, bool, NCBI, STATIC_ARRAY_COPY_WARNING);
typedef NCBI_PARAM_TYPE(NCBI, STATIC_ARRAY_COPY_WARNING) TParamStaticArrayCopyWarning;


/// Parameter to control printing diagnostic message about unsafe static type.
/// Default value is "off".
NCBI_PARAM_DECL_EXPORT(NCBI_XUTIL_EXPORT, bool, NCBI, STATIC_ARRAY_UNSAFE_TYPE_WARNING);
typedef NCBI_PARAM_TYPE(NCBI, STATIC_ARRAY_UNSAFE_TYPE_WARNING) TParamStaticArrayUnsafeTypeWarning;


/// Namespace for static array templates' implementation.
BEGIN_NAMESPACE(NStaticArray);


template<class KeyValueGetter, class KeyCompare>
struct PLessByKey : public KeyCompare
{
    typedef KeyValueGetter                  getter;
    typedef typename getter::value_type     value_type;
    typedef typename getter::key_type       key_type;
    typedef KeyCompare                      key_compare;

    PLessByKey()
    {
    }

    PLessByKey(const key_compare& comp)
        : key_compare(comp)
    {
    }

    const key_compare& key_comp() const
    {
        return *this;
    }

    template<class Type1, class Type2>
    bool operator()(const Type1& v1, const Type2& v2) const
    {
        return key_comp()(getter::get_key(v1), getter::get_key(v2));
    }
};


template<class Value>
class PKeyValueSelf
{
public:
    typedef Value       value_type;
    typedef value_type  key_type;
    typedef value_type  mapped_type;
    
    static const key_type& get_key(const value_type& value)
    {
        return value;
    }
    static const mapped_type& get_mapped(const value_type& value)
    {
        return value;
    }
    static mapped_type& get_mapped(value_type& value)
    {
        return value;
    }
};


template<class Value>
class PKeyValuePair
{
public:
    typedef Value       value_type;
    typedef typename value_type::first_type  key_type;
    typedef typename value_type::second_type mapped_type;
    
    static const key_type& get_key(const key_type& key)
    {
        return key;
    }
    static const key_type& get_key(const value_type& value)
    {
        return value.first;
    }
    static const mapped_type& get_mapped(const value_type& value)
    {
        return value.second;
    }
    static mapped_type& get_mapped(value_type& value)
    {
        return value.second;
    }
};


/// Helper class for single object conversion from static type to work type.
class NCBI_XUTIL_EXPORT IObjectConverter
{
public:
    virtual ~IObjectConverter(void) THROWS_NONE;
    virtual const type_info& GetSrcTypeInfo(void) const THROWS_NONE = 0;
    virtual const type_info& GetDstTypeInfo(void) const THROWS_NONE = 0;
    virtual size_t GetSrcTypeSize(void) const THROWS_NONE = 0;
    virtual size_t GetDstTypeSize(void) const THROWS_NONE = 0;
    virtual void Convert(void* dst, const void* src) const = 0;
    virtual void Destroy(void* dst) const THROWS_NONE = 0;
    
    DECLARE_CLASS_STATIC_FAST_MUTEX(sx_InitMutex);
};


/// Helper class for holding and correct destruction of static array copy.
class NCBI_XUTIL_EXPORT CArrayHolder
{
public:
    CArrayHolder(IObjectConverter* converter) THROWS_NONE;
    ~CArrayHolder(void) THROWS_NONE;
    
    void* GetArrayPtr(void) const
    {
        return m_ArrayPtr;
    }
    size_t GetElementCount(void) const
    {
        return m_ElementCount;
    }
    void* ReleaseArrayPtr(void)
    {
        void* ret = m_ArrayPtr;
        m_ArrayPtr = 0;
        m_ElementCount = 0;
        return ret;
    }

    /// Convert data from static array of different type using
    /// the holder's converter. The result is stored in the holder.
    /// @param src_array
    ///   Pointer to the source static array.
    /// @param size
    ///   Number of elements in the array.
    /// @param file
    ///   Source file name of the static array declaration for diagnostics.
    /// @param line
    ///   Source line number of the static array declaration for diagnostics.
    void Convert(const void* src_array,
                 size_t size,
                 const char* file,
                 int line);
    
private:
    auto_ptr<IObjectConverter> m_Converter;
    void* m_ArrayPtr;
    size_t m_ElementCount;
    
private:
    CArrayHolder(const CArrayHolder&);
    void operator=(const CArrayHolder&);
};


/// Helper class for destruction of field 'first' in pair<> if exception
/// is thrown while constructing 'second'. 
class CObjectDestroyerGuard
{
public:
    CObjectDestroyerGuard(void* ptr,
                          IObjectConverter* converter) THROWS_NONE
        : m_Ptr(ptr),
          m_Converter(converter)
    {
    }
    ~CObjectDestroyerGuard(void) THROWS_NONE
    {
        if ( m_Ptr ) {
            m_Converter->Destroy(m_Ptr);
        }
    }
    void EndOfConversion(void) THROWS_NONE
    {
        m_Ptr = 0;
    }

private:
    void* m_Ptr;
    IObjectConverter* m_Converter;
    
private:
    CObjectDestroyerGuard(const CObjectDestroyerGuard&);
    void operator=(const CObjectDestroyerGuard&);
};


template<typename DstType, typename SrcType>
class CObjectConverterBase : public IObjectConverter
{
public:
    const type_info& GetSrcTypeInfo(void) const THROWS_NONE
    {
        return typeid(SrcType);
    }
    const type_info& GetDstTypeInfo(void) const THROWS_NONE
    {
        return typeid(DstType);
    }
    size_t GetSrcTypeSize(void) const THROWS_NONE
    {
        return sizeof(SrcType);
    }
    size_t GetDstTypeSize(void) const THROWS_NONE
    {
        return sizeof(DstType);
    }
    void Destroy(void* dst) const THROWS_NONE
    {
        static_cast<DstType*>(dst)->~DstType();
    }
};


/// Implementation of converter for a single-object conversion.
template<typename DstType, typename SrcType>
class CSimpleConverter : public IObjectConverter
{
public:
    const type_info& GetSrcTypeInfo(void) const THROWS_NONE
    {
        return typeid(SrcType);
    }
    const type_info& GetDstTypeInfo(void) const THROWS_NONE
    {
        return typeid(DstType);
    }
    size_t GetSrcTypeSize(void) const THROWS_NONE
    {
        return sizeof(SrcType);
    }
    size_t GetDstTypeSize(void) const THROWS_NONE
    {
        return sizeof(DstType);
    }
    void Destroy(void* dst) const THROWS_NONE
    {
        static_cast<DstType*>(dst)->~DstType();
    }
    void Convert(void* dst, const void* src) const
    {
        new (dst)DstType(*static_cast<const SrcType*>(src));
    }
};


template<typename DstType, typename SrcType>
inline
IObjectConverter* MakeConverter(DstType* /*dst_ptr*/,
                                SrcType* /*src_ptr*/)
{
    return new CSimpleConverter<DstType, SrcType>();
}


template<typename DstType, typename SrcType>
IObjectConverter* MakePairConverter(DstType* /*dst_ptr*/,
                                    SrcType* /*src_ptr*/);


template<typename DstType1, typename DstType2,
         typename SrcType1, typename SrcType2>
inline
IObjectConverter* MakeConverter(pair<DstType1, DstType2>* dst_ptr,
                                pair<SrcType1, SrcType2>* src_ptr)
{
    return MakePairConverter(dst_ptr, src_ptr);
}


template<typename DstType1, typename DstType2,
         typename SrcType1, typename SrcType2>
inline
IObjectConverter* MakeConverter(pair<DstType1, DstType2>* dst_ptr,
                                SStaticPair<SrcType1, SrcType2>* src_ptr)
{
    return MakePairConverter(dst_ptr, src_ptr);
}


template<typename DstType1, typename DstType2,
         typename SrcType1, typename SrcType2>
inline
IObjectConverter* MakeConverter(SStaticPair<DstType1, DstType2>* dst_ptr,
                                SStaticPair<SrcType1, SrcType2>* src_ptr)
{
    return MakePairConverter(dst_ptr, src_ptr);
}


/// Implementation of converter for pair<> conversion.
template<typename DstType, typename SrcType>
class CPairConverter : public CObjectConverterBase<DstType, SrcType>
{
public:
    const type_info& GetSrcTypeInfo(void) const THROWS_NONE
    {
        return typeid(SrcType);
    }
    const type_info& GetDstTypeInfo(void) const THROWS_NONE
    {
        return typeid(DstType);
    }
    size_t GetSrcTypeSize(void) const THROWS_NONE
    {
        return sizeof(SrcType);
    }
    size_t GetDstTypeSize(void) const THROWS_NONE
    {
        return sizeof(DstType);
    }
    void Destroy(void* dst) const THROWS_NONE
    {
        static_cast<DstType*>(dst)->~DstType();
    }
    void Convert(void* dst_ptr, const void* src_ptr) const
    {
        auto_ptr<IObjectConverter> conv1
            (MakeConverter(static_cast<typename DstType::first_type*>(0),
                           static_cast<typename SrcType::first_type*>(0)));
        auto_ptr<IObjectConverter> conv2
            (MakeConverter(static_cast<typename DstType::second_type*>(0),
                           static_cast<typename SrcType::second_type*>(0)));
        DstType& dst = *static_cast<DstType*>(dst_ptr);
        const SrcType& src = *static_cast<const SrcType*>(src_ptr);
        conv1->Convert((void*)&dst.first, &src.first);
        CObjectDestroyerGuard guard((void*)&dst.first, conv1.get());
        conv2->Convert((void*)&dst.second, &src.second);
        guard.EndOfConversion();
    }
};


template<typename DstType, typename SrcType>
inline
IObjectConverter* MakePairConverter(DstType* /*dst_ptr*/,
                                    SrcType* /*src_ptr*/)
{
    return new CPairConverter<DstType, SrcType>();
}


/// Log error message about non-MT-safe static type (string, pair<>) if it's
/// configured by TParamStaticArrayUnsafeTypeWarning parameter.
NCBI_XUTIL_EXPORT
void ReportUnsafeStaticType(const char* type_name,
                            const char* file,
                            int line);


/// Log error message about wrong order of elements in array and abort.
NCBI_XUTIL_EXPORT
void ReportIncorrectOrder(size_t curr_index,
                          const char* file,
                          int line);


/// Template for checking if the static array type is MT-safe,
/// i.e. doesn't have a constructor.
/// Only few standard types are detected - std::string, and std::pair<>.
template<typename Type>
inline
void CheckStaticType(const Type* /*type_ptr*/,
                     const char* /*file*/,
                     int /*line*/)
{
    // By default all types are allowed in static variables
}


template<typename Type1, typename Type2>
inline
void CheckStaticType(const pair<Type1, Type2>* type_ptr,
                     const char* file,
                     int line);


template<typename Type1, typename Type2>
inline
void CheckStaticType(const SStaticPair<Type1, Type2>* type_ptr,
                     const char* file,
                     int line);


inline
void CheckStaticType(const string* /*type_ptr*/,
                     const char* file,
                     int line)
{
    // Strings are bad in static variables
    NStaticArray::ReportUnsafeStaticType("std::string", file, line);
}


template<typename Type1, typename Type2>
inline
void CheckStaticType(const pair<Type1, Type2>* /*type_ptr*/,
                     const char* file,
                     int line)
{
    // The std::pair<> is not good for static variables
    NStaticArray::ReportUnsafeStaticType("std::pair<>", file, line);
    // check types of both members of the pair
    CheckStaticType(static_cast<const Type1*>(0), file, line);
    CheckStaticType(static_cast<const Type2*>(0), file, line);
}


template<typename Type1, typename Type2>
inline
void CheckStaticType(const SStaticPair<Type1, Type2>* /*type_ptr*/,
                     const char* file,
                     int line)
{
    // check types of both members of the pair
    CheckStaticType(static_cast<const Type1*>(0), file, line);
    CheckStaticType(static_cast<const Type2*>(0), file, line);
}


template<typename Type>
inline
void CheckStaticType(const char* file,
                     int line)
{
    CheckStaticType(static_cast<const Type*>(0), file, line);
}


END_NAMESPACE(NStaticArray);

///
/// class CStaticArraySet<> is an array adaptor that provides an STLish
/// interface to statically-defined arrays, while making efficient use
/// of the inherent sort order of such arrays.
///
/// This class can be used both to verify sorted order of a static array
/// and to access a static array cleanly.  The template parameters are
/// as follows:
///
///   KeyType    -- type of object used for access
///   KeyCompare -- comparison functor.  This must provide an operator(). 
///         This is patterned to accept PCase and PNocase and similar objects.
///
/// To use this class, define your static array as follows:
///
///  static const char* sc_MyArray[] = {
///      "val1",
///      "val2",
///      "val3"
///  };
///
/// Then, declare a static variable such as:
///
///     typedef StaticArraySet<const char*, PNocase_CStr> TStaticArray;
///     static TStaticArray sc_Array(sc_MyArray, sizeof(sc_MyArray));
///
/// In debug mode, the constructor will scan the list of items and insure
/// that they are in the sort order defined by the comparator used.  If the
/// sort order is not correct, then the constructor will ASSERT().
///
/// This can then be accessed as
///
///     if (sc_Array.find(some_value) != sc_Array.end()) {
///         ...
///     }
///
/// or
///
///     size_t idx = sc_Array.index_of(some_value);
///     if (idx != TStaticArray::eNpos) {
///         ...
///     }
///
///
template <typename KeyValueGetter, typename KeyCompare>
class CStaticArraySearchBase
{
public:
    enum {
        eNpos = -1
    };

    typedef KeyValueGetter      getter;
    typedef typename getter::value_type   value_type;
    typedef typename getter::key_type     key_type;
    typedef typename getter::mapped_type  mapped_type;
    typedef KeyCompare          key_compare;
    typedef NStaticArray::PLessByKey<getter, key_compare> value_compare;
    typedef const value_type&   const_reference;
    typedef const value_type*   const_iterator;
    typedef size_t              size_type;
    typedef ssize_t             difference_type;

    /// Default constructor.  This will build a set around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<size_t Size>
    CStaticArraySearchBase(const value_type (&arr)[Size],
                           const char* file, int line)
    {
        x_Set(arr, sizeof(arr), file, line);
    }

    /// Constructor to initialize comparator object.
    template<size_t Size>
    CStaticArraySearchBase(const value_type (&arr)[Size],
                           const key_compare& comp,
                           const char* file, int line)
        : m_Begin(comp)
    {
        x_Set(arr, sizeof(arr), file, line);
    }

    /// Default constructor.  This will build a set around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<typename Type>
    CStaticArraySearchBase(const Type* array_ptr, size_type array_size,
                           const char* file, int line)
    {
        x_Set(array_ptr, array_size, file, line);
    }

    /// Constructor to initialize comparator object.
    template<typename Type>
    CStaticArraySearchBase(const Type* array_ptr, size_type array_size,
                           const key_compare& comp,
                           const char* file, int line)
        : m_Begin(comp)
    {
        x_Set(array_ptr, array_size, file, line);
    }

    /// Destructor
    ~CStaticArraySearchBase(void)
    {
        if ( m_DeallocateFunc ) {
            m_DeallocateFunc(m_Begin.second(), m_End);
        }
    }

    const value_compare& value_comp() const
    {
        return m_Begin.first();
    }

    const key_compare& key_comp() const
    {
        return value_comp().key_comp();
    }

    /// Return the start of the controlled sequence.
    const_iterator begin() const
    {
        return m_Begin.second();
    }

    /// Return the end of the controlled sequence.
    const_iterator end() const
    {
        return m_End;
    }

    /// Return true if the container is empty.
    bool empty() const
    {
        return begin() == end();
    }

    /// Return number of elements in the container.
    size_type size() const
    {
        return end() - begin();
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is less than or equal to the indicated key.
    const_iterator lower_bound(const key_type& key) const
    {
        return std::lower_bound(begin(), end(), key, value_comp());
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is greater than the indicated key.
    const_iterator upper_bound(const key_type& key) const
    {
        return std::upper_bound(begin(), end(), key, value_comp());
    }

    /// Return a const_iterator pointing to the specified element, or
    /// to the end if the element is not found.
    const_iterator find(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? end(): iter;
    }

    /// Return the count of the elements in the sequence.  This will be
    /// either 0 or 1, as this structure holds unique keys.
    size_type count(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? 0: 1;
    }

    /// Return a pair of iterators bracketing the given element in
    /// the controlled sequence.
    pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        const_iterator start = lower_bound(key);
        const_iterator iter  = start;
        if ( !x_Bad(key, iter) ) {
            ++iter;
        }
        return make_pair(start, iter);
    }

    /// Return the index of the indicated element, or eNpos if the element is
    /// not found.
    difference_type index_of(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? eNpos: iter - begin();
    }

protected:

    /// Perform sort-order validation.  This is a no-op in release mode.
    static void x_Validate(const value_type* _DEBUG_ARG(array),
                           size_t _DEBUG_ARG(size),
                           const value_compare& _DEBUG_ARG(comp),
                           const char* _DEBUG_ARG(file),
                           int _DEBUG_ARG(line))
    {
        using namespace NStaticArray;
#ifdef _DEBUG
        for ( size_t i = 1; i < size; ++i ) {
            if ( !comp(array[i-1], array[i]) ) {
                ReportIncorrectOrder(i, file, line);
            }
        }
#endif
    }

    /// Assign array pointer and end pointer without conversion.
    void x_Set(const value_type* array_ptr, size_t array_size,
               const char* file, int line)
    {
        using namespace NStaticArray;
        CheckStaticType<value_type>(file, line);
        _ASSERT(array_size % sizeof(value_type) == 0);
        size_t size = array_size / sizeof(value_type);
        if ( m_Begin.second() ) {
            _ASSERT(m_Begin.second() == array_ptr);
            _ASSERT(m_End == array_ptr + size);
            _ASSERT(!m_DeallocateFunc);
        }
        else {
            x_Validate(array_ptr, size, value_comp(), file, line);
        }
        m_DeallocateFunc = 0;
        m_Begin.second() = array_ptr;
        m_End = array_ptr + size;
    }

    /// Assign array pointer and end pointer from differently typed array.
    /// Allocate necessarily typed array and copy its content.
    template<typename Type>
    void x_Set(const Type* array2_ptr, size_t array2_size,
               const char* file, int line)
    {
        using namespace NStaticArray;
        CheckStaticType<Type>(file, line);
        _ASSERT(array2_size % sizeof(Type) == 0);
        size_t size = array2_size / sizeof(Type);
        CArrayHolder holder(MakeConverter(static_cast<value_type*>(0),
                                          static_cast<Type*>(0)));
        holder.Convert(array2_ptr, size, file, line);
        if ( !m_Begin.second() ) {
            x_Validate(static_cast<const value_type*>(holder.GetArrayPtr()),
                       holder.GetElementCount(), value_comp(), file, line);
        }
        {{
            CFastMutexGuard guard(IObjectConverter::sx_InitMutex);
            if ( !m_Begin.second() ) {
                m_Begin.second() =
                    static_cast<const value_type*>(holder.ReleaseArrayPtr());
                m_End = m_Begin.second() + size;
                m_DeallocateFunc = x_DeallocateFunc;
            }
        }}
    }

    /// Function used for array destruction and deallocation if it
    /// was created from a differently typed static array.
    static void x_DeallocateFunc(const_iterator& begin_ref,
                                 const_iterator& end_ref)
    {
        using namespace NStaticArray;
        const_iterator begin, end;
        {{
            CFastMutexGuard guard(IObjectConverter::sx_InitMutex);
            begin = begin_ref;
            end = end_ref;
            begin_ref = 0;
            end_ref = 0;
        }}
        if ( begin ) {
            for ( ; end != begin; ) { // destruct in reverse order
                (--end)->~value_type();
            }
            free((void*)begin);
        }
    }

    typedef void (*TDeallocateFunc)(const_iterator& begin,
                                    const_iterator& end);

private:
    pair_base_member<value_compare, const_iterator> m_Begin;
    const_iterator m_End;
    TDeallocateFunc m_DeallocateFunc;

    bool x_Bad(const key_type& key, const_iterator iter) const
    {
        return iter == end()  ||  value_comp()(key, *iter);
    }
};


template <class KeyType, class KeyCompare = less<KeyType> >
class CStaticArraySet
    : public CStaticArraySearchBase<NStaticArray::PKeyValueSelf<KeyType>, KeyCompare>
{
    typedef CStaticArraySearchBase<NStaticArray::PKeyValueSelf<KeyType>, KeyCompare> TBase;
public:
    typedef typename TBase::value_type value_type;
    typedef typename TBase::const_iterator const_iterator;
    typedef typename TBase::size_type size_type;
    typedef typename TBase::key_compare key_compare;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<size_t Size>
    CStaticArraySet(const value_type (&arr)[Size],
                    const char* file, int line)
        : TBase(arr, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<size_t Size>
    CStaticArraySet(const value_type (&arr)[Size],
                    const key_compare& comp,
                    const char* file, int line)
        : TBase(arr, comp, file, line)
    {
    }

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<class Type>
    CStaticArraySet(const Type* array_ptr, size_t array_size,
                    const char* file, int line)
        : TBase(array_ptr, array_size, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<class Type>
    CStaticArraySet(const Type* array_ptr, size_t array_size,
                    const key_compare& comp,
                    const char* file, int line)
        : TBase(array_ptr, array_size, comp, file, line)
    {
    }

    NCBI_DEPRECATED_CTOR
    (CStaticArraySet(const_iterator obj,
                     size_type array_size));

    NCBI_DEPRECATED_CTOR
    (CStaticArraySet(const_iterator obj,
                     size_type array_size,
                     const key_compare& comp));
};


#define DEFINE_STATIC_ARRAY_MAP(Type, Var, Array) \
    static const Type (Var)((Array), sizeof(Array), __FILE__, __LINE__)

#define DECLARE_CLASS_STATIC_ARRAY_MAP(Type, Var) \
    static const Type Var

#define DEFINE_CLASS_STATIC_ARRAY_MAP(Type, Var, Array) \
    const Type (Var)((Array), sizeof(Array), __FILE__, __LINE__)


// Deprecated constructors (defined here to avoid GCC 3.3 parse errors)

template <class KeyType, class KeyCompare>
inline
CStaticArraySet<KeyType, KeyCompare>::CStaticArraySet
(const_iterator obj,
 size_type array_size)
  : TBase(obj, array_size, 0, 0)
{
}

template <class KeyType, class KeyCompare>
inline
CStaticArraySet<KeyType, KeyCompare>::CStaticArraySet
(const_iterator obj,
 size_type array_size,
 const key_compare& comp)
  : TBase(obj, array_size, comp, 0, 0)
{
}


END_NCBI_SCOPE

#endif  // UTIL___STATIC_SET__HPP
