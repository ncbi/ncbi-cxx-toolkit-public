#ifndef CORELIB___NCBI_QUEUE__HPP
#define CORELIB___NCBI_QUEUE__HPP

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

/// @file ncbi_queue.hpp
/// 
/// Definition of synchronized queue (CSyncQueue template) and templates
/// related to it.


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
template <class Type, class Container>
class CSyncQueue_InternalAutoLock;


// forward class declarations
template <class Type, class Container, class TNativeIterator>
class CSyncQueue_I;

template <class Type, class Container>
class CSyncQueue_ConstAccessGuard;

template <class Type, class Container>
class CSyncQueue_AccessGuard;



/// Thread-safe queue object with a blocking mechanism.
///
/// An attempt to pop from empty queue or push to full queue function call
/// waits for normal operation finishing.
///
/// Queue can also be locked for a
/// long time for some bulk operations. This can be achieved by
/// CSyncQueue_*AccessGuard classes. When using this queue with access
/// guardian it is mandatory to use it only in the threads created by
/// C++ Toolkit (using CThread class or some of its wrappers).
///
/// @param Type
///   Type of elements saved in queue
/// @param Container
///   Type of underlying container used in queue. To be applicable for using
///   in CSyncQueue container must have methods push_back(), front() and
///   pop_front(). For other containers you need to implement an adaptor to
///   use it in CSyncQueue. For STL set, multiset and priority_queue classes
///   adaptors implemented further in this file.

template < class Type, class Container = deque<Type> >
class CSyncQueue
{
public:
    /// Type of underlying container of the queue
    typedef Container                                TContainer;
    /// Type of values stored in the queue
    typedef typename TContainer::value_type          TValue;
    /// Type of size of the queue
    typedef typename TContainer::size_type           TSize;
    /// Type of the difference between to iterators
    typedef typename TContainer::difference_type     TDiff;
    /// Constant and reference types for values in this queue
    typedef typename TContainer::const_reference     TValue_CR;
    typedef typename TContainer::reference           TValue_R;
    typedef typename TContainer::const_pointer       TValue_CP;
    typedef typename TContainer::pointer             TValue_P;

    /// Type of access guardian for constant queue
    typedef CSyncQueue_ConstAccessGuard<Type, Container>     TConstAccessGuard;
    /// Type of access guardian for non-constant queue
    typedef CSyncQueue_AccessGuard<Type, Container>          TAccessGuard;
    /// Type of underlying iterator
    typedef typename TContainer::iterator                    TNativeIter;
    /// Type of underlying constant iterator
    typedef typename TContainer::const_iterator              TNativeConstIter;
    /// Constant iterator on this queue
    typedef CSyncQueue_I<Type, Container, TNativeConstIter>  TConstIterator;
    /// Non-constant iterator on this queue
    typedef CSyncQueue_I<Type, Container, TNativeIter>       TIterator;

#ifdef NCBI_COMPILER_WORKSHOP
    /// Constant reverse iterator on this queue
    typedef
    reverse_iterator<TConstIterator,
           typename TNativeConstIter::iterator_category,
           const TValue>                                     TRevConstIterator;
    /// Non-constant reverse iterator on this queue
    typedef
    reverse_iterator<TIterator,
           typename TNativeIter::iterator_category, TValue>  TRevIterator;
#else
    /// Constant reverse iterator on this queue
    typedef reverse_iterator<TConstIterator>                 TRevConstIterator;
    /// Non-constant reverse iterator on this queue
    typedef reverse_iterator<TIterator>                      TRevIterator;
#endif

    /// Short name of this queue type
    typedef CSyncQueue<Type, Container>                      TMyType;


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

    /// Retrieve an element from the queue.
    /// @note  This call will block if the queue is empty or if there are
    ///        competing operations by other threads
    ///
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    TValue Pop(const CTimeSpan* timeout = NULL);

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
    /// @warning This call can lead to a deadlock in some situations when
    ///          either queue is locked by access guardian.
    /// @throws  CSyncQueueException Does nothing and throws with "eNoRoom"
    ///          err.code if there is not enough room in the destination queue 
    ///
    /// @param other
    ///   Another queue to which all elements will be copied
    void CopyTo(TMyType* other) const;

    // Typedefs to mimic standard STL containers
    typedef TIterator       iterator;
    typedef TConstIterator  const_iterator;


private:
    // Prohibit copy and assignment
    CSyncQueue(const TMyType&);
    TMyType& operator= (const TMyType&);

    /// Short name of auto-lock for this queue type.
    /// For internal use only.
    typedef CSyncQueue_InternalAutoLock<Type, Container> TAutoLock;

    /// Lock access to the queue.
    /// @note  This call will block if the queue is already locked by
    ///        another thread 
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void x_Lock(const CTimeSpan* timeout = NULL) const;

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
    void x_DoubleLock(TAutoLock*     my_lock,
                      TAutoLock*     other_lock,
                      const TMyType& other_obj)
        const;

    /// Calculate next timeout value from initial value and time spent
    /// in previous waits
    /// @param timeout
    ///   Initial timeout
    /// @param timer
    ///   Timer showing amount of time spent since the beginning of wait
    CTimeSpan x_GetNextTimeout(const CTimeSpan&  timeout,
                               const CStopWatch& timer)
        const;

    /// Function to check condition; usually points to IsFull() or IsEmpty()
    /// @sa x_LockAndWait()
    typedef bool (CSyncQueue::*TCheckFunc)(void) const;

    /// Lock the queue and wait until the condition function returns FALSE.
    /// @param lock
    ///   Auto-lock object to acquire the lock on the queue
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    /// @param func_to_check
    ///   Function to check condition
    /// @param trigger
    ///   Semaphore to wait for the condition
    /// @param counter
    ///   Counter of threads waiting on this semaphore
    void x_LockAndWait(TAutoLock*       lock,
                       const CTimeSpan* timeout,
                       TCheckFunc       func_to_check,
                       CSemaphore*      trigger,
                       CAtomicCounter*  counter)
        const;

    /// Lock the queue and wait until it has room for more elements
    /// @param lock
    ///   Auto-lock object  to acquire the lock on the queue
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void x_LockAndWaitWhileFull(TAutoLock*       lock,
                                const CTimeSpan* timeout)
        const;

    /// Lock the queue and wait until it has at least one element
    /// @param lock
    ///   Auto-lock object  to acquire the lock on the queue
    /// @param timeout
    ///   Maximum time period to wait on this call; NULL to wait infinitely.
    ///   If the timeout is exceeded, then throw CSyncQueueException.
    void x_LockAndWaitWhileEmpty(TAutoLock* lock, const CTimeSpan* timeout)
        const;

    /// Mark this queue as locked by an access guard in current thread
    void x_SetGuarded(void) const;

    /// Mark this queue as unlocked by an access guard in current thread
    void x_SetUnguarded(void) const;

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


    /// Underlying container to store queue elements
    TContainer m_Store;

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
    mutable CAtomicCounter m_CntWaitNotEmpty;

    /// Semaphore to signal that the queue has become not full
    mutable CSemaphore m_TrigNotFull;

    /// Number of threads waiting for the queue to become non-full
    mutable CAtomicCounter m_CntWaitNotFull;

    /// ID of the thread in which the queue has been locked by a guardian
    mutable CThread::TID m_CurGuardTID;

    //
    friend class CSyncQueue_ConstAccessGuard <Type, Container>;
    friend class CSyncQueue_AccessGuard      <Type, Container>;
    friend class CSyncQueue_InternalAutoLock <Type, Container>;
};




/// Access guard to a constant CSyncQueue. This guard guarantees that
/// while it is alive the queue is locked and no other thread can change it.
/// So you can freely iterate through queue - guard gives access to
/// iterators. However this guard does not allow you to change the queue - you
/// can change it if you still have non-const reference to object but you
/// cannot do it using methods of this guard itself. For this purpose you are
/// to use CSyncQueue_AccessGuard object.

template <class Type, class Container>
class CSyncQueue_ConstAccessGuard
{
public:
    /// Queue type that this object can guard
    typedef const CSyncQueue<Type, Container>           TQueue;
    /// Type of size of the queue
    typedef typename TQueue::TSize                      TSize;
    /// Type of values stored in the queue
    typedef typename TQueue::TValue                     TValue;
    /// Type of iterator returned from this guard
    typedef typename TQueue::TConstIterator             TIterator;

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
    typedef CSyncQueue_ConstAccessGuard<Type, Container> TMyType;
    CSyncQueue_ConstAccessGuard(const TMyType&);
    TMyType& operator= (const TMyType&);

    /// Non-constant iterator type
    typedef typename TQueue::TIterator TNonConstIterator;

    /// Add iterator to the list of iterators owned by this object
    void x_AddIter(TIterator* iter);

    /// Add non-const iterator to the list of iterators owned by this object
    void x_AddIter(TNonConstIterator* iter);

    /// Remove iterator from the list of iterators owned by this object
    void x_RemoveIter(TIterator* iter);

    /// Remove non-const iterator from the list of iterators owned
    /// by this object
    void x_RemoveIter(TNonConstIterator* iter);

    /// The queue object that this guard locks
    TQueue& m_Queue;

    /// List of iterators owned by this guard
    list<TIterator*> m_Iters;

    /// List of non-constant iterators owned by this guard
    list<TNonConstIterator*> m_NonConstIters;

    // friends
    friend
    class CSyncQueue_I<Type, Container, typename TQueue::TNativeIter>;
    friend
    class CSyncQueue_I<Type, Container, typename TQueue::TNativeConstIter>;
};




/// Access guard to non-constant CSyncQueue. This guard guarantees that
/// while it is alive the queue is locked and no other thread can change it.
/// So you can freely iterate through queue and change it. All changes can
/// be done via methods of this quardian or via methods of the queue itself.

template <class Type, class Container>
class CSyncQueue_AccessGuard
    : public CSyncQueue_ConstAccessGuard<Type, Container>
{
public:
    /// Short name of the ancestor class
    typedef CSyncQueue_ConstAccessGuard<Type, Container>   TBase;
    /// Queue type that this object can guard
    typedef CSyncQueue<Type, Container>                    TQueue;
    /// Type of size of the queue
    typedef typename TQueue::TSize                         TSize;
    /// Type of values stored in the queue
    typedef typename TQueue::TValue                        TValue;
    /// Type of iterator returned from this guard
    typedef typename TQueue::TIterator                     TIterator;

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
    typedef CSyncQueue_AccessGuard<Type, Container> TMyType;
    CSyncQueue_AccessGuard(const TMyType&);
    TMyType& operator= (const TMyType&);
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


/// Iterator for CSyncQueue
/// (constant or non-constant depending on template parameters).
/// All iterators can normally operate only when access guardian is active.
/// When access guardian is destroyed all iterator methods will throw
/// CSyncQueueException.
template <class Type, class Container, class TNativeIterator>
class CSyncQueue_I
{
public:
    /// Short name for this type
    typedef CSyncQueue_I<Type, Container, TNativeIterator>  TMyType;
    /// Queue type that this object will iterate over
    typedef CSyncQueue<Type, Container>                     TQueue;
    /// Version of this class for non-constant iterating
    typedef typename TQueue::TIterator                      TIterator;
    /// Version of this class for constant iterating
    typedef typename TQueue::TConstIterator                 TConstIterator;
    /// Type of constant access guardian
    typedef CSyncQueue_ConstAccessGuard<Type, Container>    TConstAccessGuard;
    /// Type of access guardian
    typedef CSyncQueue_AccessGuard<Type, Container>         TAccessGuard;
    /// Type of the difference between to iterators
    typedef typename TNativeIterator::difference_type       TDiff;
    /// Type of reference to stored value
    typedef typename TNativeIterator::reference             TRef;
    /// Type of pointer to stored value
    typedef typename TNativeIterator::pointer               TPtr;


    /// Type for internal use only: non-constant iterator type if this
    /// iterator is constant and nothing if this iterator is non-constant
    typedef typename
    GetTypeWhenNotEqual<TIterator, TMyType>::Result         TInternalNonConst;


    // Copy ctors, assignment, dtor
    CSyncQueue_I(const TMyType& other);
    CSyncQueue_I(const TInternalNonConst& other);
    TMyType& operator= (const TMyType& other);
    ~CSyncQueue_I(void);

    // All arithmetic operators
    TMyType& operator++ (void);
    TMyType  operator++ (int);
    TMyType& operator-- (void);
    TMyType  operator-- (int);
    TMyType& operator+= (TDiff offset);
    TMyType& operator-= (TDiff offset);
    TMyType  operator+  (TDiff offset) const;
    TMyType  operator-  (TDiff offset) const;
    TDiff    operator-  (const TMyType& other) const;

    // Dereference
    TRef operator*  (void) const;
    TPtr operator-> (void) const;
    TRef operator[] (TDiff offset) const;

    // Comparing
    bool operator== (const TMyType& other) const;
    bool operator!= (const TMyType& other) const;
    bool operator<  (const TMyType& other) const;
    bool operator>  (const TMyType& other) const;
    bool operator<= (const TMyType& other) const;
    bool operator>= (const TMyType& other) const;

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
    void CheckMatched(const TMyType& other) const;


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
    friend class CSyncQueue_ConstAccessGuard<Type, Container>;
    friend class CSyncQueue_AccessGuard<Type, Container>;

    // Const iterator must be friend of non-const iterator. But const
    // iterator cannot be friend of itself (gives compile error on
    // gcc 2.95).
    typedef typename
    GetTypeWhenNotEqual<typename TQueue::TNativeConstIter,
                        TNativeIterator>::Result       TIntrnNativeConstIter;
    friend class CSyncQueue_I<Type, Container, TIntrnNativeConstIter>;
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
        /// An attempt to guard a queue that is already guarded by another
        /// guardian in the same thread
        eDoubleGuard,
        /// An attempt to specify the interval with iterators when "from"
        /// iterator is greater than "to" iterator
        eWrongInterval
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
    typedef set<Key, Compare, Allocator>  TBase;

    CSyncQueue_set() : set<Key, Compare, Allocator>()  {}

    void push_back(const typename TBase::value_type& elem)
    { TBase::insert(elem); }
    typename TBase::const_reference front() const
    { return *TBase::begin(); }
    void pop_front()
    { TBase::erase(TBase::begin()); }
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
    typedef multiset<Key, Compare, Allocator>  TBase;

    CSyncQueue_multiset() : multiset<Key, Compare, Allocator>() {}

    void push_back(const typename TBase::value_type& elem)
    { TBase::insert(elem); }
    typename TBase::const_reference front() const
    { return *TBase::begin(); }
    void pop_front()
    { TBase::erase(TBase::begin()); }
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
    typedef priority_queue<Type, Container, Compare>  TBase;

    // Fake types to force overall code to compile
    typedef typename Container::difference_type       difference_type;
    typedef typename Container::const_pointer         const_pointer;
    typedef typename Container::pointer               pointer;
    typedef typename Container::iterator              iterator;
    typedef typename Container::const_iterator        const_iterator;

    CSyncQueue_priority_queue() : priority_queue<Type, Container, Compare>() {}

    void push_back(const typename TBase::value_type& elem)
    { TBase::push(elem); }
    typename TBase::const_reference front() const
    { return TBase::top(); }
    void pop_front()
    { TBase::pop(); }
    void clear()
    { while ( !TBase::empty() ) TBase::pop(); }
};




// --------------------------------------
// All template methods implementation
// --------------------------------------


/// Throw an exception about expired timeout with standard message
inline void ThrowSyncQueueTimeout(void) {
    NCBI_THROW(CSyncQueueException, eTimeout,
               "Cannot obtain necessary queue state within a given timeout.");
}



/// Auto-lock the queue and unlock it when object will be destroyed.
/// For internal use in CSyncQueue only. Do not use it in your applications.
/// Use CSyncQueue_AccessLock instead.

template <class Type, class Container>
class CSyncQueue_InternalAutoLock
{
public:
    /// Short name for queue type that this object can lock
    typedef CSyncQueue<Type, Container>   TQueue;

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
        Lock(pqueue, timeout);
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
    void Lock(const TQueue* pqueue, const CTimeSpan* timeout = NULL)
    {
        Unlock();
        m_Queue = pqueue;
        m_Queue->x_Lock(timeout);
    }

    /// Unlock the queue
    void Unlock() {
        if (m_Queue)
            m_Queue->x_Unlock();
        m_Queue = NULL;
    }

private:
    // Prohibit assignment and copy constructor
    typedef CSyncQueue_InternalAutoLock<Type, Container> TMyType;
    CSyncQueue_InternalAutoLock(const TMyType&);
    TMyType& operator= (const TMyType&);

    /// The queue that this object locks
    const TQueue* m_Queue;
};



//   CSyncQueue

template <class Type, class Container>
inline
CSyncQueue<Type, Container>::CSyncQueue(TSize max_size)
    : m_Size(0),
      m_MaxSize(max_size),
      m_TrigLock(1, 1),
      // Setting maximum to kMax_Int to avoid crushes in some race conditions.
      // Without races 1 is enough here
      m_TrigNotEmpty(0, kMax_Int),
      m_TrigNotFull (0, kMax_Int),
      m_CurGuardTID(kThreadID_None)
{
    if (max_size <= 0) {
        NCBI_THROW(CSyncQueueException, eWrongMaxSize,
                   "Maximum size of the queue must be greater than zero");
    }

    m_CntWaitNotEmpty.Set(0);
    m_CntWaitNotFull.Set(0);
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_Lock(const CTimeSpan* timeout) const
{
    if (timeout) {
        if ( !m_TrigLock.TryWait(timeout->GetCompleteSeconds(),
                                 timeout->GetNanoSecondsAfterSecond()) )
        {
            ThrowSyncQueueTimeout();
        }
    }
    else {
        m_TrigLock.Wait();
    }
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_Unlock(void) const
{
    if (!IsFull()   &&  m_CntWaitNotFull.Get() > 0) {
        m_TrigNotFull.Post();
    }
    if (!IsEmpty()  &&  m_CntWaitNotEmpty.Get() > 0) {
        m_TrigNotEmpty.Post();
    }

    m_TrigLock.Post();
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_DoubleLock(TAutoLock*     my_lock,
                                               TAutoLock*     other_lock,
                                               const TMyType& other_obj) const
{
    // The order of locking is significant
    if (this < &other_obj) {
        my_lock->Lock(this);
        other_lock->Lock(&other_obj);
    }
    else {
        other_lock->Lock(&other_obj);
        my_lock->Lock(this);
    }
}


template <class Type, class Container>
inline
CTimeSpan CSyncQueue<Type, Container>::x_GetNextTimeout
(const CTimeSpan&   timeout,
 const CStopWatch&  timer) const
{
    double next_wait = timeout.GetAsDouble() - timer.Elapsed();
    if (next_wait <= 0.0)
        ThrowSyncQueueTimeout();

    return CTimeSpan(next_wait);
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_LockAndWait(TAutoLock*       lock,
                                                const CTimeSpan* timeout,
                                                TCheckFunc       func_to_check,
                                                CSemaphore*      trigger,
                                                CAtomicCounter*  counter)
    const
{
    if (timeout) {
        // finite timeout
        CStopWatch timer(CStopWatch::eStart);
        lock->Lock(this, timeout);

        while ( (this->*func_to_check)() ) {
            CTimeSpan tmo = x_GetNextTimeout(*timeout, timer);

            // Counter is checked only in locked queue. So we have to
            // increase it before unlocking.
            counter->Add(1);
            lock->Unlock();
            bool is_success = trigger->TryWait(tmo.GetCompleteSeconds(),
                                               tmo.GetNanoSecondsAfterSecond());

            // To minimize unnecessary semaphore increasing we decrease
            // the counter asap, before we can acquire queue lock.
            counter->Add(-1);
            if ( !is_success ) {
                ThrowSyncQueueTimeout();
            }
            tmo = x_GetNextTimeout(*timeout, timer);
            lock->Lock(this, &tmo);
        }
    }
    else {
        // infinite timeout
        lock->Lock(this);
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


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>
    ::x_LockAndWaitWhileFull(TAutoLock* lock, const CTimeSpan* timeout) const
{
    x_LockAndWait(lock, timeout,
                  &TMyType::IsFull, &m_TrigNotFull, &m_CntWaitNotFull);
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>
    ::x_LockAndWaitWhileEmpty(TAutoLock* lock, const CTimeSpan* timeout) const
{
    x_LockAndWait(lock, timeout,
                  &TMyType::IsEmpty, &m_TrigNotEmpty, &m_CntWaitNotEmpty);
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_SetGuarded(void) const
{
    m_CurGuardTID = CThread::GetSelf();
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_SetUnguarded(void) const
{
    m_CurGuardTID = kThreadID_None;
}


template <class Type, class Container>
inline
bool CSyncQueue<Type, Container>::x_IsGuarded(void) const
{
    return
        m_CurGuardTID != kThreadID_None  &&
        m_CurGuardTID == CThread::GetSelf();
}


template <class Type, class Container>
inline
bool CSyncQueue<Type, Container>::IsEmpty(void) const
{
    return m_Size == 0;
}


template <class Type, class Container>
inline
bool CSyncQueue<Type, Container>::IsFull(void) const
{
    return m_Size >= m_MaxSize;
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TSize
    CSyncQueue<Type, Container>::GetSize(void) const
{
    return m_Size;
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TSize
    CSyncQueue<Type, Container>::GetMaxSize(void) const
{
    return m_MaxSize;
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_Push_NonBlocking(const TValue& elem)
{
    // NOTE. This check is active only when the queue is under access guard
    if ( IsFull() ) {
        NCBI_THROW(CSyncQueueException, eNoRoom,
                   "The queue has reached its size limit. "
                   "Cannot push to it anymore.");
    }

    m_Store.push_back(elem);
    ++m_Size;
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TValue
    CSyncQueue<Type, Container>::x_Pop_NonBlocking(void)
{
    // NOTE. This check is active only when the queue is under access guard
    if (IsEmpty()) {
        NCBI_THROW(CSyncQueueException, eEmpty,
                   "The queue is empty. Can't pop from it any value.");
    }

    TValue elem = m_Store.front();
    m_Store.pop_front();
    --m_Size;
    return elem;
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::x_Clear_NonBlocking(void)
{
    m_Store.clear();
    m_Size = 0;
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::Push(const TValue&    elem,
                                       const CTimeSpan* timeout)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        x_LockAndWaitWhileFull(&lock, timeout);
    }

    x_Push_NonBlocking(elem);
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TValue
    CSyncQueue<Type, Container>::Pop(const CTimeSpan* timeout)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        x_LockAndWaitWhileEmpty(&lock, timeout);
    }

    return x_Pop_NonBlocking();
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::Clear(const CTimeSpan* timeout)
{
    TAutoLock lock;

    if ( !x_IsGuarded() ) {
        lock.Lock(this, timeout);
    }

    x_Clear_NonBlocking();
}


template <class Type, class Container>
inline
void CSyncQueue<Type, Container>::CopyTo(TMyType* other) const
{
    if (this != other) {
        TAutoLock my_lock;
        TAutoLock other_lock;

        // Some complicated locking which anyway is robust only when
        // both queues guarded or both not guarded
        if ( x_IsGuarded() ) {
            if ( !other->x_IsGuarded() ) {
                other_lock.Lock(other);
            }
        }
        else if ( other->x_IsGuarded() ) {
            my_lock.Lock(this);
        }
        else {
            x_DoubleLock(&my_lock, &other_lock, *other);
        }

        if (other->m_Size + m_Size > other->m_MaxSize) {
            NCBI_THROW(CSyncQueueException, eNoRoom,
                       "Queue copy cannot be done due to the lack of "
                       "room in the destination queue");
        }

        copy(m_Store.begin(), m_Store.end(), back_inserter(other->m_Store));
        other->m_Size += m_Size;
    }
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeIter
    CSyncQueue<Type, Container>::x_Begin()
{
    return m_Store.begin();
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeConstIter
    CSyncQueue<Type, Container>::x_Begin() const
{
    return m_Store.begin();
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeIter
    CSyncQueue<Type, Container>::x_End()
{
    return m_Store.end();
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeConstIter
    CSyncQueue<Type, Container>::x_End() const
{
    return m_Store.end();
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeIter
    CSyncQueue<Type, Container>::x_Erase(TNativeIter iter)
{
    TNativeIter res = m_Store.erase(iter);
    --m_Size;
    return res;
}


template <class Type, class Container>
inline
typename CSyncQueue<Type, Container>::TNativeIter
    CSyncQueue<Type, Container>::x_Erase(TNativeIter from_iter,
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

template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>::
CSyncQueue_I(TConstAccessGuard* guard, const TNativeIterator& iter)
    : m_Guard(guard),
      m_Iter (iter),
      m_Valid(false)
{
    m_Guard->x_AddIter(this);
    m_Valid = true;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>::~CSyncQueue_I(void)
{
    if ( m_Valid )
        m_Guard->x_RemoveIter(this);
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>::
CSyncQueue_I(const TMyType& other)
    : m_Valid(false)
{
    *this = other;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>::
CSyncQueue_I(const TInternalNonConst& other)
    : m_Guard(other.m_Guard),
      m_Iter (other.m_Iter),
      m_Valid(other.m_Valid)
{
    if ( m_Valid )
        m_Guard->x_AddIter(this);
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>&
    CSyncQueue_I<Type, Container, TNativeIterator>
        ::operator= (const TMyType& other)
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


template <class Type, class Container, class TNativeIterator>
inline
void CSyncQueue_I<Type, Container, TNativeIterator>::Invalidate(void)
{
    m_Guard->x_RemoveIter(this);
    m_Valid = false;
    m_Guard = NULL;
    m_Iter = TNativeIterator();
}


template <class Type, class Container, class TNativeIterator>
inline
void CSyncQueue_I<Type, Container, TNativeIterator>
    ::CheckGuard(TConstAccessGuard *guard) const
{
    if (m_Guard != guard) {
        NCBI_THROW(CSyncQueueException, eWrongGuardIter,
                   "Cannot work with iterators from another access guards.");
    }
}

template <class Type, class Container, class TNativeIterator>
inline
void CSyncQueue_I<Type, Container, TNativeIterator>::CheckValid(void) const
{
    if ( !m_Valid ) {
        NCBI_THROW(CSyncQueueException, eIterNotValid,
                   "Iterator can't be used after "
                   "destroying related access guard.");
    }
}


template <class Type, class Container, class TNativeIterator>
inline
void CSyncQueue_I<Type, Container, TNativeIterator>
    ::CheckMatched(const TMyType& other) const
{
    if (m_Guard != other.m_Guard) {
        NCBI_THROW(CSyncQueueException, eMismatchedIters,
                   "Cannot compare iterators from different queue guards.");
    }
}


template <class Type, class Container, class TNativeIterator>
inline
TNativeIterator
    CSyncQueue_I<Type, Container, TNativeIterator>::x_GetBase() const
{
    return m_Iter;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>&
    CSyncQueue_I<Type, Container, TNativeIterator>::operator++ (void)
{
    CheckValid();

    ++m_Iter;
    return *this;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>
    CSyncQueue_I<Type, Container, TNativeIterator>::operator++ (int)
{
    CheckValid();

    TMyType tmp(*this);
    m_Iter++;
    return tmp;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>&
    CSyncQueue_I<Type, Container, TNativeIterator>::operator-- (void)
{
    CheckValid();

    --m_Iter;
    return *this;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>
    CSyncQueue_I<Type, Container, TNativeIterator>::operator-- (int)
{
    CheckValid();

    TMyType tmp(*this);
    m_Iter--;
    return tmp;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>&
    CSyncQueue_I<Type, Container, TNativeIterator>::operator+= (TDiff offset)
{
    CheckValid();

    m_Iter += offset;
    return *this;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>&
    CSyncQueue_I<Type, Container, TNativeIterator>::operator-= (TDiff offset)
{
    CheckValid();

    m_Iter -= offset;
    return *this;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>
    CSyncQueue_I<Type, Container, TNativeIterator>
        ::operator+ (TDiff offset) const
{
    CheckValid();

    TMyType tmp(*this);
    tmp.m_Iter = tmp.m_Iter + offset;
    return tmp;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator> operator+
(typename CSyncQueue_I<Type, Container, TNativeIterator>::TDiff  offset,
 const    CSyncQueue_I<Type, Container, TNativeIterator>&        iter)
{
    return iter + offset;
}


template <class Type, class Container, class TNativeIterator>
inline
CSyncQueue_I<Type, Container, TNativeIterator>
    CSyncQueue_I<Type, Container, TNativeIterator>
        ::operator- (TDiff offset) const
{
    CheckValid();

    TMyType tmp(*this);
    tmp.m_Iter = tmp.m_Iter - offset;
    return tmp;
}


template <class Type, class Container, class TNativeIterator>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator>::TDiff
    CSyncQueue_I<Type, Container, TNativeIterator>
        ::operator- (const TMyType& other) const
{
    CheckValid();
    other.CheckValid();
    CheckMatched(other);

    return m_Iter - other.m_Iter;
}


// Additional difference between const and non-const iterators
template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
typename CSyncQueue<Type, Container>::TDiff
    operator- (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
               const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    typedef
        typename CSyncQueue<Type, Container>::TConstIterator TConstIterator;

    return TConstIterator(left) - TConstIterator(right);
}


template <class Type, class Container, class TNativeIterator>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator>::TRef
    CSyncQueue_I<Type, Container, TNativeIterator>::operator* (void) const
{
    CheckValid();

    return *m_Iter;
}


template <class Type, class Container, class TNativeIterator>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator>::TPtr
    CSyncQueue_I<Type, Container, TNativeIterator>::operator-> (void) const
{
    CheckValid();

    return m_Iter.operator->();
}


template <class Type, class Container, class TNativeIterator>
inline
typename CSyncQueue_I<Type, Container, TNativeIterator>::TRef
    CSyncQueue_I<Type, Container, TNativeIterator>
        ::operator[] (TDiff offset) const
{
    CheckValid();

    return m_Iter[offset];
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator== (const TMyType& other) const
{
    CheckMatched(other);

    return m_Iter == other.m_Iter;
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator!= (const TMyType& other) const
{
    return !(*this == other);
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator< (const TMyType& other) const
{
    CheckMatched(other);

    return m_Iter < other.m_Iter;
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator> (const TMyType& other) const
{
    return other < *this;
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator<= (const TMyType& other) const
{
    return !(other < *this);
}


template <class Type, class Container, class TNativeIterator>
inline
bool CSyncQueue_I<Type, Container, TNativeIterator>
    ::operator>= (const TMyType& other) const
{
    return !(*this < other);
}


// Additional comparing between const and non-const iterators

template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator== (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                 const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    typedef
        typename CSyncQueue<Type, Container>::TConstIterator  TConstIterator;

    return TConstIterator(left) == TConstIterator(right);
}


template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator!= (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                 const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    return !(left == right);
}


template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator< (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    typedef
        typename CSyncQueue<Type, Container>::TConstIterator  TConstIterator;

    return TConstIterator(left) < TConstIterator(right);
}


template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator> (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    return right < left;
}


template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator<= (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                 const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    return !(right < left);
}


template <class Type, class Container, class TNativeIterL, class TNativeIterR>
inline
bool operator>= (const CSyncQueue_I<Type, Container, TNativeIterL>&  left,
                 const CSyncQueue_I<Type, Container, TNativeIterR>&  right)
{
    return !(left < right);
}



//   CSyncQueue_ConstAccessGuard

template <class Type, class Container>
inline
CSyncQueue_ConstAccessGuard<Type, Container>::
CSyncQueue_ConstAccessGuard(TQueue& queue_to_guard)
    : m_Queue(queue_to_guard)
{
    if ( m_Queue.x_IsGuarded() ) {
        NCBI_THROW(CSyncQueueException, eDoubleGuard,
                   "Cannot guard an already guarded queue once again");
    }

    m_Queue.x_Lock();
    m_Queue.x_SetGuarded();
}


template <class Type, class Container>
inline
CSyncQueue_ConstAccessGuard<Type, Container>::~CSyncQueue_ConstAccessGuard()
{
    NON_CONST_ITERATE(typename list<TIterator*>, it, m_Iters)
    {
        (*it)->Invalidate();
    }

    NON_CONST_ITERATE(typename list<TNonConstIterator*>, it, m_NonConstIters)
    {
        (*it)->Invalidate();
    }

    m_Queue.x_SetUnguarded();
    m_Queue.x_Unlock();
}


template <class Type, class Container>
inline
void CSyncQueue_ConstAccessGuard<Type, Container>::x_AddIter(TIterator* iter)
{
    m_Iters.push_back(iter);
}


template <class Type, class Container>
inline
void CSyncQueue_ConstAccessGuard<Type, Container>
    ::x_AddIter(TNonConstIterator* iter)
{
    m_NonConstIters.push_back(iter);
}


template <class Type, class Container>
inline
void CSyncQueue_ConstAccessGuard<Type, Container>
    ::x_RemoveIter(TIterator* iter)
{
    m_Iters.remove(iter);
}


template <class Type, class Container>
inline
void CSyncQueue_ConstAccessGuard<Type, Container>
    ::x_RemoveIter(TNonConstIterator* iter)
{
    m_NonConstIters.remove(iter);
}


template <class Type, class Container>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container>::TIterator
    CSyncQueue_ConstAccessGuard<Type, Container>::Begin(void)
{
    return TIterator(this, m_Queue.x_Begin());
}


template <class Type, class Container>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container>::TIterator
    CSyncQueue_ConstAccessGuard<Type, Container>::End(void)
{
    return TIterator(this, m_Queue.x_End());
}


template <class Type, class Container>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container>::TRevIterator
    CSyncQueue_ConstAccessGuard<Type, Container>::RBegin(void)
{
    return TRevIterator(End());
}


template <class Type, class Container>
inline
typename CSyncQueue_ConstAccessGuard<Type, Container>::TRevIterator
    CSyncQueue_ConstAccessGuard<Type, Container>::REnd(void)
{
    return TRevIterator(Begin());
}


// Strange behaviour of MSVC compiler here:
// if 'const CSyncQueue<Type,Container>' change to
// 'typename CSyncQueue_ConstAccessGuard<Type, Container>::TQueue'
// then MSVC gives an error - it cannot propagate const specifier here
template <class Type, class Container>
inline
const CSyncQueue<Type,Container>&
    CSyncQueue_ConstAccessGuard<Type, Container>::Queue(void) const
{
    return m_Queue;
}



//   CSyncQueue_AccessGuard

template <class Type, class Container>
inline
CSyncQueue_AccessGuard<Type, Container>
    ::CSyncQueue_AccessGuard(TQueue& queue_to_guard)
        : TBase(queue_to_guard)
{}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TQueue&
    CSyncQueue_AccessGuard<Type, Container>::Queue(void) const
{
    return const_cast<TQueue&> (TBase::Queue());
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TIterator
    CSyncQueue_AccessGuard<Type, Container>::Erase(TIterator iter)
{
    iter.CheckGuard(this);
    return TIterator(this, Queue().x_Erase(iter.x_GetBase()));
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TIterator
    CSyncQueue_AccessGuard<Type, Container>::Erase(TIterator  from_iter,
                                                   TIterator  to_iter)
{
    from_iter.CheckGuard(this);
    to_iter.CheckGuard(this);
    return TIterator
        (this,
         Queue().x_Erase(from_iter.x_GetBase(), to_iter.x_GetBase()));
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TIterator
    CSyncQueue_AccessGuard<Type, Container>::Begin(void)
{
    return TIterator(this, Queue().x_Begin());
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TIterator
    CSyncQueue_AccessGuard<Type, Container>::End(void)
{
    return TIterator(this, Queue().x_End());
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TRevIterator
    CSyncQueue_AccessGuard<Type, Container>::RBegin(void)
{
    return TRevIterator(End());
}


template <class Type, class Container>
inline
typename CSyncQueue_AccessGuard<Type, Container>::TRevIterator
    CSyncQueue_AccessGuard<Type, Container>::REnd(void)
{
    return TRevIterator(Begin());
}



END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_QUEUE__HPP */
