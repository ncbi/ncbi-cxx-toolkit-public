#ifndef GBLOADER_UTIL_HPP_INCLUDED
#define GBLOADER_UTIL_HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Michael Kimelman
*
*  File Description:
*   GB loader Utilities
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitime.hpp>

#include <map>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//========================================================================
class NCBI_XLOADER_GENBANK_EXPORT CTimer
{
public:
    CTimer(void);
    time_t Time(void);
    void   Start(void);
    void   Stop(void);
    time_t RetryTime(void);
    bool   NeedCalibration(void);
private:
    time_t m_ReasonableRefreshDelay;
    int    m_RequestsDevider;
    int    m_Requests;
    CMutex m_RequestsLock;
    time_t m_Time;
    time_t m_LastCalibrated;
  
    time_t m_StartTime;
    CMutex m_TimerLock;
};


#if 0
class NCBI_XLOADER_GENBANK_EXPORT CRefresher
{
public:
    CRefresher(void)
        : m_RefreshTime(0)
        {
        }

    void Reset(CTimer &timer);
    void Reset(void)
        {
            m_RefreshTime = 0;
        }

    bool NeedRefresh(CTimer &timer) const;

private:
    time_t m_RefreshTime;
};


class NCBI_XLOADER_GENBANK_EXPORT CMutexPool
{
#if defined(NCBI_THREADS)
    int         m_size;
    CMutex     *m_Locks;
    int        *spread;
#else
    static CMutex sm_Lock;
#endif
public:
#if defined(NCBI_THREADS)
    CMutexPool(void);
    ~CMutexPool(void);

    void SetSize(int size);

    CMutex& GetMutex(int x)
        {
            int y=x%m_size; spread[y]++; return m_Locks[y];
        }

    int Select(unsigned key) const
        {
            return key % m_size;
        }
    template<class A> int  Select(A *a) const
        {
            return Select((unsigned long)a/sizeof(A));
        }
#else
    CMutexPool(void)
        {
        }
    ~CMutexPool(void)
        {
        }
    void SetSize(int /* size */)
        {
        }
    CMutex& GetMutex(int /* x */)
        {
            return sm_Lock;
        }
    int  Select(unsigned /* key */) const
        {
            return 0;
        }
    template<class A> int  Select(A *a) const
        {
            return 0;
        }
#endif
};


class NCBI_XLOADER_GENBANK_EXPORT CGBLGuard
{
public:
    enum EState
    {
        eNone,
        eMain,
        eBoth,
        eLocal,
        eNoneToMain,
        eLast
    };

    struct SLeveledMutex
    {
        unsigned        m_SlowTraverseMode;
        CMutex          m_Lookup;
        CMutexPool      m_Pool;
    };

    typedef SLeveledMutex TLMutex;

private:
    TLMutex      *m_Locks;
    const char   *m_Loc;
    EState        m_orig;
    EState        m_current;
    int           m_select;

public:

    CGBLGuard(TLMutex& lm,EState orig,const char *loc="",int select=-1); // just accept start position
    CGBLGuard(TLMutex& lm,const char *loc=""); // assume orig=eNone, switch to e.Main in constructor
    CGBLGuard(CGBLGuard &g,const char *loc);   // inherit state from g1 - for SubGuards

    ~CGBLGuard();
    template<class A> void Lock(A *a)   { Switch(eBoth,a); }
    template<class A> void Unlock(A *a) { Switch(eMain,a); }
    void Lock(unsigned key)   { Switch(eBoth,key); }
    void Unlock(unsigned key) { Switch(eMain,key); }
    void Lock()   { Switch(eMain);};
    void Unlock() { Switch(eNone);};
    void Local()  { Switch(eLocal);};
  
private:
  
#if defined (NCBI_THREADS)
    void MLock();
    void MUnlock();
    void PLock();
    void PUnlock();
    void Select(int s);

    template<class A> void Switch(EState newstate,A *a)
        {
            Select(m_Locks->m_Pool.Select(a));
            Switch(newstate);
        }
    void Switch(EState newstate,unsigned key)
        {
            Select(m_Locks->m_Pool.Select(key));
            Switch(newstate);
        }
    void Switch(EState newstate);
#else
    void Switch(EState /* newstate */) {}
    template<class A> void Switch(EState newstate,A *a) {}
    void Switch(EState /* newstate */, unsigned /* key */) {}
#endif
};
#endif

//#define LOAD_INFO_MAP_CHECK 1
//#define LOAD_INFO_MAP_ALWAYS_CLEAR 1
//#define LOAD_INFO_MAP_NO_SPLICE 1

template<class Key, class Info>
class CLoadInfoMap
{
public:
    enum {
        kDefaultMaxSize = 2048
    };

    CLoadInfoMap(size_t max_size = 0)
        : m_MaxSize(max_size? max_size: kDefaultMaxSize)
    {
    }

    size_t GetMaxSize(void) const
    {
        return m_MaxSize;
    }

    void SetMaxSize(size_t max_size)
    {
        TWriteLockGuard guard(m_Lock);
#ifdef LOAD_INFO_MAP_CHECK
        _ASSERT(m_Index.size() <= m_MaxSize);
        _ASSERT(m_Queue.size() == m_Index.size());
#endif
        m_MaxSize = max_size? max_size: kDefaultMaxSize;
        x_GC();
#ifdef LOAD_INFO_MAP_CHECK
        _ASSERT(m_Index.size() <= m_MaxSize);
        _ASSERT(m_Queue.size() == m_Index.size());
#endif
    }
    
    CRef<Info> Get(const Key& key)
    {
        TWriteLockGuard guard(m_Lock);
#ifdef LOAD_INFO_MAP_CHECK
        _ASSERT(m_Index.size() <= m_MaxSize);
        _ASSERT(m_Queue.size() == m_Index.size());
#endif
#ifdef LOAD_INFO_MAP_ALWAYS_CLEAR
        if ( !m_Queue.empty() && m_Queue.begin()->first != key ) {
            m_Queue.clear();
            m_Index.clear();
        }
        if ( m_Queue.empty() ) {
            m_Queue.push_front(TQueueValue(key, Ref(new Info(key))));
            m_Index.insert(TIndexValue(key, m_Queue.begin()));
        }
#else
        pair<TIndexIter, bool> ins =
            m_Index.insert(TIndexValue(key, TQueueIter()));
        _ASSERT(ins.first->first == key);
        if ( ins.second ) {
            // new slot
#ifdef LOAD_INFO_MAP_CHECK
            _ASSERT(m_Index.size() == m_Queue.size() + 1);
#endif
            m_Queue.push_front(TQueueValue(key, Ref(new Info(key))));
            x_GC();
        }
        else {
            // old slot
            _ASSERT(ins.first->second->first == key);
            // move old slot to the back of queue
#ifdef LOAD_INFO_MAP_NO_SPLICE
            CRef<Info> info = ins.first->second->second;
            m_Queue.erase(ins.first->second);
            m_Queue.push_front(TQueueValue(key, info));
#else
            m_Queue.splice(m_Queue.begin(), m_Queue, ins.first->second);
#endif
        }
        // update for new position in queue
        _ASSERT(m_Queue.begin()->first == key);
        ins.first->second = m_Queue.begin();
#endif
#ifdef LOAD_INFO_MAP_CHECK
        _ASSERT(!m_Index.empty() && m_Index.size() <= m_MaxSize);
        _ASSERT(m_Queue.size() == m_Index.size());
#endif
        return m_Queue.begin()->second;
    }

    void GC(void)
    {
        TWriteLockGuard guard(m_Lock);
        x_GC();
    }

    void Clear(void)
    {
        TWriteLockGuard guard(m_Lock);
        m_Queue.clear();
        m_Index.clear();
    }

protected:
    typedef CFastMutex                  TLock;
    typedef TLock::TReadLockGuard       TReadLockGuard;
    typedef TLock::TWriteLockGuard      TWriteLockGuard;
    
    TLock  m_Lock;

    void x_GC(void)
    {
        while ( m_Index.size() > m_MaxSize ) {
            m_Index.erase(m_Queue.back().first);
            m_Queue.pop_back();
        }
    }

private:
    typedef pair<Key, CRef<Info> >      TQueueValue;
    typedef list<TQueueValue>           TQueue;
    typedef typename TQueue::iterator   TQueueIter;
    typedef map<Key, TQueueIter>        TIndex;
    typedef typename TIndex::value_type TIndexValue;
    typedef typename TIndex::iterator   TIndexIter;

    size_t m_MaxSize;
    TQueue m_Queue;
    TIndex m_Index;

private:
    // prevent copying
    CLoadInfoMap(const CLoadInfoMap<Key, Info>&);
    void operator=(const CLoadInfoMap<Key, Info>&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/* ---------------------------------------------------------------------------
 *
 * $Log$
 * Revision 1.6  2005/03/28 19:26:02  vasilche
 * Added auto cleaning maps for id resolution information.
 * Commented out obsolete classes CRefresher, CMutexPool, and CGBLGuard.
 *
 * Revision 1.5  2005/03/28 18:28:31  jcherry
 * Added export specifiers
 *
 * Revision 1.4  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.3  2004/08/04 14:55:17  vasilche
 * Changed TSE locking scheme.
 * TSE cache is maintained by CDataSource.
 * Added ID2 reader.
 * CSeqref is replaced by CBlobId.
 *
 * Revision 1.2  2003/12/30 22:14:39  vasilche
 * Updated genbank loader and readers plugins.
 *
 * Revision 1.2  2003/12/02 00:08:15  vasilche
 * Fixed segfault in multithreaded apps.
 *
 * Revision 1.1  2003/11/26 17:55:55  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.5  2003/06/02 16:06:37  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.4  2003/03/03 20:34:51  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.3  2003/03/01 22:26:56  kimelman
 * performance fixes
 *
 * Revision 1.2  2002/07/22 23:10:04  kimelman
 * make sunWS happy
 *
 * Revision 1.1  2002/07/22 22:53:24  kimelman
 * exception handling fixed: 2level mutexing moved to Guard class + added
 * handling of confidential data.
 *
 */
