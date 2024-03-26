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
*           Eugene Vasilchenko
*
* File Description:
*           API for sorting bioseq handles to improve the performance of data loaders.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/bioseq_sort.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_Scope

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSortableBioseq::CSortableBioseq(const CBioseq_Handle& bioseq, size_t idx)
    : m_Bioseq(bioseq), m_Index(idx), m_SortedIndex(0)
{
}


bool CSortableBioseq::operator<(const CSortableBioseq& bioseq) const
{
    return m_Bioseq < bioseq.m_Bioseq;
}


CSortedBioseqs::CSortedBioseqs(const TBioseqs& bioseqs)
    : m_OriginalSize(bioseqs.size())
{
    m_SortedBioseqs.reserve(m_OriginalSize);
    for (size_t i = 0; i < m_OriginalSize; ++i) {
        if ( bioseqs[i] && !bioseqs[i].GetId().empty() ) {
            m_SortedBioseqs.push_back(Ref(new CSortableBioseq(bioseqs[i], i)));
        }
    }
    sort(m_SortedBioseqs.begin(), m_SortedBioseqs.end());
}


void CSortedBioseqs::GetSortedBioseqs(TBioseqs& bioseqs) const
{
    bioseqs.clear();
    for ( auto& sortable_seq : m_SortedBioseqs ) {
        if ( bioseqs.empty() || bioseqs.back() != sortable_seq->GetBioseq() ) {
            bioseqs.push_back(sortable_seq->GetBioseq());
        }
        sortable_seq.GetNCObject().SetSortedIndex(bioseqs.size()-1);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
