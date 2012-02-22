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
#include <corelib/ncbithr.hpp>
#include <util/thread_pool.hpp>


#ifdef _DEBUG
#  undef _ASSERT
#  define _ASSERT(x)  if (x) ; else abort()
#  undef _VERIFY
#  define _VERIFY(x)  if (x) ; else abort()
#endif

#ifdef NCBI_COMPILER_GCC
# define ATTR_PACKED    __attribute__ ((packed))
# define ATTR_ALIGNED_8 __attribute__ ((aligned(8)))
#else
# define ATTR_PACKED
# define ATTR_ALIGNED_8
#endif



BEGIN_NCBI_SCOPE


enum {
    /// Maximum number of threads when NetCache will most effectively avoid
    /// contention between threads for some internal data structures.
    /// This constant was made different from kNCMMMaxThreadsCnt (though with
    /// similar meaning) because in memory manager changing in number of threads
    /// can result in pretty significant change in memory consumption when in all
    /// other parts of NetCache it's not so significant. So this constant is much
    /// more flexible for changes than kNCMMMaxThreadsCnt.
    kNCMaxThreadsCnt = 100,
    kNCTimeTicksInSec = 1000000
};


enum ENCPeerFailure {
    ePeerActionOK,
    ePeerNeedAbort,
    ePeerBadNetwork
};

/// Type of access to NetCache blob
enum ENCAccessType {
    eNCRead,        ///< Read meta information only
    eNCReadData,    ///< Read blob data
    eNCCreate,      ///< Create blob or re-write its contents
    eNCCopyCreate,
    eNCGCDelete
};


/*
class CSpinRWLock;

typedef CGuard< CSpinRWLock,
                SSimpleReadLock  <CSpinRWLock>,
                SSimpleReadUnlock<CSpinRWLock> >  CSpinReadGuard;
typedef CGuard< CSpinRWLock,
                SSimpleWriteLock  <CSpinRWLock>,
                SSimpleWriteUnlock<CSpinRWLock> > CSpinWriteGuard;

///
class CSpinRWLock
{
public:
    typedef CSpinReadGuard   TReadLockGuard;
    typedef CSpinWriteGuard  TWriteLockGuard;

    CSpinRWLock(void);
    ~CSpinRWLock(void);

    /// Acquire read lock
    void ReadLock(void);
    /// Release read lock
    void ReadUnlock(void);

    /// Acquire write lock
    void WriteLock(void);
    /// Release write lock
    void WriteUnlock(void);

private:
    CSpinRWLock(const CSpinRWLock&);
    CSpinRWLock& operator= (const CSpinRWLock&);

    enum {
        /// Number in lock count showing that read lock is acquired
        kReadLockValue  = 0x000001,
        /// Number in lock count showing that write lock is acquired
        kWriteLockValue = 0x100000
    };

    /// Number of read locks acquired or value of kWriteLockValue if write
    /// lock was acquired
    CAtomicCounter m_LockCount;
};
*/

#ifdef NCBI_OS_LINUX
typedef struct timespec TTimeSpec;
#else
typedef void* TTimeSpec;
#endif


class CFutex
{
public:
    CFutex(void);

    int GetValue(void);
    int AddValue(int cnt_to_add);

    void SetValueNonAtomic(int new_value);

    enum EWaitResult {
        eWaitWokenUp,
        eValueChanged,
        eTimedOut
    };

    EWaitResult WaitValueChange(int old_value);
    int WakeUpWaiters(int cnt_to_wake);

private:
    CFutex(const CFutex&);
    CFutex& operator= (const CFutex&);

    volatile int m_Value;
};


class CMiniMutex
{
public:
    CMiniMutex(void);
    ~CMiniMutex(void);

    void Lock(void);
    void Unlock(void);

private:
    /// Prohibit copying of the object
    CMiniMutex(const CMiniMutex&);
    CMiniMutex& operator= (const CMiniMutex&);


    CFutex m_Futex;
};


template <class C, class Locker = typename CLockerTraits<C>::TLockerType>
class CNCRef : public CRef<C, Locker>
{
    typedef CNCRef<C, Locker> TThisType;
    typedef CRef<C, Locker> TParent;
    typedef typename TParent::TObjectType TObjectType;
    typedef typename TParent::locker_type locker_type;

public:
    CNCRef(void)
    {}
    CNCRef(ENull /*null*/)
    {}
    explicit
    CNCRef(TObjectType* ptr)
        : TParent(ptr)
    {}
    CNCRef(TObjectType* ptr, const locker_type& locker_value)
        : TParent(ptr, locker_value)
    {}
    CNCRef(const TThisType& ref)
        : TParent(ref)
    {}

    TThisType& operator= (const TThisType& ref)
    {
        TParent::operator= (ref);
        return *this;
    }
    TThisType& operator= (TObjectType* ptr)
    {
        TParent::operator= (ptr);
        return *this;
    }
    TThisType& operator= (ENull /*null*/)
    {
        TParent::operator= (null);
        return *this;
    }

    TObjectType& operator* (void)
    {
        return *TParent::GetPointerOrNull();
    }
    TObjectType* operator-> (void)
    {
        return TParent::GetPointerOrNull();
    }
    const TObjectType& operator* (void) const
    {
        return *TParent::GetPointerOrNull();
    }
    const TObjectType* operator-> (void) const
    {
        return TParent::GetPointerOrNull();
    }
};


template<class C>
inline
CNCRef<C> NCRef(C* object)
{
    return CNCRef<C>(object);
}


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


///
#define INFO_POST(message)                         \
    ( NCBI_NS_NCBI::CNcbiDiag(                     \
      eDiag_Error,                                 \
      eDPF_Log | eDPF_IsMessage).GetRef()          \
      << message                                   \
      << NCBI_NS_NCBI::Endm )


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
    CNCRef<CRequestContext> m_OldContext;
    /// Accumulator of the current output line
    CQuickStrStream       m_LineStream;
};


/// Class representing one statistical value.
/// Object collects set of values and can return number of values in set,
/// sum of all values, maximum value and average of all values.
template <class T>
class CNCStatFigure
{
public:
    /// Empty constructor, all initialization should be made in Initialize()
    /// because in memory manager static objects are used before constructors
    /// are called.
    CNCStatFigure   (void);
    /// Initialize all data members
    void Initialize (void);

    /// Add next value into the set.
    void  AddValue  (T value);

    /// Get number of values in the set.
    Uint8 GetCount  (void) const;
    /// Get sum of all values in the set.
    T     GetSum    (void) const;
    /// Get maximum value in the set.
    T     GetMaximum(void) const;
    /// Get average of all values in the set.
    T     GetAverage(void) const;

    /// Add all values from another set.
    void  AddValues (const CNCStatFigure<T>& other);

private:
    /// Sum of all values collected.
    T      m_ValuesSum;
    /// Number of all values collected.
    Uint8  m_ValuesCount;
    /// Maximum value among collected.
    T      m_ValuesMax;
};


/// Thread running background task.
/// All this thread does is calls given functor and then exits.
///
/// @param Functor
///   Type of functor that will be executed in the thread. The only
///   requirements for functor type are:
///   - it should be copyable;
///   - it should execute code when applied with function call parenthesis.
template <class Functor>
class CNCBGThread : public CThread
{
public:
    /// Create new background thread executing given functor.
    /// Thread is not started automatically - you have to call Run() for that.
    CNCBGThread(const Functor& functor);
    virtual ~CNCBGThread(void);

protected:
    virtual void* Main(void);

private:
    /// The functor to be executed in the thread
    Functor m_Functor;
};

/// Special wrapper for object's method that can be used as functor, e.g. in
/// CNCBGThread.
///
/// @param Executor
///   Class containing method with no parameters and no return value which
///   will be called in this wrapper.
/// @sa CNCBGThread
template <class Executor>
class CNCMethodWrap
{
public:
    typedef void (Executor::*TMethod)(void);

    /// Create wrapper for given method of given executor object.
    CNCMethodWrap(Executor* executor, TMethod method);
    /// Functor's call redirection.
    void operator() (void);

private:
    /// Executor object
    Executor* m_Executor;
    /// Method to run in the executor object
    TMethod   m_Method;
};

/// Convenient function creating new background thread for given functor.
/// Convenience is in automatic instantiation based on parameter value so that
/// you don't need to state template parameter for CNCBGThread.
template <class Functor>
CNCBGThread<Functor>*
NewBGThread(Functor functor);

/// Convenient function creating new background thread for given executor
/// object and given method in it.
/// Convenience is in automatic instantiation based on parameter value so that
/// you don't need to state template parameters for CNCBGThread and
/// CNCMethodWrap.
template <class Executor>
CNCBGThread< CNCMethodWrap<Executor> >*
NewBGThread(Executor* executor, void (Executor::*method) (void));


/// Type to use for bit masks
typedef size_t  TNCBitMaskElem;
/// Number of bits in one element of type TNCBitMaskElem
static const unsigned int kNCMaskElemSize = SIZEOF_SIZE_T * 8;

/// Base class for bit masks of any size.
/// Bit mask is a set of bits with indexes starting with 0.
/// General template class implements functionality suitable for working with
/// bit masks of any size although for special cases of 1 or 2 elements it can
/// be optimized (see template's specializations) and for now only
/// 1- and 2-elements specializations are used in NetCache.
///
/// @param num_elems
///   Number of elements of type TNCBitMaskElem that class should contain.
template <int num_elems>
class CNCBitMaskBase
{
public:
    /// Empty constructor.
    /// All initialization is made via Initialize() method.
    CNCBitMaskBase            (void);
    /// Initialize bit mask with all zeros.
    void         Initialize   (void);
    /// Check if bit with given number is set.
    bool         IsBitSet     (unsigned int bit_num);
    /// Invert value of bits_cnt bits starting from start_bit.
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    /// Get number of bits set in the mask
    unsigned int GetCntSet    (void);
    /// Get index of lowest set bit with index greater or equal to bit_num.
    /// If there's no such bit in the mask then return -1.
    int          GetClosestSet(unsigned int bit_num);

private:
    /// Calculate coordinates of the bit with index bit_index (calculate index
    /// of byte it located in and index of bit in that byte.
    void x_CalcCoords(unsigned int  bit_index,
                      unsigned int& byte_num,
                      unsigned int& bit_num);

    /// Bit mask itself
    TNCBitMaskElem m_Mask[num_elems];
};

/// Optimized specialization of bit mask that fits entirely in 1 element of
/// type TNCBitMaskElem.
template <>
class CNCBitMaskBase<1>
{
public:
    CNCBitMaskBase            (void);
    void         Initialize   (void);
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);

private:
    TNCBitMaskElem m_Mask;
};

/// Optimized specialization of bit mask that fits entirely in 2 elements of
/// type TNCBitMaskElem.
template <>
class CNCBitMaskBase<2>
{
public:
    CNCBitMaskBase            (void);
    void         Initialize   (void);
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);

private:
    TNCBitMaskElem m_Mask[2];
};

/// Class for working with bit masks containing given number of bits.
template <int num_bits>
class CNCBitMask : public CNCBitMaskBase<(num_bits + kNCMaskElemSize - 1)
                                         / kNCMaskElemSize>
{
    // Expression here should be a copy of expression in declaration.
    typedef CNCBitMaskBase<(num_bits + kNCMaskElemSize - 1)
                           / kNCMaskElemSize>                  TBase;

public:
    /// Empty constructor.
    /// All initialization happens in Initialize().
    CNCBitMask                (void);
    /// Initialize bit mask.
    ///
    /// @param init_value
    ///   Value for initialization of all bits. If this value is 0 then all
    ///   bits are initialized with 0, otherwise all bits are initialized
    ///   with 1.
    void         Initialize   (unsigned int init_value);

    // Redirections to methods of base class performing additional checks in
    // Debug mode.
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);
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


///
enum ENCBlockingOpResult {
    eNCSuccessNoBlock,
    eNCWouldBlock
};


class CStdPoolOfThreads;

///
struct INCBlockedOpListener
{
    ///
    static void BindToThreadPool(CStdPoolOfThreads* pool);

    ///
    virtual void OnBlockedOpFinish(void) = 0;
    ///
    virtual ~INCBlockedOpListener(void);

    ///
    void Notify(void);
};


///
enum ENCLongOpState {
    eNCOpNotDone,
    eNCOpInProgress,
    eNCOpCompleted
};


struct SNCBlockedOpListeners;

///
class CNCLongOpTrigger
{
public:
    ///
    CNCLongOpTrigger(void);
    ~CNCLongOpTrigger(void);
    ///
    void Reset(void);

    ///
    ENCLongOpState GetState(void);
    ///
    void SetState(ENCLongOpState state);
    ///
    ENCBlockingOpResult StartWorking(INCBlockedOpListener* listener);
    ///
    void OperationCompleted(void);

private:
    CNCLongOpTrigger(const CNCLongOpTrigger&);
    CNCLongOpTrigger& operator= (const CNCLongOpTrigger&);

    ///
    CSpinLock               m_ObjLock;
    ///
    ENCLongOpState volatile m_State;
    ///
    SNCBlockedOpListeners*  m_Listeners;
};


///
class CNCLongOpGuard
{
public:
    ///
    CNCLongOpGuard(CNCLongOpTrigger& trigger);
    ~CNCLongOpGuard(void);

    ///
    ENCBlockingOpResult Start(INCBlockedOpListener* listener);
    ///
    bool IsCompleted(void);

private:
    CNCLongOpGuard(const CNCLongOpGuard&);
    CNCLongOpGuard& operator= (const CNCLongOpGuard&);

    ///
    CNCLongOpTrigger* m_Trigger;
};


///
template <class        ElemType,
          class        FinalDeleter,
          unsigned int DeleteDelay,
          unsigned int ElemsPerStorage>
class CNCDeferredDeleter {
public:
    CNCDeferredDeleter(const FinalDeleter& deleter);
    ~CNCDeferredDeleter(void);

    ///
    void AddElement(const ElemType& elem);
    ///
    void HeartBeat(void);

private:
    CNCDeferredDeleter(const CNCDeferredDeleter&);
    CNCDeferredDeleter& operator= (const CNCDeferredDeleter&);

    ///
    enum {
        kDeleteDelay     = DeleteDelay,
        kElemsPerStorage = ElemsPerStorage
    };

    ///
    class CStorageForDelete
    {
    public:
        ///
        CStorageForDelete(void);
        ~CStorageForDelete(void);

        ///
        void AddElement(const ElemType& elem);
        ///
        void Clean(const FinalDeleter& deleter);

    private:
        ///
        CStorageForDelete(const CStorageForDelete&);
        CStorageForDelete& operator= (const CStorageForDelete&);

        ///
        void x_CreateNextAndAdd(const ElemType& elem);


        ElemType                    m_Elems[kElemsPerStorage];
        CAtomicCounter              m_CurIdx;
        CSpinLock                   m_ObjLock;
        CStorageForDelete* volatile m_Next;
    };

    ///
    CStorageForDelete*          m_Stores[kDeleteDelay];
    ///
    CStorageForDelete* volatile m_CurStore;
    ///
    FinalDeleter                m_FinalDeleter;
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


string g_ToSizeStr(Uint8 size);

inline string
g_ToSmartStr(Uint8 num)
{
    return NStr::UInt8ToString(num, NStr::fWithCommas);
}


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
g_GetLogBase2(Uint8 value)
{
    unsigned int result = 0;
    if (value > 0xFFFFFFFF) {
        value >>= 32;
        result += 32;
    }
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
/*
inline
CSpinRWLock::CSpinRWLock(void)
{
    m_LockCount.Set(0);
}

inline
CSpinRWLock::~CSpinRWLock(void)
{
    _ASSERT(m_LockCount.Get() == 0);
}
*/

template <class T>
inline T
AtomicAdd(T volatile& var, T add_value)
{
#ifdef NCBI_COMPILER_GCC
    return __sync_add_and_fetch(&var, add_value);
#else
    return var;
#endif
}

template <class T1, class T2>
inline T1
AtomicAdd(T1 volatile& var, T2 add_value)
{
    return AtomicAdd(var, (T1)add_value);
}

template <class T>
inline T
AtomicSub(T volatile& var, T sub_value)
{
#ifdef NCBI_COMPILER_GCC
    return __sync_sub_and_fetch(&var, sub_value);
#else
    return var;
#endif
}

template <class T1, class T2>
inline T1
AtomicSub(T1 volatile& var, T2 sub_value)
{
    return AtomicSub(var, (T1)sub_value);
}


inline
CFutex::CFutex(void)
{}

inline int
CFutex::GetValue(void)
{
    return m_Value;
}

inline int
CFutex::AddValue(int cnt_to_add)
{
    return AtomicAdd(m_Value, cnt_to_add);
}

inline void
CFutex::SetValueNonAtomic(int new_value)
{
    m_Value = new_value;
}


inline
CMiniMutex::CMiniMutex(void)
{
    m_Futex.SetValueNonAtomic(0);
}

inline
CMiniMutex::~CMiniMutex(void)
{
    _ASSERT(m_Futex.GetValue() == 0);
}


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
    const string str(m_LineStream);
    switch (m_PrintMode) {
    case ePrintLog:
        if (!str.empty())
            INFO_POST(str);
        break;
    case ePrintStream:
        (*m_PrintStream) << str << endl;
        break;
    }
    m_LineStream.Clear();

    return *this;
}


template <class T>
inline
CNCStatFigure<T>::CNCStatFigure(void)
{}

template <class T>
inline void
CNCStatFigure<T>::Initialize(void)
{
    m_ValuesSum   = 0;
    m_ValuesCount = 0;
    m_ValuesMax   = 0;
}

template <class T>
inline void
CNCStatFigure<T>::AddValue(T value)
{
    m_ValuesSum += value;
    ++m_ValuesCount;
    m_ValuesMax = max(m_ValuesMax, value);
}

template <class T>
inline void
CNCStatFigure<T>::AddValues(const CNCStatFigure<T>& other)
{
    m_ValuesSum   += other.m_ValuesSum;
    m_ValuesCount += other.m_ValuesCount;
    m_ValuesMax    = max(other.m_ValuesMax, m_ValuesMax);
}

template <class T>
inline Uint8
CNCStatFigure<T>::GetCount(void) const
{
    return m_ValuesCount;
}

template <class T>
inline T
CNCStatFigure<T>::GetSum(void) const
{
    return m_ValuesSum;
}

template <class T>
inline T
CNCStatFigure<T>::GetMaximum(void) const
{
    return m_ValuesMax;
}

template <class T>
inline T
CNCStatFigure<T>::GetAverage(void) const
{
    return m_ValuesCount == 0? 0: T(m_ValuesSum / m_ValuesCount);
}


template <class Functor>
inline
CNCBGThread<Functor>::CNCBGThread(const Functor& functor)
    : m_Functor(functor)
{}

template <class Functor>
inline
CNCBGThread<Functor>::~CNCBGThread(void)
{}

template <class Functor>
inline void*
CNCBGThread<Functor>::Main(void)
{
    m_Functor();
    return NULL;
}


template <class Executor>
inline
CNCMethodWrap<Executor>::CNCMethodWrap(Executor* executor, TMethod method)
    : m_Executor(executor),
      m_Method(method)
{}

template <class Executor>
inline void
CNCMethodWrap<Executor>::operator() (void)
{
    (m_Executor->*m_Method)();
}


template <class Functor>
inline CNCBGThread<Functor>*
NewBGThread(Functor functor)
{
    return new CNCBGThread<Functor>(functor);
}

template <class Executor>
inline CNCBGThread< CNCMethodWrap<Executor> >*
NewBGThread(Executor* executor, void (Executor::*method)(void))
{
    return new CNCBGThread< CNCMethodWrap<Executor> >
                                  (CNCMethodWrap<Executor>(executor, method));
}

    
template <int num_elems>
inline
CNCBitMaskBase<num_elems>::CNCBitMaskBase(void)
{}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::Initialize(void)
{
    memset(m_Mask, 0, sizeof(m_Mask));
}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::x_CalcCoords(unsigned int  bit_index,
                                        unsigned int& byte_num,
                                        unsigned int& bit_num)
{
    byte_num = bit_index / kNCMaskElemSize;
    bit_num  = bit_index - byte_num * kNCMaskElemSize;
}

template <int num_elems>
inline bool
CNCBitMaskBase<num_elems>::IsBitSet(unsigned int bit_num)
{
    unsigned int byte_num, bit_index;
    x_CalcCoords(bit_num, byte_num, bit_index);
    return (m_Mask[byte_num] & (TNCBitMaskElem(1) << bit_index)) != 0;
}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::InvertBits(unsigned int start_bit,
                                      unsigned int bits_cnt)
{
    unsigned int byte_num, bit_num;
    x_CalcCoords(start_bit, byte_num, bit_num);
    do {
        TNCBitMaskElem inv_mask;
        if (bits_cnt >= kNCMaskElemSize)
            inv_mask = TNCBitMaskElem(-1);
        else
            inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[byte_num] ^= inv_mask << bit_num;
        unsigned int num_inved = kNCMaskElemSize - bit_num;
        bits_cnt = (bits_cnt > num_inved? bits_cnt - num_inved: 0);
        ++byte_num;
        bit_num = 0;
    }
    while (bits_cnt != 0);
}

template <int num_elems>
inline unsigned int
CNCBitMaskBase<num_elems>::GetCntSet(void)
{
    unsigned int cnt_set = 0;
    for (unsigned int i = 0; i < num_elems; ++i) {
        cnt_set += g_GetBitsCnt(m_Mask[i]);
    }
    return cnt_set;
}

template <int num_elems>
inline int
CNCBitMaskBase<num_elems>::GetClosestSet(unsigned int bit_num)
{
    unsigned int byte_num, bit_index;
    x_CalcCoords(bit_num, byte_num, bit_index);
    TNCBitMaskElem mask = 0;
    if (bit_index < kNCMaskElemSize - 1) {
        mask = m_Mask[byte_num] & ~((TNCBitMaskElem(1) << bit_index) - 1);
    }
    if (mask == 0) {
        ++byte_num;
        while (byte_num < kNCMaskElemSize  &&  (mask = m_Mask[byte_num]) == 0)
        {
            ++byte_num;
        }
    }
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask) + byte_num * kNCMaskElemSize;
    }
}


inline
CNCBitMaskBase<1>::CNCBitMaskBase(void)
{}

inline void
CNCBitMaskBase<1>::Initialize(void)
{
    m_Mask = 0;
}

inline bool
CNCBitMaskBase<1>::IsBitSet(unsigned int bit_num)
{
    return (m_Mask & (TNCBitMaskElem(1) << bit_num)) != 0;
}

inline void
CNCBitMaskBase<1>::InvertBits(unsigned int start_bit,
                              unsigned int bits_cnt)
{
    TNCBitMaskElem inv_mask = (bits_cnt == kNCMaskElemSize? 0:
                                           (TNCBitMaskElem(1) << bits_cnt));
    --inv_mask;
    m_Mask ^= inv_mask << start_bit;
}

inline unsigned int
CNCBitMaskBase<1>::GetCntSet(void)
{
    return g_GetBitsCnt(m_Mask);
}

inline int
CNCBitMaskBase<1>::GetClosestSet(unsigned int bit_num)
{
    TNCBitMaskElem mask = m_Mask & ~((TNCBitMaskElem(1) << bit_num) - 1);
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask);
    }
}


inline
CNCBitMaskBase<2>::CNCBitMaskBase(void)
{}

inline void
CNCBitMaskBase<2>::Initialize(void)
{
    m_Mask[0] = m_Mask[1] = 0;
}

inline bool
CNCBitMaskBase<2>::IsBitSet(unsigned int bit_num)
{
    unsigned int byte_num = (bit_num >= kNCMaskElemSize? 1: 0);
    return (m_Mask[byte_num] & (TNCBitMaskElem(1) << bit_num)) != 0;
}

inline void
CNCBitMaskBase<2>::InvertBits(unsigned int start_bit,
                              unsigned int bits_cnt)
{
    TNCBitMaskElem inv_mask;
    if (start_bit < kNCMaskElemSize) {
        if (bits_cnt >= kNCMaskElemSize)
            inv_mask = TNCBitMaskElem(-1);
        else
            inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[0] ^= inv_mask << start_bit;
        unsigned int last_bit = start_bit + bits_cnt;
        if (last_bit > kNCMaskElemSize) {
            inv_mask = (TNCBitMaskElem(1) << (last_bit - kNCMaskElemSize)) - 1;
            m_Mask[1] ^= inv_mask;
        }
    }
    else {
        inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[1] ^= inv_mask << (start_bit - kNCMaskElemSize);
    }
}

inline unsigned int
CNCBitMaskBase<2>::GetCntSet(void)
{
    return g_GetBitsCnt(m_Mask[0]) + g_GetBitsCnt(m_Mask[1]);
}

inline int
CNCBitMaskBase<2>::GetClosestSet(unsigned int bit_num)
{
    TNCBitMaskElem mask;
    if (bit_num < kNCMaskElemSize) {
        mask = m_Mask[0] & ~((TNCBitMaskElem(1) << bit_num) - 1);
        if (mask != 0)
            return g_GetLeastSetBit(mask);
        mask = m_Mask[1];
    }
    else {
        unsigned int second_num = bit_num - kNCMaskElemSize;
        mask = m_Mask[1] & ~((TNCBitMaskElem(1) << (second_num)) - 1);
    }
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask) + kNCMaskElemSize;
    }
}


template <int num_bits>
inline
CNCBitMask<num_bits>::CNCBitMask(void)
{}

template <int num_bits>
inline void
CNCBitMask<num_bits>::Initialize(unsigned int init_value)
{
    TBase::Initialize();
    if (init_value)
        InvertBits(0, num_bits);
}

template <int num_bits>
inline bool
CNCBitMask<num_bits>::IsBitSet(unsigned int bit_num)
{
    _ASSERT(bit_num < num_bits);
    return TBase::IsBitSet(bit_num);
}

template <int num_bits>
inline void
CNCBitMask<num_bits>::InvertBits(unsigned int start_bit,
                                 unsigned int bits_cnt)
{
    _ASSERT(start_bit + bits_cnt <= num_bits);
    TBase::InvertBits(start_bit, bits_cnt);
}

template <int num_bits>
inline unsigned int
CNCBitMask<num_bits>::GetCntSet(void)
{
    return TBase::GetCntSet();
}

template <int num_bits>
inline int
CNCBitMask<num_bits>::GetClosestSet(unsigned int bit_num)
{
    _ASSERT(bit_num < num_bits);
    return TBase::GetClosestSet(bit_num);
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


inline
CNCLongOpTrigger::CNCLongOpTrigger(void)
    : m_State(eNCOpNotDone),
      m_Listeners(NULL)
{}

inline
CNCLongOpTrigger::~CNCLongOpTrigger(void)
{
    _ASSERT(!m_Listeners);
}

inline void
CNCLongOpTrigger::Reset(void)
{
    _ASSERT(!m_Listeners);
    m_State = eNCOpNotDone;
}

inline ENCLongOpState
CNCLongOpTrigger::GetState(void)
{
    return m_State;
}


inline
CNCLongOpGuard::CNCLongOpGuard(CNCLongOpTrigger& trigger)
    : m_Trigger(&trigger)
{}

inline
CNCLongOpGuard::~CNCLongOpGuard(void)
{
    if (m_Trigger)
        m_Trigger->OperationCompleted();
}

inline ENCBlockingOpResult
CNCLongOpGuard::Start(INCBlockedOpListener* listener)
{
    ENCBlockingOpResult result = m_Trigger->StartWorking(listener);
    if (result == eNCWouldBlock  ||  m_Trigger->GetState() == eNCOpCompleted)
        m_Trigger = NULL;
    return result;
}

inline bool
CNCLongOpGuard::IsCompleted(void)
{
    return m_Trigger == NULL;
}


template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CStorageForDelete::CStorageForDelete(void)
    : m_Next(NULL)
{
    m_CurIdx.Set(0);
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CStorageForDelete::~CStorageForDelete(void)
{
    _ASSERT(m_CurIdx.Get() == 0);
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline void
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CStorageForDelete::x_CreateNextAndAdd(const ElemType& elem)
{
    CStorageForDelete* storage = this;
    unsigned int index;
    do {
        while (storage->m_Next) {
            storage = storage->m_Next;
        }
        {{
            CSpinGuard guard(storage->m_ObjLock);
            if (!storage->m_Next)
                storage->m_Next = new CStorageForDelete();
            storage = storage->m_Next;
        }}
        index = static_cast<unsigned int>(storage->m_CurIdx.Add(1)) - 1;
    }
    while (index >= kElemsPerStorage);
    storage->m_Elems[index] = elem;
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline void
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CStorageForDelete::AddElement(const ElemType& elem)
{
    CStorageForDelete* storage = this;
    while (storage->m_Next)
        storage = storage->m_Next;
    unsigned int index = static_cast<unsigned int>(storage->m_CurIdx.Add(1)) - 1;
    if (index >= kElemsPerStorage)
        storage->x_CreateNextAndAdd(elem);
    else
        storage->m_Elems[index] = elem;
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline void
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CStorageForDelete::Clean(const FinalDeleter& deleter)
{
    if (m_Next) {
        m_Next->Clean(deleter);
        delete m_Next;
        m_Next = NULL;
    }
    unsigned int max_index = static_cast<unsigned int>(m_CurIdx.Get());
    if (max_index >= kElemsPerStorage)
        max_index = kElemsPerStorage;
    for (unsigned int i = 0; i < max_index; ++i) {
        deleter.Delete(m_Elems[i]);
    }
    m_CurIdx.Set(0);
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::CNCDeferredDeleter(const FinalDeleter& deleter)
    : m_FinalDeleter(deleter)
{
    for (unsigned int i = 0; i < kDeleteDelay; ++i) {
        m_Stores[i] = new CStorageForDelete();
    }
    m_CurStore = m_Stores[0];
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::~CNCDeferredDeleter(void)
{
    for (unsigned int i = 0; i < kDeleteDelay; ++i) {
        m_Stores[i]->Clean(m_FinalDeleter);
        delete m_Stores[i];
    }
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline void
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::AddElement(const ElemType& elem)
{
    m_CurStore->AddElement(elem);
}

template <class ElemType, class FinalDeleter,
          unsigned int DeleteDelay, unsigned int ElemsPerStorage>
inline void
CNCDeferredDeleter<ElemType, FinalDeleter, DeleteDelay, ElemsPerStorage>
                    ::HeartBeat(void)
{
    CStorageForDelete* last_store = m_Stores[kDeleteDelay - 1];
    last_store->Clean(m_FinalDeleter);
    memmove(&m_Stores[1], &m_Stores[0], sizeof(m_Stores) - sizeof(m_Stores[0]));
    m_Stores[0] = last_store;
    SwapPointers(reinterpret_cast<void* volatile *>(&m_CurStore), last_store);
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_UTILS__HPP */
