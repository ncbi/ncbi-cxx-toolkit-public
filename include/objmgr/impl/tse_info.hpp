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
* ---------------------------------------------------------------------------
* $Log$
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


#include "bioseq_info.hpp"
#include <objects/seqset/Seq_entry.hpp>
#include <util/rangemap.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
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


class CTSE_Info : public CObject
{
public:
    // 'ctors
    CTSE_Info(void);
    virtual ~CTSE_Info(void);

    void Lock(void) const;
    void Unlock(void) const;
    bool Locked(void) const;

    bool operator< (const CTSE_Info& info) const;
    bool operator== (const CTSE_Info& info) const;
    typedef map<CSeq_id_Handle, CRef<CBioseq_Info> >                 TBioseqMap;
    typedef CRange<int>                                              TRange;
    typedef CRangeMultimap<CRef<CAnnotObject>,TRange::position_type> TRangeMap;
    typedef map<CSeq_id_Handle, TRangeMap>                           TAnnotMap;

    // Reference to the TSE
    CRef<CSeq_entry> m_TSE;
    // Dead seq-entry flag
    bool m_Dead;
    TBioseqMap m_BioseqMap;
    TAnnotMap  m_AnnotMap;

private:
    // Hide copy methods
    CTSE_Info(const CTSE_Info& info);
    CTSE_Info& operator= (const CTSE_Info& info);

    mutable int m_LockCount;
    static CFastMutex sm_LockMutex;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
void CTSE_Info::Lock(void) const
{
    CFastMutexGuard guard(sm_LockMutex);
    m_LockCount++;
}

inline
void CTSE_Info::Unlock(void) const
{
    CFastMutexGuard guard(sm_LockMutex);
    if (m_LockCount > 0)
        m_LockCount--;
    else
        throw runtime_error(
        "CTSE_Info: can not Unlock() -- the TSE is not locked");
}

inline
bool CTSE_Info::Locked(void) const
{
    CFastMutexGuard guard(sm_LockMutex);
    return m_LockCount > 0;
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___TSE_INFO__HPP */
