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
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/annot_types_ci.hpp>
#include "handle_range_map.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_Location(0)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(TDataSources::iterator source,
                               TDataSources* sources,
                               const CSeq_loc& loc,
                               SAnnotSelector selector)
    : m_CurrentSource(source),
      m_Sources(sources),
      m_Selector(selector),
      m_Location(new CHandleRangeMap())
{
    m_Location->AddLocation(loc);
    // Protect datasources list
    CMutexGuard guard(CScope::sm_Scope_Mutex);
    for (TDataSources::iterator it = source; it != m_Sources->end(); ++it) {
        (*it)->x_ResloveLocationHandles(*m_Location);
    }
    for ( ; m_CurrentSource != m_Sources->end(); ++m_CurrentSource ) {
        m_CurrentAnnot = (*m_CurrentSource)->
            BeginAnnot(*m_Location, m_Selector);
        if ( m_CurrentAnnot )
            break;
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
    : m_CurrentSource(it.m_CurrentSource),
      m_Sources(it.m_Sources),
      m_Selector(it.m_Selector),
      m_Location(new CHandleRangeMap(*it.m_Location)),
      m_CurrentAnnot(it.m_CurrentAnnot)
{
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    return;
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    m_CurrentSource = it.m_CurrentSource;
    m_Sources = it.m_Sources;
    m_Selector = it.m_Selector;
    m_Location.reset(new CHandleRangeMap(*it.m_Location));
    m_CurrentAnnot = it.m_CurrentAnnot;
    return *this;
}


bool CAnnotTypes_CI::IsValid(void) const
{
    return (m_CurrentSource != m_Sources->end()  &&  m_CurrentAnnot);
}


void CAnnotTypes_CI::Walk(void)
{
    _ASSERT(m_CurrentAnnot);
    CMutexGuard guard(CScope::sm_Scope_Mutex);
    ++m_CurrentAnnot;
    if ( m_CurrentAnnot  ||  m_CurrentSource == m_Sources->end() )
        // Got the next annot or no more annots and data sources
        return;
    while ( !m_CurrentAnnot ) {
        // Search for the next data source with annots
        ++m_CurrentSource;
        if (m_CurrentSource == m_Sources->end())
            return;
        m_CurrentAnnot = (*m_CurrentSource)->
            BeginAnnot(*m_Location, m_Selector);
    }
}


CAnnotObject* CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return &*m_CurrentAnnot;
}

END_SCOPE(objects)
END_NCBI_SCOPE
