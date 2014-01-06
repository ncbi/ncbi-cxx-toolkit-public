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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_align
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/sparse_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_builders.hpp>

#include <map>
#include <vector>
#include <algorithm>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

USING_SCOPE(sequence);


// ================================  Public  ================================


CValidError_align::CValidError_align(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_align::~CValidError_align(void)
{
}


void CValidError_align::ValidateSeqAlign(const CSeq_align& align)
{
    if (!align.IsSetSegs()) {
        PostErr (eDiag_Error, eErr_SEQ_ALIGN_NullSegs, 
                 "Segs: This alignment is missing all segments.  This is a non-correctable error -- look for serious formatting problems.",
                 align);
        return;
    }
                 
    const CSeq_align::TSegs& segs = align.GetSegs();
    CSeq_align::C_Segs::E_Choice segtype = segs.Which();
    switch ( segtype ) {
    
    case CSeq_align::C_Segs::e_Dendiag:
        x_ValidateDendiag(segs.GetDendiag(), align);
        break;

    case CSeq_align::C_Segs::e_Denseg:
        x_ValidateDenseg(segs.GetDenseg(), align);
        break;

    case CSeq_align::C_Segs::e_Std:
        x_ValidateStd(segs.GetStd(), align);
        break;
    case CSeq_align::C_Segs::e_Packed:
        x_ValidatePacked(segs.GetPacked(), align);
        break;

    case CSeq_align::C_Segs::e_Disc:
        // call recursively
        ITERATE(CSeq_align_set::Tdata, sali, segs.GetDisc().Get()) {
            ValidateSeqAlign(**sali);
        }
        return;

    case CSeq_align::C_Segs::e_not_set:
        PostErr (eDiag_Error, eErr_SEQ_ALIGN_NullSegs, 
                 "Segs: This alignment is missing all segments.  This is a non-correctable error -- look for serious formatting problems.",
                 align);
        return;
        break;
    case CSeq_align::C_Segs::e_Sparse:
    case CSeq_align::C_Segs::e_Spliced:
        PostErr(eDiag_Warning, eErr_SEQ_ALIGN_Segtype,
                "Segs: This alignment has an undefined or unsupported Seqalign segtype "
                + NStr::IntToString(segtype), align);
        return;
        break;
    default:
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_Segtype,
                "Segs: This alignment has an undefined or unsupported Seqalign segtype "
                + NStr::IntToString(segtype), align);
        return;
        break;
    }  // end of switch statement

    if (segtype != CSeq_align::C_Segs::e_Denseg 
        && align.IsSetType() 
        && (align.GetType() == CSeq_align::eType_partial
            || align.GetType() == CSeq_align::eType_global)) {
		    PostErr(eDiag_Error, eErr_SEQ_ALIGN_UnexpectedAlignmentType, "UnexpectedAlignmentType: This is not a DenseSeg alignment.", align);
    }
	  try {
        x_ValidateAlignPercentIdentity (align, false);
	  } catch (CException &) {
	  } catch (std::exception &) {
	  }

}


// ================================  Private  ===============================

typedef struct ambchar {
  char ambig_char;
  const char * match_list;
} AmbCharData;

static const AmbCharData ambiguity_list[] = {
 { 'R', "AG" },
 { 'Y', "CT" },
 { 'M', "AC" },
 { 'K', "GT" },
 { 'S', "CG" },
 { 'W', "AT" },
 { 'H', "ACT" },
 { 'B', "CGT" },
 { 'V', "ACG" },
 { 'D', "AGT" }};

static const int num_ambiguities = sizeof (ambiguity_list) / sizeof (AmbCharData);

static bool s_AmbiguousMatch (char a, char b)
{
    if (a == b) {
        return true;
    } else if (a == 'N' || b == 'N') {
        return true;
    } else {
        char search[2];
        search[1] = 0;
        for (int i = 0; i < num_ambiguities; i++) {
            search[0] = b;
            if (a == ambiguity_list[i].ambig_char
                && NStr::Find (ambiguity_list[i].match_list, search) != string::npos) {
                return true;
            }
            search[0] = a;
            if (b == ambiguity_list[i].ambig_char
                && NStr::Find (ambiguity_list[i].match_list, search) != string::npos) {
                return true;
            }
        }
    }
    return false;
}


static int s_GetNumIdsToUse (const CDense_seg& denseg)
{
    size_t dim     = denseg.GetDim();
    if (!denseg.IsSetIds()) {
        dim = 0;
    } else if (denseg.GetIds().size() < dim) {
        dim = denseg.GetIds().size();
    }
    return dim;
}


void CValidError_align::x_ValidateAlignPercentIdentity (const CSeq_align& align, bool internal_gaps)
{
    TSeqPos col = 0;
    size_t num_match = 0;
    size_t match_25 = 0;
    bool   ids_missing = false;

    // Now calculate Percent Identity
    if (!align.IsSetSegs()) {
        return;
    } else if (align.GetSegs().IsDendiag()) {
        return;
    } else if (align.GetSegs().IsDenseg()) {
        const CDense_seg& denseg = align.GetSegs().GetDenseg();
        // first, make sure this isn't a TPA alignment
        bool is_tpa = false;
        int dim     = denseg.GetDim();
        if (dim != s_GetNumIdsToUse(denseg)) {
            return;
        }

        for (CDense_seg::TDim row = 0;  row < dim && !is_tpa;  ++row) {
            CRef<CSeq_id> id = denseg.GetIds()[row];
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle (*id);
            if (bsh) {
                CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
                while (desc_ci && !is_tpa) {
                    if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr()
                        && NStr::EqualNocase (desc_ci->GetUser().GetType().GetStr(), "TpaAssembly")) {
                        is_tpa = true;
                    }
                    ++desc_ci;
                }
            }
        }
        if (is_tpa) {
            return;
        }            

        try {
            CRef<CAlnVec> av(new CAlnVec(denseg, *m_Scope));
            av->SetGapChar('-');
            av->SetEndChar('.');

            TSeqPos aln_len = av->GetAlnStop() + 1;

            try {
                while (col < aln_len && !ids_missing) {
                    string column;
                    av->GetColumnVector(column, col);
                    bool match = true;
                    if (internal_gaps && NStr::Find(column, "-") != string::npos) {
                        match = false;
                    } else {
                        // don't care about end gaps, ever
                        NStr::ReplaceInPlace(column, ".", "");            
                        // if we cared about internal gaps, it would have been handled above
                        NStr::ReplaceInPlace(column, "-", "");
                        if (!NStr::IsBlank (column)) {
                            string::iterator it1 = column.begin();
                            string::iterator it2 = it1;
                            ++it2;
                            while (match && it2 != column.end()) {
                                if (!s_AmbiguousMatch (*it1, *it2)) {
                                    match = false;
                                }
                                ++it2;
                                if (it2 == column.end()) {
                                    ++it1;
                                    it2 = it1;
                                    ++it2;
                                }
                            }
                        }
                        if (match) {
                            ++num_match;
                            ++match_25;
                        }
                    }
                    col++;
                    if (col % 25 == 0) {
                        match_25 = 0;
                    }
                }
	          } catch (CException &x1) {
                  // if sequence is not in scope,
                  // the above is impossible
                  // report 0 %, same as C Toolkit
                  col = aln_len;
                  if (NStr::StartsWith(x1.GetMsg(), "iterator out of range")) {
                      // bad offsets
                  } else {
                      ids_missing = true;
                  }
	          } catch (std::exception &) {
                  // if sequence is not in scope,
                  // the above is impossible
                  // report 0 %, same as C Toolkit
                  col = aln_len;
                  ids_missing = true;
            }
        } catch (CException &) {
            // if AlnVec can't resolve seq id, 
            // the above is impossible
            // report 0 %, same as C Toolkit
            col = 1;
            num_match = 0;
            ids_missing = true;
        }

    } else {
        try {
            TIdExtract id_extract;
            TAlnIdMap aln_id_map(id_extract, 1);
            aln_id_map.push_back (align);
            TAlnStats aln_stats (aln_id_map);

            // Create user options
            CAlnUserOptions aln_user_options;
            TAnchoredAlnVec anchored_alignments;

            CreateAnchoredAlnVec (aln_stats, anchored_alignments, aln_user_options);

            /// Build a single anchored aln
            CAnchoredAln out_anchored_aln;

            /// Optionally, create an id for the alignment pseudo sequence
            /// (otherwise one would be created automatically)
            CRef<CSeq_id> seq_id (new CSeq_id("lcl|PSEUDO ALNSEQ"));
            CRef<CAlnSeqId> aln_seq_id(new CAlnSeqId(*seq_id));
            TAlnSeqIdIRef pseudo_seqid(aln_seq_id);

            BuildAln(anchored_alignments,
                     out_anchored_aln,
                     aln_user_options,
                     pseudo_seqid);

            CSparseAln sparse_aln(out_anchored_aln, *m_Scope);

            // check to see if alignment is TPA
            bool is_tpa = false;
            for (CSparseAln::TDim row = 0;  row < sparse_aln.GetDim() && !is_tpa;  ++row) {
                const CSeq_id& id = sparse_aln.GetSeqId(row);
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle (id);
                if (bsh) {
                    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
                    while (desc_ci && !is_tpa) {
                        if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr()
                            && NStr::EqualNocase (desc_ci->GetUser().GetType().GetStr(), "TpaAssembly")) {
                            is_tpa = true;
                        }
                        ++desc_ci;
                    }
                }
            }
            if (is_tpa) {
                return;
            }            

            vector <string> aln_rows;
            vector <TSeqPos> row_starts;
            vector <TSeqPos> row_stops;

            for (CSparseAln::TDim row = 0;  row < sparse_aln.GetDim() && !ids_missing;  ++row) {
                try {
                    string sequence;
                    sparse_aln.GetAlnSeqString
                        (row,
                         sequence,
                         sparse_aln.GetAlnRange());
                    aln_rows.push_back (sequence);
                    TSignedSeqPos aln_start = sparse_aln.GetSeqAlnStart(row);
                    TSignedSeqPos start = sparse_aln.GetSeqPosFromAlnPos(row, aln_start);
                    row_starts.push_back (start);
                    row_stops.push_back (sparse_aln.GetAlnPosFromSeqPos(row, sparse_aln.GetSeqAlnStop(row)));
		            } catch (CException &) {
                    ids_missing = true;
		            } catch (std::exception &) {
                    // if sequence is not in scope,
                    // the above is impossible
                    ids_missing = true;
                }
            }

            if (!ids_missing) {
                TSeqPos aln_len = sparse_aln.GetAlnRange().GetLength();
                while (col < aln_len) {
                    string column;
                    bool match = true;
                    for (size_t row = 0; row < aln_rows.size() && match; row++) {
                        if (row_starts[row] >= col && row_stops[row] <= col
                            && aln_rows[row].length() > col) {
                            string nt = aln_rows[row].substr(col - row_starts[row], 1);
                            if (NStr::Equal (nt, "-")) {
                                if (internal_gaps) {
                                    match = false;
                                }
                            } else {                    
                                column += nt;
                            }
                        }
                    }
                    if (match) {
                        if (!NStr::IsBlank (column)) {
                            string::iterator it1 = column.begin();
                            string::iterator it2 = it1;
                            ++it2;
                            while (match && it2 != column.end()) {
                                if (!s_AmbiguousMatch (*it1, *it2)) {
                                    match = false;
                                }
                                ++it2;
                                if (it2 == column.end()) {
                                    ++it1;
                                    it2 = it1;
                                    ++it2;
                                }
                            }
                        }
                        if (match) {
                            ++num_match;
                        }
                    }
                    col++;
                }
            }
        } catch (CException &) {
            ids_missing = true;
        } catch (std::exception &) {
            ids_missing = true;
        }
    }

    if (ids_missing) {
        // if no columns, set col to one, so that we'll get a zero percent id error
        col = 1;
        num_match = 0;
    }

    if (col > 0) {
        int pct_id = (num_match * 100) / col;
        if (pct_id < 50) {
            PostErr (eDiag_Warning, eErr_SEQ_ALIGN_PercentIdentity,
                "PercentIdentity: This alignment has a percent identity of " + NStr::IntToString (pct_id) + "%",
                     align);
        }
    }
}


void CValidError_align::x_ValidateDenseg
(const TDenseg& denseg,
 const CSeq_align& align)
{
    // assert dim >= 2
    if ( !x_ValidateDim(denseg, align) ) {
        return;
    }

    size_t dim     = denseg.GetDim();
    size_t numseg  = denseg.GetNumseg();
    string label;
    denseg.GetIds()[0]->GetLabel (&label);

    // assert dim == Ids.size()
    if ( dim != denseg.GetIds().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_AlignDimSeqIdNotMatch,
                "SeqId: The Seqalign has more or fewer ids than the number of rows in the alignment (context "
                    + label + ").  Look for possible formatting errors in the ids.", align);
    }

    // assert numseg == Lens.size()
    if ( numseg != denseg.GetLens().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsNumsegMismatch,
                "Mismatch between specified numseg (" +
                NStr::SizetToString(numseg) + 
                ") and number of Lens (" + 
                NStr::SizetToString(denseg.GetLens().size()) + ")",
                align);
    }

    // assert dim * numseg == Starts.size()
    if ( dim * numseg != denseg.GetStarts().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsStartsMismatch,
                "The number of Starts (" + 
                NStr::SizetToString(denseg.GetStarts().size()) +
                ") does not match the expected size of dim * numseg (" +
                NStr::SizetToString(dim * numseg) + ")", align);
    } 

    x_ValidateStrand(denseg, align);
    x_ValidateFastaLike(denseg, align);
    x_ValidateSegmentGap(denseg, align);

#if 0
	// commented out in C Toolkit
	// look for short alignment
	int align_len = 0;
	for (size_t i = 0; i < numseg; i++) {
		align_len += denseg.GetLens()[i];
	}
	bool is_short = false;
	for (size_t i = 0; i < dim && !is_short; i++) {
		CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*(denseg.GetIds()[i]), CScope::eGetBioseq_Loaded);
		if (bsh && bsh.IsSetInst() && bsh.IsSetInst_Length() && align_len < bsh.GetInst_Length()) {
			is_short = true;
		}
	}
	if (is_short) {
		PostErr (eDiag_Info, eErr_SEQ_ALIGN_ShortAln, "This alignment is shorter than at least one non-farpointer sequence.", align);
	}
#endif
    
	// operations that require remote fetching
    if ( m_Imp.IsRemoteFetch() ) {
        x_ValidateSeqId(align);
        x_ValidateSeqLength(denseg, align);
    }
}




void CValidError_align::x_ValidatePacked
(const TPacked& packed,
 const CSeq_align& align)
{

    // assert dim >= 2
    if ( !x_ValidateDim(packed, align) ) {
        return;
    }

    size_t dim     = packed.GetDim();
    size_t numseg  = packed.GetNumseg();
    
    // assert dim == Ids.size()
    if ( dim != packed.GetIds().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_AlignDimSeqIdNotMatch,
                "SeqId: The Seqalign has more or fewer ids than the number of rows in the alignment.  Look for possible formatting errors in the ids.", align);
    }
    
    // assert numseg == Lens.size()
    if ( numseg != packed.GetLens().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
            "Mismatch between specified numseg (" +
            NStr::SizetToString(numseg) + 
            ") and number of Lens (" + 
            NStr::SizetToString(packed.GetLens().size()) + ")",
            align);
    }
    
    // assert dim * numseg == Present.size()
    if ( dim * numseg != packed.GetPresent().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentMismatch,
                "The number of Present (" + 
                NStr::SizetToString(packed.GetPresent().size()) +
                ") does not match the expected size of dim * numseg (" +
                NStr::SizetToString(dim * numseg) + ")", align);
    } else {
        // assert # of '1' bits in present == Starts.size()
        size_t bits = x_CountBits(packed.GetPresent());
        if ( bits != packed.GetStarts().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentStartsMismatch,
                "Starts does not have the same number of elements (" +
                NStr::SizetToString(packed.GetStarts().size()) + 
                ") as specified by Present (" + NStr::SizetToString(bits) +
                ")", align);
        }

        // assert # of '1' bits in present == Strands.size() (if exists)
        if ( packed.IsSetStrands() ) {
            if ( bits != packed.GetStarts().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentStrandsMismatch,
                    "Strands does not have the same number of elements (" +
                    NStr::SizetToString(packed.GetStrands().size()) +
                    ") as specified by Present (" +
                    NStr::SizetToString(bits) + ")", align);
            }
        }
    }
    
    x_ValidateStrand(packed, align);
    x_ValidateSegmentGap(packed, align);
    
    if ( m_Imp.IsRemoteFetch() ) {
        x_ValidateSeqId(align);
        x_ValidateSeqLength(packed, align);
    }
}


size_t CValidError_align::x_CountBits(const CPacked_seg::TPresent& present)
{
    size_t bits = 0;
    ITERATE( CPacked_seg::TPresent, iter, present ) {
        if ( *iter != 0 ) {
            Uchar mask = 0x01;
            for ( size_t i = 0; i < 7; ++i ) {
                bits += (*iter & mask) ? 1 : 0;
                mask = mask << 1;
            }
        }
    }
    return bits;
}


void CValidError_align::x_ValidateDendiag
(const TDendiag& dendiags,
 const CSeq_align& align)
{
    size_t num_dendiag = 0;
    ITERATE( TDendiag, dendiag_iter, dendiags ) {
        ++num_dendiag;

        const CDense_diag& dendiag = **dendiag_iter;
        size_t dim = dendiag.GetDim();

        // assert dim >= 2
        if ( !x_ValidateDim(dendiag, align, num_dendiag) ) {
            continue;
        }

        string label;
        dendiag.GetIds()[0]->GetLabel (&label);

        // assert dim == Ids.size()
        if ( dim != dendiag.GetIds().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimSeqIdNotMatch,
                    "SeqId: In segment " + NStr::SizetToString (num_dendiag) 
                    + ", there are more or fewer rows than there are seqids (context "
                    + label + ").  Look for possible formatting errors in the ids.", align);
        }

        // assert dim == Starts.size()
        if ( dim != dendiag.GetStarts().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                "Mismatch between specified dimension (" +
                NStr::SizetToString(dim) + 
                ") and number ofStarts (" + 
                NStr::SizetToString(dendiag.GetStarts().size()) + 
                ") in dendiag " + NStr::SizetToString(num_dendiag), align);
        } 

        // assert dim == Strands.size() (if exist)
        if ( dendiag.IsSetStrands() ) {
            if ( dim != dendiag.GetStrands().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                    "Mismatch between specified dimension (" +
                    NStr::SizetToString(dim) + 
                    ") and number of Strands (" + 
                    NStr::SizetToString(dendiag.GetStrands().size()) + 
                    ") in dendiag " + NStr::SizetToString(num_dendiag), align);
            } 
        }

        if ( m_Imp.IsRemoteFetch() ) {
            x_ValidateSeqLength(dendiag, num_dendiag, align);
        }
    }
    if ( m_Imp.IsRemoteFetch() ) {
        x_ValidateSeqId(align);
    }
    x_ValidateSegmentGap (dendiags, align);
}


void CValidError_align::x_ValidateStd
(const TStd& std_segs,
 const CSeq_align& align)
{
    size_t num_stdseg = 0;
    ITERATE( TStd, stdseg_iter, std_segs) {
        ++num_stdseg;

        const CStd_seg& stdseg = **stdseg_iter;
        size_t dim = stdseg.GetDim();

        // assert dim >= 2
        if ( !x_ValidateDim(stdseg, align, num_stdseg) ) {
        }

        // assert dim == Loc.size()
        if ( dim != stdseg.GetLoc().size() ) {
            string label;
            stdseg.GetLoc()[0]->GetLabel(&label);
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimSeqIdNotMatch,
                    "SeqId: In segment " + NStr::SizetToString (num_stdseg) 
                    + ", there are more or fewer rows than there are seqids (context "
                    + label + ").  Look for possible formatting errors in the ids.", align);
        }

        // assert dim == Ids.size()
        if ( stdseg.IsSetIds() ) {
            if ( dim != stdseg.GetIds().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                    "Mismatch between specified dimension (" +
                    NStr::SizetToString(dim) + 
                    ") and number of Seq-ids (" + 
                    NStr::SizetToString(stdseg.GetIds().size()) + ")",
                    align);
            }
        }
    }

    x_ValidateStrand(std_segs, align);
    x_ValidateSegmentGap(std_segs, align);
    
    if ( m_Imp.IsRemoteFetch() ) {
        x_ValidateSeqId(align);
        x_ValidateSeqLength(std_segs, align);
    }
}


template <typename T>
bool CValidError_align::x_ValidateDim
(T& obj,
 const CSeq_align& align,
 size_t part)
{
    bool rval = false;

    if ( !obj.IsSetDim() || obj.GetDim() == 0) {
        if (part > 0) {
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_SegsDimOne,
                     "Segs: Segment " + NStr::SizetToString (part) + "has dimension zero", align);
        } else {
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_AlignDimOne,
                     "Dim: This alignment has dimension zero", align);
        }
    } else if (obj.GetDim() == 1) {
        string msg = "";
        EErrType et;
        if (part > 0) {
            et = eErr_SEQ_ALIGN_SegsDimOne;
            msg = "Segs: Segment " + NStr::SizetToString (part) + "apparently has only one sequence.  Each portion of the alignment must have at least two sequences.";
        } else {
            et = eErr_SEQ_ALIGN_AlignDimOne;
            msg = "Dim: This seqalign apparently has only one sequence.  Each alignment must have at least two sequences.";
        }
        CConstRef<CSeq_id> id = GetReportableSeqIdForAlignment(align, *m_Scope);
        if (id) {
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
            if (bsh) {
                int version = 0;
                const string& label = GetAccessionFromObjects(bsh.GetCompleteBioseq(), NULL, *m_Scope, &version);
                msg += "  context " + label;
            }
        }
        PostErr (eDiag_Error, et, msg, align);
    } else {
        rval = true;
    }

    return rval;
}


//===========================================================================
// x_ValidateStrand:
//
//  Check if the  strand is consistent in SeqAlignment of global
//  or partial type.
//===========================================================================

void CValidError_align::x_ValidateStrand
(const TDenseg& denseg,
 const CSeq_align& align)
{
    if ( !denseg.IsSetStrands() ) {
        return;
    }
    
    size_t dim = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();
    const CDense_seg::TStrands& strands = denseg.GetStrands();

    // go through id for each alignment sequence
    for ( size_t id = 0; id < dim; ++id ) {
        ENa_strand strand1 = strands[id];
        
        for ( size_t seg = 0; seg < numseg; ++seg ) {
            ENa_strand strand2 = strands[id + (seg * dim)];

            // skip undefined strand
            if ( strand2 == eNa_strand_unknown  ||
                 strand2 == eNa_strand_other ) {
                continue;
            }

            if ( strand1 == eNa_strand_unknown  ||
                 strand1 == eNa_strand_other ) {
                strand1 = strand2;
                continue;
            }

            // strands should be same for a given seq-id
            if ( strand1 != strand2 ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StrandRev,
                    "Strand: The strand labels for SeqId " + 
                    denseg.GetIds()[id]->AsFastaString() +
                    " are inconsistent across the alignment. "
                    "The first inconsistent region is the " + 
                    NStr::SizetToString(seg + 1) + "(th) region, near sequence position "
                    + NStr::SizetToString(denseg.GetStarts()[id + (seg * dim)]), align);
                    break;
            }
        }
    }
}


void CValidError_align::x_ValidateStrand
(const TPacked& packed,
 const CSeq_align& align)
{
    if ( !packed.IsSetStrands() ) {
        return;
    }

    size_t dim = packed.GetDim();
    const CPacked_seg::TPresent& present = packed.GetPresent();
    const CPacked_seg::TStrands& strands = packed.GetStrands();
    CPacked_seg::TStrands::const_iterator strand = strands.begin();
    
    vector<ENa_strand> id_strands(dim, eNa_strand_unknown);

    Uchar mask = 0x01;
    size_t present_size = present.size();
    for ( size_t i = 0; i < present_size; ++i ) {
        if ( present[i] == 0 ) {   // no bits in this char
            continue;
        }
        for ( int shift = 7; shift >= 0; --shift ) {
            if ( present[i] & (mask << shift) ) {
                size_t id = i * 8 + 7 - shift;
                if ( *strand == eNa_strand_unknown  ||  
                     *strand == eNa_strand_other ) {
                    ++strand;
                    continue;
                }
                
                if ( id_strands[id] == eNa_strand_unknown  || 
                     id_strands[id] == eNa_strand_other ) {
                    id_strands[id] = *strand;
                    ++strand;
                    continue;
                }
                
                if ( id_strands[id] != *strand ) {
                    CPacked_seg::TIds::const_iterator id_iter =
                        packed.GetIds().begin();
                    for ( ; id; --id ) {
                        ++id_iter;
                    }
                    
                    PostErr(eDiag_Error, eErr_SEQ_ALIGN_StrandRev,
                        "Strand: The strand labels for SeqId " + (*id_iter)->AsFastaString() +
                        " are inconsistent across the alignment", align);
                }
                ++strand;
            }
        }
    }
}


void CValidError_align::x_ValidateStrand
(const TStd& std_segs,
 const CSeq_align& align)
{
    map< string, ENa_strand > strands;
    map< string, bool> reported;
    int region = 1;

    ITERATE ( TStd, stdseg, std_segs ) {
        ITERATE ( CStd_seg::TLoc, loc_iter, (*stdseg)->GetLoc() ) {
            const CSeq_loc& loc = **loc_iter;
            
            if ( !IsOneBioseq(loc, m_Scope) ) {
                // !!! should probably be an error
                continue;
            }
            CConstRef<CSeq_id> id(&GetId(loc, m_Scope));
            string id_label = id->AsFastaString();

            ENa_strand strand = GetStrand(loc, m_Scope);

            if ( strand == eNa_strand_unknown  || 
                 strand == eNa_strand_other ) {
                continue;
            }

            if ( strands[id_label] == eNa_strand_unknown  ||
                 strands[id_label] == eNa_strand_other ) {
                strands[id_label] = strand;
                reported[id_label] = false;
            } else if (!reported[id_label]
                && strands[id_label] != strand ) {
                TSeqPos start = loc.GetStart(eExtreme_Positional);
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StrandRev,
                    "Strand: The strand labels for SeqId " + id_label + 
                    " are inconsistent across the alignment.  The first inconsistent region is the "
                    + NStr::IntToString (region) + "(th) region, near sequence position "
                    + NStr::IntToString (start), align);
                reported[id_label] = true;
            }
        }
        region++;
    }
}


static size_t s_PercentBioseqMatch (CBioseq_Handle b1, CBioseq_Handle b2)
{
    size_t match = 0;
    size_t min_len = b1.GetInst().GetLength();
    if (b2.GetInst().GetLength() < min_len) {
        min_len = b2.GetInst().GetLength();
    }
    if (min_len == 0) {
        return 0;
    }
    if (b1.IsAa() && !b2.IsAa()) {
        return 0;
    } else if (!b1.IsAa() && b2.IsAa()) {
        return 0;
    }

    try {
        CSeqVector sv1 = b1.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        CSeqVector sv2 = b2.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        for ( CSeqVector_CI sv1_iter(sv1), sv2_iter(sv2); (sv1_iter) && (sv2_iter); ++sv1_iter, ++sv2_iter ) {
            if (*sv1_iter == *sv2_iter || *sv1_iter == 'N' || *sv2_iter == 'N') {
                match++;
            }
        }

        match = (match * 100) / min_len;

    } catch (CException& ) {
        match = 0;
    }
    return match;
}


//===========================================================================
// x_ValidateFastaLike:
//
//  Check if an alignment is FASTA-like. 
//  Alignment is FASTA-like if all gaps are at the end with dimensions > 2.
//===========================================================================

void CValidError_align::x_ValidateFastaLike
(const TDenseg& denseg,
 const CSeq_align& align)
{
    // check only global or partial type
    if ( (align.GetType() != CSeq_align::eType_global  &&
         align.GetType() != CSeq_align::eType_partial) ||
         denseg.GetDim() <= 2 ) {
        return;
    }

    size_t dim = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();

    vector<string> fasta_like;

    for ( int id = 0; id < s_GetNumIdsToUse(denseg); ++id ) {
        bool gap = false;
        
        const CDense_seg::TStarts& starts = denseg.GetStarts();
        for ( size_t seg = 0; seg < numseg; ++ seg ) {
            // if start value is -1, set gap flag to true
            if ( starts[id + (dim * seg)] < 0 ) {
                gap = true;
            } else if ( gap  ) {
                // if a positive start value is found after the initial -1 
                // start value, it's not fasta like. 
                //no need to check this sequence further
                return;
            } 

            if ( seg == numseg - 1) {
                // if no more positive start value are found after the initial
                // -1 start value, it's fasta like
                fasta_like.push_back(denseg.GetIds()[id]->AsFastaString());
            }
        }
    }

    if ( !fasta_like.empty() ) {
        CDense_seg::TIds::const_iterator id_it = denseg.GetIds().begin();
        string context = (*id_it)->GetSeqIdString();
        CBioseq_Handle master_seq = m_Scope->GetBioseqHandle(**id_it);
        bool is_fasta_like = false;
        if (master_seq) {
            ++id_it;
            while (id_it != denseg.GetIds().end() && !is_fasta_like) {
                CBioseq_Handle seq = m_Scope->GetBioseqHandle(**id_it);
                if (!seq || s_PercentBioseqMatch (master_seq, seq) < 50) {
                    is_fasta_like = true;
                }
                ++id_it;
            }
        } else {
            is_fasta_like = true;
        }
        if (is_fasta_like) {
            PostErr(eDiag_Warning, eErr_SEQ_ALIGN_FastaLike,
                    "Fasta: This may be a fasta-like alignment for SeqId: "
                    + fasta_like.front() + " in the context of " + context, align);
        }                    
    }

}           




//===========================================================================
// x_ValidateSegmentGap:
//
// Check if there is a gap for all sequences in a segment.
//===========================================================================

void CValidError_align::x_ValidateSegmentGap
(const TDenseg& denseg,
 const CSeq_align& align)
{
    int numseg  = denseg.GetNumseg();
    int dim     = denseg.GetDim();
    const CDense_seg::TStarts& starts = denseg.GetStarts();
    size_t align_pos = 0;

    for ( int seg = 0; seg < numseg; ++seg ) {
        bool seggap = true;
        for ( int id = 0; id < dim; ++id ) {
            if ( starts[seg * dim + id] != -1 ) {
                seggap = false;
                break;
            }
        }
        if ( seggap ) {
            // no sequence is present in this segment
            string label = "";
            if (denseg.IsSetIds() && denseg.GetIds().size() > 0) {
                denseg.GetIds()[0]->GetLabel(&label, CSeq_id::eContent);
            }
            if (NStr::IsBlank(label)) {
                label = "unknown";
            }
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                     "Segs: Segment " + NStr::SizetToString (seg + 1) + " (near alignment position "
                     + NStr::SizetToString(align_pos) + ") in the context of "
                     + label + " contains only gaps.  Each segment must contain at least one actual sequence -- look for columns with all gaps and delete them.",
                     align);
        }
        if (denseg.IsSetLens() && denseg.GetLens().size() > (unsigned int) seg) {
            align_pos += denseg.GetLens()[seg];
        }
    }
}


void CValidError_align::x_ValidateSegmentGap
(const TPacked& packed,
 const CSeq_align& align)
{
    static Uchar bits[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

    size_t numseg = packed.GetNumseg();
    size_t dim = packed.GetDim();
    const CPacked_seg::TPresent& present = packed.GetPresent();

    size_t align_pos = 1;
    for ( size_t seg = 0; seg < numseg;  ++seg) {
        size_t id = 0;
        for ( ; id < dim; ++id ) {
            size_t i = id + (dim * seg);
            if ( (present[i / 8] & bits[i % 8]) ) {
                break;
            }
        }
        if ( id == dim ) {
            // no sequence is present in this segment
            string label = "Unknown";
            if (packed.IsSetIds() && packed.GetIds().size() > 0) {
                packed.GetIds()[0]->GetLabel(&label);
            }
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                     "Segs: Segment " + NStr::SizetToString (seg + 1) + " (near alignment position "
                     + NStr::SizetToString(align_pos) + ") in the context of "
                     + label + " contains only gaps.  Each segment must contain at least one actual sequence -- look for columns with all gaps and delete them.",
                     align);
        }
        if (packed.IsSetLens() && packed.GetLens().size() > seg) {
            align_pos += packed.GetLens()[seg];
        }
    }       
}


void CValidError_align::x_ValidateSegmentGap
(const TStd& std_segs,
 const CSeq_align& align)
{
    size_t seg = 0;
    size_t align_pos = 1;
    ITERATE ( TStd, stdseg, std_segs ) {
        bool gap = true;
        size_t len = 0;
        string label = "Unknown";
        ITERATE ( CStd_seg::TLoc, loc, (*stdseg)->GetLoc() ) {
            if ( !(*loc)->IsEmpty()  &&  !(*loc)->IsNull() ) {
                gap = false;
                break;
            } else if (len == 0) {
                len = GetLength (**loc, m_Scope);
                (*loc)->GetId()->GetLabel(&label);
            }
        }
        if ( gap ) {
            // no sequence is present in this segment
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                     "Segs: Segment " + NStr::SizetToString (seg + 1) + " (near alignment position "
                     + NStr::SizetToString(align_pos) + ") in the context of "
                     + label + " contains only gaps.  Each segment must contain at least one actual sequence -- look for columns with all gaps and delete them.",
                     align);
        }
        align_pos += len;
        ++seg;
    }
}


void CValidError_align::x_ValidateSegmentGap
(const TDendiag& dendiags,
 const CSeq_align& align)
{
    size_t seg = 0;
    TSeqPos align_pos = 1;
    ITERATE( TDendiag, diag_seg, dendiags ) {
        if (!(*diag_seg)->IsSetDim() || (*diag_seg)->GetDim() == 0) {
            string label = "Unknown";
            if ((*diag_seg)->IsSetIds() && (*diag_seg)->GetIds().size() > 0) {
                (*diag_seg)->GetIds().front()->GetLabel(&label);
            }
            PostErr (eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                     "Segs: Segment " + NStr::SizetToString (seg + 1) + " (near alignment position "
                     + NStr::IntToString(align_pos) + ") in the context of "
                     + label + " contains only gaps.  Each segment must contain at least one actual sequence -- look for columns with all gaps and delete them.",
                     align);
        }
        if ((*diag_seg)->IsSetLen()) {
            align_pos += (*diag_seg)->GetLen();
        }
        ++seg;
    }
}


//===========================================================================
// x_ValidateSeqIdInSeqAlign:
//
//  Validate SeqId in sequence alignment.
//===========================================================================

void CValidError_align::x_ValidateSeqId(const CSeq_align& align)
{
    vector< CRef< CSeq_id > > ids;
    x_GetIds(align, ids);

    ITERATE( vector< CRef< CSeq_id > >, id_iter, ids ) {
        const CSeq_id& id = **id_iter;
        if ( id.IsLocal() ) {
            if ( !m_Scope->GetBioseqHandle(id) ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SeqIdProblem,
                    "The sequence corresponding to SeqId " + 
                    id.AsFastaString() + " could not be found.",
                    align);
            }
        }
    }
}


void CValidError_align::x_GetIds
(const CSeq_align& align,
 vector< CRef< CSeq_id > >& ids)
{
    ids.clear();

    switch ( align.GetSegs().Which() ) {

    case CSeq_align::C_Segs::e_Dendiag:
        ITERATE( TDendiag, diag_seg, align.GetSegs().GetDendiag() ) {
            const vector< CRef< CSeq_id > >& diag_ids = (*diag_seg)->GetIds();
            copy(diag_ids.begin(), diag_ids.end(), back_inserter(ids));
        }
        break;
        
    case CSeq_align::C_Segs::e_Denseg:
        ids = align.GetSegs().GetDenseg().GetIds();
        break;
        
    case CSeq_align::C_Segs::e_Packed:
        copy(align.GetSegs().GetPacked().GetIds().begin(),
             align.GetSegs().GetPacked().GetIds().end(),
             back_inserter(ids));
        break;
        
    case CSeq_align::C_Segs::e_Std:
        ITERATE( TStd, std_seg, align.GetSegs().GetStd() ) {
            ITERATE( CStd_seg::TLoc, loc, (*std_seg)->GetLoc() ) {
                CSeq_id* idp = const_cast<CSeq_id*>(&GetId(**loc, m_Scope));
                CRef<CSeq_id> ref(idp);
                ids.push_back(ref);
            }
        }
        break;
            
    default:
        break;
    }
}


//===========================================================================
// x_ValidateSeqLength:
//
//  Check segment length, start and end point in Dense_diag, Dense_seg,  
//  Packed_seg and Std_seg.
//===========================================================================

// Make sure that, in Dense_diag alignment, segment length is not greater
// than Bioseq length
void CValidError_align::x_ValidateSeqLength
(const CDense_diag& dendiag,
 size_t dendiag_num,
 const CSeq_align& align)
{
    size_t dim = dendiag.GetDim();
    TSeqPos len = dendiag.GetLen();
    const CDense_diag::TIds& ids = dendiag.GetIds();
    
    CDense_diag::TStarts::const_iterator starts_iter = 
            dendiag.GetStarts().begin();
    
    for ( size_t id = 0; id < dim; ++id ) {
        TSeqPos bslen = GetLength(*(ids[id]), m_Scope);
        TSeqPos start = *starts_iter;

        string label;
        ids[id]->GetLabel(&label);

        // verify start
        if ( start >= bslen ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_StartMorethanBiolen,
                    "Start: In sequence " + label + ", segment 1 (near sequence position " + NStr::IntToString (start)
                    + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                    align);
        }
        
        // verify length
        if ( start + len > bslen ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SumLenStart,
                    "Start: In sequence " + label + ", segment 1 (near sequence position " + NStr::IntToString (start)
                    + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                    align);
        }
        ++starts_iter;
    }
}


        

void CValidError_align::x_ValidateSeqLength
(const TDenseg& denseg,
 const CSeq_align& align)
{
    int dim     = denseg.GetDim();
    int numseg  = denseg.GetNumseg();
    const CDense_seg::TIds& ids       = denseg.GetIds();
    const CDense_seg::TStarts& starts = denseg.GetStarts();
    const CDense_seg::TLens& lens      = denseg.GetLens();
    bool minus = false;

    if (numseg > lens.size()) {
        numseg = lens.size();
    }

    for ( int id = 0; id < ids.size(); ++id ) {
        TSeqPos bslen = GetLength(*(ids[id]), m_Scope);
        minus = denseg.IsSetStrands()  &&
            denseg.GetStrands()[id] == eNa_strand_minus;

        string label;
        ids[id]->GetLabel(&label);

        for ( int seg = 0; seg < numseg; ++seg ) {
            size_t curr_index = 
                id + (minus ? numseg - seg - 1 : seg) * dim;
            // no need to verify if segment is not present
            if ( starts[curr_index] == -1 ) {
                continue;
            }
            size_t lens_index = minus ? numseg - seg - 1 : seg;

            // verify that start plus segment does not exceed total bioseq len
            if ( starts[curr_index] + lens[lens_index] > bslen ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SumLenStart,
                        "Start: In sequence " + label + ", segment " 
                        + NStr::IntToString (seg + 1) + " (near sequence position " + NStr::IntToString (starts[curr_index])
                        + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                        align);
            }

            // find the next segment that is present
            size_t next_index = curr_index;
            int next_seg;
            for ( next_seg = seg + 1; next_seg < numseg; ++next_seg ) {
                next_index = 
                    id + (minus ? numseg - next_seg - 1 : next_seg) * dim;
                
                if ( starts[next_index] != -1 ) {
                    break;
                }
            }
            if ( next_seg == numseg  ||  next_index == curr_index ) {
                continue;
            }

            // length plus start should be equal to the closest next 
            // start that is not -1
            if ( starts[curr_index] + (TSignedSeqPos)lens[lens_index] !=
                starts[next_index] ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_DensegLenStart,
                        "Start/Length: There is a problem with sequence " + label + 
                        ", in segment " + NStr::SizetToString (curr_index + 1)
                        + " (near sequence position " + NStr::IntToString (starts[curr_index])
                        + "), context " + label + ": the segment is too long or short or the next segment has an incorrect start position", align);
            }
        }
    }
}


void CValidError_align::x_ValidateSeqLength
(const TPacked& packed,
 const CSeq_align& align)
{
    static Uchar bits[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

    if (!packed.IsSetDim() || !packed.IsSetIds() || !packed.IsSetPresent() || !packed.IsSetNumseg()) {
        return;
    }

    size_t dim = packed.GetDim();
    size_t numseg = packed.GetNumseg();

    const CPacked_seg::TPresent& present = packed.GetPresent();
    CPacked_seg::TIds::const_iterator id_it = packed.GetIds().begin();

    for ( size_t id = 0; id < dim && id_it != packed.GetIds().end(); ++id, ++id_it ) {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle (**id_it);
        if (bsh) {
            string label;
            (*id_it)->GetLabel (&label);
            TSeqPos seg_start = packed.GetStarts()[id];
            if (seg_start >= bsh.GetBioseqLength()) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StartMorethanBiolen,
                        "Start: In sequence " + label + ", segment 1 (near sequence position " + NStr::IntToString (seg_start)
                        + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                        align);
            }
            for ( size_t seg = 0; seg < numseg; ++seg ) {
                size_t i = id + seg * dim;
                if ( i/8 < present.size() && (present[i / 8] & bits[i % 8]) ) {
                    seg_start += packed.GetLens()[seg];
                    if (seg_start > bsh.GetBioseqLength()) {                    
                        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SumLenStart,
                                "Start: In sequence " + label + ", segment " + NStr::SizetToString (seg + 1)
                                + " (near sequence position " + NStr::IntToString (seg_start)
                                + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                                align);
                    }
                }
            }
        }
    }
}


void CValidError_align::x_ValidateSeqLength
(const TStd& std_segs,
 const CSeq_align& align)
{
    int seg = 1;
    ITERATE( TStd, iter, std_segs ) {
        const CStd_seg& stdseg = **iter;

        ITERATE ( CStd_seg::TLoc, loc_iter, stdseg.GetLoc() ) {
            const CSeq_loc& loc = **loc_iter;
    
            if ( loc.IsWhole()  || loc.IsEmpty()  ||  loc.IsNull() ) {
                continue;
            }

            if ( !IsOneBioseq(loc, m_Scope) ) {
                continue;
            }

            TSeqPos from = loc.GetTotalRange().GetFrom();
            TSeqPos to   = loc.GetTotalRange().GetTo();
            TSeqPos loclen = GetLength( loc, m_Scope);
            TSeqPos bslen = GetLength(GetId(loc, m_Scope), m_Scope);
            string  bslen_str = NStr::UIntToString(bslen);
            string label;
            loc.GetId()->GetLabel(&label);

            if ( from > bslen - 1 ) { 
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StartMorethanBiolen,
                        "Start: In sequence " + label + ", segment " + NStr::IntToString (seg)
                        + " (near sequence position " + NStr::IntToString(from)
                        + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                        align);
            }

            if ( to > bslen - 1 ) { 
                
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_EndMorethanBiolen,
                        "Length: In sequence " + label + ", segment " + NStr::IntToString (seg)
                        + " (near sequence position " + NStr::IntToString(to)
                        + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                        align);
            }

            if ( loclen > bslen ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_LenMorethanBiolen,
                        "Length: In sequence " + label + ", segment " + NStr::IntToString (seg)
                        + " (near sequence position " + NStr::IntToString(to)
                        + "), the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment",
                        align);

            }
        }
        seg++;
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
