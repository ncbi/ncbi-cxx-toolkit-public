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


/// Helper class which collects seq-align statistics: seq-ids participating in
/// alignments and rows, potential anchors etc. The class is used to create
/// anchored alignments.
/// @sa TAlnStats
/// @sa TScopeAlnStats
/// @sa CreateAnchoredAlnFromAln
/// @sa CreateAnchoredAlnVec
template <class _TAlnIdVec>
class CAlnStats : public CObject
{
public:
    /// Container with one entry per seq-align using the same indexing as
    /// m_AlnVec. Each element is a vector of ids referenced by the seq-align.
    /// See CAlnIdMap for an example of the container implementation.
    /// @sa CAlnIdMap
    typedef _TAlnIdVec TAlnIdVec;

    /// Vector of original seq-aligns.
    typedef typename _TAlnIdVec::TAlnVec TAlnVec;

    /// Vector of ids used in all alignments. Some ids may be included several
    /// times in case of self-aligned sequences.
    typedef typename _TAlnIdVec::TIdVec TIdVec;

    /// Vector of indexes in TIdVec.
    typedef vector<size_t> TIdxVec;

    /// Each id mapped to a vector of indexes in TIdVec.
    typedef map<TAlnSeqIdIRef, TIdxVec, SAlnSeqIdIRefComp> TIdMap;

    typedef int TDim;

    /// Vector, describing how a single id participates in all alignments from
    /// TAlnVec. Each element contains row index for the id or -1 if the id is
    /// not present in the alignment.
    typedef vector<TDim> TRowVec;

    /// One entry per id (in sync with TIdVec). Each entry describes how a
    /// single id participates in all alignments (see TRowVec).
    typedef vector<TRowVec> TRowVecVec;

    /// Bitmap where each bit indicates an alignment from TAlnVec.
    typedef bm::bvector<> TBitVec;

    /// One entry per id (in sync with TIdVec). Each entry is a bitmap - one
    /// bit per alignment, indicating if the id is participating in this
    /// alignment.
    typedef vector<TBitVec> TBitVecVec;

    /// Constructor
    /// @param aln_id_vec
    ///   An instance of CAlnIdMap<> containing the alignments to be indexed.
    /// @sa CAlnIdMap
    CAlnStats(const TAlnIdVec& aln_id_vec) :
        m_AlnIdVec(aln_id_vec),
        m_AlnVec(aln_id_vec.GetAlnVec()),
        m_AlnCount(m_AlnVec.size()),
        m_CanBeAnchored(-1)
    {
        _ASSERT(m_AlnVec.size() == m_AlnIdVec.size());

        for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
            for (size_t row_i = 0;  row_i < m_AlnIdVec[aln_i].size();  ++row_i) {

                const TAlnSeqIdIRef& id = m_AlnIdVec[aln_i][row_i];
                _ASSERT( !id.Empty() );
                TIdMap::iterator it = m_IdMap.lower_bound(id);
                if (it == m_IdMap.end()  ||  *id < *it->first) { // id encountered for a first time, insert it
                    it = m_IdMap.insert(it,
                         TIdMap::value_type(id, TIdxVec()));
                    it->second.push_back(x_AddId(id, aln_i, row_i));
                }
                else { // id exists already
                    TIdxVec& idx_vec = it->second;
                    TIdxVec::iterator idx_it = idx_vec.begin();
                    while (idx_it != idx_vec.end()) {
                        if ( !m_BitVecVec[*idx_it][bm::id_t(aln_i)] ) {
                            // create a mapping b/n the id and the alignment
                            m_BitVecVec[*idx_it][bm::id_t(aln_i)] = true;
                            _ASSERT(m_RowVecVec[*idx_it][aln_i] == -1);
                            m_RowVecVec[*idx_it][aln_i] = int(row_i);
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
        x_IdentifyPotentialAnchors();
    }

    /// How many alignments do we have?
    size_t GetAlnCount(void) const
    {
        return m_AlnCount;
    }

    /// Access the underlying vector of alignments
    const TAlnVec& GetAlnVec(void) const
    {
        return m_AlnVec;
    }

    /// What is the dimension of an alignment?
    TDim GetDimForAln(size_t aln_idx) const
    {
        _ASSERT(aln_idx < GetAlnCount());
        return TDim(m_AlnIdVec[aln_idx].size());
    }

    /// Access the vector of seq-ids of a particular alignment
    const TIdVec& GetSeqIdsForAln(size_t aln_idx) const
    {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnIdVec[aln_idx];
    }

    /// Access the vector of seq-ids of a particular alignment
    const TIdVec& GetSeqIdsForAln(const CSeq_align& aln) const
    {
        return m_AlnIdVec[aln];
    }

    /// Get a set of ids that are aligned to a particular id
    const TIdVec& GetAlignedIds(const TAlnSeqIdIRef& id) const
    {
        typename TAlignedIdsMap::const_iterator it = m_AlignedIdsMap.find(id);
        if (it != m_AlignedIdsMap.end()) {
            // get from cache
            return it->second;
        }
        else {
            TIdMap::const_iterator it = m_IdMap.find(id);
            if (it == m_IdMap.end()) {
                NCBI_THROW(CAlnException, eInvalidRequest,
                    "Seq-id not present in map");
            }
            else {
                // create in cache
                TIdVec& aligned_ids_vec = m_AlignedIdsMap[id];

                // temp, to keep track of already found aligned ids
                TBitVec id_bit_vec;
                id_bit_vec.resize(bm::bvector<>::size_type(m_IdVec.size()));

                const size_t& id_idx = it->second[0];
                for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
                    // if query paricipates in alignment
                    if (m_BitVecVec[id_idx][bm::id_t(aln_i)]) {
                        // find all participating subjects for this alignment
                        for (size_t aligned_id_idx = 0;
                             aligned_id_idx < m_BitVecVec.size();
                             ++aligned_id_idx) {
                            // if an aligned subject
                            if (aligned_id_idx != id_idx  &&
                                m_BitVecVec[aligned_id_idx][bm::id_t(aln_i)]) {
                                    if ( !id_bit_vec[bm::id_t(aligned_id_idx)] ) {
                                    // add only if not already added
                                    id_bit_vec[bm::id_t(aligned_id_idx)] = true;
                                    aligned_ids_vec.push_back
                                        (m_IdVec[aligned_id_idx]);
                                }
                            }
                        }
                    }
                }
                return aligned_ids_vec;
            }
        }
    }

    /// Get vector describing ids usage in each alignment.
    /// @sa TRowVecVec
    const TRowVecVec& GetRowVecVec(void) const
    {
        return m_RowVecVec;
    }

    /// Get map of ids to there indexes in TIdVec.
    /// @sa TIdMap
    const TIdMap& GetIdMap(void) const
    {
        return m_IdMap;
    }

    /// Get vector of all ids from all alignments.
    /// @sa TIdVec
    const TIdVec& GetIdVec(void) const
    {
        return m_IdVec;
    }

    /// Canonical Query-Anchored: all alignments have 2 or 3 rows and
    /// exactly 2 sequences (A and B), A is present on all alignments
    /// on row 1, B on rows 2 (and possibly 3). B can be present on 2
    /// rows only if they represent different strands.
    bool IsCanonicalQueryAnchored(void) const
    {
        // Is the first sequence present in all aligns?
        if (m_BitVecVec[0].count() != m_AlnCount) {
            return false;
        }
        switch (m_IdVec.size()) {
        case 2:
            // two rows: canonical
            return true;
            break;
        case 3:
            // three rows: A, B and B?
            if (*m_IdVec[1] == *m_IdVec[2]) {
                return true;
            }
            else {
                return false;
            }
            break;
        default:
            break;
        }
        return false;
    }

    /// Canonical Multiple: Single alignment with multiple sequences.
    bool IsCanonicalMultiple(void) const
    {
        return GetAlnCount() == 1  &&  ! IsCanonicalQueryAnchored();
    }

    /// Check if there are any ids which can be used as anchors for the
    /// whole set of alignments.
    bool CanBeAnchored(void) const
    {
        if (m_CanBeAnchored < 0) {
            x_IdentifyPotentialAnchors();
        }
        return m_CanBeAnchored;
    }

    /// Get map of potential anchor ids.
    /// NOTE: each is is mapped to vector of indexes in IdVec, not AnchorIdVec.
    const TIdMap& GetAnchorIdMap(void) const
    {
        if (m_CanBeAnchored < 0) {
            x_IdentifyPotentialAnchors();
        }
        return m_AnchorIdMap;
    }

    /// Get vector of potential anchor ids.
    const TIdVec& GetAnchorIdVec(void) const
    {
        if (m_CanBeAnchored < 0) {
            x_IdentifyPotentialAnchors();
        }
        return m_AnchorIdVec;
    }

    /// Get vector of id indexes (from IdVec) for potential anchors.
    const TIdxVec& GetAnchorIdxVec(void) const
    {
        if (m_CanBeAnchored < 0) {
            x_IdentifyPotentialAnchors();
        }
        return m_AnchorIdxVec;
    }

private:
    size_t x_AddId(const TAlnSeqIdIRef& id, size_t aln_i, size_t row_i)
    {
        m_IdVec.push_back(id);
        {
            m_BitVecVec.push_back(TBitVec());
            TBitVec& bit_vec = m_BitVecVec.back();
            bit_vec.resize(bm::bvector<>::size_type(m_AlnCount));
            bit_vec[bm::id_t(aln_i)] = true;
            _ASSERT(m_IdVec.size() == m_BitVecVec.size());
        }
        {
            m_RowVecVec.push_back(TRowVec());
            TRowVec& rows = m_RowVecVec.back();
            rows.resize(m_AlnCount, -1);
            rows[aln_i] = int(row_i);
            _ASSERT(m_IdVec.size() == m_RowVecVec.size());
        }
        return m_IdVec.size() - 1;
    }

    void x_IdentifyPotentialAnchors(void) const
    {
        _ASSERT(m_IdVec.size() == m_BitVecVec.size());
        _ASSERT(m_CanBeAnchored < 0);
        _ASSERT(m_AnchorIdxVec.empty());
        _ASSERT(m_AnchorIdVec.empty());
        _ASSERT(m_AnchorIdMap.empty());
        for (size_t id_idx = 0; id_idx < m_BitVecVec.size(); ++id_idx) {
            if (m_BitVecVec[id_idx].count() == m_AlnCount) {
                // insert into the anchor idx vec:
                m_AnchorIdxVec.push_back(id_idx);

                const TAlnSeqIdIRef& id = m_IdVec[id_idx];

                // insert in the anchor vec
                m_AnchorIdVec.push_back(id);

                // insert in the anchor map
                TIdMap::iterator it = m_AnchorIdMap.lower_bound(id);
                if (it == m_AnchorIdMap.end()  ||  *id < *it->first) { // id encountered for a first time, insert it
                    it = m_AnchorIdMap.insert(it,
                         TIdMap::value_type(id, TIdxVec()));
                }
                it->second.push_back(id_idx);
            }
        }
        m_CanBeAnchored = (m_AnchorIdxVec.empty() ? 0 : 1);
    }

    const TAlnIdVec& m_AlnIdVec; // Vectors of ids for each alignment
    const TAlnVec& m_AlnVec;     // Vector of alignments
    size_t m_AlnCount;           // Number of alignments
    TIdVec m_IdVec;              // Vector of ids
    TIdMap m_IdMap;              // List of m_IdVec indexes for each id
    TBitVecVec m_BitVecVec;      // For each id list alignments which
                                 // contain this id
    TRowVecVec m_RowVecVec;      // For each id list row index in each
                                 // alignment (-1 if not present).

    typedef map<TAlnSeqIdIRef, TIdVec> TAlignedIdsMap;
    mutable TAlignedIdsMap m_AlignedIdsMap;  // cache for GetAlignedIds
    mutable TIdxVec m_AnchorIdxVec; // vector of indexes (as
                                    // represented in m_RowVecVec
                                    // and m_BitVecVec) of the ids
                                    // of potential anchors
    mutable TIdMap m_AnchorIdMap;   // Maps a the id of each
                                    // potential anchor to its
                                    // index(es) in m_RowVecVec
    mutable TIdVec m_AnchorIdVec;   // A vector of potential anchors
    mutable int m_CanBeAnchored;    // If there's at least one
                                    // potential anchor
};


/// Default implementations for alignment stats.
/// @sa TAlnIdMap
/// @sa TScopeAlnIdMap
typedef CAlnStats<TAlnIdMap> TAlnStats;
typedef CAlnStats<TScopeAlnIdMap> TScopeAlnStats;


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_STATS__HPP
