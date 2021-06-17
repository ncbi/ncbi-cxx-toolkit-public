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
* Author: Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_table_info.hpp>
#include <objects/seqloc/seqloc__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_table_CI::CSeq_table_CI(CScope& scope, const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, scope, loc)
{
}


CSeq_table_CI::CSeq_table_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, scope, loc, &sel)
{
}


CSeq_table_CI::CSeq_table_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown)
{
}


CSeq_table_CI::CSeq_table_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table,
                     bioseq,
                     range,
                     strand)
{
}


CSeq_table_CI::CSeq_table_CI(const CBioseq_Handle& bioseq,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown,
                     &sel)
{
}


CSeq_table_CI::CSeq_table_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table,
                     bioseq,
                     range,
                     eNa_strand_unknown,
                     &sel)
{
}


CSeq_table_CI::CSeq_table_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table,
                     bioseq,
                     range,
                     strand,
                     &sel)
{
}


CSeq_table_CI::CSeq_table_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, annot)
{
}


CSeq_table_CI::CSeq_table_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, annot, &sel)
{
}


CSeq_table_CI::CSeq_table_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, entry)
{
}


CSeq_table_CI::CSeq_table_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Seq_table, entry, &sel)
{
}


CSeq_table_CI::~CSeq_table_CI(void)
{
}


bool CSeq_table_CI::IsMapped(void) const
{
    return Get().GetMappingInfo().IsMapped();
}


const CSeq_loc& CSeq_table_CI::GetOriginalLocation(void) const
{
    return *GetAnnot().x_GetInfo().GetTableInfo().GetTableLocation();
}


const CSeq_loc& CSeq_table_CI::GetMappedLocation(void) const
{
    const CAnnotMapping_Info& info = Get().GetMappingInfo();
    if ( info.IsMapped() ) {
        if ( info.MappedSeq_locNeedsUpdate() ) {
            CRef<CSeq_loc>      created_loc;
            CRef<CSeq_point>    created_pnt;
            CRef<CSeq_interval> created_int;
            info.UpdateMappedSeq_loc(created_loc,
                                     created_pnt,
                                     created_int,
                                     0);
            m_MappedLoc = created_loc;
        }
        else {
            m_MappedLoc = &info.GetMappedSeq_loc();
        }
    }
    else {
        m_MappedLoc = &GetOriginalLocation();
    }
    return *m_MappedLoc;
}


END_SCOPE(objects)
END_NCBI_SCOPE
