#ifndef CORELIB___NCBI_CACHE__HPP
#define CORELIB___NCBI_CACHE__HPP
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
 * Author: Aleksey Grichenko, Denis Vakatov
 *
 * File Description:
 *	 Generic cache.
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.hpp>
#include <set>
#include <map>


BEGIN_NCBI_SCOPE

/** @addtogroup Cache
 *
 * @{
 */


/// @file ncbi_cache.hpp
/// The NCBI C++ generic cache template


/////////////////////////////////////////////////////////////////////////////
///
///    Generic cache.
///


/// Internal structure to hold cache elements
template <class TKey, class TSize>
struct SCacheElement
{
    typedef TKey     TKeyType;
    typedef TSize    TSizeType;

    SCacheElement(void) : m_Weight(0), m_Order(0) {}
    SCacheElement(const TKeyType&  key,
                  const TSizeType& weight,
                  const TSizeType& order)
        : m_Key(key), m_Weight(weight), m_Order(order) {}

    TKeyType  m_Key;     // cache element key
    TSizeType m_Weight;  // element weight
    TSizeType m_Order;   // order of insertion
};


/// Compare cache elements by weight/order
template <class TCacheElementPtr>
struct CCacheElement_Less
{
    bool operator()(const TCacheElementPtr& x,
                    const TCacheElementPtr& y) const
        {
            _ASSERT(x  &&  y);
            if (x->m_Weight != y->m_Weight) {
                return x->m_Weight < y->m_Weight;
            }
            return x->m_Order < y->m_Order;
        }
};


/// Flag indicating if an element can be inserted into cache
enum ECache_InsertFlag {
    eCache_CheckSize,   ///< Insert the element after checking max cache size
    eCache_CanInsert,   ///< The element can be inserted
    eCache_NeedCleanup, ///< Need to cleanup cache before inserting the element
    eCache_DoNotCache   ///< The element can not be inserted (e.g. too big)
};


/// Default (NOP) element handler
template <class TKey, class TValue>
class CCacheElement_Handler
{
public:
    /// Special processing of a removed element (e.g. deleting the object)
    void RemoveElement(const TKey& /*key*/, TValue& /*value*/) {}

    /// Special processing of an element to be inserted (e.g. count total
    /// memory used by the cached objects)
    ///
    /// NOTE:  This method is called *before* the element is actually
    ///        added to the cache.
    void InsertElement(const TKey& /*key*/, const TValue& /*value*/) {}

    /// Check if the element can be inserted into the cache
    ECache_InsertFlag CanInsertElement(const TKey& /*key*/,
                                       const TValue& /*value*/)
    {
        return eCache_CheckSize;
    }

    /// Element factory -- to create elements by key.
    /// This gets called when there is no element matching the key in the cache
    /// @sa CCache::Get()
    TValue CreateValue(const TKey& /*key*/)  { return TValue(); }
};


/// Default cache lock
typedef CMutex TCacheLock_Default;


/// Cache template. TKey and TValue define types stored in the cache.
/// TLock must define TWriteLockGuard subtype so that a value of type
/// TLock can be used to initialize TWriteLockGuard.
/// TSize is an integer type used for element indexing.
/// THandler must provide the following callback methods:
///   void RemoveElement(const TKey& key, TValue& value)
///   void InsertElement(const TKey& key, const TValue& value)
///   ECache_InsertFlag CanInsertElement(const TKey& key, const TValue& value)
///   TValue CreateValue(const TKey& key)
/// @sa CCacheElement_Handler
template <class TKey,
          class TValue,
          class THandler = CCacheElement_Handler<TKey, TValue>,
          class TLock    = TCacheLock_Default,
          class TSize    = Uint4>
class CCache
{
public:
    typedef TKey          TKeyType;
    typedef TValue        TValueType;
    typedef TSize         TSizeType;
    typedef SCacheElement<TKeyType, TSizeType>  TCacheElement;
    typedef TSizeType                           TWeight;
    typedef TSizeType                           TOrder;

    /// Create cache object with the given capacity
    CCache(TSizeType capacity, THandler *handler = NULL);

    /// Get cache element by the key. If the key is not cached yet,
    /// the handler's CreateValue() will be called to create one and
    /// the new element will be stored in the cache.
    /// @sa Get()
    TValueType operator[](const TKeyType& key);

    /// Get current capacity of the cache (max allowed number of elements)
    TSizeType GetCapacity(void) const { return m_Capacity; }

    /// Set new capacity of the cache. The number of elements in the cache
    /// may be reduced to match the new capacity.
    /// @param new_capacity
    ///   new cache capacity, must be > 0.
    void SetCapacity(TSizeType new_capacity);

    /// Get current number of elements in the cache
    TSizeType GetSize(void) const { return TSizeType(m_CacheSet.size()); }

    /// Truncate the cache leaving at most new_size elements.
    /// Does not affect cache capacity. If new_size is zero
    /// all elements will be removed.
    void SetSize(TSizeType new_size);

    /////////////////////////////////////////////////////

    /// Flags to control the details of adding new elements to the cache
    /// via Add().
    /// @sa Add()
    enum EAddFlags {
        fAdd_NoReplace = (1 << 0) ///< Do not replace existing values if any
    };
    typedef int TAddFlags;        ///< bitwise OR of EAddFlags

    /// Result of element insertion
    enum EAddResult {
        eAdd_Inserted,    ///< The element was added to the cache
        eAdd_Replaced,    ///< The element existed and was replaced
        eAdd_NotInserted  ///< The element was not added or replaced
    };

    /// Add new element to the cache or replace the existing value.
    /// @param key
    ///   Element key
    /// @param value
    ///   Element value
    /// @param weight
    ///   Weight adjustment. The lifetime of each object in the cache
    ///   is proportional to its weight.
    /// @param add_flags
    ///   Flags to control Add() behavior.
    /// @param result
    ///   Pointer to a variable to store operation result code to.
    /// @return
    ///   Index of the new element in the cache.
    TOrder Add(const TKeyType&   key,
               const TValueType& value,
               TWeight           weight    = 1,
               TAddFlags         add_flags = 0,
               EAddResult*       result    = NULL);

    /// Cache retrieval flags
    enum EGetFlags {
        fGet_NoTouch  = (1 << 0),  ///< Do not update the object's position.
        fGet_NoCreate = (1 << 1),  ///< Do not create value if not found, throw
                                   ///< an exception instead.
        fGet_NoInsert = (1 << 2)   ///< Do not insert created values.
    };
    typedef int TGetFlags;         ///< bitwise OR of EGetFlags

    /// Get() result
    enum EGetResult {
        eGet_Found,            ///< The key was found in the cache
        eGet_CreatedAndAdded,  ///< A new value was created and cached
        eGet_CreatedNotAdded   ///< A new value was created but not cached
    };

    /// Get an object from the cache by its key. Depending on flags create
    /// and cache a new value if the key is not found. If the flags do not
    /// allow creating new elements, throws an exception.
    /// @param key
    ///   Element key
    /// @param get_flags
    ///   Flags to control element retrieval
    /// @param result
    ///   pointer to a variable to store the result code to.
    /// @return
    ///   The value referenced by the key or a new value created by the
    ///   handler's CreateValue().
    TValueType Get(const TKeyType& key,
                   TGetFlags       get_flags = 0,
                   EGetResult*     result = NULL);

    /// Remove element from cache. Do nothing if the key is not cached.
    bool Remove(const TKeyType& key);

    ~CCache(void);

private:
    // Prohibit copy constructor and assignment.
    CCache(const CCache&);
    CCache& operator=(const CCache&);

    struct SValueWithIndex {
        TCacheElement* m_CacheElement;
        TValueType     m_Value;
    };

    typedef CCacheElement_Less<TCacheElement*>   TCacheLess;
    typedef set<TCacheElement*, TCacheLess>      TCacheSet;
    typedef typename TCacheSet::iterator         TCacheSet_I;
    typedef map<TKeyType, SValueWithIndex>       TCacheMap;
    typedef typename TCacheMap::iterator         TCacheMap_I;
    typedef TLock                                TLockType;
    typedef typename TLockType::TWriteLockGuard  TGuardType;
    typedef THandler                             THandlerType;

    // Get next counter value, adjust order of all elements if the counter
    // approaches its limit.
    TOrder x_GetNextCounter(void);
    void x_PackElementIndex(void);
    TCacheElement* x_InsertElement(const TKeyType& key, TWeight weight);
    void x_UpdateElement(TCacheElement* elem);
    void x_EraseElement(TCacheSet_I& set_iter, TCacheMap_I& map_iter);
    void x_EraseLast(void);
    TWeight x_GetBaseWeight(void) const
        {
            return m_CacheSet.empty() ? 0 : (*m_CacheSet.begin())->m_Weight;
        }

    TLockType    m_Lock;
    TSizeType    m_Capacity;
    TCacheSet    m_CacheSet;
    TCacheMap    m_CacheMap;
    TOrder       m_Counter;
    auto_ptr<THandlerType> m_Handler;
};


/// Exception thrown by CCache
class NCBI_XNCBI_EXPORT CCacheException : public CException
{
public:
    enum EErrCode {
        eIndexOverflow,    ///< Element index overflow
        eWeightOverflow,   ///< Element weight overflow
        eNotFound,         ///< The requested key was not found in the cache
        eOtherError
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CCacheException, CException);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CCache<> implementation
//

template <class TKey, class TValue, class THandler, class TLock, class TSize>
CCache<TKey, TValue, THandler, TLock, TSize>::CCache(TSizeType capacity,
                                                     THandler *handler)
    : m_Capacity(capacity),
      m_Counter(0)
{
    _ASSERT(capacity > 0);
    if ( handler != NULL ) m_Handler.reset(handler);
    else                   m_Handler.reset(new THandler());
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
CCache<TKey, TValue, THandler, TLock, TSize>::~CCache(void)
{
    TGuardType guard(m_Lock);

    while ( !m_CacheSet.empty() ) {
        x_EraseLast();
    }
    _ASSERT(m_CacheMap.empty());
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::x_EraseElement(
    TCacheSet_I& set_iter,
    TCacheMap_I& map_iter)
{
    _ASSERT(set_iter != m_CacheSet.end());
    _ASSERT(map_iter != m_CacheMap.end());
    TCacheElement* next = *set_iter;
    _ASSERT(next);
    m_Handler->RemoveElement(map_iter->first, map_iter->second.m_Value);
    m_CacheMap.erase(map_iter);
    m_CacheSet.erase(set_iter);
    delete next;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::x_EraseLast(void)
{
    _ASSERT(!m_CacheSet.empty());
    TCacheSet_I set_iter = m_CacheSet.begin();
    TCacheMap_I map_iter = m_CacheMap.find((*set_iter)->m_Key);
    x_EraseElement(set_iter, map_iter);
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
typename CCache<TKey, TValue, THandler, TLock, TSize>::TCacheElement*
CCache<TKey, TValue, THandler, TLock, TSize>::x_InsertElement(
    const TKeyType& key,
    TWeight         weight)
{
    if (weight == 0) {
        weight = 1;
    }
    TWeight adjusted_weight = (TWeight)(weight + x_GetBaseWeight());
    if (adjusted_weight < weight) {
        x_PackElementIndex();
        adjusted_weight = (TWeight)(weight + x_GetBaseWeight());
        if (adjusted_weight < weight) {
            NCBI_THROW(CCacheException, eWeightOverflow,
                "Cache element weight overflow");
        }
    }
    TCacheElement* elem = new TCacheElement(key, adjusted_weight,
        x_GetNextCounter());
    m_CacheSet.insert(elem);
    return elem;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::x_UpdateElement(
    TCacheElement* elem)
{
    _ASSERT(elem);
    TCacheSet_I it = m_CacheSet.find(elem);
    _ASSERT(it != m_CacheSet.end());
    _ASSERT(*it == elem);
    m_CacheSet.erase(it);
    elem->m_Order = x_GetNextCounter();
    if (TWeight(elem->m_Weight + 1) <= 0) {
        x_PackElementIndex();
    }
    elem->m_Weight++;
    m_CacheSet.insert(elem);
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
typename CCache<TKey, TValue, THandler, TLock, TSize>::TOrder
CCache<TKey, TValue, THandler, TLock, TSize>::x_GetNextCounter(void)
{
    if (TSizeType(m_Counter + 1) <= 0) {
        x_PackElementIndex();
    }
    return ++m_Counter;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::x_PackElementIndex(void)
{
    // Overflow detected - adjust orders
    TOrder order_shift = (TOrder)(m_Counter - 1);
    TOrder order_max = numeric_limits<TOrder>::max();
    if ( !m_CacheSet.empty() ) {
        TWeight weight_shift = (TWeight)((*m_CacheSet.begin())->m_Weight - 1);
        TWeight weight_max = weight_shift;
        TOrder order_min = 0;
        ITERATE(typename TCacheSet, it, m_CacheSet) {
            TCacheElement* e = *it;
            if (e->m_Order < order_shift  &&  e->m_Order > order_min) {
                if (e->m_Order >= (order_shift + order_min)/2) {
                    order_shift = e->m_Order;
                }
                else {
                    order_min = e->m_Order;
                }
            }
            if (e->m_Weight > weight_max) {
                weight_max = e->m_Weight;
            }
        }
        order_shift = (TOrder)(order_shift - order_min);
        if (order_shift < 2) {
            // Can not pack orders, try slow method
            typedef set<TOrder> TOrderSet;
            TOrderSet orders;
            ITERATE(typename TCacheSet, it, m_CacheSet) {
                orders.insert((*it)->m_Order);
            }
            if (*orders.rbegin() < order_max) {
                m_Counter = *orders.rbegin();
                order_min = order_max;
                order_shift = 1; // will be decremented to 0
            }
            else {
                TOrder rg_from = 0;
                TOrder rg_to = 0;
                TOrder last = 1;
                // Find the longest unused range
                ITERATE(typename TOrderSet, it, orders) {
                    if (*it - last > rg_to - rg_from) {
                        rg_from = last;
                        rg_to = *it;
                    }
                    last = *it;
                }
                if (rg_to - rg_from < 2) {
                    NCBI_THROW(CCacheException, eIndexOverflow,
                            "Cache element index overflow");
                }
                order_min = rg_from;
                order_shift = (TOrder)(rg_to - rg_from);
            }
        }
        if (weight_shift <= 1  &&  weight_max + 1 <= 0) {
            // Can not pack weights
            NCBI_THROW(CCacheException, eWeightOverflow,
                       "Cache element weight overflow");
        }
        order_shift--;
        // set<> elements are modified but the order is not changed
        NON_CONST_ITERATE(typename TCacheSet, it, m_CacheSet) {
            TCacheElement* e = *it;
            if (e->m_Order > order_min) {
                e->m_Order = (TOrder)(e->m_Order - order_shift);
            }
            e->m_Weight = (TOrder)(e->m_Weight - weight_shift);
        }
    }
    m_Counter = (TOrder)(m_Counter - order_shift);
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
typename CCache<TKey, TValue, THandler, TLock, TSize>::TOrder
CCache<TKey, TValue, THandler, TLock, TSize>::Add(const TKeyType&   key,
                                                  const TValueType& value,
                                                  TWeight           weight,
                                                  TAddFlags         add_flags,
                                                  EAddResult*       result)
{
    TGuardType guard(m_Lock);
    TCacheMap_I it = m_CacheMap.find(key);
    if (it != m_CacheMap.end() ) {
        if ((add_flags & fAdd_NoReplace) != 0) {
            if ( result ) {
                *result = eAdd_NotInserted;
                return 0;
            }
        }
        TCacheSet_I set_it = m_CacheSet.find(it->second.m_CacheElement);
        x_EraseElement(set_it, it);
        if ( result ) {
            *result = eAdd_Replaced;
        }
    }
    else if ( result ) {
        *result = eAdd_Inserted;
    }
    
    for (ECache_InsertFlag ins_flag = m_Handler->CanInsertElement(key, value);;
         ins_flag = m_Handler->CanInsertElement(key, value)) {
        if (ins_flag == eCache_CheckSize) {
            while (GetSize() >= m_Capacity) {
                x_EraseLast();
            }
            break;
        }
        else if (ins_flag == eCache_CanInsert) {
            break;
        }
        else if (ins_flag == eCache_DoNotCache) {
            if ( result ) {
                *result = eAdd_NotInserted;
            }
            return 0;
        }
        else if (ins_flag == eCache_NeedCleanup) {
            if ( GetSize() == 0 ) {
                // Can not cleanup
                if ( result ) {
                    *result = eAdd_NotInserted;
                }
                return 0;
            }
            x_EraseLast();
        }
    }

    m_Handler->InsertElement(key, value);

    SValueWithIndex& map_val = m_CacheMap[key];
    map_val.m_CacheElement = x_InsertElement(key, weight);
    map_val.m_Value = value;
    _ASSERT(map_val.m_CacheElement);
    return map_val.m_CacheElement->m_Order;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
typename CCache<TKey, TValue, THandler, TLock, TSize>::TValueType
CCache<TKey, TValue, THandler, TLock, TSize>::operator[](const TKeyType& key)
{
    TGuardType guard(m_Lock);

    TCacheMap_I it = m_CacheMap.find(key);
    if (it != m_CacheMap.end()) {
        x_UpdateElement(it->second.m_CacheElement);
        return it->second.m_Value;
    }
    TValueType value = m_Handler->CreateValue(key);
    Add(key, value);
    return value;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
typename CCache<TKey, TValue, THandler, TLock, TSize>::TValueType
CCache<TKey, TValue, THandler, TLock, TSize>::Get(const TKeyType& key,
                                                  TGetFlags       get_flags,
                                                  EGetResult*     result)
{
    TGuardType guard(m_Lock);

    TCacheMap_I it = m_CacheMap.find(key);
    if (it != m_CacheMap.end()) {
        if ((get_flags & fGet_NoTouch) == 0) {
            x_UpdateElement(it->second.m_CacheElement);
        }
        if ( result ) {
            *result = eGet_Found;
        }
        return it->second.m_Value;
    }

    // Could not find the key - try to create a new element
    if ((get_flags & fGet_NoCreate) != 0) {
        NCBI_THROW(CCacheException, eNotFound,
            "Can not find the requested key");
    }

    TValueType value = m_Handler->CreateValue(key);
    if ((get_flags & fGet_NoInsert) == 0) {
        // Insert the new element
        Add(key, value);
        if ( result ) {
            *result = eGet_CreatedAndAdded;
        }
    }
    else {
        if ( result ) {
            *result = eGet_CreatedNotAdded;
        }
    }
    return value;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
bool CCache<TKey, TValue, THandler, TLock, TSize>::Remove(const TKeyType& key)
{
    TGuardType guard(m_Lock);

    TCacheMap_I it = m_CacheMap.find(key);
    if (it == m_CacheMap.end()) {
        return false;
    }
    TCacheSet_I set_it = m_CacheSet.find(it->second.m_CacheElement);
    x_EraseElement(set_it, it);
    return true;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::SetCapacity(TSizeType new_capacity)
{
    if (new_capacity <= 0) {
        NCBI_THROW(CCacheException, eOtherError,
                   "Cache capacity must be positive");
    }
    TGuardType guard(m_Lock);
    while (GetSize() > new_capacity) {
        x_EraseLast();
    }
    m_Capacity = new_capacity;
}


template <class TKey, class TValue, class THandler, class TLock, class TSize>
void CCache<TKey, TValue, THandler, TLock, TSize>::SetSize(TSizeType new_size)
{
    TGuardType guard(m_Lock);
    while (GetSize() > new_size) {
        x_EraseLast();
    }
}


/* @} */

END_NCBI_SCOPE

#endif  // CORELIB___NCBI_CACHE__HPP
