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

#include <objtools/alnmgr/seqid_comp.hpp>
#include <objtools/alnmgr/seqids_extractor.hpp>

#include <util/bitset/ncbi_bitset.hpp>


/// Implementation includes


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Vector of Seq-ids per Seq-align
template <class TAlnVector,
          class TSeqIdPtrComp,
          class TSeqIdPtr = const CSeq_id*>
class CAlnSeqIdVector
{
public:
    CAlnSeqIdVector(const TAlnVector& aln_container,
                    TSeqIdPtrComp& seq_id_ptr_comp) :
        m_AlnContainer(aln_container),
        m_Comp(seq_id_ptr_comp),
        m_Size(m_AlnContainer.size()),
        m_AlnSeqIdVector(m_Size),
        m_SeqIdsExtract(m_Comp)
    {
    }

    typedef vector<TSeqIdPtr> TSeqIdVector;

    const TSeqIdVector& operator[](size_t aln_idx) const
    {
        _ASSERT(aln_idx < m_Size);
        if (m_AlnSeqIdVector[aln_idx].empty()) {
            m_SeqIdsExtract(*m_AlnContainer[aln_idx],
                            m_AlnSeqIdVector[aln_idx]);
            _ASSERT( !m_AlnSeqIdVector[aln_idx].empty() );
        }
        return m_AlnSeqIdVector[aln_idx];
    }

private:
    typedef vector<TSeqIdVector> TAlnSeqIdVector;

public:
    typedef typename TAlnSeqIdVector::value_type value_type;

    typedef size_t size_type;
    size_type size() const {
        return m_Size;
    }

private:
    const TAlnVector& m_AlnContainer;
    TSeqIdPtrComp& m_Comp;
    const size_t m_Size;
    mutable TAlnSeqIdVector m_AlnSeqIdVector;
    CSeqIdsExtractor<TSeqIdPtrComp> m_SeqIdsExtract;
};


/// Seq-id to Seq-align map
template<class TAlnSeqIdVector>
class CSeqIdAlnBitmap
{
public:
    /// Constructor
    CSeqIdAlnBitmap(const TAlnSeqIdVector& aln_id_vec,
                    CScope& scope) :
        m_Scope(scope),
        m_AlnCount(aln_id_vec.size()),
        m_IsQueryAnchoredSet(false),
        m_IsQueryAnchored(false),
        m_NucProtBitmapsInitialized(false)
    {
        typedef typename TAlnSeqIdVector::value_type::value_type TSeqIdPtr;
        TSeqIdPtr seq_id;

        for (size_t aln_i = 0; aln_i < m_AlnCount; ++aln_i) {
            for (size_t seq_i = 0;  seq_i < aln_id_vec[aln_i].size();  ++seq_i) {

                seq_id = aln_id_vec[aln_i][seq_i];

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
        if ( !m_IsQueryAnchoredSet ) {
            ITERATE(TSeqIdAlnMap, it, m_Bitmap) {
                _ASSERT(it->second.count() <= m_AlnCount);
                if (it->second.count() == m_AlnCount) {
                    m_IsQueryAnchored = true;
                    return m_IsQueryAnchored;
                }
            }
            m_IsQueryAnchored = false;
        }
        return m_IsQueryAnchored;
    }

    /// Number of alignments
    size_t AlnCount() const {
        return m_AlnCount;
    }

    /// Number of seqs
    size_t SeqCount() const {
        return m_Bitmap.size();
    }

    /// Number of seqs aligned to itself
    size_t SelfAlignedSeqCount() const {
        return m_SelfAlignedBitmap.size();
    }

    /// Number of aligns containing at least one nuc seq
    size_t AlnWithNucCount() const {
        x_InitNucProtBitmaps();
        return m_NucleotideBitmap.count();
    }

    /// Number of aligns containing at least one prot seq
    size_t AlnWithProtCount() const {
        x_InitNucProtBitmaps();
        return m_ProteinBitmap.count();
    }

    /// Number of aligns containing nuc seqs only
    size_t NucOnlyAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_NucleotideBitmap & (~(m_NucleotideBitmap & m_ProteinBitmap))).count();
    }

    /// Number of aligns containing prot seqs only
    size_t ProtOnlyAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_ProteinBitmap & (~(m_ProteinBitmap & m_NucleotideBitmap))).count();
    }

    /// Number of aligns containing both nuc and prot seqs
    size_t TranslatedAlnCount() const {
        x_InitNucProtBitmaps();
        return (m_ProteinBitmap & m_NucleotideBitmap).count();
    }

    typedef bm::bvector<> TBitVector;
    typedef map<CBioseq_Handle, TBitVector> TSeqIdAlnMap;

    /// Get the internally-built structure
    const TSeqIdAlnMap& Get() const {
        return m_Bitmap;
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

    TSeqIdAlnMap m_Bitmap;
    CScope& m_Scope;
    size_t m_AlnCount;

    mutable bool m_IsQueryAnchoredSet;
    mutable bool m_IsQueryAnchored;

    mutable bool m_NucProtBitmapsInitialized;
    mutable TBitVector m_ProteinBitmap;
    mutable TBitVector m_NucleotideBitmap;

    TSeqIdAlnMap m_SelfAlignedBitmap;
};



END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.1  2006/10/17 19:23:57  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_TESTS__HPP
