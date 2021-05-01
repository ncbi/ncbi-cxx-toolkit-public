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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_ALIGN__HPP
#define VALIDATOR___VALIDERROR_ALIGN__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_diag;
class CDense_seg;
class CSeq_align_set;

BEGIN_SCOPE(validator)

// ============================  Validate SeqAlign  ============================


class CValidError_align : private CValidError_base
{
public:
    CValidError_align(CValidError_imp& imp);
    virtual ~CValidError_align(void);

    void ValidateSeqAlign(const CSeq_align& align, int order = -1);

    struct TSegmentGap {
        size_t seg_num; size_t align_pos; string label;
        TSegmentGap(size_t _seg_num, size_t _align_pos, string _label)
            : seg_num(_seg_num), align_pos(_align_pos), label(_label) {}
    };
    typedef vector<TSegmentGap> TSegmentGapV;

    typedef CSeq_align::C_Segs::TDendiag    TDendiag;
    typedef CSeq_align::C_Segs::TDenseg     TDenseg;
    typedef CSeq_align::C_Segs::TPacked     TPacked;
    typedef CSeq_align::C_Segs::TStd        TStd;

    static TSegmentGapV FindSegmentGaps(const TPacked& packed, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TDenseg& std_segs, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TStd& denseg, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TDendiag& dendiags, CScope* scope);

private:
    typedef CSeq_align::C_Segs::TDisc       TDisc;

    void x_ValidateAlignPercentIdentity(const CSeq_align& align, bool internal_gaps);
    static bool AlignmentScorePercentIdOk(const CSeq_align& align);
    static bool IsTpaAlignment(const CDense_seg& denseg, CScope& scope);
    static bool IsTpaAlignment(const CSparseAln& sparse_aln, CScope& scope);
    void x_ValidateDendiag(const TDendiag& dendiags, const CSeq_align& align);
    void x_ValidateDenseg(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStd(const TStd& stdsegs, const CSeq_align& align);
    void x_ValidatePacked(const TPacked& packed, const CSeq_align& align);

    // Check if dimension is valid
    template <typename T>
    bool x_ValidateDim(T& obj, const CSeq_align& align, size_t part = 0);

    // Check if the  strand is consistent in SeqAlignment of global
    // or partial type
    void x_ValidateStrand(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStrand(const TStd& std_segs, const CSeq_align& align);

    // Check if an alignment is FASTA-like.
    // Alignment is FASTA-like if all gaps are at the end with dimensions > 2.
    void x_ValidateFastaLike(const TDenseg& denseg, const CSeq_align& align);

    // Check if there is a gap for all sequences in a segment.
    void x_ReportSegmentGaps(const TSegmentGapV& seggaps, const CSeq_align& align);
    void x_ValidateSegmentGap(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSegmentGap(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSegmentGap(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSegmentGap(const TDendiag& dendiags, const CSeq_align& align);

    // Validate SeqId in sequence alignment.
    void x_ValidateSeqId(const CSeq_align& align);
    void x_GetIds(const CSeq_align& align, vector< CRef< CSeq_id > >& ids);

    // Check segment length, start and end point in Dense_seg, Dense_diag
    // and Std_seg
    void x_ReportAlignErr(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context,
        size_t segment, size_t pos,
        EErrType et, EDiagSev sev, const string& prefix, const string& message);
    void x_ReportSumLenStart(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context, size_t segment, size_t pos);
    void x_ReportStartMoreThanBiolen(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context, size_t segment, size_t pos);
    void x_ValidateSeqLength(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSeqLength(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSeqLength(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSeqLength(const CDense_diag& dendiag, size_t dendiag_num,
        const CSeq_align& align);
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_ALIGN__HPP */
