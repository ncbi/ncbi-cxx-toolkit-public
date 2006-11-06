#ifndef CORELIB___NCBIMISC__HPP
#define CORELIB___NCBIMISC__HPP

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
 * Author:  Denis Vakatov, Eugene Vasilchenko
 *
 *
 */

/// @file ncbistd.hpp
/// Miscellaneous common-use basic types and functionality


#include <corelib/ncbistl.hpp>
#ifdef NCBI_OS_UNIX
#  include <sys/types.h>
#endif
#ifdef NCBI_COMPILER_ICC
// Preemptively pull in <cctype>, which breaks if we've already
// repointed is* at NCBI_is*.
#  include <cctype>
#else
#  include <ctype.h>
#endif

#if defined(_DEBUG)  &&  !defined(NCBI_NO_STRICT_CTYPE_ARGS)
#  define NCBI_STRICT_CTYPE_ARGS
#endif


/** @addtogroup AppFramework
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Which type of ownership between objects.
///
/// Can be used to specify ownership relationship between objects.
/// For example, specify if a CSocket object owns the underlying
/// SOCK object. 
enum EOwnership {
    eNoOwnership,       ///< No ownership relationship
    eTakeOwnership      ///< An object can take ownership of another
};


/// Whether a value is nullable.
enum ENullable {
    eNullable,          ///< Value can be null
    eNotNullable        ///< Value cannot be null
};


/// Whether a value is nullable.
enum ESign {
    eNegative = -1,     ///< Value is negative
    eZero     =  0,     ///< Value is zero
    ePositive =  1      ///< Value is positive
};


/// Whether to truncate/round a value.
enum ERound {
    eTrunc,             ///< Value must be truncated
    eRound              ///< Value must be rounded
};


/// Whether to follow symbolic links (also known as shortcuts or aliases)
enum EFollowLinks {
    eIgnoreLinks,
    eFollowLinks
};


/// Interrupt on signal mode
///
/// On UNIX some functions can be interrupted by a signal and EINTR errno
/// value. We can restart or cancel its execution.
enum EInterruptOnSignal {
    eInterruptOnSignal, ///< Cancel operation if interrapted by a signal
    eRestartOnSignal    ///< Restart operation if interrupted by a signal
};


/////////////////////////////////////////////////////////////////////////////
/// Support for safe bool operators
/////////////////////////////////////////////////////////////////////////////


/// Macro to hide all oprators with bool argument which may be used
/// unintentially when second argument is of class having operator bool().
/// All methods are simply declared private without body definition.
#define HIDE_SAFE_BOOL_OPERATORS()                      \
    private:                                            \
    void operator<(bool) const;                         \
    void operator>(bool) const;                         \
    void operator<=(bool) const;                        \
    void operator>=(bool) const;                        \
    void operator==(bool) const;                        \
    void operator!=(bool) const;                        \
    void operator+(bool) const;                         \
    void operator-(bool) const;                         \
    public:


/// Low level macro for declaring bool operator.
#define DECLARE_SAFE_BOOL_METHOD(Expr)                  \
    operator bool(void) const {                         \
        return (Expr);                                  \
    }


/// Declaration of safe bool operator from boolean expression.
/// Actual operator declaration will be:
///    operator bool(void) const;
#define DECLARE_OPERATOR_BOOL(Expr)             \
    HIDE_SAFE_BOOL_OPERATORS()                  \
    DECLARE_SAFE_BOOL_METHOD(Expr)


/// Declaration of safe bool operator from pointer expression.
/// Actual operator declaration will be:
///    operator bool(void) const;
#define DECLARE_OPERATOR_BOOL_PTR(Ptr)          \
    DECLARE_OPERATOR_BOOL((Ptr) != 0)


/// Declaration of safe bool operator from CRef<>/CConstRef<> expression.
/// Actual operator declaration will be:
///    operator bool(void) const;
#define DECLARE_OPERATOR_BOOL_REF(Ref)          \
    DECLARE_OPERATOR_BOOL((Ref).NotNull())


/// Template used for empty base class optimization.
/// See details in the August '97 "C++ Issue" of Dr. Dobb's Journal
/// Also available from http://www.cantrip.org/emptyopt.html
/// We store usually empty template argument class together with data member.
/// This template is much like STL's pair<>, but the access to members
/// is done though methods first() and second() returning references
/// to corresponding members.
/// First template argument is represented as private base class,
/// while second template argument is represented as private member.
/// In addition to constructor taking two arguments,
/// we add constructor for initialization of only data member (second).
/// This is useful since usually first type is empty and doesn't require
/// non-trivial constructor.
/// We do not define any comparison functions as this template is intented
/// to be used internally within another templates,
/// which themselves should provide any additional functionality.

template<class Base, class Member>
class pair_base_member : private Base
{
public:
    typedef Base base_type;
    typedef Base first_type;
    typedef Member member_type;
    typedef Member second_type;
    
    pair_base_member(void)
        : base_type(), m_Member()
        {
        }
    
    explicit pair_base_member(const member_type& member_value)
        : base_type(), m_Member(member_value)
        {
        }
    
    explicit pair_base_member(const first_type& first_value,
                              const second_type& second_value)
        : base_type(first_value), m_Member(second_value)
        {
        }
    
    const first_type& first() const
        {
            return *this;
        }
    first_type& first()
        {
            return *this;
        }

    const second_type& second() const
        {
            return m_Member;
        }
    second_type& second()
        {
            return m_Member;
        }

    void Swap(pair_base_member<first_type, second_type>& p)
        {
            if (static_cast<void*>(&first()) != static_cast<void*>(&second())) {
                // work around an IBM compiler bug which causes it to perform
                // a spurious 1-byte swap, yielding mixed-up values.
                swap(first(), p.first());
            }
            swap(second(), p.second());
        }

private:
    member_type m_Member;
};


#ifdef HAVE_NO_AUTO_PTR


/////////////////////////////////////////////////////////////////////////////
///
/// auto_ptr --
///
/// Define auto_ptr if needed.
///
/// Replacement of STL's std::auto_ptr for compilers with poor "auto_ptr"
/// implementation.
/// 
/// See C++ Toolkit documentation for limitations and use of auto_ptr.

template <class X>
class auto_ptr
{
    // temporary class for auto_ptr copying
    template<class Y>
    struct auto_ptr_ref
    {
        auto_ptr_ref(auto_ptr<Y>& ptr)
            : m_AutoPtr(ptr)
            {
            }
        auto_ptr<Y>& m_AutoPtr;
    };
public:
    typedef X element_type;         ///< Define element_type

    /// Explicit conversion to auto_ptr.
    explicit auto_ptr(X* p = 0) : m_Ptr(p) {}

    /// Copy constructor with implicit conversion.
    ///
    /// Note that the copy constructor parameter is not a const
    /// because it is modified -- ownership is transferred.
    auto_ptr(auto_ptr<X>& a) : m_Ptr(a.release()) {}

    /// Assignment operator.
    auto_ptr<X>& operator=(auto_ptr<X>& a) {
        if (this != &a) {
            if (m_Ptr  &&  m_Ptr != a.m_Ptr) {
                delete m_Ptr;
            }
            m_Ptr = a.release();
        }
        return *this;
    }

    auto_ptr(auto_ptr_ref<X> ref)
        : m_Ptr(ref.m_AutoPtr.release())
    {
    }
    template <typename Y>
    operator auto_ptr_ref<Y>()
    {
        return auto_ptr_ref<Y>(*this);
    }

    /// Destructor.
    ~auto_ptr(void) {
        if ( m_Ptr )
            delete m_Ptr;
    }

    /// Deference operator.
    X&  operator*(void) const { return *m_Ptr; }

    /// Reference operator.
    X*  operator->(void) const { return m_Ptr; }

    /// Equality operator.
    int operator==(const X* p) const { return (m_Ptr == p); }

    /// Get pointer value.
    X*  get(void) const { return m_Ptr; }

    /// Release pointer.
    X* release(void) {
        X* x_Ptr = m_Ptr;  m_Ptr = 0;  return x_Ptr;
    }

    /// Reset pointer.
    void reset(X* p = 0) {
        if (m_Ptr != p) {
            delete m_Ptr;
            m_Ptr = p;
        }
    }

private:
    X* m_Ptr;               ///< Internal pointer implementation.
};

#endif /* HAVE_NO_AUTO_PTR */



/// Functor template for allocating object.
template<class X>
struct Creater
{
    /// Default create function.
    static X* Create(void)
    { return new X; }
};

/// Functor tempate for deleting object.
template<class X>
struct Deleter
{
    /// Default delete function.
    static void Delete(X* object)
    { delete object; }
};

/// Functor template for deleting array of objects.
template<class X>
struct ArrayDeleter
{
    /// Array delete function.
    static void Delete(X* object)
    { delete[] object; }
};

/// Functor template for the C language deallocation function, free().
template<class X>
struct CDeleter
{
    /// C Language deallocation function.
    static void Delete(X* object)
    { free(object); }
};



/////////////////////////////////////////////////////////////////////////////
///
/// AutoPtr --
///
/// Define an "auto_ptr" like class that can be used inside STL containers.
///
/// The Standard auto_ptr template from STL doesn't allow the auto_ptr to be
/// put in STL containers (list, vector, map etc.). The reason for this is
/// the absence of copy constructor and assignment operator.
/// We decided that it would be useful to have an analog of STL's auto_ptr
/// without this restriction - AutoPtr.
///
/// Due to nature of AutoPtr its copy constructor and assignment operator
/// modify the state of the source AutoPtr object as it transfers ownership
/// to the target AutoPtr object. Also, we added possibility to redefine the
/// way pointer will be deleted: the second argument of template allows
/// pointers from "malloc" in AutoPtr, or you can use "ArrayDeleter" (see
/// above) to properly delete an array of objects using "delete[]" instead
/// of "delete". By default, the internal pointer will be deleted by C++
/// "delete" operator.
///
/// @sa
///   Deleter(), ArrayDeleter(), CDeleter()

template< class X, class Del = Deleter<X> >
class AutoPtr
{
public:
    typedef X element_type;         ///< Define element type.
    typedef Del deleter_type;       ///< Alias for template argument.

    /// Constructor.
    AutoPtr(element_type* p = 0)
        : m_Ptr(p), m_Data(true)
    {
    }

    /// Constructor.
    AutoPtr(element_type* p, const deleter_type& deleter)
        : m_Ptr(p), m_Data(deleter, true)
    {
    }

    /// Copy constructor.
    AutoPtr(const AutoPtr<X, Del>& p)
        : m_Ptr(0), m_Data(p.m_Data)
    {
        m_Ptr = p.x_Release();
    }

    /// Destructor.
    ~AutoPtr(void)
    {
        reset();
    }

    /// Assignment operator.
    AutoPtr<X, Del>& operator=(const AutoPtr<X, Del>& p)
    {
        if (this != &p) {
            bool owner = p.m_Data.second();
            reset(p.x_Release());
            m_Data.second() = owner;
        }
        return *this;
    }

    /// Assignment operator.
    AutoPtr<X, Del>& operator=(element_type* p)
    {
        reset(p);
        return *this;
    }

    /// Bool operator for use in if() clause.
    DECLARE_OPERATOR_BOOL_PTR(m_Ptr);

    // Standard getters.

    /// Dereference operator.
    element_type& operator* (void) const { return *m_Ptr; }

    /// Reference operator.
    element_type* operator->(void) const { return  m_Ptr; }

    /// Get pointer.
    element_type* get       (void) const { return  m_Ptr; }

    /// Release will release ownership of pointer to caller.
    element_type* release(void)
    {
        m_Data.second() = false;
        return m_Ptr;
    }

    /// Reset will delete old pointer, set content to new value,
    /// and accept ownership upon the new pointer.
    void reset(element_type* p = 0)
    {
        if (m_Ptr  &&  m_Data.second()) {
            m_Data.first().Delete(release());
        }
        m_Ptr   = p;
        m_Data.second() = true;
    }

    void Swap(AutoPtr<X, Del>& a)
    {
        swap(m_Ptr, a.m_Ptr);
        swap(m_Data, a.m_Data);
    }

private:
    element_type* m_Ptr;                  ///< Internal pointer representation.
    mutable pair_base_member<deleter_type, bool> m_Data; ///< State info.

    /// Release for const object.
    element_type* x_Release(void) const
    {
        return const_cast<AutoPtr<X, Del>*>(this)->release();
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// AutoArray --
///
/// "AutoPtr" like class for using with arrays
///
/// vector<> template comes with a performance penalty, since it always
/// initializes its content. This template is not a vector replacement,
/// it's a version of AutoPtr<> tuned for array pointers. For convinience
/// it defines array style access operator [] and size based contructor.
///
/// @sa AutoPtr
///

template< class X, class Del = ArrayDeleter<X> >
class AutoArray
{
public:
    typedef X element_type;         ///< Define element type.
    typedef Del deleter_type;       ///< Alias for template argument.

public:

    /// Construct the arrya using C++ new[] operator
    /// @note In this case you should use ArrayDeleter<> or compatible
    explicit AutoArray(size_t size)
        : m_Ptr(new element_type[size]), m_Data(true)
    {}

    explicit AutoArray(element_type* p = 0)
        : m_Ptr(p), m_Data(true)
    {}

    AutoArray(element_type* p, const deleter_type& deleter)
        : m_Ptr(p), m_Data(deleter, true)
    {
    }

    AutoArray(const AutoArray<X, Del>& p)
        : m_Ptr(0), m_Data(p.m_Data)
    {
        m_Ptr = p.x_Release();
    }

    ~AutoArray(void)
    {
        reset();
    }

    /// Assignment operator.
    AutoArray<X, Del>& operator=(const AutoArray<X, Del>& p)
    {
        if (this != &p) {
            bool owner = p.m_Data.second();
            reset(p.x_Release());
            m_Data.second() = owner;
        }
        return *this;
    }

    /// Assignment operator.
    AutoArray<X, Del>& operator=(element_type* p)
    {
        reset(p);
        return *this;
    }
    /// Bool operator for use in if() clause.
    DECLARE_OPERATOR_BOOL_PTR(m_Ptr);

    /// Get pointer.
    element_type* get       (void) const { return  m_Ptr; }

    /// Release will release ownership of pointer to caller.
    element_type* release(void)
    {
        m_Data.second() = false;
        return m_Ptr;
    }

    /// array style dereference (returns value)
    const element_type& operator[](size_t pos) const { return m_Ptr[pos]; }

    /// array style dereference (returns reference)
    element_type& operator[](size_t pos) { return m_Ptr[pos]; }

    /// Reset will delete old pointer, set content to new value,
    /// and accept ownership upon the new pointer.
    void reset(element_type* p = 0)
    {
        if (m_Ptr  &&  m_Data.second()) {
            m_Data.first().Delete(release());
        }
        m_Ptr   = p;
        m_Data.second() = true;
    }

    void Swap(AutoPtr<X, Del>& a)
    {
        swap(m_Ptr, a.m_Ptr);
        swap(m_Data, a.m_Data);
    }

private:
    /// Release for const object.
    element_type* x_Release(void) const
    {
        return const_cast<AutoArray<X, Del>*>(this)->release();
    }

private:
    element_type*  m_Ptr;
    mutable pair_base_member<deleter_type, bool> m_Data; ///< State info.
};



// "min" and "max" templates
//

// Always get rid of the old non-conformant min/max macros
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

#if defined(HAVE_NO_MINMAX_TEMPLATE)
#  define NOMINMAX

/// Min function template.
template <class T>
inline
const T& min(const T& a, const T& b) {
    return b < a ? b : a;
}

/// Max function template.
template <class T>
inline
const T& max(const T& a, const T& b) {
    return  a < b ? b : a;
}
#endif /* HAVE_NO_MINMAX_TEMPLATE */



// strdup()
//

#ifndef HAVE_STRDUP
/// Supply string duplicate function, if one is not defined.
extern char* strdup(const char* str);
#endif



// ctype hacks
//

#ifdef NCBI_STRICT_CTYPE_ARGS

END_NCBI_SCOPE

#define NCBI_CTYPEFAKEBODY \
  { return See_the_standard_on_proper_argument_type_for_ctype_macros(c); }

#ifdef isalpha
inline int NCBI_isalpha(unsigned char c) { return isalpha(c); }
inline int NCBI_isalpha(int           c) { return isalpha(c); }
template<class C>
inline int NCBI_isalpha(C c) NCBI_CTYPEFAKEBODY
#undef  isalpha
#define isalpha NCBI_isalpha
#endif

#ifdef isalnum
inline int NCBI_isalnum(unsigned char c) { return isalnum(c); }
inline int NCBI_isalnum(int           c) { return isalnum(c); }
template<class C>
inline int NCBI_isalnum(C c) NCBI_CTYPEFAKEBODY
#undef  isalnum
#define isalnum NCBI_isalnum
#endif

#ifdef isascii
inline int NCBI_isascii(unsigned char c) { return isascii(c); }
inline int NCBI_isascii(int           c) { return isascii(c); }
template<class C>
inline int NCBI_isascii(C c) NCBI_CTYPEFAKEBODY
#undef  isascii
#define isascii NCBI_isascii
#endif

#ifdef isblank
inline int NCBI_isblank(unsigned char c) { return isblank(c); }
inline int NCBI_isblank(int           c) { return isblank(c); }
template<class C>
inline int NCBI_isblank(C c) NCBI_CTYPEFAKEBODY
#undef  isblank
#define isblank NCBI_isblank
#endif

#ifdef iscntrl
inline int NCBI_iscntrl(unsigned char c) { return iscntrl(c); }
inline int NCBI_iscntrl(int           c) { return iscntrl(c); }
template<class C>
inline int NCBI_iscntrl(C c) NCBI_CTYPEFAKEBODY
#undef  iscntrl
#define iscntrl NCBI_iscntrl
#endif

#ifdef isdigit
inline int NCBI_isdigit(unsigned char c) { return isdigit(c); }
inline int NCBI_isdigit(int           c) { return isdigit(c); }
template<class C>
inline int NCBI_isdigit(C c) NCBI_CTYPEFAKEBODY
#undef  isdigit
#define isdigit NCBI_isdigit
#endif

#ifdef isgraph
inline int NCBI_isgraph(unsigned char c) { return isgraph(c); }
inline int NCBI_isgraph(int           c) { return isgraph(c); }
template<class C>
inline int NCBI_isgraph(C c) NCBI_CTYPEFAKEBODY
#undef  isgraph
#define isgraph NCBI_isgraph
#endif

#ifdef islower
inline int NCBI_islower(unsigned char c) { return islower(c); }
inline int NCBI_islower(int           c) { return islower(c); }
template<class C>
inline int NCBI_islower(C c) NCBI_CTYPEFAKEBODY
#undef  islower
#define islower NCBI_islower
#endif

#ifdef isprint
inline int NCBI_isprint(unsigned char c) { return isprint(c); }
inline int NCBI_isprint(int           c) { return isprint(c); }
template<class C>
inline int NCBI_isprint(C c) NCBI_CTYPEFAKEBODY
#undef  isprint
#define isprint NCBI_isprint
#endif

#ifdef ispunct
inline int NCBI_ispunct(unsigned char c) { return ispunct(c); }
inline int NCBI_ispunct(int           c) { return ispunct(c); }
template<class C>
inline int NCBI_ispunct(C c) NCBI_CTYPEFAKEBODY
#undef  ispunct
#define ispunct NCBI_ispunct
#endif

#ifdef isspace
inline int NCBI_isspace(unsigned char c) { return isspace(c); }
inline int NCBI_isspace(int           c) { return isspace(c); }
template<class C>
inline int NCBI_isspace(C c) NCBI_CTYPEFAKEBODY
#undef  isspace
#define isspace NCBI_isspace
#endif

#ifdef isupper
inline int NCBI_isupper(unsigned char c) { return isupper(c); }
inline int NCBI_isupper(int           c) { return isupper(c); }
template<class C>
inline int NCBI_isupper(C c) NCBI_CTYPEFAKEBODY
#undef  isupper
#define isupper NCBI_isupper
#endif

#ifdef isxdigit
inline int NCBI_isxdigit(unsigned char c) { return isxdigit(c); }
inline int NCBI_isxdigit(int           c) { return isxdigit(c); }
template<class C>
inline int NCBI_isxdigit(C c) NCBI_CTYPEFAKEBODY
#undef  isxdigit
#define isxdigit NCBI_isxdigit
#endif

#ifdef toascii
inline int NCBI_toascii(unsigned char c) { return toascii(c); }
inline int NCBI_toascii(int           c) { return toascii(c); }
template<class C>
inline int NCBI_toascii(C c) NCBI_CTYPEFAKEBODY
#undef  toascii
#define toascii NCBI_toascii
#endif

#ifdef tolower
inline int NCBI_tolower(unsigned char c) { return tolower(c); }
inline int NCBI_tolower(int           c) { return tolower(c); }
template<class C>
inline int NCBI_tolower(C c) NCBI_CTYPEFAKEBODY
#undef  tolower
#define tolower NCBI_tolower
#endif

#ifdef toupper
inline int NCBI_toupper(unsigned char c) { return toupper(c); }
inline int NCBI_toupper(int           c) { return toupper(c); }
template<class C>
inline int NCBI_toupper(C c) NCBI_CTYPEFAKEBODY
#undef  toupper
#define toupper NCBI_toupper
#endif

#undef NCBI_CTYPEFAKEBODY

BEGIN_NCBI_SCOPE

#endif // NCBI_STRICT_CTYPE_ARGS



//  ITERATE
//  NON_CONST_ITERATE
//
// Useful macro to write 'for' statements with the STL container iterator as
// a variable.
//

/// ITERATE macro to sequence through container elements.
#define ITERATE(Type, Var, Cont) \
    for ( Type::const_iterator Var = (Cont).begin(), NCBI_NAME2(Var,_end) = (Cont).end();  Var != NCBI_NAME2(Var,_end);  ++Var )

/// Non constant version of ITERATE macro.
#define NON_CONST_ITERATE(Type, Var, Cont) \
    for ( Type::iterator Var = (Cont).begin();  Var != (Cont).end();  ++Var )

/// ITERATE macro to reverse sequence through container elements.
#define REVERSE_ITERATE(Type, Var, Cont) \
    for ( Type::const_reverse_iterator Var = (Cont).rbegin(), NCBI_NAME2(Var,_end) = (Cont).rend();  Var != NCBI_NAME2(Var,_end);  ++Var )

/// Non constant version of REVERSE_ITERATE macro.
#define NON_CONST_REVERSE_ITERATE(Type, Var, Cont) \
    for ( Type::reverse_iterator Var = (Cont).rbegin();  Var != (Cont).rend();  ++Var )


/// Type for sequence locations and lengths.
///
/// Use this typedef rather than its expansion, which may change.
typedef unsigned int TSeqPos;

/// Define special value for invalid sequence position.
const TSeqPos kInvalidSeqPos = ((TSeqPos) (-1));


/// Type for signed sequence position.
///
/// Use this type when and only when negative values are a possibility
/// for reporting differences between positions, or for error reporting --
/// though exceptions are generally better for error reporting.
/// Use this typedef rather than its expansion, which may change.
typedef int TSignedSeqPos;

/// Helper address class
class CRawPointer
{
public:
    /// add offset to object reference (to get object's member)
    static void* Add(void* object, ssize_t offset);
    static const void* Add(const void* object, ssize_t offset);
    /// calculate offset inside object
    static ssize_t Sub(const void* first, const void* second);
};


inline
void* CRawPointer::Add(void* object, ssize_t offset)
{
    return static_cast<char*> (object) + offset;
}

inline
const void* CRawPointer::Add(const void* object, ssize_t offset)
{
    return static_cast<const char*> (object) + offset;
}

inline
ssize_t CRawPointer::Sub(const void* first, const void* second)
{
    return (ssize_t)/*ptrdiff_t*/
        (static_cast<const char*> (first) - static_cast<const char*> (second));
}

/// Macro used to mark a constructor as deprecated.
///
/// The correct syntax for this varies from compiler to compiler:
/// older versions of GCC (prior to 3.4) require NCBI_DEPRECATED to
/// follow any relevant constructor declarations, but some other
/// compilers (Microsoft Visual Studio 2005, IBM Visual Age / XL)
/// require it to precede any relevant declarations, whether or not
/// they are for constructors.
#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_COMPILER_VISUALAGE)
#  define NCBI_DEPRECATED_CTOR(decl) NCBI_DEPRECATED decl
#else
#  define NCBI_DEPRECATED_CTOR(decl) decl NCBI_DEPRECATED
#endif

END_NCBI_SCOPE

BEGIN_STD_SCOPE

template<class T1, class T2>
inline
void swap(NCBI_NS_NCBI::pair_base_member<T1,T2>& pair1,
          NCBI_NS_NCBI::pair_base_member<T1,T2>& pair2)
{
    pair1.Swap(pair2);
}


template<class P, class D>
inline
void swap(NCBI_NS_NCBI::AutoPtr<P,D>& ptr1,
          NCBI_NS_NCBI::AutoPtr<P,D>& ptr2)
{
    ptr1.Swap(ptr2);
}


#if (defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION < 340)  ||  defined(NCBI_COMPILER_WORKSHOP)  ||  defined(NCBI_COMPILER_MIPSPRO)

#define ArraySize(array) sizeof(array)/sizeof((array)[0])

#else

template<class Element, size_t Size>
inline
size_t ArraySize(const Element (&)[Size])
{
    return Size;
}

#endif

END_STD_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.99  2006/11/06 20:50:38  ucko
 * Also use the macro version of ArraySize on WorkShop and MIPSpro
 * (in addition to old GCC), as the template can make them choke. :-/
 *
 * Revision 1.98  2006/10/24 19:11:55  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.97  2006/07/17 14:24:17  ucko
 * * pair_base_member::Swap: don't swap the first() elements if they take
 *   up no room, as it should be a no-op but VisualAge gets it wrong.
 * * NCBI_DEPRECATED_CTOR: Use MSVC-style ordering for NCBI_COMPILER_VISUALAGE.
 *
 * Revision 1.96  2006/06/20 18:03:19  vakatov
 * Unconditionally #undef the old non-conformant min() and max() macros
 *
 * Revision 1.95  2006/03/14 13:16:51  ivanov
 * + enum EInterruptOnSignal
 *
 * Revision 1.94  2006/01/17 17:23:20  vasilche
 * Fixed definition of CAutoArray.
 *
 * Revision 1.93  2005/12/19 18:01:31  ucko
 * Introduce a special NCBI_DEPRECATED_CTOR macro to work around a
 * fundamental incompatibility between GCC 3.2/3.3 and MS Visual Studio 2005.
 *
 * Revision 1.92  2005/12/12 15:07:48  vasilche
 * Added auto_ptr_ref<> for safe rvalue to lvalue conversion.
 *
 * Revision 1.91  2005/11/17 18:41:27  grichenk
 * Defined ArraySize macro for compilers not supporting ArraySize template.
 *
 * Revision 1.90  2005/11/15 17:56:30  grichenk
 * Moved Beep to ncbi_os_mswin.hpp
 * Added ArraySize template
 *
 * Revision 1.89  2005/11/02 15:48:10  kuznets
 * return const reference in operator[] const (AutoArray)
 *
 * Revision 1.88  2005/11/02 15:10:40  kuznets
 * +AutoArray<>
 *
 * Revision 1.87  2005/06/17 15:16:36  vasilche
 * New template pair_base_member<> for empty base optimization.
 * Changed AutoPtr to store Deleter object using pair_base_member.
 * Implemented AutoPtr::Swap.
 *
 * Revision 1.86  2005/06/16 20:18:20  lavr
 * Spell substitutes
 *
 * Revision 1.85  2005/06/16 20:13:41  lavr
 * Define ctype substitutes outside any namespace
 *
 * Revision 1.84  2005/06/15 15:21:07  ucko
 * When building with ICC, preemptively pull in <cctype> rather than <ctype.h>.
 *
 * Revision 1.83  2005/06/13 19:05:23  lavr
 * Engage <ctype.h> argument checks (DEBUG builds only)
 *
 * Revision 1.82  2005/06/10 21:04:19  lavr
 * <ctype.h> hacks:  fix some typos, add inline keywords;  still inactive
 *
 * Revision 1.81  2005/06/10 20:45:40  lavr
 * <ctype.h> hacks added; inactive for now
 *
 * Revision 1.80  2005/04/28 14:01:02  ivanov
 * Added REVERSE_ITERATE and NON_CONST_REVERSE_ITERATE macros
 *
 * Revision 1.79  2005/04/12 19:06:39  ucko
 * Move EFollowLinks to ncbimisc.hpp.
 *
 * Revision 1.78  2005/01/25 01:10:08  lavr
 * Explicit ptrdiff_t->ssize_t casting in CRawPointer::Sub
 *
 * Revision 1.77  2005/01/24 17:03:58  vasilche
 * New boolean operators support.
 *
 * Revision 1.76  2005/01/13 16:42:26  lebedev
 * CSafeBool template: dummy method added to avoid warnings on Mac OS X
 *
 * Revision 1.75  2005/01/12 15:21:43  vasilche
 * Added helper template and macro for easy implementation of boolean operator via pointer.
 *
 * Revision 1.74  2004/11/08 13:04:35  dicuccio
 * Removed definitions for lower-case iterate() and non_const_iterate()
 *
 * Revision 1.73  2004/09/27 13:48:42  ivanov
 * + ERound enum
 *
 * Revision 1.72  2004/09/07 16:21:11  ivanov
 * Added ESign enum
 *
 * Revision 1.71  2004/01/20 17:10:08  ivanov
 * Rollback previous commit
 *
 * Revision 1.70  2004/01/20 17:06:42  ivanov
 * Added #include <ncbiconf.h>
 *
 * Revision 1.69  2003/12/01 20:44:46  ucko
 * +<sys/types.h> on Unix for ssize_t
 *
 * Revision 1.68  2003/12/01 19:04:20  grichenk
 * Moved Add and Sub from serialutil to ncbimisc, made them methods
 * of CRawPointer class.
 *
 * Revision 1.67  2003/10/02 17:43:43  vakatov
 * The code extracted from NCBISTD.HPP. -- The latter is a "pure virtual"
 * header now.
 *
 * Revision 1.66  2003/09/17 15:17:30  vasilche
 * Fixed self references in template class AutoPtr.
 *
 * Revision 1.65  2003/08/14 12:48:18  siyan
 * Documentation changes.
 *
 * Revision 1.64  2003/04/21 14:31:12  kuznets
 * lower case iterate returned back to the header
 *
 * Revision 1.63  2003/04/18 18:10:08  kuznets
 * + enum ENullable
 *
 * Revision 1.62  2003/03/10 17:43:45  kuznets
 * iterate -> ITERATE cleanup
 *
 * Revision 1.61  2003/02/04 18:15:54  gouriano
 * removed reference to ncbifloat.h
 *
 * Revision 1.60  2003/02/04 17:02:53  gouriano
 * added reference to ncbifloat.h
 *
 * Revision 1.59  2002/09/19 22:17:11  vakatov
 * + kInvalidSeqPos
 *
 * Revision 1.58  2002/08/12 14:57:52  lavr
 * +EOwnership
 *
 * Revision 1.57  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.56  2002/05/03 21:27:59  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.55  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.54  2001/05/30 16:04:22  vakatov
 * AutoPtr::  -- do not make it owner if the source AutoPtr object was not
 * an owner (for copy-constructor and operator=).
 *
 * Revision 1.53  2001/05/17 14:54:12  lavr
 * Typos corrected
 *
 * Revision 1.52  2001/04/13 02:52:34  vakatov
 * Rollback to R1.50.  It is premature to check for #HAVE_NCBI_C until
 * we can configure it on MS-Windows...
 *
 * Revision 1.51  2001/04/12 22:53:00  vakatov
 * Apply fix R1.50 only #if HAVE_NCBI_C
 *
 * Revision 1.50  2001/03/16 02:20:40  vakatov
 * [MSWIN]  Avoid the "Beep()" clash between MS-Win and C Toolkit headers
 *
 * Revision 1.49  2001/02/22 00:09:28  vakatov
 * non_const_iterate() -- added parenthesis around the "Cont" arg
 *
 * Revision 1.48  2000/12/24 00:01:48  vakatov
 * Moved some code from NCBIUTIL to NCBISTD.
 * Fixed AutoPtr to always work with assoc.containers
 *
 * Revision 1.46  2000/12/15 15:36:30  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * Revision 1.45  2000/12/12 14:20:14  vasilche
 * Added operator bool to CArgValue.
 * Added standard typedef element_type to CRef<> and CConstRef<>.
 * Macro iterate() now calls method end() only once and uses temporary variable
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
 *
 * Revision 1.44  2000/12/11 20:42:48  vakatov
 * + NStr::PrintableString()
 *
 * Revision 1.43  2000/11/07 04:07:19  vakatov
 * kEmptyCStr, kEmptyStr (equiv. to NcbiEmptyCStr,NcbiEmptyString)
 *
 * Revision 1.42  2000/10/05 20:01:10  vakatov
 * auto_ptr -- no "const" in constructor and assignment
 *
 * Revision 1.41  2000/08/03 20:21:10  golikov
 * Added predicate PCase for AStrEquiv
 * PNocase, PCase goes through NStr::Compare now
 *
 * Revision 1.40  2000/07/19 19:03:52  vakatov
 * StringToBool() -- short and case-insensitive versions of "true"/"false"
 * ToUpper/ToLower(string&) -- fixed
 *
 * Revision 1.39  2000/04/17 19:30:12  vakatov
 * Allowed case-insensitive comparison for StartsWith() and EndsWith()
 *
 * Revision 1.38  2000/04/17 04:14:20  vakatov
 * NStr::  extended Compare(), and allow case-insensitive string comparison
 * NStr::  added ToLower() and ToUpper()
 *
 * Revision 1.37  2000/04/04 22:28:06  vakatov
 * NStr::  added conversions for "long"
 *
 * Revision 1.36  2000/02/17 20:00:24  vasilche
 * Added EResetVariant enum for serialization package.
 *
 * Revision 1.35  2000/01/20 16:24:20  vakatov
 * Kludging around the "NcbiEmptyString" to ensure its initialization when
 * it is used by the constructor of a statically allocated object
 * (I believe that it is actually just another Sun WorkShop compiler "feature")
 *
 * Revision 1.34  1999/12/28 19:04:22  vakatov
 * #HAVE_NO_MINMAX_TEMPLATE
 *
 * Revision 1.33  1999/12/28 18:55:25  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.32  1999/12/17 19:04:06  vasilche
 * NcbiEmptyString made extern.
 *
 * Revision 1.31  1999/12/03 21:36:45  vasilche
 * Added forward decaration of CEnumeratedTypeValues
 *
 * Revision 1.30  1999/11/26 18:45:08  golikov
 * NStr::Replace added
 *
 * Revision 1.29  1999/11/17 22:05:02  vakatov
 * [!HAVE_STRDUP]  Emulate "strdup()" -- it's missing on some platforms
 *
 * Revision 1.28  1999/10/26 18:10:24  vakatov
 * [auto_ptr] -- simpler and more standard
 *
 * Revision 1.27  1999/09/29 22:22:37  vakatov
 * [auto_ptr] Use "mutable" rather than "static_cast" on m_Owns; fixed a
 *            double "delete" bug in reset().
 *
 * Revision 1.26  1999/09/14 18:49:40  vasilche
 * Added forward declaration of CTypeInfo class.
 *
 * Revision 1.25  1999/07/08 14:44:52  vakatov
 * Tiny fix in EndsWith()
 *
 * Revision 1.24  1999/07/06 15:21:04  vakatov
 * + NStr::TruncateSpaces(const string& str, ETrunc where=eTrunc_Both)
 *
 * Revision 1.23  1999/06/21 15:59:40  vakatov
 * [auto_ptr] -- closer to standard:  added an ownership and
 * initialization/assignment with "auto_ptr<>&", made "release()" be "const"
 *
 * Revision 1.22  1999/06/15 20:50:03  vakatov
 * NStr::  +BoolToString, +StringToBool
 *
 * Revision 1.21  1999/05/28 20:12:29  vakatov
 * [HAVE_NO_AUTO_PTR]  Prohibit "operator=" in the home-made "auto_ptr::"
 *
 * Revision 1.20  1999/04/16 17:45:31  vakatov
 * [MSVC++] Replace the <windef.h>'s min/max macros by the hand-made templates.
 *
 * Revision 1.19  1999/04/15 21:56:47  vakatov
 * Introduced NcbiMin/NcbiMax to workaround some portability issues with
 * the standard "min/max"
 *
 * Revision 1.18  1999/04/14 21:20:31  vakatov
 * Dont use "snprintf()" as it is not quite portable yet
 *
 * Revision 1.17  1999/04/14 19:46:01  vakatov
 * Fixed for the features:
 *    { NCBI_OBSOLETE_STR_COMPARE, HAVE_NO_AUTO_PTR, HAVE_NO_SNPRINTF }
 *
 * Revision 1.16  1999/04/09 19:51:36  sandomir
 * minor changes in NStr::StringToXXX - base added
 *
 * Revision 1.15  1999/01/21 16:18:04  sandomir
 * minor changes due to NStr namespace to contain string utility functions
 *
 * Revision 1.14  1999/01/11 22:05:45  vasilche
 * Fixed CHTML_font size.
 * Added CHTML_image input element.
 *
 * Revision 1.13  1998/12/28 17:56:29  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.12  1998/12/21 17:19:36  sandomir
 * VC++ fixes in ncbistd; minor fixes in Resource
 *
 * Revision 1.11  1998/12/17 21:50:43  sandomir
 * CNCBINode fixed in Resource; case insensitive string comparison predicate
 * added
 *
 * Revision 1.10  1998/12/15 17:38:16  vasilche
 * Added conversion functions string <> int.
 *
 * Revision 1.9  1998/12/04 23:36:30  vakatov
 * + NcbiEmptyCStr and NcbiEmptyString (const)
 *
 * Revision 1.8  1998/11/06 22:42:38  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * ==========================================================================
 */

#endif  /* CORELIB___NCBIMISC__HPP */
