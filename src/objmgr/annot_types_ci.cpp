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
*   Object manager iterators
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.8  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.7  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.6  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.5  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:51:18  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/annot_types_ci.hpp>
#include "data_source.hpp"
#include "tse_info.hpp"
#include "handle_range_map.hpp"
#include <objects/objmgr1/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <serial/typeinfo.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_Location(0)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               SAnnotSelector selector)
    : m_Selector(selector),
      m_Location(new CHandleRangeMap(scope.x_GetIdMapper()))
{
    m_Location->AddLocation(loc);
    // Search all possible TSEs
    scope.x_PopulateTSESet(*m_Location, m_Entries);
    non_const_iterate(TTSESet, tse_it, m_Entries) {
        (*tse_it)->Lock();
    }
    m_CurrentTSE = m_Entries.begin();
    for ( ; m_CurrentTSE != m_Entries.end(); ++m_CurrentTSE) {
        m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE, *m_Location, m_Selector);
        if ( m_CurrentAnnot )
            break;
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(CBioseq_Handle& bioseq,
                               int start, int stop,
                               SAnnotSelector selector)
    : m_Selector(selector),
      m_Location(new CHandleRangeMap(bioseq.m_Scope->x_GetIdMapper()))
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    SerialAssign<CSeq_id>(*id, *bioseq.GetSeqId());
    if ( start == 0  &&  stop == 0 ) {
        loc->SetWhole(*id);
    }
    else {
        loc->SetInt().SetId(*id);
        loc->SetInt().SetFrom(start);
        loc->SetInt().SetTo(stop);
    }
    m_Location->AddLocation(*loc);
    bioseq.m_TSE->Lock();
    m_Entries.insert(bioseq.m_TSE);
    m_CurrentTSE = m_Entries.begin();
    m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE, *m_Location, m_Selector);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
    : m_Selector(it.m_Selector),
      m_Location(new CHandleRangeMap(*it.m_Location)),
      m_CurrentAnnot(it.m_CurrentAnnot)
{
    iterate (TTSESet, itr, it.m_Entries) {
        TTSESet::const_iterator cur = m_Entries.insert(*itr).first;
        (*cur)->Lock();
        if (*itr == *it.m_CurrentTSE)
            m_CurrentTSE = cur;
    }
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    non_const_iterate(TTSESet, tse_it, m_Entries) {
        (*tse_it)->Unlock();
    }
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    {{
        CMutexGuard guard(CDataSource::sm_DataSource_Mutex);
        non_const_iterate(TTSESet, tse_it, m_Entries) {
            (*tse_it)->Unlock();
        }
    }}
    m_Entries.clear();
    iterate (TTSESet, itr, it.m_Entries) {
        TTSESet::const_iterator cur = m_Entries.insert(*itr).first;
        (*cur)->Lock();
        if (*itr == *it.m_CurrentTSE)
            m_CurrentTSE = cur;
    }
    m_Selector = it.m_Selector;
    m_Location.reset(new CHandleRangeMap(*it.m_Location));
    m_CurrentAnnot = it.m_CurrentAnnot;
    return *this;
}


bool CAnnotTypes_CI::IsValid(void) const
{
    return (m_CurrentTSE != m_Entries.end()  &&  m_CurrentAnnot);
}


void CAnnotTypes_CI::Walk(void)
{
    _ASSERT(m_CurrentAnnot);
    CMutexGuard guard(CScope::sm_Scope_Mutex);
    ++m_CurrentAnnot;
    if ( m_CurrentAnnot  ||  m_CurrentTSE == m_Entries.end() )
        // Got the next annot or no more annots and data sources
        return;
    while ( !m_CurrentAnnot ) {
        // Search for the next data source with annots
        ++m_CurrentTSE;
        if (m_CurrentTSE == m_Entries.end())
            return;
        m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE, *m_Location, m_Selector);
    }
}


CAnnotObject* CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return &*m_CurrentAnnot;
}

END_SCOPE(objects)
END_NCBI_SCOPE
