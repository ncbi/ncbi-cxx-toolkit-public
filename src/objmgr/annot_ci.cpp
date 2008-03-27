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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/annot_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAnnot_CI::CAnnot_CI(void)
{
}


CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : m_SeqAnnotSet(iter.m_SeqAnnotSet),
      m_Iterator(iter? m_SeqAnnotSet.find(*iter): m_SeqAnnotSet.end())
{
}


CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_loc& loc)
{
    x_Initialize(
        CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                       scope, loc,
                       &SAnnotSelector()
                       .SetNoMapping(true)
                       .SetCollectSeq_annots(true)
                       .SetSortOrder(SAnnotSelector::eSortOrder_None)));
}


CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
{
    x_Initialize(
        CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                       scope, loc,
                       &SAnnotSelector(sel)
                       .SetNoMapping(true)
                       .SetCollectSeq_annots(true)
                       .SetSortOrder(SAnnotSelector::eSortOrder_None)));
}


CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq)
{
    x_Initialize(
        CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                       bioseq,
                       CRange<TSeqPos>::GetWhole(),
                       eNa_strand_unknown,
                       &SAnnotSelector()
                       .SetNoMapping(true)
                       .SetCollectSeq_annots(true)
                       .SetSortOrder(SAnnotSelector::eSortOrder_None)));
}


CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq,
                     const SAnnotSelector& sel)
{
    x_Initialize(
        CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                       bioseq,
                       CRange<TSeqPos>::GetWhole(),
                       eNa_strand_unknown,
                       &SAnnotSelector(sel)
                       .SetNoMapping(true)
                       .SetCollectSeq_annots(true)
                       .SetSortOrder(SAnnotSelector::eSortOrder_None)));
}


CAnnot_CI::CAnnot_CI(const CAnnotTypes_CI& iter)
{
    x_Initialize(iter);
}


CAnnot_CI::~CAnnot_CI(void)
{
}


CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    if ( this != &iter ) {
        m_SeqAnnotSet = iter.m_SeqAnnotSet;
        m_Iterator = iter? m_SeqAnnotSet.find(*iter): m_SeqAnnotSet.end();
    }
    return *this;
}


void CAnnot_CI::x_Initialize(const CAnnotTypes_CI& iter)
{
    _ASSERT(m_SeqAnnotSet.empty());
    ITERATE(CAnnot_Collector::TAnnotSet, it, iter.m_DataCollector->m_AnnotSet){
        m_SeqAnnotSet.insert(it->GetSeq_annot_Handle());
    }
    /*
    if ( iter.m_DataCollector->m_FirstAnnotLock ) {
        m_SeqAnnotSet.insert(iter.m_DataCollector->m_FirstAnnotLock);
        ITERATE ( CAnnot_Collector::TAnnotLocks, it,
                  iter.m_DataCollector->m_AnnotLocks ) {
            m_SeqAnnotSet.insert(*it);
        }
    }
    */
    Rewind();
}


END_SCOPE(objects)
END_NCBI_SCOPE
