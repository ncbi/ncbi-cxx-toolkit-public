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


class CRefresher
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


class CGBLGuard
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

END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/* ---------------------------------------------------------------------------
 *
 * $Log$
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
