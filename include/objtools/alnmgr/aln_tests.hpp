#ifndef OBJTOOLS_ALNMGR___ALN_TESTS__HPP
#define OBJTOOLS_ALNMGR___ALN_TESTS__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Tests on Seq-align containers
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/seqalign_exception.hpp>

#include <objtools/alnmgr/seqids_extractor.hpp>

#include <util/bitset/ncbi_bitset.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Vector of Seq-ids per Seq-align
template <class _TAlnVector,
          class _TSeqIdPtrComp,
          class _TSeqIdPtr = const CSeq_id*>
class CAlnSeqIdVector : public CObject
{
public:
    /// Types:
    typedef _TAlnVector TAlnVector;
    typedef _TSeqIdPtrComp TSeqIdPtrComp;
    typedef _TSeqIdPtr TSeqIdPtr;
    typedef vector<TSeqIdPtr> TSeqIdVector;
private:
    typedef vector<TSeqIdVector> TAlnSeqIdVector;
public:
    typedef typename TAlnSeqIdVector::value_type value_type;
    typedef size_t size_type;


    /// Construction
    CAlnSeqIdVector(const TAlnVector& aln_vector,
                    TSeqIdPtrComp& seq_id_ptr_comp) :
        m_AlnVector(aln_vector),
        m_Comp(seq_id_ptr_comp),
        m_Size(m_AlnVector.size()),
        m_AlnSeqIdVector(m_Size),
        m_SeqIdsExtract(m_Comp)
    {
    }


    /// Accessing the seq-ids of a particular seq-align
    const TSeqIdVector& operator[](size_t aln_idx) const
    {
        _ASSERT(aln_idx < m_Size);
        if (m_AlnSeqIdVector[aln_idx].empty()) {
            m_SeqIdsExtract(*m_AlnVector[aln_idx],
                            m_AlnSeqIdVector[aln_idx]);
            _ASSERT( !m_AlnSeqIdVector[aln_idx].empty() );
        }
        return m_AlnSeqIdVector[aln_idx];
    }


    /// Accessing the underlying TAlnVector
    const TAlnVector& GetAlnVector() const {
        return m_AlnVector;
    }


    /// Size
    size_type size() const {
        return m_Size;
    }

private:
    const TAlnVector& m_AlnVector;
    TSeqIdPtrComp& m_Comp;
    const size_t m_Size;
    mutable TAlnSeqIdVector m_AlnSeqIdVector;
    CSeqIdsExtractor<TSeqIdPtrComp> m_SeqIdsExtract;
};


/// Seq-id to Seq-align map
template<class TAlnSeqIdVector>
class CSeqIdAlnBitmap : public CObject
{
public:
    /// Typedefs
    typedef CSeq_align::TDim TDim;
    typedef vector< vector<int> > TBaseWidthVector;
    typedef vector<TDim> TAnchorRowVector;
    typedef typename TAlnSeqIdVector::TSeqIdVector TSeqIdVector;
    typedef typename TAlnSeqIdVector::TSeqIdPtr TSeqIdPtr;
    typedef typename TAlnSeqIdVector::TSeqIdPtrComp TSeqIdPtrComp;

    /// Constructor
    CSeqIdAlnBitmap(const TAlnSeqIdVector& aln_id_vec,
                    CScope& scope,
                    const TSeqIdPtrComp& comp) :
        m_AlnIdVec(aln_id_vec),
        m_AlnCount(m_AlnIdVec.size()),
        m_Scope(scope),
        m_Comp(comp),
        m_IsQueryAnchoredTestDone(false),
        m_NucProtBitmapsInitialized(false)
    {
        TSeqIdPtr seq_id;

        for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
            for (size_t seq_i = 0;  seq_i < m_AlnIdVec[aln_i].size();  ++seq_i) {

                seq_id = m_AlnIdVec[aln_i][seq_i];

                CBioseq_Handle bioseq_handle = m_Scope.GetBioseqHandle(*seq_id);
                
                if ( !bioseq_handle ) {
                    string err_str = 
                        string("Seq-id cannot be resolved: ")
                        + seq_id->AsFastaString();
                    NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
                }
        
                if (m_Bitmap[bioseq_handle][aln_i]) {
                    /// we have already encountered this sequence in this alignment
                    /// the sequence is aligned to itself
                    m_SelfAlignedBitmap[bioseq_handle][aln_i] = true;
                } else {
                    m_Bitmap[bioseq_handle][aln_i] = true;
                }

            }
        }
    }

    /// Determine if the alignments are "query-anchored" i.e. if
    /// there's at least one common sequence present in all of them
    bool IsQueryAnchored() const {
        if ( !m_IsQueryAnchoredTestDone ) {
            ITERATE(TSeqIdAlnMap, it, m_Bitmap) {
                _ASSERT(it->second.count() <= m_AlnCount);
                if (it->second.count() == m_AlnCount) {
                    m_AnchorHandle = it->first;
                    return m_AnchorHandle;
                }
            }
        }
        return m_AnchorHandle;
    }

    CBioseq_Handle GetAnchorHandle() const {
        return m_AnchorHandle;
    }

    /// Number of alignments
    size_t GetAlnCount() const {
        return m_AlnCount;
    }

    /// Number of seqs
    size_t GetSeqCount() const {
        return m_Bitmap.size();
    }

    /// Number of seqs aligned to itself
    size_t GetSelfAlignedSeqCount() const {
        return m_SelfAlignedBitmap.size();
    }

    /// Number of aligns containing at least one nuc seq
    size_t GetAlnWithNucCount() const {
        x_InitNucProtBitmaps();
        return m_NucleotideBitmap.count();
    }

    /// Number of aligns containing at least one prot seq
    size_t GetAlnWithProtCount() const {
        x_InitNucProtBitmaps();
        return m_ProteinBitmap.count();
    }

    /// Number of aligns containing nuc seqs only
    size_t GetNucOnlyAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_NucleotideBitmap & (~(m_NucleotideBitmap & m_ProteinBitmap))).count();
    }

    /// Number of aligns containing prot seqs only
    size_t GetProtOnlyAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_ProteinBitmap & (~(m_ProteinBitmap & m_NucleotideBitmap))).count();
    }

    /// Number of aligns containing both nuc and prot seqs
    size_t GetTranslatedAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_ProteinBitmap & m_NucleotideBitmap).count();
    }

    typedef bm::bvector<> TBitVector;
    typedef map<CBioseq_Handle, TBitVector> TSeqIdAlnMap;

    /// Get the internally-built structure
    const TSeqIdAlnMap& GetSeqIdAlnMap() const {
        return m_Bitmap;
    }


    /// If translated, what are the base widths?  (Empty vector otherwise)
    const TBaseWidthVector& GetBaseWidths() const {
        if (m_BaseWidths.empty() &&  GetTranslatedAlnCount()) {
            m_BaseWidths.resize(GetAlnCount());
            for (size_t aln_idx = 0;  aln_idx < m_AlnIdVec.size();  ++aln_idx) {
                const TSeqIdVector& ids = m_AlnIdVec[aln_idx];
                m_BaseWidths[aln_idx].resize(ids.size());
                for (size_t row = 0; row < ids.size(); ++row)   {
                    CBioseq_Handle bioseq_handle = m_Scope.GetBioseqHandle(*ids[row]);
                    if (bioseq_handle.IsProtein()) {
                        m_BaseWidths[aln_idx][row] = 3;
                    } else if (bioseq_handle.IsNucleotide()) {
                        m_BaseWidths[aln_idx][row] = 1;
                    } else {
                        string err_str =
                            string("Cannot determine molecule type for seq-id: ")
                            + ids[row]->AsFastaString();
                        NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
                    }
                }
            }
        }
        return m_BaseWidths;
    }


    /// If query-anchored, what are the anchor rows?  (Empty vector otherwise)
    const TAnchorRowVector& GetAnchorRows() const {
        if (m_AnchorRows.empty()  &&  IsQueryAnchored()) {
            TSeqIdPtr anchor_id = GetAnchorHandle().GetSeqId();
            m_AnchorRows.resize(GetAlnCount(), -1);
            for (size_t aln_idx = 0;  aln_idx < m_AnchorRows.size();  ++aln_idx) {
                const TSeqIdVector& ids = m_AlnIdVec[aln_idx];
                for (size_t row = 0; row < ids.size(); ++row) {
                    if ( !(m_Comp(ids[row], anchor_id) ||
                           m_Comp(anchor_id, ids[row])) ) {
                        m_AnchorRows[aln_idx] = row;
                        break;
                    }
                }
                _ASSERT(m_AnchorRows[aln_idx] >= 0);
            }
        }
        return m_AnchorRows;
    }


    /// Dump in human readable text format
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        os << "QueryAnchoredTest:" 
           << IsQueryAnchored() << endl;
        os << "Number of alignments: " << GetAlnCount() << endl;
        os << "Number of alignments containing nuc seqs:" << GetAlnWithNucCount() << endl;
        os << "Number of alignments containing prot seqs:" << GetAlnWithProtCount() << endl;
        os << "Number of alignments containing nuc seqs only:" << GetNucOnlyAlnCount() << endl;
        os << "Number of alignments containing prot seqs only:" << GetProtOnlyAlnCount() << endl;
        os << "Number of alignments containing both nuc and prot seqs:" << GetTranslatedAlnCount() << endl;
        os << "Number of sequences:" << GetSeqCount() << endl;
        os << "Number of self-aligned sequences:" << GetSelfAlignedSeqCount() << endl;
        os << endl;
    }
    

private:

    void x_InitNucProtBitmaps() const {
        if ( !m_NucProtBitmapsInitialized ) {
            ITERATE(TSeqIdAlnMap, it, m_Bitmap) {
                if (it->first.IsProtein()) {
                    m_ProteinBitmap |= it->second;
                    _ASSERT(m_ProteinBitmap.count());
                } else if (it->first.IsNucleotide()) {
                    m_NucleotideBitmap |= it->second;
                    _ASSERT(m_NucleotideBitmap.count());
                } else {
                    string err_str =
                        string("Cannot determine molecule type for seq-id: ")
                        + it->first.GetSeqId()->AsFastaString();
                    NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
                }
            }
            m_NucProtBitmapsInitialized = true;
        }
    }

    TAlnSeqIdVector m_AlnIdVec;
    size_t m_AlnCount;
    CScope& m_Scope;
    TSeqIdAlnMap m_Bitmap;
    const TSeqIdPtrComp& m_Comp;

    mutable bool m_IsQueryAnchoredTestDone;
    mutable CBioseq_Handle m_AnchorHandle;

    mutable bool m_NucProtBitmapsInitialized;
    mutable TBitVector m_ProteinBitmap;
    mutable TBitVector m_NucleotideBitmap;

    TSeqIdAlnMap m_SelfAlignedBitmap;

    mutable TBaseWidthVector m_BaseWidths;
    mutable TAnchorRowVector m_AnchorRows;
};


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.10  2006/12/11 20:43:09  yazhuk
* Removed conflicting TSeqIdPtr definition.
*
* Revision 1.9  2006/11/27 19:39:19  todorov
* Require comp in CSeqIdAlnBitmap
*
* Revision 1.8  2006/11/20 18:47:16  todorov
* + CSeqIdAlnBitmap::GetBaseWidths()
* + CSeqIdAlnBitmap::GetAnchorRows()
*
* Revision 1.7  2006/11/09 00:16:54  todorov
* Fixed Dump.
*
* Revision 1.6  2006/11/08 22:27:59  todorov
* + template <class TOutStream> void Dump(TOutStream& os)
*
* Revision 1.5  2006/11/06 19:59:54  todorov
* Minor changes.  Added GetAlnVector()
*
* Revision 1.4  2006/10/23 17:17:12  todorov
* name change AlnContainer -> AlnVector, since random access is required.
*
* Revision 1.3  2006/10/19 17:08:12  todorov
* - #include <objtools/alnmgr/seqid_comp.hpp>
*
* Revision 1.2  2006/10/17 21:53:31  todorov
* + GetAnchorHandle
* Renamed all accessors with Get*
*
* Revision 1.1  2006/10/17 19:23:57  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_TESTS__HPP
