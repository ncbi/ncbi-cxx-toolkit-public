#ifndef CORELIB__CACHE_WITH_LOCK__HPP
#define CORELIB__CACHE_WITH_LOCK__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Cache for loaded objects with lock to hold their destruction
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

template<class Key, class Value, class Less = less<Key> >
class CCacheWithLock : public CObject
{
public:
    typedef Key key_type;
    typedef Value mapped_type;

protected:
    class CSlot;
    typedef Less TLess;
    typedef map<key_type, CRef<CSlot>, TLess> TMap;
    typedef typename TMap::iterator TMapIterator;
    typedef typename TMap::const_iterator TMapConstIterator;
    typedef list<TMapIterator> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveListIterator;

    class CSlot : public CObject {
    public:
        CSlot() {
            m_LockCounter.Set(1);
        }
        TMapIterator        m_MapIter;
        TRemoveListIterator m_RemoveListIter;
        CAtomicCounter      m_LockCounter;
        CFastMutex          m_ValueMutex;
        mapped_type         m_Value;
    };

    TMap m_Map;
    size_t m_SizeLimit;
    size_t m_RemoveSize;
    TRemoveList m_RemoveList;
    CMutex m_Mutex;

public:
    class CLock {
    protected:
        CRef<CCacheWithLock> m_Cache;
        CRef<CSlot> m_Slot;
        friend class CCacheWithLock<key_type, mapped_type, TLess>;
        
        CLock(CCacheWithLock* cache, CSlot* slot)
            : m_Cache(cache),
              m_Slot(slot)
            {
                _ASSERT(cache);
                _ASSERT(slot->m_LockCounter.Get() > 0);
            }
        
    public:
        CLock() {
        }
        ~CLock() {
            Reset();
        }
        CLock(const CLock& lock)
            : m_Cache(lock.m_Cache),
              m_Slot(lock.m_Slot)
            {
                if ( m_Slot ) {
                    m_Slot->m_LockCounter.Add(1);
                }
            }
        CLock& operator=(const CLock& lock)
            {
                if ( m_Slot != lock.m_Slot ) {
                    if ( m_Slot ) {
                        m_Cache->Unlock(m_Slot);
                    }
                    m_Cache = lock.m_Cache;
                    m_Slot = lock.m_Slot;
                    if ( m_Slot ) {
                        m_Slot->m_LockCounter.Add(1);
                    }
                }
                return *this;
            }
        CLock(CLock&& lock)
            : m_Cache(move(lock.m_Cache)),
              m_Slot(move(lock.m_Slot))
            {
            }
        CLock& operator=(CLock&& lock)
            {
                if ( m_Slot != lock.m_Slot ) {
                    Reset();
                    m_Cache.Swap(lock.m_Cache);
                    m_Slot.Swap(lock.m_Slot);
                }
                return *this;
            }

        DECLARE_OPERATOR_BOOL_REF(m_Slot);
        
        void Reset() {
            if ( m_Slot ) {
                m_Cache->Unlock(m_Slot);
                m_Slot = null;
                m_Cache = null;
            }
        }

        CFastMutex& GetValueMutex() { return m_Slot.GetNCObject().m_ValueMutex; }
        
        mapped_type& operator*() const { return m_Slot.GetNCObject().m_Value; }
        mapped_type* operator->() const { return &m_Slot.GetNCObject().m_Value; }
        
        bool operator==(CLock a) const {
            return m_Slot == a.m_Slot;
        }
        bool operator!=(CLock a) const {
            return !(*this == a);
        }
    };

    CCacheWithLock(size_t size_limit = 0)
        : m_SizeLimit(size_limit),
          m_RemoveSize(0)
        {
        }
    
    CLock get_lock(const key_type& key) {
        CMutexGuard guard(m_Mutex);
        TMapIterator iter = m_Map.lower_bound(key);
        if ( iter == m_Map.end() || m_Map.key_comp()(key, iter->first) ) {
            // insert
            typedef typename TMap::value_type TValue;
            iter = m_Map.insert(iter, TValue(key, Ref(new CSlot())));
            iter->second->m_MapIter = iter;
        }
        else if ( iter->second->m_LockCounter.Add(1) == 1 ) {
            // first lock from remove list
            _ASSERT(m_RemoveSize > 0);
            _ASSERT(m_RemoveSize == m_RemoveList.size());
            m_RemoveList.erase(iter->second->m_RemoveListIter);
            --m_RemoveSize;
        }
        return CLock(this, iter->second);
    }

    size_t get_size_limit(void) const {
        return m_SizeLimit;
    }
    void set_size_limit(size_t size_limit) {
        if ( size_limit != m_SizeLimit ) {
            CMutexGuard guard(m_Mutex);
            x_GC(m_SizeLimit = size_limit);
        }
    }

    void clear() {
        CMutexGuard guard(m_Mutex);
        x_GC(0);
    }

protected:
    void Unlock(CSlot* slot) {
        CMutexGuard guard(m_Mutex);
        _ASSERT(slot);
        _ASSERT(slot->m_MapIter->second == slot);
        if ( slot->m_LockCounter.Add(-1) == 0 ) {
            // last lock removed
            slot->m_RemoveListIter =
                m_RemoveList.insert(m_RemoveList.end(), slot->m_MapIter);
            ++m_RemoveSize;
            x_GC(m_SizeLimit);
        }
    }
    
    void x_GC(size_t size) {
        while ( m_RemoveSize > size ) {
            m_Map.erase(m_RemoveList.front());
            m_RemoveList.pop_front();
            --m_RemoveSize;
        }
    }
    
public:
};

END_NCBI_SCOPE

#endif // CORELIB__CACHE_WITH_LOCK__HPP
