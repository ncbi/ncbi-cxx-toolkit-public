#ifndef NETCACHE_NC_UTILS__HPP
#define NETCACHE_NC_UTILS__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 *   Utility classes that now are used only in NetCache but can be used
 *   anywhere else.
 */

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE


/// Stream-like class to accumulate all in one string without actual
/// streams-related overhead. Can be automatically converted to string or
/// CTempString.
class CQuickStrStream
{
public:
    /// Clear the accumulated value and start from the beginning
    void Clear(void);

    /// Streaming operator for all supported data types
    CQuickStrStream& operator<<(const string&          str);
    CQuickStrStream& operator<<(const char*            str);
    CQuickStrStream& operator<<(CTempString            str);
    CQuickStrStream& operator<<(int                    i);
    CQuickStrStream& operator<<(unsigned int           ui);
    CQuickStrStream& operator<<(long                   l);
    CQuickStrStream& operator<<(unsigned long          ul);
#if !NCBI_INT8_IS_LONG
    CQuickStrStream& operator<<(Int8                   i64);
    CQuickStrStream& operator<<(Uint8                  ui64);
#endif
    CQuickStrStream& operator<<(double                 d);
    CQuickStrStream& operator<<(const CTime&           ctime);
    CQuickStrStream& operator<<(const CQuickStrStream& str);

    /// Automatic conversion to string
    operator string(void) const;
    /// Automatic conversion to CTempString
    operator CTempString(void) const;

private:
    /// Accumulated data
    string m_Data;
};

/// Additional overloaded operator to allow automatic transformation of
/// CQuickStrStream to string when outputting to any standard stream.
CNcbiOstream& operator<<(CNcbiOstream& stream, const CQuickStrStream& str);


/// Stream-like class to help unify printing some text messages to diagnostics
/// and to any iostream. Class accumulates line of text end then flushes it
/// to diagnostics by LOG_POST or to iostream by operator<<. End of line is
/// recognized only by streaming std::endl into this class. Other text is not
/// analyzed for characters '\n' or alike. In case of printing to diagnostics
/// all text is printed under the same request number and it's assumed that
/// no other output gets to diagnostics between object creation and deletion.
/// Otherwise other ouput will interfere with output from this object. Also
/// it's assumed that object is used from the same thread where it's created
/// and deleted.
class CPrintTextProxy
{
public:
    /// Type of media to print to
    enum EPrintOutput {
        ePrintLog,     ///< Print to diagnostics
        ePrintStream   ///< Print to iostream
    };

    /// Create object and set output type and stream if necessary.
    /// If stream == NULL then output type is set to ePrintLog automatically.
    CPrintTextProxy(EPrintOutput output, CNcbiIostream* stream = NULL);

    ~CPrintTextProxy(void);

    /// Universal streaming operator
    template <class T>
    CPrintTextProxy& operator<< (T x);


    typedef CNcbiOstream& (*TEndlType)(CNcbiOstream&);

    /// Special streaming operator for std::endl
    CPrintTextProxy& operator<< (TEndlType endl_func);

private:
    CPrintTextProxy(const CPrintTextProxy&);
    CPrintTextProxy& operator= (const CPrintTextProxy&);


    /// Type of media to print to
    EPrintOutput          m_PrintMode;
    /// Stream to direct the output to
    CNcbiIostream*        m_PrintStream;
    /// Diagnostics context that was set before this object was created
    CRef<CRequestContext> m_OldContext;
    /// Accumulator of the current output line
    CQuickStrStream       m_LineStream;
};


/// Synonyms for TLS-related functions on different platforms
#ifdef NCBI_OS_MSWIN
   typedef DWORD          TNCTlsKey;
#  define  NCTlsGet       TlsGetValue
#  define  NCTlsSet       TlsSetValue
#  define  NCTlsFree      TlsFree
/// Special define for creation of the TLS key. Parameter 'key' must be a
/// variable.
#  define  NCTlsCreate(key, destr)  key = TlsAlloc()
#else
   typedef pthread_key_t  TNCTlsKey;
#  define  NCTlsGet       pthread_getspecific
#  define  NCTlsSet       pthread_setspecific
#  define  NCTlsFree      pthread_key_delete
/// Special define for creation of the TLS key. Parameter 'key' must be a
/// variable.
#  define  NCTlsCreate(key, destr)  pthread_key_create(&key, destr)
#endif

/// Type for index of the current thread.
/// @sa g_GetNCThreadIndex
typedef unsigned int  TNCThreadIndex;


/// Base class for the object specific for each thread.
/// CTls<> is not suitable here because it has the following "bad" features:
/// * It cannot be destroyed until the application is finished.
/// * It cannot be used from memory manager because it uses operator new
///   intensively during initialization.
/// CNCTlsObject can be used only as a base class for some another one like
/// this:
/// class CClassName : public CNCTlsObject<CClassName, ...>
/// Derived class will be responsible for actual creation and deletion of
/// objects for each thread. It should implement the following methods:
///
/// ObjType* CreateTlsObject(void);
/// static void DeleteTlsObject(void* obj_ptr);
///
/// First will be used to create object when new thread is created and the
/// latter will be used on Unix systems to delete object when thread has
/// finished its execution. Windows doesn't provide such functionality and
/// thus DeleteTlsObject() will never be called there.
///
/// @param Derived                                      
///   Class derived from this base class. 
/// @param ObjType
///   Type of object pointer to which will be stored in TLS.
template <class Derived, class ObjType>
class CNCTlsObject
{
public:
    /// Empty constructor - necessary to avoid problems at least in ICC
    CNCTlsObject(void);
    /// Initialize TLS key
    void     Initialize(void);
    /// Free TLS key, make it available for other TLS objects
    void     Finalize  (void);
    /// Get specific object for the current thread
    ObjType* GetObjPtr (void);

private:
    CNCTlsObject(const CNCTlsObject&);
    CNCTlsObject& operator= (const CNCTlsObject&);

    /// TLS key used to store specific object for each thread
    TNCTlsKey m_ObjKey;
};


/// Special equivalent of SSystemFastMutex that exposes necessary method to
/// public.
struct SNCFastMutex : public SSystemFastMutex
{
    using SSystemFastMutex::InitializeDynamic;
};


/// Initialize system that will acquire thread indexes for each thread.
/// This function must be called before g_GetNCThreadIndex() is used.
void
g_InitNCThreadIndexes(void);
/// Get index of the current thread. All threads are indexed with incrementing
/// numbers starting from 1. Thread is indexed only when this function is
/// called and if it wasn't indexed yet.
TNCThreadIndex
g_GetNCThreadIndex(void);



/// Utility function to safely do division even if divisor is 0
template <class TLeft, class TRight>
TLeft g_SafeDiv(TLeft left, TRight right)
{
    return right == 0? 0: left / right;
}

/// Get integer part of the logarithm with base 2.
/// In other words function returns index of the greatest bit set when bits
/// are indexed from lowest to highest starting with 0 (e.g. for binary number
/// 10010001 it will return 7).
inline unsigned int
g_GetLogBase2(size_t value)
{
    unsigned int result = 0;
#if SIZEOF_SIZE_T > 4
    if (value > 0xFFFFFFFF) {
        value >>= 32;
        result += 32;
    }
#endif
    if (value > 0xFFFF) {
        value >>= 16;
        result += 16;
    }
    if (value > 0xFF) {
        value >>= 8;
        result += 8;
    }
    if (value > 0xF) {
        value >>= 4;
        result += 4;
    }
    if (value > 0x3) {
        value >>= 2;
        result += 2;
    }
    if (value > 0x1)
        ++result;
    return result;
}

/// Get number of least bit set.
/// For instance for the binary number 10010100 it will return 2.
inline unsigned int
g_GetLeastSetBit(size_t value)
{
    value ^= value - 1;
    value >>= 1;
    ++value;
    return g_GetLogBase2(value);
}

/// Get number of bits set.
/// Implementation uses minimum number of operations without using additional
/// variables or memory. Trick was taken from
/// http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel.
inline unsigned int
g_GetBitsCnt(size_t value)
{
#if SIZEOF_SIZE_T == 8
    value = value - ((value >> 1) & 0x5555555555555555);
    value = (value & 0x3333333333333333) + ((value >> 2) & 0x3333333333333333);
    value = (value + (value >> 4)) & 0x0F0F0F0F0F0F0F0F;
    value = (value * 0x0101010101010101) >> 56;
    return static_cast<unsigned int>(value);
#elif SIZEOF_SIZE_T == 4
    value = value - ((value >> 1) & 0x55555555);
    value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
    value = (value + (value >> 4)) & 0x0F0F0F0F;
    value = (value * 0x01010101) >> 24;
    return value;
#else
# error "Cannot compile with this size_t"
#endif
}


//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline void
CQuickStrStream::Clear(void)
{
    m_Data.clear();
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const string& str)
{
    m_Data += str;
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const char* str)
{
    m_Data += str;
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(CTempString str)
{
    m_Data.append(str.data(), str.size());
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(int i)
{
    m_Data += NStr::IntToString(i);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(unsigned int ui)
{
    m_Data += NStr::UIntToString(ui);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(long l)
{
    m_Data += NStr::Int8ToString(l);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(unsigned long ul)
{
    m_Data += NStr::UInt8ToString(ul);
    return *this;
}

#if !NCBI_INT8_IS_LONG

inline CQuickStrStream&
CQuickStrStream::operator<<(Int8 i64)
{
    m_Data += NStr::Int8ToString(i64);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(Uint8 ui64)
{
    m_Data += NStr::UInt8ToString(ui64);
    return *this;
}

#endif

inline CQuickStrStream&
CQuickStrStream::operator<<(double d)
{
    m_Data += NStr::DoubleToString(d);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const CTime& ctime)
{
    m_Data += ctime.AsString();
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const CQuickStrStream& str)
{
    m_Data += str.m_Data;
    return *this;
}

inline
CQuickStrStream::operator string(void) const
{
    return m_Data;
}

inline
CQuickStrStream::operator CTempString(void) const
{
    return m_Data;
}


inline CNcbiOstream&
operator<<(CNcbiOstream& stream, const CQuickStrStream& str)
{
    stream << string(str);
    return stream;
}



inline
CPrintTextProxy::CPrintTextProxy(EPrintOutput   output,
                                 CNcbiIostream* stream /* = NULL */)
    : m_PrintMode(stream? output: ePrintLog),
      m_PrintStream(stream),
      m_OldContext(NULL)
{
    if (m_PrintMode == ePrintLog) {
        m_OldContext.Reset(&CDiagContext::GetRequestContext());
        AutoPtr<CRequestContext> new_context = new CRequestContext();
        new_context->SetRequestID();
        CDiagContext::SetRequestContext(new_context.release());
    }
}

inline
CPrintTextProxy::~CPrintTextProxy(void)
{
    if (m_PrintMode == ePrintLog) {
        CDiagContext::SetRequestContext(m_OldContext);
    }
}

template <class T>
inline CPrintTextProxy&
CPrintTextProxy::operator<< (T x)
{
    m_LineStream << x;
    return *this;
}

inline CPrintTextProxy&
CPrintTextProxy::operator<< (TEndlType)
{
    switch (m_PrintMode) {
    case ePrintLog:
        LOG_POST(string(m_LineStream));
        break;
    case ePrintStream:
        (*m_PrintStream) << string(m_LineStream) << endl;
        break;
    }
    m_LineStream.Clear();

    return *this;
}


template <class Derived, class ObjType>
inline
CNCTlsObject<Derived, ObjType>::CNCTlsObject(void)
{}

template <class Derived, class ObjType>
inline void
CNCTlsObject<Derived, ObjType>::Initialize(void)
{
    NCTlsCreate(m_ObjKey, &Derived::DeleteTlsObject);
}

template <class Derived, class ObjType>
inline void
CNCTlsObject<Derived, ObjType>::Finalize(void)
{
    NCTlsFree(m_ObjKey);
}

template <class Derived, class ObjType>
inline ObjType*
CNCTlsObject<Derived, ObjType>::GetObjPtr(void)
{
    ObjType* object = static_cast<ObjType*>(NCTlsGet(m_ObjKey));
    if (!object) {
        object = static_cast<Derived*>(this)->CreateTlsObject();
        NCTlsSet(m_ObjKey, object);
    }
    return object;
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_UTILS__HPP */
