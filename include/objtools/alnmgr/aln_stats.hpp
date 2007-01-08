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


BEGIN_NCBI_SCOPE


template <class _TAlnSeqIdVector>
class CAlnStats : public CObject
{
public:
    /// Typedefs
    typedef _TAlnSeqIdVector TAlnSeqIdVector;
    typedef typename _TAlnSeqIdVector::TAlnVector TAlnVector;
    typedef typename _TAlnSeqIdVector::TIdVector TIdVector;
    typedef int TDim;
    typedef vector<TDim> TRowVec;
    typedef vector<TRowVec> TRowVecVec;
    typedef bm::bvector<> TBitVec;
    typedef vector<TBitVec> TBitVecVec;
    typedef vector<size_t> TIdxVec;
    typedef map<TAlnSeqIdIRef, TIdxVec, SAlnSeqIdIRefComp> TIdMap;


    /// Constructor
    CAlnStats(const TAlnSeqIdVector& aln_seq_id_vector) :
        m_AlnIdVec(aln_seq_id_vector),
        m_AlnVec(aln_seq_id_vector.GetAlnVector()),
        m_AlnCount(m_AlnVec.size())
    {
        _ASSERT(m_AlnVec.size() == m_AlnIdVec.size());
        
        // Construct the m_IdAlnBitmap
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
                        if (m_RowVecVec[*idx_it][aln_i] == -1) {
                            break;
                        }
                        ++idx_it;
                    }
                    if (idx_it == idx_vec.end()) {
                        // we have already encountered this id in the
                        // current alignment, this means the sequence
                        // is aligned to itself
                        idx_vec.push_back(x_AddId(id, aln_i, row_i));
                    } else {
                        m_RowVecVec[*idx_it][aln_i] = row_i;
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
    const TAlnVector& GetAlnVector() const {
        return m_AlnVec;
    }


    /// What is the dimension of an alignment?
    TDim GetDimForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnIdVec[aln_idx].size();
    }


    /// Access a vector of seq-ids for an alignment
    const TIdVector& GetSeqIdsForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnIdVec[aln_idx];
    }

    
    const TRowVecVec& GetRowVecVec() const {
        return m_RowVecVec;
    }


    const TIdMap& GetIdMap() const {
        return m_IdMap;
    }
        

    const TIdVector& GetIdVector() const {
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


    /// Dump in human readable text format:
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        os << "Number of alignments: " << GetAlnCount() << endl;
        os << "IsCanonicalQueryAnchored:" << IsCanonicalQueryAnchored() << endl;
        os << "IsCanonicalMultiple:" << IsCanonicalMultiple() << endl;
        os << endl;
        os << "IdVector (" << GetIdVector().size() << "):" << endl;
        ITERATE(typename TIdVector, it, GetIdVector()) {
            os << (*it)->AsString() << " (base_width=" << (*it)->GetBaseWidth() << ")" << endl;
        }
        os << endl;
        os << "IdMap (" << GetIdMap().size() << "):" << endl;
        ITERATE(TIdMap, it, GetIdMap()) {
            os << it->first->AsString() << " (base_width=" << it->first->GetBaseWidth() << ")" << endl;
        }
        os << endl;
        for (size_t aln_idx = 0;  aln_idx < GetAlnCount();  ++aln_idx) {
            TDim dim = GetDimForAln(aln_idx);
            os << "Alignment " << aln_idx << " has " 
               << dim << " rows:" << endl;
            for (TDim row = 0;  row < dim;  ++row) {
                os << GetSeqIdsForAln(aln_idx)[row]->AsString();
                os << endl;
            }
            os << endl;
        }
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


    const TAlnSeqIdVector& m_AlnIdVec;
    const TAlnVector& m_AlnVec;
    size_t m_AlnCount;
    TIdMap m_IdMap;
    TIdVector m_IdVec;
    TRowVecVec m_RowVecVec;
    TBitVecVec m_BitVecVec;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2007/01/08 16:38:20  todorov
* Added an assertion.
* Fixed the comments.
*
* Revision 1.5  2007/01/04 21:12:02  todorov
* + IsCanonicalMultiple()
*
* Revision 1.4  2006/12/12 20:48:58  todorov
* + #include <objtools/alnmgr/aln_seqid.hpp>
*
* Revision 1.3  2006/12/12 20:40:01  todorov
* Stats are now generated upon construction and offer multiple views of the
* relationships between ids and alignments.
*
* Revision 1.2  2006/11/20 18:43:23  todorov
* anchor rows and base widtsh are now const refs.
*
* Revision 1.1  2006/11/17 05:33:35  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_STATS__HPP
