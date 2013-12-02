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


/// Sparse alignment
class NCBI_XALNMGR_EXPORT CSparseAln : public CObject, public IAlnExplorer
{
public:
    typedef CPairwiseAln::TRng TRng;
    typedef CPairwiseAln::TAlnRng TAlnRng;
    typedef CPairwiseAln::TAlnRngColl TAlnRngColl;
    typedef CAnchoredAln::TDim TDim; ///< Synonym of TNumrow

    /// Constructor
    /// @param anchored_aln
    ///   Input CAnchoredAln object. Should be built using BuildAln function
    ///   for the alignment coordinates to be correct.
    /// @param scope
    ///   CScope used to fetch sequence data.
    /// @sa BuildAln
    CSparseAln(const CAnchoredAln& anchored_aln,
               objects::CScope& scope);

    /// Destructor
    virtual ~CSparseAln(void);

    /// Gap character modifier
    void SetGapChar(TResidue gap_char);

    /// Scope accessor
    CRef<objects::CScope> GetScope(void) const;

    /// Alignment dimension (number of sequence rows in the alignment)
    TDim GetDim(void) const;
    /// Synonym of the above
    TNumrow GetNumRows(void) const { return GetDim(); }

    /// Get seq-id for the row.
    const objects::CSeq_id& GetSeqId(TNumrow row) const;

    /// Get whole alignment range.
    TRng GetAlnRange(void) const;

    /// Get pairwise alignment for the row.
    const TAlnRngColl& GetAlignCollection(TNumrow row);

    /// Check if anchor is set - always true for sparse alignments.
    bool IsSetAnchor(void) const { return true; }

    /// Get anchor row index.
    TNumrow GetAnchor(void) const
    {
        return m_Aln->GetAnchorRow();
    }

    /// Get sequence range in alignment coords (strand ignored).
    TSignedRange GetSeqAlnRange(TNumrow row) const;
    TSignedSeqPos GetSeqAlnStart(TNumrow row) const;
    TSignedSeqPos GetSeqAlnStop(TNumrow row) const;

    /// Get sequence range in sequence coords.
    TRange GetSeqRange(TNumrow row) const;
    TSeqPos GetSeqStart(TNumrow row) const;
    TSeqPos GetSeqStop(TNumrow row) const;

    /// Check direction of the row.
    bool IsPositiveStrand(TNumrow row) const;
    bool IsNegativeStrand(TNumrow row) const;

    /// Map sequence position to alignment coordinates.
    /// @param row
    ///   Alignment row where the input position is defined.
    /// @param seq_pos
    ///   Input position
    /// @param dir
    ///   In case the input position can not be mapped to the alignment
    ///   coordinates (e.g. the position is inside an unaligned range),
    ///   try to search for the neares alignment position in the specified
    ///   direction.
    /// @param try_reverse_dir
    ///   Not implemented
    TSignedSeqPos GetAlnPosFromSeqPos(TNumrow row, TSeqPos seq_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;
    TSignedSeqPos GetSeqPosFromAlnPos(TNumrow for_row, TSeqPos aln_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;

    typedef CSeq_data::E_Choice TCoding;
    /// Get sequence coding for nucleotides.
    TCoding GetNaCoding(void) const { return m_NaCoding; }
    /// Get sequence coding for proteins.
    TCoding GetAaCoding(void) const { return m_AaCoding; }
    /// Set sequence coding for nucleotides. If not set, Iupacna coding is used.
    void SetNaCoding(TCoding coding) { m_NaCoding = coding; }
    /// Set sequence coding for proteins. If not set, Iupacaa coding is used.
    void SetAaCoding(TCoding coding) { m_AaCoding = coding; }

    /// Fetch sequence data for the given row and range.
    /// @param row
    ///   Alignment row to fetch sequence for.
    /// @param buffer
    ///   Output buffer.
    /// @param seq_from
    ///   Start sequence position.
    /// @param seq_to
    ///   End sequence position.
    /// @param force_translation
    ///   Force nucleotide to protein sequence translation.
    /// @return
    ///   Reference to the output buffer.
    string& GetSeqString(TNumrow row,
                         string& buffer,
                         TSeqPos seq_from,
                         TSeqPos seq_to,
                         bool    force_translation = false) const;

    /// Fetch sequence data for the given row and range.
    /// @param row
    ///   Alignment row to fetch sequence for.
    /// @param buffer
    ///   Output buffer.
    /// @param seq_rng
    ///   Sequence range.
    /// @param force_translation
    ///   Force nucleotide to protein sequence translation.
    /// @return
    ///   Reference to the output buffer.
    string& GetSeqString(TNumrow       row,
                         string&       buffer,
                         const TRange& rq_seq_rng,
                         bool          force_translation = false) const;

    /// Fetch alignment sequence data. Unaligned ranges of the selected row
    /// are filled with gap char.
    /// @param row
    ///   Alignment row to fetch sequence for.
    /// @param buffer
    ///   Output buffer.
    /// @param aln_rng
    ///   Alignment range.
    /// @param force_translation
    ///   Force nucleotide to protein sequence translation.
    /// @return
    ///   Reference to the output buffer.
    string& GetAlnSeqString(TNumrow             row,
                            string&             buffer,
                            const TSignedRange& rq_aln_rng,
                            bool                force_translation = false) const;

    /// Get bioseq handle for the row. Throw exception if the handle can not be
    /// obtained.
    const objects::CBioseq_Handle& GetBioseqHandle(TNumrow row) const;

    /// Create segment iterator.
    /// @param row
    ///   Row to iterate segments for.
    /// @param range
    ///   Range to iterate.
    /// @param flags
    ///   Iterator flags.
    /// @sa CSparse_CI
    /// @sa IAlnSegmentIterator
    virtual IAlnSegmentIterator*
        CreateSegmentIterator(TNumrow                     row,
                              const TSignedRange&         range,
                              IAlnSegmentIterator::EFlags flags) const;

    /// Wheather the alignment is translated (heterogenous), e.g. nuc-prot.
    bool IsTranslated(void) const;

    enum EConstants {
        kDefaultGenCode = 1
    };

    // Static utilities:
    static void TranslateNAToAA(const string& na, string& aa,
                                int gen_code = kDefaultGenCode); //< per http://www.ncbi.nlm.nih.gov/collab/FT/#7.5.5

    /// Get base width for the sequence (1 for nucleotides, 3 for proteins).
    int GetBaseWidth(TNumrow row) const
    {
        _ASSERT(row >= 0 && row < GetDim());
        int w = m_Aln->GetPairwiseAlns()[row]->GetSecondBaseWidth();
        _ASSERT(w == 1  ||  w == 3);
        return w;
    }

    /// Convert alignment (genomic) coordinate on the selected row to real
    /// sequence position.
    TSignedSeqPos AlnPosToNativeSeqPos(TNumrow row, TSignedSeqPos aln_pos) const
    {
        return aln_pos/GetBaseWidth(row);
    }

    /// Convert sequence position to alignment (genomic) coordinate.
    TSignedSeqPos NativeSeqPosToAlnPos(TNumrow row, TSignedSeqPos seq_pos) const
    {
        return seq_pos*GetBaseWidth(row);
    }

    /// Convert alignment range (genomic coordinates) on the selected row
    /// to real sequence range.
    /// NOTE: Need to use template since there are many range types:
    /// TRng, TAlnRng, TRange, TSignedRange etc.
    template<class _TRange>
    _TRange AlnRangeToNativeSeqRange(TNumrow row, _TRange aln_range) const
    {
        if (aln_range.Empty()  ||  aln_range.IsWhole()) return aln_range;
        int w = GetBaseWidth(row);
        return _TRange(aln_range.GetFrom()/w, aln_range.GetToOpen()/w - 1);
    }

    /// Get start and stop frames for the selected row/range.
    /// 0 - no frame (native coordinates are genomic)
    /// 1..3 - frame value for protein coordinates
    typedef pair<int, int> TFrames;
    template<class _TRange>
    TFrames AlnRangeToNativeFrames(TNumrow row, _TRange aln_range) const
    {
        if (aln_range.Empty() || aln_range.IsWhole()) return TFrames(0, 0);
        int w = GetBaseWidth(row);
        if (w == 1) return TFrames(0, 0);
        return TFrames(aln_range.GetFrom() % w + 1, aln_range.GetTo() % w + 1);
    }

    /// Convert sequence range to alignment range (genomic coordinates).
    /// NOTE: Need to use template since there are many range types:
    /// TRng, TAlnRng, TRange, TSignedRange etc.
    template<class _TRange>
    _TRange NativeSeqRangeToAlnRange(TNumrow row, _TRange seq_range) const
    {
        if (seq_range.Empty()  ||  seq_range.IsWhole()) return seq_range;
        int w = GetBaseWidth(row);
        return _TRange(seq_range.GetFrom()*w, seq_range.GetToOpen()*w - 1);
    }

protected:
    friend class CSparse_CI;

    void x_Build(const CAnchoredAln& src_align);
    CSeqVector& x_GetSeqVector(TNumrow row) const;

    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;

    CRef<CAnchoredAln> m_Aln;
    mutable CRef<objects::CScope> m_Scope;
    TRng m_FirstRange; // the extent of all segments in aln coords
    vector<TRng> m_SecondRanges;
    TResidue m_GapChar;
    mutable vector<objects::CBioseq_Handle> m_BioseqHandles;
    mutable vector<CRef<CSeqVector> > m_SeqVectors;

    TCoding m_NaCoding;
    TCoding m_AaCoding;

    bool m_AnchorDirect;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
