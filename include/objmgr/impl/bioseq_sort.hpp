#ifndef OBJMGR_IMPL_BIOSEQ_SORT__HPP
#define OBJMGR_IMPL_BIOSEQ_SORT__HPP

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

#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSortableBioseq
/////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT CSortableBioseq : public CObject
{
public:
    CSortableBioseq(const CBioseq_Handle& bioseq, size_t idx);

    bool operator<(const CSortableBioseq& bioseq) const;

    const CBioseq_Handle& GetBioseq(void) const { return m_Bioseq; }
    size_t GetIndex(void) const { return m_Index; }
    size_t GetSortedIndex(void) const { return m_SortedIndex; }
    void SetSortedIndex(size_t index) { m_SortedIndex = index; }

private:
    CSortableBioseq(const CSortableBioseq&);
    CSortableBioseq& operator=(const CSortableBioseq&);

    CBioseq_Handle m_Bioseq;
    size_t m_Index;
    size_t m_SortedIndex;
};


inline
bool operator<(const CRef<CSortableBioseq>& seq1, const CRef<CSortableBioseq>& seq2)
{
    return *seq1 < *seq2;
}


class NCBI_XOBJMGR_EXPORT CSortedBioseqs
{
public:
    typedef vector<CBioseq_Handle> TBioseqs;

    CSortedBioseqs(const TBioseqs& bioseqs);

    void GetSortedBioseqs(TBioseqs& bioseqs) const;

    template<class TValue> void RestoreOrder(vector<TValue>& values) const
    {
        vector<TValue> tmp = std::move(values);
        values.clear();
        values.resize(m_OriginalSize);
        for ( auto& sortable_seq : m_SortedBioseqs ) {
            values[sortable_seq->GetIndex()] = tmp[sortable_seq->GetSortedIndex()];
        }
    }

private:
    typedef vector<CRef<CSortableBioseq>> TSortedBioseqs;

    size_t m_OriginalSize;
    TSortedBioseqs m_SortedBioseqs;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJMGR_IMPL_BIOSEQ_SORT__HPP
