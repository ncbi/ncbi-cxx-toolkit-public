#ifndef OBJTOOLS_ALNMGR___ALN_STATS__HPP
#define OBJTOOLS_ALNMGR___ALN_STATS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-align statistics
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <objtools/alnmgr/aln_seqid.hpp>
#include <objtools/alnmgr/aln_tests.hpp>


BEGIN_NCBI_SCOPE


template <class _TAlnIdVec>
class CAlnStats : public CObject
{
public:
    /// Typedefs
    typedef _TAlnIdVec TAlnIdVec;
    typedef typename _TAlnIdVec::TAlnVec TAlnVec;
    typedef typename _TAlnIdVec::TIdVec TIdVec;
    typedef int TDim;
    typedef vector<TDim> TRowVec;
    typedef vector<TRowVec> TRowVecVec;
    typedef bm::bvector<> TBitVec;
    typedef vector<TBitVec> TBitVecVec;
    typedef vector<size_t> TIdxVec;
    typedef map<TAlnSeqIdIRef, TIdxVec, SAlnSeqIdIRefComp> TIdMap;


    /// Constructor
    CAlnStats(const TAlnIdVec& aln_id_vec) :
        m_AlnIdVec(aln_id_vec),
        m_AlnVec(aln_id_vec.GetAlnVec()),
        m_AlnCount(m_AlnVec.size())
    {
        _ASSERT(m_AlnVec.size() == m_AlnIdVec.size());
        
        for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
            for (size_t row_i = 0;  row_i < m_AlnIdVec[aln_i].size();  ++row_i) {
                
                const TAlnSeqIdIRef& id = m_AlnIdVec[aln_i][row_i];
                _ASSERT( !id.Empty() );
                TIdMap::iterator it = m_IdMap.lower_bound(id);
                if (it == m_IdMap.end()  ||  *id < *it->first) { // id encountered for a first time, insert it
                    it = m_IdMap.insert
                        (it,
                         TIdMap::value_type(id, TIdxVec()));
                    it->second.push_back(x_AddId(id, aln_i, row_i));
                } else { // id exists already
                    TIdxVec& idx_vec = it->second;
                    TIdxVec::iterator idx_it = idx_vec.begin();
                    while (idx_it != idx_vec.end()) {
                        if ( !m_BitVecVec[*idx_it][aln_i] ) {
                            // create a mapping b/n the id and the alignment
                            m_BitVecVec[*idx_it][aln_i] = true;
                            _ASSERT(m_RowVecVec[*idx_it][aln_i] == -1);
                            m_RowVecVec[*idx_it][aln_i] = row_i;
                            break;
                        }
                        ++idx_it;
                    }
                    if (idx_it == idx_vec.end()) {
                        // create an extra identical id for this
                        // alignment.  (the sequence is aligned to
                        // itself)
                        idx_vec.push_back(x_AddId(id, aln_i, row_i));
                    }
                }
            }
        }
    }


    /// How many alignments do we have?
    size_t GetAlnCount() const {
        return m_AlnCount;
    }


    /// Access the underlying vector of alignments
    const TAlnVec& GetAlnVec() const {
        return m_AlnVec;
    }


    /// What is the dimension of an alignment?
    TDim GetDimForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnIdVec[aln_idx].size();
    }


    /// Access the vector of seq-ids of a particular alignment
    const TIdVec& GetSeqIdsForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnIdVec[aln_idx];
    }

    
    /// Access the vector of seq-ids of a particular alignment
    const TIdVec& GetSeqIdsForAln(const CSeq_align& aln) const {
        return m_AlnIdVec[aln];
    }


    /// Get a set of ids that are aligned to a particular id
    const TIdVec& GetAlignedIds(const TAlnSeqIdIRef& id) const {
        typename TAlignedIdsMap::const_iterator it = m_AlignedIdsMap.find(id);
        if (it != m_AlignedIdsMap.end()) {
            return it->second;
        } else {
            TIdMap::const_iterator it = m_IdMap.find(id);
            if (it == m_IdMap.end()) {
                NCBI_THROW(CAlnException, eInvalidRequest,
                           "Seq-id not present in map");
            } else {
                TIdVec& aligned_ids_vec = m_AlignedIdsMap[id];

                // Create a temp set to eliminate dups
                typedef set<TAlnSeqIdIRef> TAlignedIdsSet;
                TAlignedIdsSet aligned_ids_set;

                const size_t& id_idx = it->second[0];
                for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
                    if (m_BitVecVec[id_idx][aln_i]) {
                        ITERATE(typename TIdVec, id_i, m_AlnIdVec[aln_i]) {
                            TAlignedIdsSet::iterator it;
                            if (**id_i != *id  &&
                                (aligned_ids_set.find(*id_i) == aligned_ids_set.end())) {
                                aligned_ids_set.insert(*id_i);
                                aligned_ids_vec.push_back(*id_i);
                            }
                        }
                    }
                }
                return aligned_ids_vec;
            }
        }
    }


    const TRowVecVec& GetRowVecVec() const {
        return m_RowVecVec;
    }


    const TIdMap& GetIdMap() const {
        return m_IdMap;
    }
        

    const TIdVec& GetIdVec() const {
        return m_IdVec;
    }


    /// Canonical Query-Anchored: all alignments have 2 or 3 rows and
    /// exactly 2 sequences (A and B), A is present on all alignments
    /// on row 1, B on rows 2 (and possibly 3). B can be present on 2
    /// rows only if they represent different strands.
    bool IsCanonicalQueryAnchored() const {
        switch (m_IdVec.size()) {
        case 2:
            // Is the first sequence present in all aligns?
            if (m_BitVecVec[0].count() == m_AlnCount) {
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

    
    /// Canonical Multiple: one alignment only.
    bool IsCanonicalMultiple() const {
        return (GetAlnCount() == 1);
    }


private:
    size_t x_AddId(const TAlnSeqIdIRef& id, size_t aln_i, size_t row_i) {
        m_IdVec.push_back(id);
        {
            m_BitVecVec.push_back(TBitVec());
            TBitVec& bit_vec = m_BitVecVec.back();
            bit_vec.resize(m_AlnCount);
            bit_vec[aln_i] = true;
            _ASSERT(m_IdVec.size() == m_BitVecVec.size());
        }
        {
            m_RowVecVec.push_back(TRowVec());
            TRowVec& rows = m_RowVecVec.back();
            rows.resize(m_AlnCount, -1);
            rows[aln_i] = row_i;
            _ASSERT(m_IdVec.size() == m_RowVecVec.size());
        }
        return m_IdVec.size() - 1;
    }


    typedef map<TAlnSeqIdIRef, TIdVec> TAlignedIdsMap;

    const TAlnIdVec& m_AlnIdVec;
    const TAlnVec& m_AlnVec;
    size_t m_AlnCount;
    TIdMap m_IdMap;
    TIdVec m_IdVec;
    TRowVecVec m_RowVecVec;
    TBitVecVec m_BitVecVec;
    mutable TAlignedIdsMap m_AlignedIdsMap;
};


/// Typical usage:
typedef CAlnStats<TAlnIdMap> TAlnStats;


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_STATS__HPP
