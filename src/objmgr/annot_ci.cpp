
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
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:15  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/annot_ci.hpp>
#include "data_source.hpp"
#include "annot_object.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnot_CI::CAnnot_CI(void)
    : m_DataSource(0),
      m_HandleRangeMap(0)
{
    return;
}


CAnnot_CI::CAnnot_CI(CDataSource& datasource,
                     CHandleRangeMap& loc,
                     SAnnotSelector selector)
    : m_DataSource(&datasource),
      m_Selector(selector),
      m_RangeMap(0),
      m_HandleRangeMap(&loc)
{
    iterate ( CHandleRangeMap::TLocMap, it, m_HandleRangeMap->GetMap() ) {
        if ( !it->second.GetRanges().empty() ) {
            m_RangeMap = m_DataSource->x_GetRangeMap(it->first);
            if ( !m_RangeMap )
                continue;
            m_CurrentHandle = it->first;
            m_CoreRange = it->second.GetOverlappingRange();
            m_Current = m_RangeMap->begin(m_CoreRange);
            if ( m_Current )
                break;
        }
    }
    while ( !x_IsValid() )
        x_Walk();
}


CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : m_DataSource(iter.m_DataSource),
      m_Selector(iter.m_Selector),
      m_RangeMap(iter.m_RangeMap),
      m_CoreRange(iter.m_CoreRange),
      m_Current(iter.m_Current),
      m_HandleRangeMap(iter.m_HandleRangeMap),
      m_CurrentHandle(iter.m_CurrentHandle)
{
    return;
}


CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    m_DataSource = iter.m_DataSource;
    m_Selector = iter.m_Selector;
    m_RangeMap = iter.m_RangeMap;
    m_CoreRange = iter.m_CoreRange;
    m_Current = iter.m_Current;
    m_HandleRangeMap = iter.m_HandleRangeMap;
    m_CurrentHandle = iter.m_CurrentHandle;
    return *this;
}


CAnnot_CI::~CAnnot_CI(void)
{
    return;
}


CAnnot_CI& CAnnot_CI::operator++(void)
{
    x_Walk();
    return *this;
}


CAnnot_CI& CAnnot_CI::operator++(int)
{
    x_Walk();
    return *this;
}


CAnnot_CI::operator bool (void) const
{
    _ASSERT(m_DataSource);
    return m_Current;
}


CAnnotObject& CAnnot_CI::operator* (void) const
{
    _ASSERT(bool(m_DataSource)  &&  bool(m_Current));
    return *m_Current->second;
}


CAnnotObject* CAnnot_CI::operator-> (void) const
{
    _ASSERT(bool(m_DataSource)  &&  bool(m_Current));
    return m_Current->second;
}


bool CAnnot_CI::x_ValidLocation(const CHandleRangeMap& loc) const
{
    return m_HandleRangeMap->IntersectingWithMap(loc);
}


bool CAnnot_CI::x_IsValid(void) const
{
    if ( !bool(m_DataSource)  ||  !bool(m_Current))
        return true; // Do not need to walk ahead
    if (m_Selector.m_AnnotChoice == CSeq_annot::C_Data::e_not_set)
        return true;
    const CAnnotObject& obj = *m_Current->second;
    if (m_Selector.m_AnnotChoice != obj.Which())
        return false;

    if ( obj.IsFeat() ) {
        if (m_Selector.m_FeatChoice != obj.GetFeat().GetData().Which()  &&
            m_Selector.m_FeatChoice != CSeqFeatData::e_not_set) {
            return false; // bad feature type
        }
    }

    return x_ValidLocation(obj.GetRangeMap());
}


void CAnnot_CI::x_Walk(void)
{
    _ASSERT(bool(m_DataSource)  &&  bool(m_Current));
    for (++m_Current; m_Current; ++m_Current) {
        if ( x_IsValid() ) {
            // Valid type and location or end of annotations
            return;
        }
    }
    // No valid annotations found for the current handle -- try
    // to select the next one.
    CHandleRangeMap::TLocMap::const_iterator h =
        m_HandleRangeMap->GetMap().find(m_CurrentHandle);
    for (++h; h != m_HandleRangeMap->GetMap().end(); ++h) {
        if ( !h->second.GetRanges().empty() ) {
            m_RangeMap = m_DataSource->x_GetRangeMap(h->first);
            if ( !m_RangeMap )
                continue;
            m_CurrentHandle = h->first;
            m_CoreRange = h->second.GetOverlappingRange();
            m_Current = m_RangeMap->begin(m_CoreRange);
            for ( ; m_Current; ++m_Current) {
                if ( x_IsValid() ) {
                    // Valid type and location or end of annotations
                    return;
                }
            }
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
