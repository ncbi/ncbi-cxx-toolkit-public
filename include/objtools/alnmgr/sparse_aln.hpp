#ifndef OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
#define OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>

#include <objmgr/scope.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_explorer.hpp>


BEGIN_NCBI_SCOPE


class NCBI_XALNMGR_EXPORT CSparseAln : public CObject, public IAlnExplorer
{
public:
    /// Types
    typedef CPairwiseAln::TRng TRng;
    typedef CPairwiseAln::TAlnRng TAlnRng;
    typedef CPairwiseAln::TAlnRngColl TAlnRngColl;
    typedef CAnchoredAln::TDim TDim; ///< Synonym of TNumrow

    /// Constructor
    CSparseAln(const CAnchoredAln& anchored_aln,
               objects::CScope& scope);

    /// Destructor
    virtual ~CSparseAln();


    /// Gap character modifier
    void SetGapChar(TResidue gap_char);


    /// Scope accessor
    CRef<objects::CScope> GetScope() const;


    /// Alignment dimension (number of sequence rows in the alignment)
    TDim GetDim() const;
    /// Synonym of the above
    TNumrow GetNumRows() const {
        return GetDim();
    }


    /// Seq ids
    const objects::CSeq_id&  GetSeqId(TNumrow row) const;


    /// Alignment Range
    TRng GetAlnRange() const;

    const TAlnRngColl&  GetAlignCollection(TNumrow row);

    /// Anchor
    bool    IsSetAnchor() const {
        return true; /// Always true for sparse alignments
    }
    TNumrow GetAnchor() const {
        return m_Aln->GetAnchorRow();
    }

    /// Sequence range in alignment coords (strand ignored)
    TSignedSeqPos GetSeqAlnStart(TNumrow row) const;
    TSignedSeqPos GetSeqAlnStop(TNumrow row) const;
    TSignedRange  GetSeqAlnRange (TNumrow row) const;

    /// Sequence range in sequence coords
    TSeqPos GetSeqStart(TNumrow row) const;
    TSeqPos GetSeqStop(TNumrow row) const;
    TRange  GetSeqRange(TNumrow row) const;

    /// Strands
    bool IsPositiveStrand(TNumrow row) const;
    bool IsNegativeStrand(TNumrow row) const;

    /// Position mapping functions
    TSignedSeqPos GetAlnPosFromSeqPos(TNumrow row, TSeqPos seq_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;
    TSignedSeqPos GetSeqPosFromAlnPos(TNumrow for_row, TSeqPos aln_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;


    /// Fetch sequence
    string& GetSeqString   (TNumrow row,                           //< which row
                            string &buffer,                        //< provide an empty buffer for the output
                            TSeqPos seq_from, TSeqPos seq_to,      //< what range
                            bool force_translation = false) const; //< optional na2aa translation (na only!)

    string& GetSeqString   (TNumrow row,                           //< which row
                            string &buffer,                        //< provide an empty buffer for the output
                            const TRange &seq_rng,                 //< what range
                            bool force_translation = false) const; //< optional na2aa translation (na only!)

    string& GetAlnSeqString(TNumrow row,                           //< which row
                            string &buffer,                        //< an empty buffer for the output
                            const TSignedRange &aln_rng,           //< what range (in aln coords)
                            bool force_translation = false) const; //< optional na2aa translation (na only!)


    /// Bioseq handle accessor
    const objects::CBioseq_Handle&  GetBioseqHandle(TNumrow row) const;

    virtual IAlnSegmentIterator*
    CreateSegmentIterator(TNumrow row,
                          const TSignedRange& range,
                          IAlnSegmentIterator::EFlags flags) const;

    /// Wheather the alignment is translated (heterogenous), e.g. nuc-prot
    bool IsTranslated() const;


    // genetic code
    enum EConstants {
        kDefaultGenCode = 1
    };

    // Static utilities:
    static void TranslateNAToAA(const string& na, string& aa,
                                int gen_code = kDefaultGenCode); //< per http://www.ncbi.nlm.nih.gov/collab/FT/#7.5.5
protected:
    void x_Build(const CAnchoredAln& src_align);

    CSeqVector& x_GetSeqVector(TNumrow row) const;


    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;


    CRef<CAnchoredAln> m_Aln;
    mutable CRef<objects::CScope> m_Scope;
    TRng m_FirstRange; ///< the extent of all segments in aln coords
    vector<TRng> m_SecondRanges;
    TResidue m_GapChar;
    mutable vector<objects::CBioseq_Handle> m_BioseqHandles;
    mutable vector<CRef<CSeqVector> > m_SeqVectors;

};






END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
