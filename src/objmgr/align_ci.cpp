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
#include <objmgr/align_ci.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown)
{
}


CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq,
                     range,
                     strand)
{
}


CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown,
                     &sel)
{
}


CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq,
                     range,
                     eNa_strand_unknown,
                     &sel)
{
}


CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq,
                     range,
                     strand,
                     &sel)
{
}


CAlign_CI::CAlign_CI(CScope& scope,
                     const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, scope, loc)
{
}


CAlign_CI::CAlign_CI(CScope& scope,
                     const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, scope, loc, &sel)
{
}


CAlign_CI::CAlign_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, annot)
{
}


CAlign_CI::CAlign_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, annot, &sel)
{
}


CAlign_CI::CAlign_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, entry)
{
}


CAlign_CI::CAlign_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, entry, &sel)
{
}


CAlign_CI::~CAlign_CI(void)
{
}


CAlign_CI& CAlign_CI::operator++ (void)
{
    Next();
    m_MappedAlign.Reset();
    return *this;
}


CAlign_CI& CAlign_CI::operator-- (void)
{
    Prev();
    m_MappedAlign.Reset();
    return *this;
}


const CSeq_align& CAlign_CI::operator* (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    if (!m_MappedAlign) {
        if ( annot.GetMappingInfo().IsMapped() ) {
            m_MappedAlign.Reset(&annot.GetMappingInfo().GetMappedSeq_align(
                annot.GetAlign()));
        }
        else {
            m_MappedAlign.Reset(&annot.GetAlign());
        }
    }
    return *m_MappedAlign;
}


const CSeq_align* CAlign_CI::operator-> (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    if (!m_MappedAlign) {
        if ( annot.GetMappingInfo().IsMapped() ) {
            m_MappedAlign.Reset(&annot.GetMappingInfo().GetMappedSeq_align(
                annot.GetAlign()));
        }
        else {
            m_MappedAlign.Reset(&annot.GetAlign());
        }
    }
    return m_MappedAlign.GetPointer();
}


const CSeq_align& CAlign_CI::GetOriginalSeq_align(void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    return annot.GetAlign();
}


CSeq_align_Handle CAlign_CI::GetSeq_align_Handle(void) const
{
    return CSeq_align_Handle(GetAnnot(), GetIterator()->GetAnnotIndex());
}


END_SCOPE(objects)
END_NCBI_SCOPE
