#ifndef NCBI_SAFE_STATIC__HPP
#define NCBI_SAFE_STATIC__HPP

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
 * Author:   Aleksey Grichenko
 *
 * File Description:
 *   Static variables safety - create on demand, destroy on termination
 *
 *   CSafeStaticPtr_Base::   --  base class for CSafePtr<> and CSafeRef<>
 *   CSafeStaticPtr<>::      -- create variable on demand, destroy on program
 *                              termination (see NCBIOBJ for CSafeRef<> class)
 *   CSafeStaticRef<>::      -- create variable on demand, destroy on program
 *                              termination (see NCBIOBJ for CSafeRef<> class)
 *   CSafeStaticGuard::      -- guarantee for CSafePtr<> and CSafeRef<>
 *                              destruction and cleanup
 *
 */

/// @file ncbi_safe_static.hpp
/// Static variables safety - create on demand, destroy on application
/// termination.

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.h>
#include <array>


BEGIN_NCBI_SCOPE

class CObject;

/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticLifeSpan::
///
///    Class for specifying safe static object life span.
///

class NCBI_XNCBI_EXPORT CSafeStaticLifeSpan
{
public:
    /// Predefined life levels for the safe static objects
    enum ELifeLevel {
        eLifeLevel_Default,
        eLifeLevel_AppMain ///< Destroyed in CNcbiApplication::AppMain, if possible
    };
    /// Predefined life spans for the safe static objects
    enum ELifeSpan {
        eLifeSpan_Min      = kMin_Int, ///< std static, not adjustable
        eLifeSpan_Shortest = -20000,
        eLifeSpan_Short    = -10000,
        eLifeSpan_Normal   = 0,
        eLifeSpan_Long     = 10000,
        eLifeSpan_Longest  = 20000
    };
    /// Constructs a life span object from basic level and adjustment.
    /// Generates warning (and assertion in debug mode) if the adjustment
    /// argument is too big (<= -5000 or >= 5000). If span is eLifeSpan_Min
    /// "adjust" is ignored.
    CSafeStaticLifeSpan(ELifeSpan span, int adjust = 0);
    /// Same as above but allows to specify life level.
    /// E.g. eLifeLevel_AppMain allows to destroy such objects on AppMain exit
    /// (useful for static objects that join threads in their destructors,
    /// otherwise threads might disappear before such destructors are called).
    CSafeStaticLifeSpan(ELifeLevel level, ELifeSpan span, int adjust = 0);

    /// Get life level value.
    ELifeLevel GetLifeLevel() const { return m_LifeLevel; }

    /// Get life span value.
    int GetLifeSpan(void) const { return m_LifeSpan; }

    /// Get default life span (set to eLifeSpan_Min).
    static CSafeStaticLifeSpan& GetDefault(void);

private:
    ELifeLevel m_LifeLevel;
    int m_LifeSpan;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticPtr_Base::
///
///    Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates.
///

class NCBI_XNCBI_EXPORT CSafeStaticPtr_Base
{
public:
    /// User cleanup function type
    typedef void (*FUserCleanup)(void*  ptr);

    /// Life span
    typedef CSafeStaticLifeSpan TLifeSpan;

    ~CSafeStaticPtr_Base(void);

protected:
    typedef CGuard<CSafeStaticPtr_Base> TInstanceMutexGuard;

    /// Cleanup function type used by derived classes
    typedef void (*FSelfCleanup)(CSafeStaticPtr_Base* safe_static,
                                 TInstanceMutexGuard& guard);

    /// Constructor.
    ///
    /// @param self_cleanup
    ///   Cleanup function to be executed on destruction,
    ///   provided by a derived class.
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @param life_span
    ///   Life span allows to control destruction of objects. Objects with
    ///   the same life span are destroyed in the order reverse to their
    ///   creation order.
    /// @sa CSafeStaticLifeSpan
    CSafeStaticPtr_Base(FSelfCleanup self_cleanup,
                        FUserCleanup user_cleanup = 0,
                        TLifeSpan life_span = TLifeSpan::GetDefault())
        : m_SelfCleanup(self_cleanup),
          m_UserCleanup(user_cleanup),
          m_LifeSpan(life_span),
          m_CreationOrder(x_GetCreationOrder())
    {}

    /// Pointer to the data
    atomic<const void*> m_Ptr;

    DECLARE_CLASS_STATIC_MUTEX(sm_ClassMutex);

protected:
    const void* x_GetPtr() const {
        return m_Ptr.load(memory_order_acquire);
    }
    void x_SetPtr(const void* ptr) {
        m_Ptr.store(ptr, memory_order_release);
    }
    bool x_IsSetPtr() const {
        return x_GetPtr() != nullptr;
    }
    const void* x_ReleasePtr() {
        if ( auto ptr = x_GetPtr() ) {
            x_SetPtr(0);
            return ptr;
        }
        else {
            return nullptr;
        }
    }

private:
    friend struct SSimpleLock<CSafeStaticPtr_Base>;
    friend struct SSimpleUnlock<CSafeStaticPtr_Base>;

    void Lock(void)
    {
        CMutexGuard guard(sm_ClassMutex);
        if (!m_InstanceMutex || !m_MutexRefCount) {
            m_InstanceMutex = new CMutex;
            m_MutexRefCount = 1;
        }
        ++m_MutexRefCount;
        guard.Release();
        m_InstanceMutex->Lock();
    }

    void Unlock(void)
    {
        m_InstanceMutex->Unlock();
        x_ReleaseInstanceMutex();
    }

    void x_ReleaseInstanceMutex(void)
    {
        CMutexGuard guard(sm_ClassMutex);
        if (--m_MutexRefCount > 0) return;
        // Workaround for over-optimization - member assignments done immediately
        // before exiting destructor can be dropped, so mutex and counter should
        // be set to 0 before deleting the mutex.
        CMutex* tmp = m_InstanceMutex;
        m_InstanceMutex = 0;
        m_MutexRefCount = 0;
        delete tmp;
    }

protected:
    friend class CSafeStatic_Less;

    FSelfCleanup m_SelfCleanup;   // Derived class' cleanup function
    FUserCleanup m_UserCleanup;   // User-provided  cleanup function
    TLifeSpan    m_LifeSpan;      // Life span of the object
    int          m_CreationOrder; // Creation order of the object
    int          m_MutexRefCount; // Mutex reference counter.
    CMutex*      m_InstanceMutex; // Mutex used to create/destroy value.

    static int x_GetCreationOrder(void);

    // Return true if the object should behave like regular static
    // (no delayed destruction).
    bool x_IsStdStatic(void) const
    {
        return (m_LifeSpan.GetLifeLevel() == CSafeStaticLifeSpan::eLifeLevel_Default) &&
            (m_LifeSpan.GetLifeSpan() == int(CSafeStaticLifeSpan::eLifeSpan_Min));
    }

    // To be called by CSafeStaticGuard on the program termination
    friend class CSafeStaticGuard;
    void x_Cleanup(void)
    {
        // Note: x_Cleanup should always be called with sm_ClassMutex locked.
        if ( m_SelfCleanup ) {
            TInstanceMutexGuard guard(*this);
            m_SelfCleanup(this, guard);
        }
        // Delete instance mutex if it's not used by other threads.
        x_ReleaseInstanceMutex();
    }
};


/// Comparison for safe static ptrs. Defines order of objects' destruction:
/// short living objects go first; if life span of several objects is the same,
/// the order of destruction is reverse to the order of their creation.
class CSafeStatic_Less
{
public:
    typedef CSafeStaticPtr_Base* TPtr;
    bool operator()(const TPtr& ptr1, const TPtr& ptr2) const
    {
        if (ptr1->m_LifeSpan.GetLifeSpan() == ptr2->m_LifeSpan.GetLifeSpan()) {
            return ptr1->m_CreationOrder > ptr2->m_CreationOrder;
        }
        return ptr1->m_LifeSpan.GetLifeSpan() < ptr2->m_LifeSpan.GetLifeSpan();
    }
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticGuard::
///
///    Register all on-demand variables,
///    destroy them on the program termination.

class NCBI_XNCBI_EXPORT CSafeStaticGuard
{
public:
    /// Check if already initialized. If not - create the stack,
    /// otherwise just increment the reference count.
    CSafeStaticGuard(void);

    /// Check reference count, and if it is zero, then destroy
    /// all registered variables.
    ~CSafeStaticGuard(void);

    /// Add new on-demand variable to the cleanup stack.
    static void Register(CSafeStaticPtr_Base* ptr)
    {
        if ( sm_RefCount > 0 && ptr->x_IsStdStatic() ) {
            // Do not add the object to the stack
            return;
        }

        auto& stack = x_GetStack(ptr->m_LifeSpan.GetLifeLevel());

        if ( !stack ) {
            x_Get();
        }

        // Update creation order for objects with non-default life span,
        // so they are destroyed in the proper order
        ptr->m_CreationOrder = ptr->x_GetCreationOrder();

        // Pacify CLANG Static Analyzer
        if (stack) stack->insert(ptr);
    }

    /// Disable checking on child thread(s) running during destruction
    static void DisableChildThreadsCheck();

    /// Explicitly destroy all on-demand variables up to a specified level
    static void Destroy(CSafeStaticLifeSpan::ELifeLevel level);

private:
    // Initialize the guard, return pointer to it.
    static CSafeStaticGuard* x_Get(void);

    // Stack to keep registered variables.
    typedef multiset<CSafeStaticPtr_Base*, CSafeStatic_Less> TStack;

    // Get a stack based on a level
    static TStack*& x_GetStack(CSafeStaticLifeSpan::ELifeLevel level)
    {
        static array<TStack*, 2> stacks;

        const size_t index = level;
        _ASSERT(level >= 0);
        _ASSERT(index < stacks.size());
        return stacks[index];
    }

    // Cleanup a stack
    static void x_Cleanup(CMutexGuard& guard, TStack*& stack);

    // Reference counter. The stack is destroyed when
    // the last reference is removed.
    static int sm_RefCount;

    // Whether to check on child thread(s) running during destruction
    static bool sm_ChildThreadsCheck;
};


/// Helper class for object allocation/deallocation.
/// Required to simplify friend declarations for classes with
/// private constructors/destructors and to support CObject
/// reference counting.

template <class T>
class CSafeStatic_Allocator
{
public:
    /// Create a new class instance.
    static T* s_Create(void) { return new T; }

    // CSafeStatic must use AddReference/RemoveReference for CObject derived
    // classes. To do this there are two different versions of the following
    // methods.
    static void s_AddReference(void*) {}
    static void s_AddReference(const void*) {}
    static void s_AddReference(CObject* ptr)
    {
        if (ptr) ptr->AddReference();
    }
    static void s_RemoveReference(void* ptr)
    {
        if (ptr) delete static_cast<T*>(ptr);
    }
    static void s_RemoveReference(const void* ptr)
    {
        if (ptr) delete const_cast<T*>(static_cast<const T*>(ptr));
    }
    static void s_RemoveReference(CObject* ptr)
    {
        if (ptr) ptr->RemoveReference();
    }
    static void s_RemoveReference(const CObject* ptr)
    {
        if (ptr) ptr->RemoveReference();
    }
};


/// Initialization and cleanup of a safe-static object.
/// Must implement at least Create() and Cleanup() methods.
/// Create() must create a new object and return the initialized
/// pointer. Cleanup() can be a no-op.
/// The default implementation allows to use it as a wrapper for
/// static callback functions.
template <class T>
class CSafeStatic_Callbacks
{
public:
    /// The default implementation allows to use callback functions
    /// rather than a new class.
    typedef T* (*FCreate)(void);
    typedef void (*FCleanup)(T& value);
    typedef CSafeStatic_Allocator<T> TAllocator;

    /// The constructor allows to use CSafeStatic_Callbacks as a simple
    /// wrapper for static functions.
    /// @param create
    ///   Initialization function which must create a new object. If null,
    ///   no special initialization is performed.
    /// @param cleanup
    ///   Cleanup function. If null, no special cleanup is performed.
    CSafeStatic_Callbacks(FCreate create = 0, FCleanup cleanup = 0)
        : m_Create(create), m_Cleanup(cleanup) {}

    /// Create new object.
    /// @return
    ///   The allocated object.
    T* Create(void) {
        return (m_Create) ? m_Create() : TAllocator::s_Create();
    }

    /// Perform cleanup before destruction.
    /// @param ptr
    ///   Object to be destroyed using the selected allocator. The cleanup
    ///   method should not destroy the object itself, just perform any
    ///   additional actions (e.g. setting some external pointers to null).
    void Cleanup(T& value) {
        if ( m_Cleanup ) {
            m_Cleanup(value);
        }
    }

private:
    FCreate  m_Create;
    FCleanup m_Cleanup;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStatic<>::
///
///    For on-demand object access, both non-CObject and CObject based.
///    Create the variable of type "T" on the first access. The default
///    implementation of allocator uses reference counter of CObject
///    class to prevent premature destruction.
///    Should be used only as static object. Otherwise the correct
///    initialization is not guaranteed.
///    @param T
///      Type of the variable to be stored.
///    @param Callbacks
///      A class implementing two methods to perform additional initialization
///      and cleanup:
///      void Create(T* ptr)
///      void Cleanup(T* ptr)
///      NOTE: The Cleanup callback must not destroy the object itself, just
///      perform any additional actions.
template <class T,
    class Callbacks = CSafeStatic_Callbacks<T> >
class CSafeStatic : public CSafeStaticPtr_Base
{
    static T* x_CastPtr(const void* ptr) {
        return static_cast<T*>(const_cast<void*>(ptr));
    }
public:
    typedef Callbacks TCallbacks;
    typedef CSafeStatic<T, TCallbacks> TThisType;
    typedef CSafeStaticLifeSpan TLifeSpan;
    typedef CSafeStatic_Allocator<T> TAllocator;

    /// Callback function types. The default callback class can be used
    /// as a simple wrapper for static functions.
    /// @sa CSafeStatic_Callbacks
    typedef typename CSafeStatic_Callbacks<T>::FCreate FCreate;
    typedef typename CSafeStatic_Callbacks<T>::FCleanup FCleanup;

    /// Constructor.
    /// @param life_span
    ///   Life span allows to control destruction of objects. Objects with
    ///   the same life span are destroyed in the order reverse to their
    ///   creation order.
    /// @sa CSafeStaticLifeSpan
    CSafeStatic(TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(sx_SelfCleanup, 0, life_span)
    {}

    /// Constructor.
    /// @param init
    ///   Callback function to be used to create the stored object.
    /// @param cleanup
    ///   Callback function to be used for additional cleanup.
    /// @param life_span
    ///   Life span allows to control destruction of objects. Objects with
    ///   the same life span are destroyed in the order reverse to their
    ///   creation order.
    /// @sa CSafeStaticLifeSpan
    CSafeStatic(FCreate  create,
                FCleanup cleanup,
                TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(sx_SelfCleanup, 0, life_span),
          m_Callbacks(create, cleanup)
    {}

    /// Constructor.
    /// @param callbacks
    ///   Callbacks class instance.
    /// @param life_span
    ///   Life span allows to control destruction of objects. Objects with
    ///   the same life span are destroyed in the order reverse to their
    ///   creation order.
    /// @sa CSafeStaticLifeSpan
    CSafeStatic(TCallbacks callbacks,
                TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(sx_SelfCleanup, 0, life_span),
          m_Callbacks(callbacks)
    {}

    /// Create the variable if not created yet, return the reference.
    T& Get(void)
    {
        if ( !x_IsSetPtr() ) {
            x_Init();
        }
        return *x_CastPtr(x_GetPtr());
    }

    T* operator-> (void) { return &Get(); }
    T& operator*  (void) { return  Get(); }

private:
    CSafeStatic(const CSafeStatic&) = delete;
    CSafeStatic& operator=(const CSafeStatic&) = delete;

    void x_Init(void) {
        TInstanceMutexGuard guard(*this);
        if ( !x_IsSetPtr() ) {
            // Create the object and register for cleanup
            T* ptr = 0;
            try {
                ptr = m_Callbacks.Create();
                TAllocator::s_AddReference(ptr);
                x_SetPtr(ptr);
                CSafeStaticGuard::Register(this);
            }
            catch (CException& e) {
                TAllocator::s_RemoveReference(ptr);
                NCBI_RETHROW_SAME(e, "CSafeStatic::Init: Register() failed");
            }
            catch (...) {
                TAllocator::s_RemoveReference(ptr);
                NCBI_THROW(CCoreException,eCore,
                           "CSafeStatic::Init: Register() failed");
            }
        }
    }

    // "virtual" cleanup function
    static void sx_SelfCleanup(CSafeStaticPtr_Base* safe_static,
                               TInstanceMutexGuard& guard)
    {
        TThisType* this_ptr = static_cast<TThisType*>(safe_static);
        if ( T* ptr = x_CastPtr(this_ptr->x_ReleasePtr()) ) {
            TCallbacks callbacks = this_ptr->m_Callbacks;
            guard.Release();
            callbacks.Cleanup(*ptr);
            TAllocator::s_RemoveReference(ptr);
        }
    }

    TCallbacks m_Callbacks;
};


/// Safe static callbacks version allowing initial value of type V
// to be set through template argument.
template<class T, class V, V value>
class CSafeStaticInit_Callbacks : public CSafeStatic_Callbacks<T>
{
public:
    T* Create(void) {
        return new T(value);
    }
};


/// Declare CSafeStatic<const type>, initialize it with 'init_value' of the same type.
#define SAFE_CONST_STATIC(type, init_value) \
    SAFE_CONST_STATIC_EX(type, type, init_value)

/// Declare CSafeStatic<const type>, initialize it with 'init_value' of type
/// 'init_value_type'.
#define SAFE_CONST_STATIC_EX(type, init_value_type, init_value) \
    CSafeStatic< const type, \
    CSafeStaticInit_Callbacks<const type, init_value_type, init_value> >

/// Declare CSafeStatic<const string>, initialize it with 'const char* value'.
#define SAFE_CONST_STATIC_STRING(var, value) \
    char SAFE_CONST_STATIC_STRING_##var[] = value; \
    SAFE_CONST_STATIC_EX(std::string, const char*, \
    SAFE_CONST_STATIC_STRING_##var) var



/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticPtr<>::
///
///    For simple on-demand variables.
///    Create the variable of type "T" on demand,
///    destroy it on the program termination.
///    Should be used only as static object. Otherwise
///    the correct initialization is not guaranteed.
///    @deprecated Use CSafeStatic<> instead.

template <class T>
class CSafeStaticPtr : public CSafeStaticPtr_Base
{
    static T* x_CastPtr(const void* ptr) {
        return static_cast<T*>(const_cast<void*>(ptr));
    }
public:
    typedef CSafeStaticLifeSpan TLifeSpan;

    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @param life_span
    ///   Life span allows to control destruction of objects.
    /// @sa CSafeStaticPtr_Base
    CSafeStaticPtr(FUserCleanup user_cleanup = 0,
                   TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(sx_SelfCleanup, user_cleanup, life_span)
    {}

    /// Create the variable if not created yet, return the reference.
    T& Get(void)
    {
        if ( !x_IsSetPtr() ) {
            x_Init();
        }
        return *x_CastPtr(x_GetPtr());
    }
    /// Get the existing object or create a new one using the provided
    /// FUserCreate object.
    /// @deprecated Use CSafeStatic class instead.
    template <class FUserCreate>
    NCBI_DEPRECATED
    T& Get(FUserCreate user_create)
    {
        if ( !x_IsSetPtr() ) {
            x_Init(user_create);
        }
        return *x_CastPtr(x_GetPtr());
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    /// Initialize with an existing object. The object MUST be
    /// allocated with "new T" -- it will be destroyed with
    /// "delete object" in the end. Set() works only for
    /// not yet initialized safe-static variables.
    void Set(T* object);

private:
    // Initialize the object
    void x_Init(void);

    template <class FUserCreate>
    void x_Init(FUserCreate user_create);

    // "virtual" cleanup function
    static void sx_SelfCleanup(CSafeStaticPtr_Base* safe_static,
                               TInstanceMutexGuard& guard)
    {
        CSafeStaticPtr<T>* this_ptr = static_cast<CSafeStaticPtr<T>*>(safe_static);
        if ( T* ptr = x_CastPtr(this_ptr->x_ReleasePtr()) ) {
            CSafeStaticPtr_Base::FUserCleanup user_cleanup = this_ptr->m_UserCleanup;
            guard.Release();
            if ( user_cleanup ) {
                user_cleanup(ptr);
            }
            delete ptr;
        }
    }
};



/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticRef<>::
///
///    For on-demand CObject-derived object.
///    Create the variable of type "T" using CRef<>
///    (to avoid premature destruction).
///    Should be used only as static object. Otherwise
///    the correct initialization is not guaranteed.
///    @deprecated Use CSafeStatic<> instead.
template <class T>
class CSafeStaticRef : public CSafeStaticPtr_Base
{
    static T* x_CastPtr(const void* ptr) {
        return static_cast<T*>(const_cast<void*>(ptr));
    }
public:
    typedef CSafeStaticLifeSpan TLifeSpan;

    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @param life_span
    ///   Life span allows to control destruction of objects.
    /// @sa CSafeStaticPtr_Base
    CSafeStaticRef(FUserCleanup user_cleanup = 0,
                   TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(sx_SelfCleanup, user_cleanup, life_span)
    {}

    /// Create the variable if not created yet, return the reference.
    T& Get(void)
    {
        if ( !x_IsSetPtr() ) {
            x_Init();
        }
        return *x_CastPtr(x_GetPtr());
    }
    /// Get the existing object or create a new one using the provided
    /// FUserCreate object.
    /// @deprecated Use CSafeStatic class instead.
    template <class FUserCreate>
    NCBI_DEPRECATED
    T& Get(FUserCreate user_create)
    {
        if ( !x_IsSetPtr() ) {
            x_Init(user_create);
        }
        return *x_CastPtr(x_GetPtr());
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    /// Initialize with an existing object. The object MUST be
    /// allocated with "new T" to avoid premature destruction.
    /// Set() works only for un-initialized safe-static variables.
    void Set(T* object);

private:
    // Initialize the object and the reference
    void x_Init(void);

    template <class FUserCreate>
    void x_Init(FUserCreate user_create);

    // "virtual" cleanup function
    static void sx_SelfCleanup(CSafeStaticPtr_Base* safe_static,
                               TInstanceMutexGuard& guard)
    {
        CSafeStaticRef<T>* this_ptr = static_cast<CSafeStaticRef<T>*>(safe_static);
        if ( T* ptr = x_CastPtr(this_ptr->x_ReleasePtr()) ) {
            CSafeStaticPtr_Base::FUserCleanup user_cleanup = this_ptr->m_UserCleanup;
            guard.Release();
            if ( user_cleanup ) {
                user_cleanup(ptr);
            }
            ptr->RemoveReference();
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// This static variable must be present in all modules using
/// on-demand static variables. The guard must be created first
/// regardless of the modules initialization order.

static CSafeStaticGuard s_CleanupGuard;


/////////////////////////////////////////////////////////////////////////////
//
// Large inline methods

template <class T>
inline
void CSafeStaticPtr<T>::Set(T* object)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Set the new object and register for cleanup
        if ( object ) {
            x_SetPtr(object);
            CSafeStaticGuard::Register(this);
        }
    }
}


template <class T>
inline
void CSafeStaticPtr<T>::x_Init(void)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Create the object and register for cleanup
        x_SetPtr(new T);
        CSafeStaticGuard::Register(this);
    }
}


template <class T>
template <class FUserCreate>
inline
void CSafeStaticPtr<T>::x_Init(FUserCreate user_create)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Create the object and register for cleanup
        if ( auto ptr = user_create() ) {
            x_SetPtr(ptr);
            CSafeStaticGuard::Register(this);
        }
    }
}


template <class T>
inline
void CSafeStaticRef<T>::Set(T* object)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Set the new object and register for cleanup
        if ( object ) {
            object->AddReference();
            x_SetPtr(object);
            CSafeStaticGuard::Register(this);
        }
    }
}


template <class T>
inline
void CSafeStaticRef<T>::x_Init(void)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Create the object and register for cleanup
        T* ptr = new T;
        ptr->AddReference();
        x_SetPtr(ptr);
        CSafeStaticGuard::Register(this);
    }
}


template <class T>
template <class FUserCreate>
inline
void CSafeStaticRef<T>::x_Init(FUserCreate user_create)
{
    TInstanceMutexGuard guard(*this);
    if ( !x_IsSetPtr() ) {
        // Create the object and register for cleanup
        T* ptr = user_create();
        if ( ptr ) {
            ptr->AddReference();
            x_SetPtr(ptr);
            CSafeStaticGuard::Register(this);
        }
    }
}



END_NCBI_SCOPE

#endif  /* NCBI_SAFE_STATIC__HPP */
