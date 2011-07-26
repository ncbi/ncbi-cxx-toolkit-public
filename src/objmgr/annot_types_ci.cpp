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
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CAnnotTypes_CI
/////////////////////////////////////////////////////////////////////////////


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_DataCollector(0)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope)
    : m_DataCollector(new CAnnot_Collector(scope))
{
    return;
}

/*
CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CBioseq_Handle& bioseq,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(bioseq.GetScope()))
{
    if ( !params ) {
        SAnnotSelector sel(type);
        m_DataCollector->x_Initialize(sel,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    else if ( !params->CheckAnnotType(type) ) {
        SAnnotSelector sel(*params);
        sel.ForceAnnotType(type);
        m_DataCollector->x_Initialize(sel,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    else {
        m_DataCollector->x_Initialize(*params,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    Rewind();
}
*/

CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CBioseq_Handle& bioseq,
                               const CRange<TSeqPos>& range,
                               ENa_strand strand,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(bioseq.GetScope()))
{
    if ( !params ) {
        SAnnotSelector sel(type);
        m_DataCollector->x_Initialize(sel, bioseq, range, strand);
    }
    else if ( type != CSeq_annot::C_Data::e_not_set &&
              !params->CheckAnnotType(type) ) {
        SAnnotSelector sel(*params);
        sel.ForceAnnotType(type);
        m_DataCollector->x_Initialize(sel, bioseq, range, strand);
    }
    else {
        m_DataCollector->x_Initialize(*params, bioseq, range, strand);
    }
    Rewind();
}


void CAnnotTypes_CI::x_Init(CScope& scope,
                            const CSeq_loc& loc,
                            const SAnnotSelector& params)
{
    if ( loc.IsWhole() ) {
        CBioseq_Handle bh = scope.GetBioseqHandle(loc.GetWhole());
        if ( bh ) {
            m_DataCollector->x_Initialize(params,
                                          bh,
                                          CRange<TSeqPos>::GetWhole(),
                                          eNa_strand_unknown);
            Rewind();
            return;
        }
    }
    else if ( loc.IsInt() ) {
        const CSeq_interval& seq_int = loc.GetInt();
        CBioseq_Handle bh = scope.GetBioseqHandle(seq_int.GetId());
        if ( bh ) {
            CRange<TSeqPos> range(seq_int.GetFrom(), seq_int.GetTo());
            ENa_strand strand =
                seq_int.IsSetStrand()? seq_int.GetStrand(): eNa_strand_unknown;
            m_DataCollector->x_Initialize(params, bh, range, strand);
            Rewind();
            return;
        }
    }
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    m_DataCollector->x_Initialize(params, master_loc);
    Rewind();
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               CScope& scope,
                               const CSeq_loc& loc,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(scope))
{
    if ( !params ) {
        x_Init(scope, loc, SAnnotSelector(type));
    }
    else if ( type != CSeq_annot::C_Data::e_not_set &&
              !params->CheckAnnotType(type) ) {
        SAnnotSelector sel(*params);
        sel.ForceAnnotType(type);
        x_Init(scope, loc, sel);
    }
    else {
        x_Init(scope, loc, *params);
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CSeq_annot_Handle& annot,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(annot.GetScope()))
{
    SAnnotSelector sel = params ? *params : SAnnotSelector();
    sel.ForceAnnotType(type)
        .SetResolveNone() // nothing to resolve
        .SetLimitSeqAnnot(annot);
    m_DataCollector->x_Initialize(sel);
    Rewind();
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CSeq_entry_Handle& entry,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(entry.GetScope()))
{
    SAnnotSelector sel = params ? *params : SAnnotSelector();
    sel.ForceAnnotType(type)
        .SetResolveNone() // nothing to resolve
        .SetSortOrder(SAnnotSelector::eSortOrder_None)
        .SetLimitSeqEntry(entry);
    m_DataCollector->x_Initialize(sel);
    Rewind();
}


CSeq_annot_Handle CAnnotTypes_CI::GetAnnot(void) const
{
    return Get().GetSeq_annot_Handle();
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    return;
}


const CAnnotTypes_CI::TAnnotTypes& CAnnotTypes_CI::GetAnnotTypes(void) const
{
    if (m_AnnotTypes.empty() && m_DataCollector->m_AnnotTypes.any()) {
        for (size_t i = 0; i < m_DataCollector->m_AnnotTypes.size(); ++i) {
            if ( m_DataCollector->m_AnnotTypes.test(i) ) {
                m_AnnotTypes.push_back(CAnnotType_Index::GetTypeSelector(i));
            }
        }
    }
    return m_AnnotTypes;
}


const CAnnotTypes_CI::TAnnotNames& CAnnotTypes_CI::GetAnnotNames(void) const
{
    return m_DataCollector->x_GetAnnotNames();
}


END_SCOPE(objects)
END_NCBI_SCOPE
