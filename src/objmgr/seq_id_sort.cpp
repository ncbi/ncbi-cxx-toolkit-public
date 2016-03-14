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
* Author:
*           Aleksey Grichenko
*
* File Description:
*           API for sorting seq-ids to improve the performance of data loaders.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_id_sort.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_Scope

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSortableSeq_id::CSortableSeq_id(const CSeq_id_Handle& idh, size_t idx)
    : m_Id(idh), m_Index(idx)
{
    if ( !m_Id ) return;
    switch ( m_Id.Which() ) {
    // Use default comparison for most seq-id types.
    // Int ids
    case CSeq_id::e_not_set:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Gi:
    // Some non-int ids
    case CSeq_id::e_Local:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_Pdb:
    // Textseq-id
    case CSeq_id::e_Genbank:
    case CSeq_id::e_Embl:
    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot:
    case CSeq_id::e_Other:
    case CSeq_id::e_Ddbj:
    case CSeq_id::e_Prf:
    case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:
    case CSeq_id::e_Tpd:
    case CSeq_id::e_Gpipe:
        break;

    case CSeq_id::e_General:
    {
        CConstRef<CSeq_id> id = m_Id.GetSeqIdOrNull();
        _ASSERT(id);
        const CDbtag& dbt = id->GetGeneral();

        m_Parts.push_back(SIdPart(dbt.GetDb()));
        if ( dbt.GetTag().IsId() ) {
            m_Parts.push_back(SIdPart((Uint8)dbt.GetTag().GetId()));
        }
        else {
            x_ParseParts(dbt.GetTag().GetStr());
        }
        break;
    }

    default:
        break;
    }
}


bool CSortableSeq_id::operator<(const CSortableSeq_id& id) const
{
    if (m_Id.Which() != id.m_Id.Which()  ||
        (m_Parts.empty()  &&  id.m_Parts.empty())) {
        return CSeq_id_Handle::PLessOrdered()(m_Id, id.m_Id);
    }

    size_t i = 0;
    for (; i < m_Parts.size(); ++i) {
        if (i >= id.m_Parts.size()) return false; // shorter first
        const SIdPart& p1 = m_Parts[i];
        const SIdPart& p2 = id.m_Parts[i];
        if (p1.m_IsInt != p2.m_IsInt) {
            return p1.m_IsInt; // numbers first
        }
        if (p1.m_IsInt) {
            if (p1.m_Int != p2.m_Int) return p1.m_Int < p2.m_Int; // by number
        }
        else {
            int c = p1.m_Str.compare(p2.m_Str); // by string
            if (c != 0) return c < 0;
        }
    }
    return i < id.m_Parts.size(); // shorter (first) or equal.
}


void CSortableSeq_id::x_ParseParts(const string& s)
{
    size_t pos = 0;
    size_t delim = s.find('.');
    while (delim != NPOS) {
        // TODO: How to handle duplicate delimiters?
        if (delim > pos) {
            m_Parts.push_back(SIdPart(s.substr(pos, delim - pos)));
        }
        pos = delim + 1;
        delim = s.find('.', pos);
    }
    if (pos < s.size()) {
        m_Parts.push_back(SIdPart(s.substr(pos, NPOS)));
    }
}


CSortedSeq_ids::CSortedSeq_ids(const TIds& ids)
{
    m_SortedIds.reserve(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        m_SortedIds.push_back(Ref(new CSortableSeq_id(ids[i], i)));
    }
    sort(m_SortedIds.begin(), m_SortedIds.end());
}


void CSortedSeq_ids::GetSortedIds(TIds& ids) const
{
    ids.resize(m_SortedIds.size());
    for (size_t i = 0; i < m_SortedIds.size(); ++i) {
        ids[i] = m_SortedIds[i]->GetId();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
