#ifndef UTIL___SYNC_QUEUE__HPP
#define UTIL___SYNC_QUEUE__HPP

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
 */

/// @file sync_queue.hpp
///
/// Definition of synchronized queue (CSyncQueue template) and templates
/// related to it.
/// See also: @ref CSyncQueueDescription.

/*! @page CSyncQueueDescription Using CSyncQueue class

    CSyncQueue is a thread-safe queue object with additional blocking
    mechanism. It supports operations that wait for the queue to become
    non-empty when retrieving an element, and wait for space to become
    available in the queue when storing an element.

    CSyncQueue method Pop() will automatically block and wait while the queue
    has no elements. It will wait for a given timeout until some other
    thread push element to the queue. CSyncQueue method Push() will
    automatically block and wait while the queue is full and contains maximum
    allowed amount of elements in it. It will wait for a given timeout until
    some other thread pops one or more elements from the queue (or maybe
    erases them via iterators).

    CSyncQueue is designed to be used primarily for producer-consumer queues,
    but additionally it supports the iterator-based access. So, for example,
    it is possible to remove an arbitrary element from a queue using method
    Erase(it) in CSyncQueue::TAccessGuard.

    Typical use of CSyncQueue in producer-consumer environment can be
    as follows:

    <code>
    typedef CSyncQueue&lt;TSomeObject&gt; TObjQueue;

    TObjQueue s_queue(kSomeMaxSize);

    class CProducer : public CThread {
    protected:
        virtual void* Main(void) {
            while (1) {
                s_queue.Push(Produce());
            }
        }
    private:
        TSomeObject Produce() {
            ...
        }
    };

    class CConsumer : public CThread {
    protected:
        virtual void* Main(void) {
            while (1) {
                Consume(s_queue.Pop());
            }
        }
    private:
        void Consume(TSomeObject obj) {
            ...
        }
    };
    </code>

    It is safe to use CSyncQueue object with multiple producers and
    multiple consumers.

    Extended use of CSyncQueue class for iterator-based access or for
    performing some bulk operations may look like following:

    <code>
    void FunctionWithGuard(void) {
        TObjQueue::TAccessGuard guard(s_queue);

        for (TObjQueue::TAccessGuard::TIterator it = guard.Begin();
                                                it != guard.End(); )
        {
            if (NeedErase(*it)) {
                it = guard.Erase(it);
            }
            else {
                ++it;
            }
        }

        while (HasMoreSomeObjects()) {
            guard.Queue().Push(GetNextSomeObject());
        }
    }
    </code>

    CSyncQueue::TAccessGuard object here ensures that while the function
    is working other threads will not be able to push or pop any elements
    from the queue. Methods Push() and Pop() in other threads will block
    and wait while some CSyncQueue::TAccessGuard object is active.

 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbithr.hpp>

#include <deque>
#include <list>
#include <set>
#include <vector>
#include <queue>


BEGIN_NCBI_SCOPE


// This class is for internal use only.
template <class Type, class Container, class Traits>
class CSyncQueue_InternalAutoLock;


// forward class declarations
template <class Type, class Container>
class CSyncQueue_I_Base;

template <class Type, class Container, class TNativeIterator, class Traits>
class CSyncQueue_I;

template <class Type, class Container, class Traits>
class CSyncQueue_ConstAccessGuard;

template <class Type, class Container, class Traits>
class CSyncQueue_AccessGuard;


class CSyncQueue_DefaultTraits;


/// Thread-safe queue object with a blocking mechanism.
///
/// An attempt to pop from empty queue or push to full queue function call
/// waits for normal operation finishing. Queue can also be locked for a
/// long time for some bulk operations. This can be achieved by
/// CSyncQueue::T*AccessGuard classes.
///
/// CSyncQueue can be used in single threaded applications or multithreaded
/// applications with C++ Toolkit threads as well as with any kind of
/// native threads. In case of multithreaded applications a CSyncQueue object
/// could be used only from one thread or from many threads. To distinguish
/// how the object is accessed the template supports the 'Traits' parameter.
/// The traits in particular report if the the queue is accessed from many
/// threads (see the CSyncQueue_DefaultTraits::IsUsedConcurrently() description
/// for the details of what could happen if the IsUsedConcurrently() is not
/// implemented properly). The default traits suppose that the C++ Toolkit
/// CThread class is used to create threads.
///
/// There is a difference as well of how the timeouts are handled depending on
/// what threads are used in the application if so: the C++ Toolkit threads or
/// some other threads. The only difference between these cases is passing
/// NULL as timeout in methods Push(), Pop() and Clear(). When C++ Toolkit
/// CThread class is used to create application's threads then NULL value will
/// mean indefinite timeout (as stated in comments to those methods). But if
/// all threads in the application (if any) were created by some native
/// function without using CThread class then NULL value will mean 0 timeout
/// and it's impossible to set indefinite timeout in this case.
///
/// Full description and examples of using look here:
/// @ref CSyncQueueDescription.
///
///
/// @param Type
///   Type of elements saved in queue
/// @param Container
///   Type of underlying container used in queue. To be applicable for using
///   in CSyncQueue container must have methods push_back(), front() and
///   pop_front(). For other containers you need to implement an adaptor to
///   use it in CSyncQueue. For STL set, multiset and priority_queue classes
///   adaptors implemented further in this file.
/// @param Traits
///   Traits which are used to alter internal behavior. Currently it only
///   affects the way how the number of running threads is detected.
///   @sa CSyncQueue_DefaultTraits

template < class Type, class Container = deque<Type>,
           class Traits = CSyncQueue_DefaultTraits >
class CSyncQueue
{
public:
    /// Short name of this queue type
    typedef CSyncQueue<Type, Container, Traits>      TThisType;
    /// Type of values stored in the queue
    typedef typename Container::value_type           TValue;
    /// Type of size of the queue
    typedef typename Container::size_type            TSize;


    /// Construct queue
    ///
    /// @param max_size
    ///   Maximum size of the queue. Must be greater than zero.
    CSyncQueue(TSize max_size = numeric_limits<TSize>::max());

    /// Add new element to the end of queue.
    /// @note  This call will block if the queue is full or if there are
    ///        competing operations by other threads
    ///
    /// @param elem
    ///   Element to push
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void Push(const TValue& elem, const CTimeSpan* timeout = NULL);

    /// Add new element to the end of queue.
    /// @note  This call will block if the queue is full or if there are
    ///        competing operations by other threads
    ///
    /// @param elem
    ///   Element to push
    /// @param full_tmo
    ///   Maximum time period to wait on this call (including waiting for
    ///   other threads to unlock the queue and waiting until there is space
    ///   in the queue to add element to); NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    /// @param service_tmo
    ///   Maximum time period to wait for other threads to unlock the queue;
    ///   NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void Push(const TValue&    elem,
              const CTimeSpan* full_tmo,
              const CTimeSpan* service_tmo);

    /// Retrieve an element from the queue.
    /// @note  This call will block if the queue is empty or if there are
    ///        competing operations by other threads
    ///
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    TValue Pop(const CTimeSpan* timeout = NULL);

    /// Retrieve an element from the queue.
    /// @note  This call will block if the queue is empty or if there are
    ///        competing operations by other threads
    ///
    /// @param full_tmo
    ///   Maximum time period to wait on this call (including waiting for
    ///   other threads to unlock the queue and waiting until there is any
    ///   element in the queue to retrieve); NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    /// @param service_tmo
    ///   Maximum time period to wait for other threads to unlock the queue;
    ///   NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    TValue Pop(const CTimeSpan* full_tmo,
               const CTimeSpan* service_tmo);

    /// Check if the queue is empty.
    /// @note  This call always returns immediately, without any blocking
    bool IsEmpty(void) const;

    /// Check if the queue is full (has maxSize elements)
    /// @note  This call always returns immediately, without any blocking
    bool IsFull(void) const;

    /// Get count of elements already stored in the queue
    /// @note  This call always returns immediately, without any blocking
    TSize GetSize(void) const;

    /// Get the maximum # of elements allowed to be kept in the queue
    /// @note  This call always returns immediately, without any blocking
    TSize GetMaxSize(void) const;

    /// Remove all elements from the queue
    /// @note  This call will block if there are
    ///        competing operations by other threads
    ///
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void Clear(const CTimeSpan* timeout = NULL);

    /// Copy (add) all queue elements to another queue.
    /// @note This method will not copy to queue blocked by some access
    ///       guardian because this pattern can lead to a deadlock in some
    ///       situations.
    /// @throws  CSyncQueueException Does nothing and throws with "eNoRoom"
    ///          err.code if there is not enough room in the destination queue.
    ///          Throws with "eGuardedCopy" if other queue is guarded in the
    ///          running thread with some access guardian.
    ///
    /// @param other
    ///   Another queue to which all elements will be copied
    void CopyTo(TThisType* other) const;

public:
    /// Type of access guardian for constant queue
    typedef CSyncQueue_ConstAccessGuard<Type, Container,
                                        Traits>             TConstAccessGuard;
    /// Type of access guardian for non-constant queue
    typedef CSyncQueue_AccessGuard<Type, Container, Traits> TAccessGuard;

private:
    /// Type of underlying iterator
    typedef typename Container::iterator                    TNativeIter;
    /// Type of underlying constant iterator
    typedef typename Container::const_iterator              TNativeConstIter;
    /// Constant iterator on this queue
    typedef CSyncQueue_I<Type, Container,
                         TNativeConstIter, Traits>          TConstIterator;
    /// Non-constant iterator on this queue
    typedef CSyncQueue_I<Type, Container,
                         TNativeIter, Traits>               TIterator;

private:
    // Prohibit copy and assignment
    CSyncQueue(const TThisType&);
    TThisType& operator= (const TThisType&);

    /// Short name of auto-lock for this queue type.
    /// For internal use only.
    typedef CSyncQueue_InternalAutoLock<Type, Container, Traits>
                                                            TAutoLock;

    /// Lock access to the queue.
    /// @note  This call will block if the queue is already locked by
    ///        another thread
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    /// @return
    ///   TRUE if the queue is locked successfully,
    ///   FALSE if timeout is exceeded
    bool x_Lock(const CTimeSpan* timeout = NULL) const;

    /// Unlock access to queue.
    void x_Unlock(void) const;

    /// Lock two queues simultaneously to guard against
    /// otherwise possible inter-thread dead-lock
    /// @param my_lock
    ///   Auto-lock object for this queue
    /// @param other_lock
    ///   Auto-lock object for another queue
    /// @param other_obj
    ///   The [other] queue to lock
    void x_DoubleLock(TAutoLock*       my_lock,
                      TAutoLock*       other_lock,
                      const TThisType& other_obj)
        const;

    /// Function to check condition; usually points to IsFull() or IsEmpty()
    /// @sa x_LockAndWait()
    typedef bool (CSyncQueue::*TCheckFunc)(void) const;

    /// Function to throw an exception; usually points to
    /// ThrowSyncQueueNoRoom() or ThrowSyncQueueEmpty()
    /// @sa x_LockAndWait()
    typedef void (*TErrorThrower)(void);

    /// Lock the queue and wait until the condition function returns FALSE.
    /// @param lock
    ///   Auto-lock object to acquire the lock on the queue
    /// @param full_tmo
    ///   Maximum time period to wait on this call (including waiting for
    ///   other threads to unlock the queue and waiting for condition to
    ///   return FALSE); NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (with or without locking).
    /// @param service_tmo
    ///   Maximum time period to wait for other threads to unlock the queue;
    ///   NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (without locking).
    /// @param func_to_check
    ///   Function to check condition
    /// @param trigger
    ///   Semaphore to wait for the condition
    /// @param counter
    ///   Counter of threads waiting on this semaphore
    /// @param throw_error
    ///   Function to throw exception when timeout is exceeded
    void x_LockAndWait(TAutoLock*       lock,
                       const CTimeSpan* full_tmo,
                       const CTimeSpan* service_tmo,
                       TCheckFunc       func_to_check,
                       CSemaphore*      trigger,
                       CAtomicCounter*  counter,
                       TErrorThrower    throw_error)
        const;

    /// Lock the queue and wait until it has room for more elements
    /// @param lock
    ///   Auto-lock object  to acquire the lock on the queue
    /// @param full_tmo
    ///   Maximum time period to wait on this call (including waiting for
    ///   other threads to unlock the queue and waiting until there is space
    ///   in the queue to add element to); NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (with or without locking).
    /// @param service_tmo
    ///   Maximum time period to wait for other threads to unlock the queue;
    ///   NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (without locking).
    void x_LockAndWaitWhileFull(TAutoLock*       lock,
                                const CTimeSpan* full_tmo,
                                const CTimeSpan* service_tmo)
        const;

    /// Lock the queue and wait until it has at least one element
    /// @param lock
    ///   Auto-lock object  to acquire the lock on the queue
    /// @param full_tmo
    ///   Maximum time period to wait on this call (including waiting for
    ///   other threads to unlock the queue and waiting until there is any
    ///   element in the queue to retrieve); NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (with or without locking).
    /// @param service_tmo
    ///   Maximum time period to wait for other threads to unlock the queue;
    ///   NULL to wait infinitely.
    ///   If the timeout is exceeded, then return (without locking).
    void x_LockAndWaitWhileEmpty(TAutoLock*       lock,
                                 const CTimeSpan* full_tmo,
                                 const CTimeSpan* service_tmo)
        const;

    /// Lock the queue by an access guard
    void x_GuardedLock(const CTimeSpan* timeout = NULL) const;

    /// Unlock the queue by an access guard
    void x_GuardedUnlock(void) const;

    /// Check if this queue is locked by some access guard in current thread
    bool x_IsGuarded(void) const;

    /// Add new element to the end of queue -- without locking and blocking
    void x_Push_NonBlocking(const TValue& elem);

    /// Get first element from the queue -- without locking and blocking
    TValue x_Pop_NonBlocking(void);

    /// Clear the queue -- without locking and blocking
    void x_Clear_NonBlocking(void);

    /// Get iterator pointing to the start of underlying container
    TNativeIter x_Begin(void);

    /// Get constant iterator pointing to the start of underlying container
    TNativeConstIter x_Begin(void) const;

    /// Get iterator pointing to the end of underlying container
    TNativeIter x_End(void);

    /// Get constant iterator pointing to the end of underlying container
    TNativeConstIter x_End(void) const;

    /// Erase one element from the underlying container
    ///
    /// @param iter
    ///   Iterator pointing to the element to be deleted
    /// @return
    ///   Iterator pointing to the element next to the deleted one
    TNativeIter x_Erase(TNativeIter iter);

    /// Erase several elements from the underlying container
    ///
    /// @param from_iter
    ///   Iterator pointing to the start of elements block to be deleted
    /// @param to_iter
    ///   Iterator pointing to the end of the block of elements to be deleted
    ///   (one element after the last element to delete)
    /// @return
    ///   Iterator pointing to the first element after the deleted ones.
    //    If to_iter <= from_iter, then throw an exception.
    TNativeIter x_Erase(TNativeIter from_iter, TNativeIter to_iter);


    enum {
        /// Value of thread system id that cannot be equal to any thread's id.
        kThreadSystemID_None = 0xFFFFFFFF
    };


    /// Underlying container to store queue elements
    Container m_Store;

    /// Current number of elements in the queue.
    /// Stored separately because some containers do not provide a size()
    /// method working in constant time
    volatile TSize m_Size;

    /// Maximum size of the queue
    const TSize m_MaxSize;

    /// Semaphore to signal that the queue can be safely modified
    mutable CSemaphore m_TrigLock;

    /// Semaphore to signal that the queue has become not empty
    mutable CSemaphore m_TrigNotEmpty;

    /// Number of threads waiting for the queue to become non-empty
    mutable CAtomicCounter_WithAutoInit m_CntWaitNotEmpty;

    /// Semaphore to signal that the queue has become not full
    mutable CSemaphore m_TrigNotFull;

    /// Number of threads waiting for the queue to become non-full
    mutable CAtomicCounter_WithAutoInit m_CntWaitNotFull;

    /// ID of the thread in which the queue has been locked by a guardian
    mutable TThreadSystemID m_CurGuardTID;

    /// Number of lockings of this queue with access guardians in one thread
    mutable int m_LockCount;

    //
    friend class CSyncQueue_ConstAccessGuard <Type, Container, Traits>;
    friend class CSyncQueue_AccessGuard      <Type, Container, Traits>;
    friend class CSyncQueue_InternalAutoLock <Type, Container, Traits>;
    friend class CSyncQueue_I                <Type, Container,
                                              TNativeIter, Traits>;
    friend class CSyncQueue_I                <Type, Container,
                                              TNativeConstIter, Traits>;
};



/// Default traits for the CSyncQueue class.
/// It also serves as a base class for user defined CSyncQueue traits.
class CSyncQueue_DefaultTraits
{
    public:
        /// Tells if the queue can be used from many threads.
        ///
        /// Derived classes can re-implement this method. If this method
        /// provides incorrect information then the following two cases lead
        /// to specific problems:
        /// 1. The application is in a single thread mode or does not use the
        ///    queue from many threads concurrently but the method tells the
        ///    queue can be used concurrently:
        ///    the methods with timeouts will wait for the specified timeout
        ///    even if it is an infinite one.
        /// 2. The application is multithreaded and the queue is used from many
        ///    threads but the method tells the queue cannot be used
        ///    concurrently:
        ///    the methods with timeouts will return right away without waiting
        ///    during the specified timeout.
        static bool  IsUsedConcurrently(void)
        {
            return CThread::GetThreadsCount() > 0;
        }

        /// At the moment the only requirement to the traits is to provide
        /// a static function which tells f the queue can be used from many
        /// threads (IsUsedConcurrently()).
        /// Later on the traits could be extended while the CSyncQueue template
        /// arguments will not be changed.
        /// To make sure that future extensions of the traits won't break the
        /// existing user code the alternative implementations of the
        /// CSyncQueue traits must derive from CSyncQueue_DefaultTraits.
        static void
        CSyncQueue_Traits_Must_Be_Inherited_From_CSyncQueue_DefaultTraits(void)
        {}
};


/// Convenience trait implementation which tells that the queue can be used
/// from many threads.
class CSyncQueue_Traits_ConcurrencyOn : public CSyncQueue_DefaultTraits
{
    public:
        static bool IsUsedConcurrently(void)
        {
            return true;
        }
};

/// Convenience trait implementation which tells that the queue cannot be used
/// from many threads.
class CSyncQueue_Traits_ConcurrencyOff : public CSyncQueue_DefaultTraits
{
    public:
        static bool IsUsedConcurrently(void)
        {
            return false;
        }
};



/// Access guard to a constant CSyncQueue. This guard guarantees that
/// while it is alive the queue is locked and no other thread can change it.
/// So you can freely iterate through queue - guard gives access to
/// iterators. However this guard does not allow you to change the queue - you
/// can change it if you still have non-const reference to object but you
/// cannot do it using methods of this guard itself. For this purpose you are
/// to use CSyncQueue_AccessGuard object.
///
/// Full description and examples of using look here:
/// @ref CSyncQueueDescription.

template <class Type, class Container, class Traits>
class CSyncQueue_ConstAccessGuard
{
public:
    /// Queue type that this object can guard
    typedef const CSyncQueue<Type, Container, Traits>   TQueue;
    /// Type of size of the queue
    typedef typename TQueue::TSize                      TSize;
    /// Type of values stored in the queue
    typedef typename TQueue::TValue                     TValue;
    /// Type of iterator returned from this guard
    typedef typename TQueue::TConstIterator             TIterator;
    /// For convinience - type of constant iterator
    typedef TIterator                                   TConstIterator;

#ifdef NCBI_COMPILER_WORKSHOP
    typedef typename TQueue::TNativeConstIter           TNativeConstIter;
    /// Type of reverse iterator returned from this guard
    typedef
    reverse_iterator<TIterator,
        typename TNativeConstIter::iterator_category,
        const TValue>                                   TRevIterator;
#else
    /// Type of reverse iterator returned from this guard
    typedef reverse_iterator<TIterator>                 TRevIterator;
#endif
    /// For convinience - type of reverse constant iterator
    typedef TRevIterator                                TRevConstIterator;

    /// Constructor -- locks a queue
    ///
    /// @param queue_to_guard
    ///   Queue object to lock
    CSyncQueue_ConstAccessGuard(TQueue& queue_to_guard);

    /// Destructor -- unlocks the guarded queue
    ~CSyncQueue_ConstAccessGuard();


    /// Get reference to queue which this object guards to use its constant
    /// access methods
    TQueue& Queue(void) const;

    /// Get iterator pointing to the head of the queue
    TIterator Begin(void);

    /// Get iterator pointing to the tail of the queue
    TIterator End(void);

    /// Get reverse iterator pointing to the tail of the queue
    TRevIterator RBegin(void);

    /// Get reverse iterator pointing to the head of the queue
    TRevIterator REnd(void);

private:
    // Prohibit assignment and copy constructor
    typedef CSyncQueue_ConstAccessGuard<Type, Container, Traits> TThisType;
    CSyncQueue_ConstAccessGuard(const TThisType&);
    TThisType& operator= (const TThisType&);

    /// Base type for iterator and const_iterator for the queue
    typedef CSyncQueue_I_Base<Type, Container>          TIterBase;

    /// Add iterator to the list of iterators owned by this object
    void x_AddIter(TIterBase* iter);

    /// Remove iterator from the list of iterators owned by this object
    void x_RemoveIter(TIterBase* iter);


    /// The queue object that this guard locks
    TQueue& m_Queue;

    /// List of iterators owned by this guard
    list<TIterBase*> m_Iters;

    // friends
    friend
    class CSyncQueue_I<Type, Container,
                       typename TQueue::TNativeIter, Traits>;
    friend
    class CSyncQueue_I<Type, Container,
                       typename TQueue::TNativeConstIter, Traits>;
};




/// Access guard to non-constant CSyncQueue. This guard guarantees that
/// while it is alive the queue is locked and no other thread can change it.
/// So you can freely iterate through queue and change it. All changes can
/// be done via methods of this quardian or via methods of the queue itself.
///
/// Full description and examples of using look here:
/// @ref CSyncQueueDescription.

template <class Type, class Container, class Traits>
class CSyncQueue_AccessGuard
    : public CSyncQueue_ConstAccessGuard<Type, Container, Traits>
{
public:
    /// Short name of the ancestor class
    typedef CSyncQueue_ConstAccessGuard<Type, Container, Traits>
                                                           TBaseType;
    /// Queue type that this object can guard
    typedef CSyncQueue<Type, Container, Traits>            TQueue;
    /// Type of size of the queue
    typedef typename TQueue::TSize                         TSize;
    /// Type of values stored in the queue
    typedef typename TQueue::TValue                        TValue;
    /// Type of iterator returned from this guard
    typedef typename TQueue::TIterator                     TIterator;
    /// Type of constant iterator returned from this guard
    typedef typename TQueue::TConstIterator                TConstIterator;

#ifdef NCBI_COMPILER_WORKSHOP
    typedef typename TQueue::TNativeIter                   TNativeIter;
    /// Type of reverse iterator returned from this guard
    typedef
    reverse_iterator<TIterator,
                typename TNativeIter::iterator_category,
                TValue>                                    TRevIterator;
#else
    /// Type of reverse iterator returned from this guard
    typedef reverse_iterator<TIterator>                    TRevIterator;
#endif
    /// Type of reverse constant iterator returned from this guard
    typedef typename TBaseType::TRevIterator               TRevConstIterator;

    /// Constructor locking given queue
    ///
    /// @param guard_queue
    ///   Queue object to lock
    CSyncQueue_AccessGuard(TQueue& queue_to_guard);

    /// Get reference to queue which this object guards to use its constant
    /// access methods
    TQueue& Queue(void) const;

    /// Erase one element in the queue.
    /// What iterators does this method invalidate depends on underlying
    /// queue container. Refer to its documentation for more information.
    ///
    /// @param iter
    ///   Iterator pointing to the element to be deleted
    /// @return
    ///   Iterator pointing to the first element after deleted one
    TIterator Erase(TIterator iter);

    /// Erase several elements in the queue.
    /// What iterators does this method invalidate depends on underlying
    /// queue container. Refer to its documentation for more information.
    ///
    /// @param from_iter
    ///   Iterator pointing to the start of elements block to be deleted
    /// @param to_iter
    ///   Iterator pointing to the end of elements block to be deleted
    ///   (one element after last element to delete)
    /// @return
    ///   Iterator pointing to the first element after deleted ones
    //    If to_iter < from_iter throw CSyncQueueException.
    TIterator Erase(TIterator from_iter, TIterator to_iter);

    /// Get iterator pointing to the head of the queue
    TIterator Begin(void);

    /// Get iterator pointing to the tail of the queue
    TIterator End(void);

    /// Get reverse iterator pointing to the tail of the queue
    TRevIterator RBegin(void);

    /// Get reverse iterator pointing to the head of the queue
    TRevIterator REnd(void);

private:
    // Prohibit assignment and copy constructor
    typedef CSyncQueue_AccessGuard<Type, Container, Traits> TThisType;
    CSyncQueue_AccessGuard(const TThisType&);
    TThisType& operator= (const TThisType&);
};



/// Helper template returning some Type when it's not equal to NotType.
/// When Type is equal to NotType this template will return itself which is
/// senseless but makes possible to distinguish.
template <class Type, class NotType>
struct GetTypeWhenNotEqual {
    typedef Type  Result;
};

template <class Type>
struct GetTypeWhenNotEqual<Type, Type> {
    typedef GetTypeWhenNotEqual<Type, Type>  Result;
};


/// Base class for both iterator and const_iterator for SyncQueue
/// Used internally for unifying storage of iterators in guardian.
template <class Type, class Container>
class CSyncQueue_I_Base
{
public:
    /// Invalidate this iterator. When iterator is invalid all methods
    /// throw CSyncQueueException
    virtual void Invalidate(void) = 0;

    /// Virtual destructor
    virtual ~CSyncQueue_I_Base(void) {}
};


/// Iterator for CSyncQueue
/// (constant or non-constant depending on template parameters).
/// All iterators can normally operate only when access guardian is active.
/// When access guardian is destroyed all iterator methods will throw
/// CSyncQueueException.
template <class Type, class Container, class TNativeIterator, class Traits>
class CSyncQueue_I : public CSyncQueue_I_Base<Type, Container>
{
public:
    /// Short name for this type
    typedef CSyncQueue_I<Type, Container,
                         TNativeIterator, Traits>           TThisType;
    /// Queue type that this object will iterate over
    typedef CSyncQueue<Type, Container, Traits>             TQueue;
    /// Type of constant access guardian
    typedef CSyncQueue_ConstAccessGuard<Type,
                                        Container, Traits>  TConstAccessGuard;
    /// Type of access guardian
    typedef CSyncQueue_AccessGuard<Type, Container, Traits> TAccessGuard;
    /// Type of the difference between to iterators
    typedef typename TNativeIterator::difference_type       TDiff;
    /// Type of reference to stored value
    typedef typename TNativeIterator::reference             TRef;
    /// Type of pointer to stored value
    typedef typename TNativeIterator::pointer               TPtr;


    /// Version of this class for non-constant iterating
    typedef typename TQueue::TIterator                      TNonConstIter;
    /// Type for internal use only: non-constant iterator type if this
    /// iterator is constant and nothing if this iterator is non-constant
    typedef typename
    GetTypeWhenNotEqual<TNonConstIter, TThisType>::Result   TInternalNonConst;


    // Copy ctors, assignment, dtor
    CSyncQueue_I(const TThisType& other);
    CSyncQueue_I(const TInternalNonConst& other);
    TThisType& operator= (const TThisType& other);
    ~CSyncQueue_I(void);

    // All arithmetic operators
    TThisType& operator++ (void);
    TThisType  operator++ (int);
    TThisType& operator-- (void);
    TThisType  operator-- (int);
    TThisType& operator+= (TDiff offset);
    TThisType& operator-= (TDiff offset);
    TThisType  operator+  (TDiff offset) const;
    TThisType  operator-  (TDiff offset) const;
    TDiff      operator-  (const TThisType& other) const;

    // Dereference
    TRef operator*  (void) const;
    TPtr operator-> (void) const;
    TRef operator[] (TDiff offset) const;

    // Comparing
    bool operator== (const TThisType& other) const;
    bool operator!= (const TThisType& other) const;
    bool operator<  (const TThisType& other) const;
    bool operator>  (const TThisType& other) const;
    bool operator<= (const TThisType& other) const;
    bool operator>= (const TThisType& other) const;

    /// Invalidate this iterator. When iterator is invalid all methods
    /// throw CSyncQueueException
    void Invalidate(void);

    /// Check that this iterator belongs to given access guardian.
    /// Throw CSyncQueueException if it does not.
    void CheckGuard(TConstAccessGuard* guard) const;

    /// Check if the iterator is valid. Throw CSyncQueueException if it is not.
    void CheckValid(void) const;

    /// Check if this iterator can be compared to or subtracted from another
    /// iterator. Throw CSyncQueueException if it cannot.
    void CheckMatched(const TThisType& other) const;


    // Typedefs to mimic standard STL containers
    typedef typename TNativeIterator::iterator_category   iterator_category;
    typedef typename TNativeIterator::value_type          value_type;
    typedef TDiff                                         difference_type;
    typedef TPtr                                          pointer;
    typedef TRef                                          reference;

private:
    /// Ctor
    /// @param guard
    ///   Access guard which this iterator will belong to
    /// @param iter
    ///   Underlying iterator - initial value of this iterator
    CSyncQueue_I(TConstAccessGuard* guard, const TNativeIterator& iter);

    /// Get underlying native iterator
    TNativeIterator x_GetBase() const;


    /// Access guard which owns this iterator
    TConstAccessGuard* m_Guard;

    /// Underlying native iterator
    TNativeIterator m_Iter;

    /// The iterator validity flag
    bool m_Valid;


    // friends
    friend class CSyncQueue_ConstAccessGuard<Type, Container, Traits>;
    friend class CSyncQueue_AccessGuard<Type, Container, Traits>;

    // Const iterator must be friend of non-const iterator. But const
    // iterator cannot be friend of itself (gives compile error on
    // gcc 2.95).
    typedef typename
    GetTypeWhenNotEqual<typename TQueue::TNativeConstIter,
                        TNativeIterator>::Result       TIntrnNativeConstIter;
    friend class CSyncQueue_I<Type, Container, TIntrnNativeConstIter, Traits>;
};



///
/// Exception object used throughout all CSyncQueue classes
///

class NCBI_XNCBI_EXPORT CSyncQueueException : public CException
{
public:
    enum EErrCode {
        /// Maximum size given is equal to zero
        eWrongMaxSize,
        /// Cannot push or pop within the given timeout
        eTimeout,
        /// Iterator belongs to an already destroyed access guardian
        eIterNotValid,
        /// An attempt to subtract or compare iterators from different
        /// access guardians
        eMismatchedIters,
        /// An attempt to erase element via iterator that belongs
        /// to another access guardian
        eWrongGuardIter,
        /// An attempt to push element to an already full queue while
        /// the latter is locked by an access guardian
        eNoRoom,
        /// An attempt to pop element from an already empty queue while
        /// the latter is locked by an access guardian
        eEmpty,
        /// An attempt to specify the interval with iterators when "from"
        /// iterator is greater than "to" iterator
        eWrongInterval,
        /// An attempt to copy the queue to another queue which is guarded
        /// by some access guardian in the running thread
        eGuardedCopy
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CSyncQueueException, CException);
};



/// Adaptor class to use STL set<> in CSyncQueue.
/// This class inherits from set<>, and in addition implements
/// methods push_back(), front() and pop_back().
/// @note  Not all operations on set iterators are permitted, so not
///        all operations on CSyncQueue iterators will compile.

template <class Key,
          class Compare   = less<Key>,
          class Allocator = allocator<Key> >
class CSyncQueue_set
    : public set<Key, Compare, Allocator>
{
public:
    typedef set<Key, Compare, Allocator>  TBaseType;

    CSyncQueue_set() : TBaseType()  {}

    void push_back(const typename TBaseType::value_type& elem)
    { TBaseType::insert(elem);              }
    typename TBaseType::const_reference front() const
    { return *TBaseType::begin();           }
    void pop_front()
    { TBaseType::erase(TBaseType::begin()); }
};



/// Adaptor class to use STL multiset<> in CSyncQueue.
/// This class inherites from multiset<>, and in addition implements
/// methods push_back(), front() and pop_back().
/// @note  Not all operations on multiset<> iterators are permitted, so not
///        all operations on CSyncQueue iterators will compile.

template <class Key,
          class Compare   = less<Key>,
          class Allocator = allocator<Key> >
class CSyncQueue_multiset
    : public multiset<Key, Compare, Allocator>
{
public:
    typedef multiset<Key, Compare, Allocator>  TBaseType;

    CSyncQueue_multiset() : TBaseType() {}

    void push_back(const typename TBaseType::value_type& elem) {
        // Without std:: MSVC8 doesn't compile this code
        TBaseType::insert(std::upper_bound(TBaseType::begin(),
                                           TBaseType::end(),
                                           elem,
                                           TBaseType::key_comp()), elem);
    }
    typename TBaseType::const_reference front() const
    { return *TBaseType::begin();                                 }
    void pop_front()
    { TBaseType::erase(TBaseType::begin());                       }
    typename TBaseType::iterator erase(typename TBaseType::iterator iter) {
        typename TBaseType::iterator res = iter;
        ++res;
        TBaseType::erase(iter);
        return res;
    }
    typename TBaseType::iterator erase(typename TBaseType::iterator left,
                                       typename TBaseType::iterator right)
    {
        typename TBaseType::iterator res = right;
        TBaseType::erase(left, right);
        return res;
    }
};



/// Adaptor class to use STL priority_queue<> in CSyncQueue.
/// This class inherites from priority_queue<>, and in addition implements
/// methods push_back(), front(), pop_back() and clear().
/// @note  As priority_queue<> does not implement iterators, so neither
///        CSyncQueue::CopyTo() nor any iterator-using methods of
///        CSyncQueue::TAccessGuard will compile.

template <class Type,
          class Container = vector<Type>,
          class Compare   = less<typename Container::value_type> >
class CSyncQueue_priority_queue
    : public priority_queue<Type, Container, Compare>
{
public:
    typedef priority_queue<Type, Container, Compare>  TBaseType;

    // Fake types to force overall code to compile
    typedef typename Container::difference_type       difference_type;
    typedef typename Container::const_pointer         const_pointer;
    typedef typename Container::pointer               pointer;
    typedef typename Container::iterator              iterator;
    typedef typename Container::const_iterator        const_iterator;

    CSyncQueue_priority_queue() : TBaseType() {}

    void push_back(const typename TBaseType::value_type& elem)
    { TBaseType::push(elem);                          }
    typename TBaseType::const_reference front() const
    { return TBaseType::top();                        }
    void pop_front()
    { TBaseType::pop();                               }
    void clear()
    { while ( !TBaseType::empty() ) TBaseType::pop(); }
};




// --------------------------------------
// All template methods implementation
// --------------------------------------


/// Throw an exception about expired timeout with standard message
NCBI_NORETURN
inline void ThrowSyncQueueTimeout(void) {
    NCBI_THROW(CSyncQueueException, eTimeout,
               "Cannot obtain necessary queue state within a given timeout.");
}


/// Throw an exception about no room in a queue with standard message
NCBI_NORETURN
inline void ThrowSyncQueueNoRoom(void) {
    NCBI_THROW(CSyncQueueException, eNoRoom,
               "The queue has reached its size limit. "
               "Cannot push to it anymore.");
}


/// Throw an exception about empty queue with standard message
NCBI_NORETURN
inline void ThrowSyncQueueEmpty(void) {
    NCBI_THROW(CSyncQueueException, eEmpty,
               "The queue is empty. Can't pop from it any value.");
}



/// Auto-lock the queue and unlock it when object will be destroyed.
/// For internal use in CSyncQueue only. Do not use it in your applications.
/// Use CSyncQueue_AccessLock instead.

template <class Type, class Container, class Traits>
class CSyncQueue_InternalAutoLock
{
public:
    /// Short name for queue type that this object can lock
    typedef CSyncQueue<Type, Container, Traits>   TQueue;

    /// Default ctor
    CSyncQueue_InternalAutoLock() : m_Queue(NULL)  {}

    /// Constructor -- lock the queue and waiting for its lock
    /// for a given timeout
    ///
    /// @param pqueue
    ///   Queue to lock
    /// @param timeout
    ///   Time period to wait until the queue can be locked.
    ///   If NULL then wait infinitely.
    CSyncQueue_InternalAutoLock(const TQueue*     pqueue,
                                const CTimeSpan*  timeout = NULL)
        : m_Queue(NULL)
    {
        if (!Lock(pqueue, timeout)) {
            ThrowSyncQueueTimeout();
        }
    }

    /// Destructor -- unlock the queue
    ~CSyncQueue_InternalAutoLock() {
        Unlock();
    }

    /// Lock a queue
    ///
    /// @param pqueue
    ///   Queue to lock
    /// @param timeout
    ///   Time period to wait until the queue can be locked.
    ///   If NULL then wait infinitely.
    /// @return
    ///   TRUE if queue is locked successfully,
    ///   FALSE if timeout is exceeded
    bool Lock(const TQueue* pqueue, const CTimeSpan* timeout = NULL)
    {
        Unlock();
        bool result = pqueue->x_Lock(timeout);
        if (result) {
            m_Queue = pqueue;
        }
        return result;
    }

    /// Unlock the queue
    void Unlock() {
        if (m_Queue)
            m_Queue->x_Unlock();
        m_Queue = NULL;
    }

private:
    // Prohibit assignment and copy constructor
    typedef CSyncQueue_InternalAutoLock<Type, Container, Traits> TMyType;
    CSyncQueue_InternalAutoLock(const TMyType&);
    TMyType& operator= (const TMyType&);

    /// The queue that this object locks
    const TQueue* m_Queue;
};



//   CSyncQueue

template <class Type, class Container, class Traits>
inline
CSyncQueue<Type, Container, Traits>::CSyncQueue(TSize max_size)
    : m_Size(0),
      m_MaxSize(max_size),
      m_TrigLock(1, 1),
      // Setting maximum to kMax_Int to avoid crushes in some race conditions.
      // Without races 1 is enough here
      m_TrigNotEmpty(0, kMax_Int),
      m_TrigNotFull (0, kMax_Int),
      m_CurGuardTID(TThreadSystemID(kThreadSystemID_None))
{
    if (max_size == 0) {
        NCBI_THROW(CSyncQueueException, eWrongMaxSize,
                   "Maximum size of the queue must be greater than zero");
    }

    // This is only to enforce inheritance of the Traits class from
    // CSyncQueue_DefaultTraits class.
    Traits::CSyncQueue_Traits_Must_Be_Inherited_From_CSyncQueue_DefaultTraits();
}


template <class Type, class Container, class Traits>
inline
bool CSyncQueue<Type, Container, Traits>::IsEmpty(void) const
{
    return m_Size == 0;
}


template <class Type, class Container, class Traits>
inline
bool CSyncQueue<Type, Container, Traits>::IsFull(void) const
{
    return m_Size >= m_MaxSize;
}


template <class Type, class Container, class Traits>
inline
bool CSyncQueue<Type, Container, Traits>::x_Lock(const CTimeSpan* timeout) const
{
    if (timeout) {
        if ( !m_TrigLock.TryWait(CTimeout(*timeout)) ) {
            return false;
        }
    }
    else {
        m_TrigLock.Wait();
    }

    return true;
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_Unlock(void) const
{
    if (!IsFull()   &&  m_CntWaitNotFull.Get() > 0) {
        m_TrigNotFull.Post();
    }
    if (!IsEmpty()  &&  m_CntWaitNotEmpty.Get() > 0) {
        m_TrigNotEmpty.Post();
    }

    m_TrigLock.Post();
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_DoubleLock(
                                                TAutoLock*       my_lock,
                                                TAutoLock*       other_lock,
                                                const TThisType& other_obj)
    const
{
    // The order of locking is significant
    bool is_success = false;
    if (this < &other_obj) {
        is_success = my_lock->Lock(this)  &&  other_lock->Lock(&other_obj);
    }
    else {
        is_success = other_lock->Lock(&other_obj)  &&  my_lock->Lock(this);
    }

    if (!is_success) {
        ThrowSyncQueueTimeout();
    }
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_LockAndWait(
                                                TAutoLock*       lock,
                                                const CTimeSpan* full_tmo,
                                                const CTimeSpan* service_tmo,
                                                TCheckFunc       func_to_check,
                                                CSemaphore*      trigger,
                                                CAtomicCounter*  counter,
                                                TErrorThrower    throw_error)
    const
{
   // TODO: change to use CTimeout instead
    auto_ptr<CTimeSpan> real_timeout;

    if (full_tmo) {
        real_timeout.reset(new CTimeSpan(*full_tmo));
    }
    #if defined(NCBI_THREADS)
    else if (!Traits::IsUsedConcurrently()) {
        // If the queue is not used from many threads then we will wait forever.
        // Avoid it and let's think that timeout was ran over.
        real_timeout.reset(new CTimeSpan(0.0));
    }
    #else
    else {
        // Single-thread mode
        real_timeout.reset(new CTimeSpan(0.0));
    }
    #endif

    if (real_timeout.get()) {
        // finite timeout
        CStopWatch timer(CStopWatch::eStart);
        if (!lock->Lock(this, service_tmo)) {
            throw_error();
        }

        while ( (this->*func_to_check)() ) {
            CTimeSpan tmo(real_timeout->GetAsDouble() - timer.Elapsed());
            if (tmo.GetSign() != ePositive) {
                throw_error();
            }

            // Counter is checked only in locked queue. So we have to
            // increase it before unlocking.
            counter->Add(1);
            lock->Unlock();

            // TODO: change 'tmo' to CTimeout!
            bool is_success = trigger->TryWait(CTimeout(tmo));

            // To minimize unnecessary semaphore increasing we decrease
            // the counter asap, before we can acquire queue lock.
            counter->Add(-1);
            if ( !is_success ) {
                throw_error();
            }

            tmo = CTimeSpan(real_timeout->GetAsDouble() - timer.Elapsed());
            if (tmo.GetSign() != ePositive) {
                throw_error();
            }

            if (!lock->Lock(this, &tmo)) {
                throw_error();
            }
        }
    }
    else {
        // infinite timeout
        // There is no timeout, so it can not be any throw_error
        lock->Lock(this, service_tmo);
        while ( (this->*func_to_check)() ) {
            // Counter is checked only in locked queue. So we have to
            // increase it before unlocking.
            counter->Add(1);
            lock->Unlock();
            trigger->Wait();
            // To minimize unnecessary semaphore increasing we decrease
            // the counter asap, before we can acquire queue lock.
            counter->Add(-1);
            lock->Lock(this);
        }
    }
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>
    ::x_LockAndWaitWhileFull(TAutoLock*       lock, 
                             const CTimeSpan* full_tmo,
                             const CTimeSpan* service_tmo)
    const
{
    x_LockAndWait(lock, full_tmo, service_tmo, &TThisType::IsFull,
                  &m_TrigNotFull, &m_CntWaitNotFull, &ThrowSyncQueueNoRoom);
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>
    ::x_LockAndWaitWhileEmpty(TAutoLock* lock, 
                             const CTimeSpan* full_tmo,
                             const CTimeSpan* service_tmo)
    const
{
    x_LockAndWait(lock, full_tmo, service_tmo, &TThisType::IsEmpty,
                  &m_TrigNotEmpty, &m_CntWaitNotEmpty, &ThrowSyncQueueEmpty);
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_GuardedLock(
                                                const CTimeSpan* timeout) const
{
    if (x_IsGuarded()) {
        ++m_LockCount;
    }
    else {
        if (!x_Lock(timeout)) {
            ThrowSyncQueueTimeout();
        }

        // Thread Checker says this races with setting in x_GuardedUnlock. But
        // it's not because this happens only after wait on m_TrigLock's value
        // (to be reset to 0) and in x_GuardedUnlock it happens before
        // m_TrigLock's value raised to 1.
        CThread::GetSystemID(&m_CurGuardTID);
        m_LockCount = 1;
    }
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_GuardedUnlock(void) const
{
    _ASSERT( x_IsGuarded() );

    --m_LockCount;
    if (0 == m_LockCount) {
        // Thread Checker says this races with setting in x_GuardedLock. But
        // it's not. See comment above for details.
        m_CurGuardTID = TThreadSystemID(kThreadSystemID_None);
        x_Unlock();
    }
}


template <class Type, class Container, class Traits>
inline
bool CSyncQueue<Type, Container, Traits>::x_IsGuarded(void) const
{
    if (m_CurGuardTID == TThreadSystemID(kThreadSystemID_None))
        return false;

    TThreadSystemID thread_id;
    CThread::GetSystemID(&thread_id);
    return m_CurGuardTID == thread_id;
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TSize
    CSyncQueue<Type, Container, Traits>::GetSize(void) const
{
    return m_Size;
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TSize
    CSyncQueue<Type, Container, Traits>::GetMaxSize(void) const
{
    return m_MaxSize;
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_Push_NonBlocking(const TValue& elem)
{
    // NOTE. This check is active only when the queue is under access guard
    if ( IsFull() ) {
        ThrowSyncQueueNoRoom();
    }

    m_Store.push_back(elem);
    ++m_Size;
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TValue
    CSyncQueue<Type, Container, Traits>::x_Pop_NonBlocking(void)
{
    // NOTE. This check is active only when the queue is under access guard
    if (IsEmpty()) {
        ThrowSyncQueueEmpty();
    }

    TValue elem = m_Store.front();
    m_Store.pop_front();
    --m_Size;
    return elem;
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::x_Clear_NonBlocking(void)
{
    m_Store.clear();
    m_Size = 0;
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::Push(const TValue&    elem,
                                               const CTimeSpan* full_tmo,
                                               const CTimeSpan* service_tmo)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        x_LockAndWaitWhileFull(&lock, full_tmo, service_tmo);
    }

    x_Push_NonBlocking(elem);
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::Push(const TValue&    elem,
                                               const CTimeSpan* timeout)
{
    Push(elem, timeout, timeout);
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TValue
    CSyncQueue<Type, Container, Traits>::Pop(const CTimeSpan* full_tmo,
                                             const CTimeSpan* service_tmo)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        x_LockAndWaitWhileEmpty(&lock, full_tmo, service_tmo);
    }

    return x_Pop_NonBlocking();
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TValue
CSyncQueue<Type, Container, Traits>::Pop(const CTimeSpan* timeout)
{
    return Pop(timeout, timeout);
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::Clear(const CTimeSpan* timeout)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        if (!lock.Lock(this, timeout)) {
            ThrowSyncQueueTimeout();
        }
    }

    x_Clear_NonBlocking();
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue<Type, Container, Traits>::CopyTo(TThisType* other) const
{
    if (this != other) {
        TAutoLock my_lock;
        TAutoLock other_lock;

        // Some complicated locking which anyway is robust only when
        // both queues guarded or both not guarded
        if ( x_IsGuarded() ) {
            if ( !other->x_IsGuarded() ) {
                if (!other_lock.Lock(other)) {
                    ThrowSyncQueueTimeout();
                }
            }
        }
        else if ( other->x_IsGuarded() ) {
            NCBI_THROW(CSyncQueueException, eGuardedCopy,
                       "Cannot copy queue to another queue locked with "
                       "access guardian");
        }
        else {
            x_DoubleLock(&my_lock, &other_lock, *other);
        }

        if (other->m_Size + m_Size > other->m_MaxSize) {
            NCBI_THROW(CSyncQueueException, eNoRoom,
                       "Queue copy cannot be done due to the lack of "
                       "room in the destination queue.");
        }

        copy(m_Store.begin(), m_Store.end(), back_inserter(other->m_Store));
        other->m_Size += m_Size;
    }
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeIter
    CSyncQueue<Type, Container, Traits>::x_Begin()
{
    return m_Store.begin();
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeConstIter
    CSyncQueue<Type, Container, Traits>::x_Begin() const
{
    return m_Store.begin();
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeIter
    CSyncQueue<Type, Container, Traits>::x_End()
{
    return m_Store.end();
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeConstIter
    CSyncQueue<Type, Container, Traits>::x_End() const
{
    return m_Store.end();
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeIter
    CSyncQueue<Type, Container, Traits>::x_Erase(TNativeIter iter)
{
    TNativeIter res = m_Store.erase(iter);
    --m_Size;
    return res;
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue<Type, Container, Traits>::TNativeIter
    CSyncQueue<Type, Container, Traits>::x_Erase(TNativeIter from_iter,
                                                 TNativeIter to_iter)
{
    // Count number of elements to delete. Some container's iterators
    // don't have operator- and operator<. So we have to imitate them.
    TSize delta = 0;
    TNativeIter srch_iter = from_iter;
    while (srch_iter != m_Store.end()  &&  srch_iter != to_iter) {
        ++delta;
        ++srch_iter;
    }

    if (to_iter != srch_iter) {
        NCBI_THROW(CSyncQueueException, eWrongInterval,
                   "The beginning of interval must be less or equal "
                   "to the end of interval.");
    }

    TNativeIter res = m_Store.erase(from_iter, to_iter);
    m_Size -= delta;

    return res;
}



//   CSyncQueue_I

template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>::
CSyncQueue_I(TConstAccessGuard* guard, const TNativeIterator& iter)
    : m_Guard(guard),
      m_Iter (iter),
      m_Valid(false)
{
    m_Guard->x_AddIter(this);
    m_Valid = true;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>::~CSyncQueue_I(void)
{
    if ( m_Valid )
        m_Guard->x_RemoveIter(this);
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>::
CSyncQueue_I(const TThisType& other)
    : m_Valid(false)
{
    *this = other;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>::
CSyncQueue_I(const TInternalNonConst& other)
    : m_Guard(other.m_Guard),
      m_Iter (other.m_Iter),
      m_Valid(other.m_Valid)
{
    if ( m_Valid )
        m_Guard->x_AddIter(this);
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>&
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>
        ::operator= (const TThisType& other)
{
    if ( m_Valid )
        m_Guard->x_RemoveIter(this);

    m_Guard = other.m_Guard;
    m_Iter  = other.m_Iter;
    m_Valid = other.m_Valid;

    if ( m_Valid )
        m_Guard->x_AddIter(this);

    return *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
void CSyncQueue_I<Type, Container, TNativeIterator, Traits>::Invalidate(void)
{
    m_Guard->x_RemoveIter(this);
    m_Valid = false;
    m_Guard = NULL;
    m_Iter = TNativeIterator();
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
void CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::CheckGuard(TConstAccessGuard *guard) const
{
    if (m_Guard != guard) {
        NCBI_THROW(CSyncQueueException, eWrongGuardIter,
                   "Cannot work with iterators from another access guards.");
    }
}

template <class Type, class Container, class TNativeIterator, class Traits>
inline
void CSyncQueue_I<Type, Container,
                  TNativeIterator, Traits>::CheckValid(void) const
{
    if ( !m_Valid ) {
        NCBI_THROW(CSyncQueueException, eIterNotValid,
                   "Iterator can't be used after "
                   "destroying related access guard.");
    }
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
void CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::CheckMatched(const TThisType& other) const
{
    if (m_Guard != other.m_Guard) {
        NCBI_THROW(CSyncQueueException, eMismatchedIters,
                   "Cannot compare iterators from different queue guards.");
    }
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
TNativeIterator
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>::x_GetBase() const
{
    return m_Iter;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>&
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>::operator++ (void)
{
    CheckValid();

    ++m_Iter;
    return *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>::operator++ (int)
{
    CheckValid();

    TThisType tmp(*this);
    m_Iter++;
    return tmp;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>&
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>::operator-- (void)
{
    CheckValid();

    --m_Iter;
    return *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>::operator-- (int)
{
    CheckValid();

    TThisType tmp(*this);
    m_Iter--;
    return tmp;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>&
    CSyncQueue_I<Type, Container,
                 TNativeIterator, Traits>::operator+= (TDiff offset)
{
    CheckValid();

    m_Iter += offset;
    return *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>&
    CSyncQueue_I<Type, Container,
                 TNativeIterator, Traits>::operator-= (TDiff offset)
{
    CheckValid();

    m_Iter -= offset;
    return *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>
        ::operator+ (TDiff offset) const
{
    CheckValid();

    TThisType tmp(*this);
    tmp.m_Iter = tmp.m_Iter + offset;
    return tmp;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits> operator+
(typename CSyncQueue_I<Type, Container, TNativeIterator, Traits>::TDiff  offset,
 const    CSyncQueue_I<Type, Container, TNativeIterator, Traits>&        iter)
{
    return iter + offset;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>
        ::operator- (TDiff offset) const
{
    CheckValid();

    TThisType tmp(*this);
    tmp.m_Iter = tmp.m_Iter - offset;
    return tmp;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator, Traits>::TDiff
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>
        ::operator- (const TThisType& other) const
{
    CheckValid();
    other.CheckValid();
    CheckMatched(other);

    return m_Iter - other.m_Iter;
}


// Additional difference between const and non-const iterators
template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
typename CSyncQueue_I<Type, Container, TNativeIterL, Traits>::TDiff
    operator- (const CSyncQueue_I<Type, Container, TNativeIterL, Traits>&  left,
               const CSyncQueue_I<Type, Container, TNativeIterR, Traits>&  right)
{
    typedef typename
        CSyncQueue<Type, Container, Traits>::TAccessGuard::TConstIterator
                                                                TConstIterator;

    return TConstIterator(left) - TConstIterator(right);
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator, Traits>::TRef
    CSyncQueue_I<Type, Container,
                 TNativeIterator, Traits>::operator* (void) const
{
    CheckValid();

    return *m_Iter;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator, Traits>::TPtr
    CSyncQueue_I<Type, Container,
                 TNativeIterator, Traits>::operator-> (void) const
{
    CheckValid();

    return m_Iter.operator->();
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator, Traits>::TRef
    CSyncQueue_I<Type, Container, TNativeIterator, Traits>
        ::operator[] (TDiff offset) const
{
    CheckValid();

    return m_Iter[offset];
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator== (const TThisType& other) const
{
    CheckMatched(other);

    return m_Iter == other.m_Iter;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator!= (const TThisType& other) const
{
    return !(*this == other);
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator< (const TThisType& other) const
{
    CheckMatched(other);

    return m_Iter < other.m_Iter;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator> (const TThisType& other) const
{
    return other < *this;
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator<= (const TThisType& other) const
{
    return !(other < *this);
}


template <class Type, class Container, class TNativeIterator, class Traits>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator, Traits>
    ::operator>= (const TThisType& other) const
{
    return !(*this < other);
}


// Additional comparing between const and non-const iterators

template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator== (const CSyncQueue_I<Type, Container,
                                    TNativeIterL, Traits>&  left,
                 const CSyncQueue_I<Type, Container,
                                    TNativeIterR, Traits>&  right)
{
    typedef
        typename CSyncQueue<Type, Container, Traits>::TConstIterator
                                                            TConstIterator;

    return TConstIterator(left) == TConstIterator(right);
}


template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator!= (const CSyncQueue_I<Type, Container,
                                    TNativeIterL, Traits>&  left,
                 const CSyncQueue_I<Type, Container,
                                    TNativeIterR, Traits>&  right)
{
    return !(left == right);
}


template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator< (const CSyncQueue_I<Type, Container,
                                   TNativeIterL, Traits>&  left,
                const CSyncQueue_I<Type, Container,
                                   TNativeIterR, Traits>&  right)
{
    typedef
        typename CSyncQueue<Type, Container, Traits>::TConstIterator
                                                            TConstIterator;

    return TConstIterator(left) < TConstIterator(right);
}


template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator> (const CSyncQueue_I<Type, Container,
                                   TNativeIterL, Traits>&  left,
                const CSyncQueue_I<Type, Container,
                                   TNativeIterR, Traits>&  right)
{
    return right < left;
}


template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator<= (const CSyncQueue_I<Type, Container,
                                    TNativeIterL, Traits>&  left,
                 const CSyncQueue_I<Type, Container,
                                    TNativeIterR, Traits>&  right)
{
    return !(right < left);
}


template <class Type, class Container,
          class TNativeIterL, class TNativeIterR, class Traits>
inline
bool operator>= (const CSyncQueue_I<Type, Container,
                                    TNativeIterL, Traits>&  left,
                 const CSyncQueue_I<Type, Container,
                                    TNativeIterR, Traits>&  right)
{
    return !(left < right);
}



//   CSyncQueue_ConstAccessGuard

template <class Type, class Container, class Traits>
inline
CSyncQueue_ConstAccessGuard<Type, Container, Traits>::
CSyncQueue_ConstAccessGuard(TQueue& queue_to_guard)
    : m_Queue(queue_to_guard)
{
    m_Queue.x_GuardedLock();
}


template <class Type, class Container, class Traits>
inline
CSyncQueue_ConstAccessGuard<Type, Container,
                            Traits>::~CSyncQueue_ConstAccessGuard()
{
    NON_CONST_ITERATE(typename list<TIterBase*>, it, m_Iters)
    {
        (*it)->Invalidate();
    }

    m_Queue.x_GuardedUnlock();
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue_ConstAccessGuard<Type, Container,
                                 Traits>::x_AddIter(TIterBase* iter)
{
    m_Iters.push_back(iter);
}


template <class Type, class Container, class Traits>
inline
void CSyncQueue_ConstAccessGuard<Type, Container, Traits>
    ::x_RemoveIter(TIterBase* iter)
{
    m_Iters.remove(iter);
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_ConstAccessGuard<Type, Container, Traits>::Begin(void)
{
    return TIterator(this, m_Queue.x_Begin());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_ConstAccessGuard<Type, Container, Traits>::End(void)
{
    return TIterator(this, m_Queue.x_End());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container, Traits>::TRevIterator
    CSyncQueue_ConstAccessGuard<Type, Container, Traits>::RBegin(void)
{
    return TRevIterator(End());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container, Traits>::TRevIterator
    CSyncQueue_ConstAccessGuard<Type, Container, Traits>::REnd(void)
{
    return TRevIterator(Begin());
}


// Strange behaviour of MSVC compiler here:
// if 'const CSyncQueue<Type,Container>' change to
// 'typename CSyncQueue_ConstAccessGuard<Type, Container>::TQueue'
// then MSVC gives an error - it cannot propagate const specifier here
template <class Type, class Container, class Traits>
inline
const CSyncQueue<Type,Container, Traits>&
    CSyncQueue_ConstAccessGuard<Type, Container, Traits>::Queue(void) const
{
    return m_Queue;
}



//   CSyncQueue_AccessGuard

template <class Type, class Container, class Traits>
inline
CSyncQueue_AccessGuard<Type, Container, Traits>
    ::CSyncQueue_AccessGuard(TQueue& queue_to_guard)
        : TBaseType(queue_to_guard)
{}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TQueue&
    CSyncQueue_AccessGuard<Type, Container, Traits>::Queue(void) const
{
    return const_cast<TQueue&> (TBaseType::Queue());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::Erase(TIterator iter)
{
    iter.CheckGuard(this);
    return TIterator(this, Queue().x_Erase(iter.x_GetBase()));
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::Erase(TIterator  from_iter,
                                                           TIterator  to_iter)
{
    from_iter.CheckGuard(this);
    to_iter.CheckGuard(this);
    return TIterator
        (this,
         Queue().x_Erase(from_iter.x_GetBase(), to_iter.x_GetBase()));
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::Begin(void)
{
    return TIterator(this, Queue().x_Begin());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::End(void)
{
    return TIterator(this, Queue().x_End());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TRevIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::RBegin(void)
{
    return TRevIterator(End());
}


template <class Type, class Container, class Traits>
inline
typename CSyncQueue_AccessGuard<Type, Container, Traits>::TRevIterator
    CSyncQueue_AccessGuard<Type, Container, Traits>::REnd(void)
{
    return TRevIterator(Begin());
}



END_NCBI_SCOPE

#endif  /* UTIL___SYNC_QUEUE__HPP */
