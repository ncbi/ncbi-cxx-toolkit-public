#ifndef OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP

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


#include <objects/objmgr/impl/bioseq_info.hpp>
#include <objects/objmgr/impl/handle_range_map.hpp>
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
class CAnnotObject_Info;
struct SAnnotSelector;

struct NCBI_XOBJMGR_EXPORT SAnnotObject_Index {
    CRef<CAnnotObject_Info>                   m_AnnotObject;
    int                                       m_IndexBy;
    CHandleRangeMap::TLocMap::const_iterator  m_HandleRange;
};

class CTSE_Lock;

class NCBI_XOBJMGR_EXPORT CTSE_Info : public CObject, public CMutableAtomicCounter
{
public:
    // 'ctors
    CTSE_Info(void);
    virtual ~CTSE_Info(void);

    bool IsIndexed(void) const { return m_Indexed; }
    void SetIndexed(bool value) { m_Indexed = value; }

    bool operator< (const CTSE_Info& info) const;
    bool operator== (const CTSE_Info& info) const;

    typedef map<CSeq_id_Handle, CRef<CBioseq_Info> >         TBioseqMap;

    typedef CRange<TSeqPos>                                  TRange;
    typedef CRangeMultimap<SAnnotObject_Index, TRange::position_type> TRangeMap;

    typedef unsigned TAnnotSelectorKey;
    typedef map<TAnnotSelectorKey, TRangeMap>                TAnnotSelectorMap;
    typedef map<CSeq_id_Handle, TAnnotSelectorMap>           TAnnotMap;

    static TAnnotSelectorKey x_GetAnnotSelectorKey(const SAnnotSelector& sel);
    const TRangeMap* x_GetRangeMap(const CSeq_id_Handle& id,
                                   const SAnnotSelector& selector) const;
    TRangeMap& x_SetRangeMap(const CSeq_id_Handle& id,
                             const SAnnotSelector& selector);
    void x_DropRangeMap(const CSeq_id_Handle& id,
                        const SAnnotSelector& sel);
    static TRangeMap& x_SetRangeMap(TAnnotSelectorMap& selMap,
                                    const SAnnotSelector& selector);
    static void x_DropRangeMap(TAnnotSelectorMap& selMap,
                               const SAnnotSelector& selector);

    // Reference to the TSE
    CRef<CSeq_entry> m_TSE;
    // Dead seq-entry flag
    bool m_Dead;
    TBioseqMap m_BioseqMap;
    TAnnotMap  m_AnnotMap;

    void CounterOverflow(void) const;
    void CounterUnderflow(void) const;
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    bool CounterLocked(void) const;

private:
    friend class CTSE_Lock;

    void LockCounter(void) const;
    void UnlockCounter(void) const;

    // Hide copy methods
    CTSE_Info(const CTSE_Info&);
    CTSE_Info& operator= (const CTSE_Info&);

    bool m_Indexed;

    mutable CMutex m_TSE_Mutex;
    friend class CTSE_Guard;
};


class NCBI_XOBJMGR_EXPORT CTSE_Guard
{
public:
    explicit CTSE_Guard(const CTSE_Info& tse);
    ~CTSE_Guard(void);
private:
    // Prohibit copy operation
    CTSE_Guard(const CTSE_Guard&);
    CTSE_Guard& operator= (const CTSE_Guard&);

    CMutexGuard m_Guard;
};


class NCBI_XOBJMGR_EXPORT CTSE_Lock
{
public:
    CTSE_Lock(void);
    explicit CTSE_Lock(CTSE_Info& tse);
    explicit CTSE_Lock(CTSE_Info* tse);
    explicit CTSE_Lock(const CRef<CTSE_Info>& tse);
    ~CTSE_Lock(void);

    CTSE_Lock(const CTSE_Lock&);
    CTSE_Lock& operator=(const CTSE_Lock&);

    void Set(CTSE_Info& tse);
    void Set(CTSE_Info* tse);
    void Set(const CRef<CTSE_Info>& tse);

    operator bool(void) const;
    bool operator!(void) const;

    CTSE_Info& operator*(void) const;
    CTSE_Info* operator->(void) const;
    CTSE_Info* GetPointer(void) const;

    bool operator==(const CTSE_Lock& lock) const;
    bool operator!=(const CTSE_Lock& lock) const;
    bool operator<(const CTSE_Lock& lock) const;

private:
    void x_Lock(void) const;
    void x_Unlock(void) const;

    CRef<CTSE_Info> m_TSE;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
void CTSE_Info::LockCounter(void) const
{
    if ( Add(1) == 0 ) {
        // rollback
        Add(-1);
        CounterOverflow();
    }
}

inline
void CTSE_Info::UnlockCounter(void) const
{
    if ( Add(-1)+1 == 0 ) {
        // rollback
        Add(1);
        CounterUnderflow();
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


inline
CTSE_Info& CTSE_Lock::operator*(void) const
{
    return const_cast<CTSE_Info&>(*m_TSE);
}


inline
CTSE_Info* CTSE_Lock::operator->(void) const
{
    return const_cast<CTSE_Info*>(&*m_TSE);
}


inline
CTSE_Info* CTSE_Lock::GetPointer(void) const
{
    return const_cast<CTSE_Info*>(m_TSE.GetPointer());
}


inline
void CTSE_Lock::x_Lock(void) const
{
    if ( m_TSE ) {
        m_TSE->LockCounter();
    }
}


inline
void CTSE_Lock::x_Unlock(void) const
{
    if ( m_TSE ) {
        m_TSE->UnlockCounter();
    }
}


inline
CTSE_Lock::CTSE_Lock(void)
{
}


inline
CTSE_Lock::CTSE_Lock(CTSE_Info* tse)
    : m_TSE(tse)
{
    x_Lock();
}


inline
CTSE_Lock::CTSE_Lock(CTSE_Info& tse)
    : m_TSE(&tse)
{
    x_Lock();
}


inline
CTSE_Lock::CTSE_Lock(const CRef<CTSE_Info>& tse)
    : m_TSE(tse)
{
    x_Lock();
}


inline
CTSE_Lock::~CTSE_Lock(void)
{
    x_Unlock();
}


inline
CTSE_Lock::CTSE_Lock(const CTSE_Lock& lock)
    : m_TSE(lock.m_TSE)
{
    x_Lock();
}


inline
void CTSE_Lock::Set(CTSE_Info* tse)
{
    if ( m_TSE.GetPointerOrNull() != tse ) {
        x_Unlock();
        m_TSE.Reset(tse);
        x_Lock();
    }
}


inline
void CTSE_Lock::Set(CTSE_Info& tse)
{
    if ( m_TSE.GetPointerOrNull() != &tse ) {
        x_Unlock();
        m_TSE.Reset(&tse);
        x_Lock();
    }
}


inline
void CTSE_Lock::Set(const CRef<CTSE_Info>& tse)
{
    if ( m_TSE != tse ) {
        x_Unlock();
        m_TSE = tse;
        x_Lock();
    }
}


inline
CTSE_Lock& CTSE_Lock::operator=(const CTSE_Lock& lock)
{
    Set(lock.m_TSE);
    return *this;
}


inline
CTSE_Lock::operator bool(void) const
{
    return m_TSE;
}


inline
bool CTSE_Lock::operator!(void) const
{
    return !m_TSE;
}


inline
bool CTSE_Lock::operator==(const CTSE_Lock& lock) const
{
    return m_TSE == lock.m_TSE;
}


inline
bool CTSE_Lock::operator!=(const CTSE_Lock& lock) const
{
    return m_TSE != lock.m_TSE;
}


inline
bool CTSE_Lock::operator<(const CTSE_Lock& lock) const
{
    return m_TSE < lock.m_TSE;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.21  2003/02/25 20:10:38  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.20  2003/02/25 14:48:07  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.19  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.18  2003/02/13 14:34:32  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.17  2003/02/05 17:57:41  dicuccio
* Moved into include/objects/objmgr/impl to permit data loaders to be defined
* outside of xobjmgr.
*
* Revision 1.16  2003/02/04 21:46:31  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.15  2003/01/29 22:02:22  grichenk
* Fixed includes for SAnnotSelector
*
* Revision 1.14  2003/01/29 17:45:05  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.13  2002/12/26 21:03:33  dicuccio
* Added Win32 export specifier.  Moved file from src/objects/objmgr to
* include/objects/objmgr.
*
* Revision 1.12  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
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

#endif  /* OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP */
