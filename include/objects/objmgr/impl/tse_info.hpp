#ifndef OBJECTS_OBJMGR___TSE_INFO__HPP
#define OBJECTS_OBJMGR___TSE_INFO__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   TSE info -- entry for data source seq-id to TSE map
*
*/


#include "bioseq_info.hpp"
#include <objects/seqset/Seq_entry.hpp>
#include <util/rangemap.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbithr.hpp>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


// forward declaration
class CSeq_entry;
class CBioseq;
class CAnnotObject;


class CTSE_Info : public CObject, public CMutableAtomicCounter
{
public:
    // 'ctors
    CTSE_Info(void);
    virtual ~CTSE_Info(void);

    void LockCounter(void) const;
    void UnlockCounter(void) const;
    bool CounterLocked(void) const;

    bool IsIndexed(void) const { return m_Indexed; }
    void SetIndexed(bool value) { m_Indexed = value; }

    bool operator< (const CTSE_Info& info) const;
    bool operator== (const CTSE_Info& info) const;

    typedef map<CSeq_id_Handle, CRef<CBioseq_Info> >                 TBioseqMap;
    typedef CRange<TSeqPos>                                          TRange;
    typedef CRangeMultimap<CRef<CAnnotObject>,TRange::position_type> TRangeMap;
    typedef map<CSeq_id_Handle, TRangeMap>                           TAnnotMap;

    // Reference to the TSE
    CRef<CSeq_entry> m_TSE;
    // Dead seq-entry flag
    bool m_Dead;
    TBioseqMap m_BioseqMap;
    TAnnotMap  m_AnnotMap;

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;
private:
    // Hide copy methods
    CTSE_Info(const CTSE_Info&);
    CTSE_Info& operator= (const CTSE_Info&);

    bool m_Indexed;

    mutable CMutex m_TSE_Mutex;
    friend class CTSE_Guard;
};


class CTSE_Guard
{
public:
    CTSE_Guard(const CTSE_Info& tse);
    ~CTSE_Guard(void);
private:
    // Prohibit copy operation
    CTSE_Guard(const CTSE_Guard&);
    CTSE_Guard& operator= (const CTSE_Guard&);

    CMutexGuard m_Guard;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
void CTSE_Info::LockCounter(void) const
{
    Add(1);
}

inline
void CTSE_Info::UnlockCounter(void) const
{
    if (Get() > 0) {
        Add(-1);
    }
    else {
        THROW1_TRACE(runtime_error,
            "CTSE_Info::Unlock() -- The TSE is not locked");
    }
}

inline
bool CTSE_Info::CounterLocked(void) const
{
    return Get() > 0;
}

inline
bool CTSE_Info::operator< (const CTSE_Info& info) const
{
    return m_TSE < info.m_TSE;
}

inline
bool CTSE_Info::operator== (const CTSE_Info& info) const
{
    return m_TSE == info.m_TSE;
}

inline
CTSE_Guard::CTSE_Guard(const CTSE_Info& tse)
    : m_Guard(tse.m_TSE_Mutex)
{
    return;
}

inline
CTSE_Guard::~CTSE_Guard(void)
{
    return;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.10  2002/10/18 19:12:41  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.9  2002/07/10 16:50:33  grichenk
* Fixed bug with duplicate and uninitialized atomic counters
*
* Revision 1.8  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.7  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.6  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.5  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.4  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.3  2002/03/14 18:39:14  gouriano
* added mutex for MT protection
*
* Revision 1.2  2002/02/21 19:27:07  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___TSE_INFO__HPP */
