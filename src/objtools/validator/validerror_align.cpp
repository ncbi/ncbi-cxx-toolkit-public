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
#include <objmgr/util/sequence.hpp>

#include <map>
#include <vector>
#include <algorithm>

#include "validatorp.hpp"


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
    default:
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_Segtype,
            "This alignment has undefined or unsupported Seq_align"
            "segtype " + NStr::IntToString(segtype) + ".", align);
        break;
    }  // end of switch statement
}


// ================================  Private  ===============================


void CValidError_align::x_ValidateDenseg
(const TDenseg& denseg,
 const CSeq_align& align)
{
    bool cont = true;

    // assert dim >= 2
    if ( !x_ValidateDim(denseg, align) ) {
        return;
    }

    size_t dim     = denseg.GetDim();
    size_t numseg  = denseg.GetNumseg();

    // assert dim == Ids.size()
    if ( dim != denseg.GetIds().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                "Mismatch between specified dimension (" +
                NStr::UIntToString(dim) + 
                ") and number of Seq-ids (" + 
                NStr::UIntToString(denseg.GetIds().size()) + ")",
                align);
        cont = false;
    }

    // assert numseg == Lens.size()
    if ( numseg != denseg.GetLens().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsNumsegMismatch,
                "Mismatch between specified numseg (" +
                NStr::UIntToString(numseg) + 
                ") and number of Lens (" + 
                NStr::UIntToString(denseg.GetLens().size()) + ")",
                align);
        cont = false;
    }

    // assert dim * numseg == Starts.size()
    if ( dim * numseg != denseg.GetStarts().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsStartsMismatch,
                "The number of Starts (" + 
                NStr::UIntToString(denseg.GetStarts().size()) +
                ") does not match the expected size of dim * numseg (" +
                NStr::UIntToString(dim * numseg) + ")", align);
        cont = false;
    } 

    if( cont ) {
        x_ValidateStrand(denseg, align);
        x_ValidateFastaLike(denseg, align);
        x_ValidateSegmentGap(denseg, align);
        
        if ( m_Imp.IsRemoteFetch() ) {
            x_ValidateSeqId(align);
            x_ValidateSeqLength(denseg, align);
        }
    }
}




void CValidError_align::x_ValidatePacked
(const TPacked& packed,
 const CSeq_align& align)
{
    bool cont = true;

    // assert dim >= 2
    if ( !x_ValidateDim(packed, align) ) {
        return;
    }

    size_t dim     = packed.GetDim();
    size_t numseg  = packed.GetNumseg();
    
    // assert dim == Ids.size()
    if ( dim != packed.GetIds().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
            "Mismatch between specified dimension (" +
            NStr::UIntToString(dim) + 
            ") and number of Seq-ids (" + 
            NStr::UIntToString(packed.GetIds().size()) + ")",
            align);
        cont = false;
    }
    
    // assert numseg == Lens.size()
    if ( numseg != packed.GetLens().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
            "Mismatch between specified numseg (" +
            NStr::UIntToString(numseg) + 
            ") and number of Lens (" + 
            NStr::UIntToString(packed.GetLens().size()) + ")",
            align);
        cont = false;
    }
    
    // assert dim * numseg == Present.size()
    if ( dim * numseg != packed.GetPresent().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentMismatch,
                "The number of Present (" + 
                NStr::UIntToString(packed.GetPresent().size()) +
                ") does not match the expected size of dim * numseg (" +
                NStr::UIntToString(dim * numseg) + ")", align);
        cont = false;
    } else {
        // assert # of '1' bits in present == Starts.size()
        size_t bits = x_CountBits(packed.GetPresent());
        if ( bits != packed.GetStarts().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentStartsMismatch,
                "Starts does not have the same number of elements (" +
                NStr::UIntToString(packed.GetStarts().size()) + 
                ") as specified by Present (" + NStr::UIntToString(bits) +
                ")", align);
            cont = false;
        }

        // assert # of '1' bits in present == Strands.size() (if exists)
        if ( packed.IsSetStrands() ) {
            if ( bits != packed.GetStarts().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsPresentStrandsMismatch,
                    "Strands does not have the same number of elements (" +
                    NStr::UIntToString(packed.GetStrands().size()) +
                    ") as specified by Present (" +
                    NStr::UIntToString(bits) + ")", align);
                cont = false;
            }
        }
    }
    
    if( cont ) {
        x_ValidateStrand(packed, align);
        x_ValidateFastaLike(packed, align);
        x_ValidateSegmentGap(packed, align);
        
        if ( m_Imp.IsRemoteFetch() ) {
            x_ValidateSeqId(align);
            x_ValidateSeqLength(packed, align);
        }
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
        bool cont = true;
        ++num_dendiag;

        const CDense_diag& dendiag = **dendiag_iter;
        size_t dim = dendiag.GetDim();

        // assert dim >= 2
        if ( !x_ValidateDim(dendiag, align, num_dendiag) ) {
            continue;
        }

        // assert dim == Ids.size()
        if ( dim != dendiag.GetIds().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                "Mismatch between specified dimension (" +
                NStr::UIntToString(dim) + 
                ") and number of Seq-ids (" + 
                NStr::UIntToString(dendiag.GetIds().size()) + ") in dendiag " +
                NStr::UIntToString(num_dendiag), align);
            cont = false;
        }

        // assert dim == Starts.size()
        if ( dim != dendiag.GetStarts().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                "Mismatch between specified dimension (" +
                NStr::UIntToString(dim) + 
                ") and number ofStarts (" + 
                NStr::UIntToString(dendiag.GetStarts().size()) + 
                ") in dendiag " + NStr::UIntToString(num_dendiag), align);
            cont = false;
        } 

        // assert dim == Strands.size() (if exist)
        if ( dendiag.IsSetStrands() ) {
            if ( dim != dendiag.GetStrands().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                    "Mismatch between specified dimension (" +
                    NStr::UIntToString(dim) + 
                    ") and number of Strands (" + 
                    NStr::UIntToString(dendiag.GetStrands().size()) + 
                    ") in dendiag " + NStr::UIntToString(num_dendiag), align);
                cont = false;
            } 
        }

        if ( cont ) {
            if ( m_Imp.IsRemoteFetch() ) {
                x_ValidateSeqId(align);
                x_ValidateSeqLength(dendiag, num_dendiag, align);
            }
        }
    }
}


void CValidError_align::x_ValidateStd
(const TStd& std_segs,
 const CSeq_align& align)
{
    bool cont = true;

    size_t num_stdseg = 0;
    ITERATE( TStd, stdseg_iter, std_segs) {
        ++num_stdseg;

        const CStd_seg& stdseg = **stdseg_iter;
        size_t dim = stdseg.GetDim();

        // assert dim >= 2
        if ( !x_ValidateDim(stdseg, align, num_stdseg) ) {
            cont = false;
            continue;
        }

        // assert dim == Loc.size()
        if ( dim != stdseg.GetLoc().size() ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                "Mismatch between specified dimension (" +
                NStr::UIntToString(dim) + 
                ") and number of Seq-locs (" + 
                NStr::UIntToString(stdseg.GetLoc().size()) + ") in stdseg " +
                NStr::UIntToString(num_stdseg), align);
            cont = false;
            continue;
        }

        // assert dim == Ids.size()
        if ( stdseg.IsSetIds() ) {
            if ( dim != stdseg.GetIds().size() ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsDimMismatch,
                    "Mismatch between specified dimension (" +
                    NStr::UIntToString(dim) + 
                    ") and number of Seq-ids (" + 
                    NStr::UIntToString(stdseg.GetIds().size()) + ")",
                    align);
                cont = false;
                continue;
            }
        }
    }

    if( cont ) {
        x_ValidateStrand(std_segs, align);
        x_ValidateFastaLike(std_segs, align);
        x_ValidateSegmentGap(std_segs, align);
        
        if ( m_Imp.IsRemoteFetch() ) {
            x_ValidateSeqId(align);
            x_ValidateSeqLength(std_segs, align);
        }
    }
}


template <typename T>
bool CValidError_align::x_ValidateDim
(T& obj,
 const CSeq_align& align,
 size_t part)
{
    if ( !obj.IsSetDim() ) {
        string msg = "Dim not set";
        if ( part > 0 ) {
            msg += " Part " + NStr::UIntToString(part);
        }
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsInvalidDim, msg, align);
        return false;
    }

    size_t dim = obj.GetDim();        
    if ( dim < 2 ) {
        string msg = "Dimension (" + NStr::UIntToString(dim) + 
            ") must be at least 2.";
        if ( part > 0 ) {
            msg += " Part " + NStr::UIntToString(part);
        }
        PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegsInvalidDim, msg, align);
        return false;
    }
    return true;
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
                    "The strand labels for SeqId " + 
                    denseg.GetIds()[id]->AsFastaString() +
                    " are inconsistent across the alignment. "
                    "The first inconsistent region is the " + 
                    NStr::UIntToString(seg), align);
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
                        "Strand for SeqId " + (*id_iter)->AsFastaString() +
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
    map< CConstRef<CSeq_id>, ENa_strand > strands;

    ITERATE ( TStd, stdseg, std_segs ) {
        ITERATE ( CStd_seg::TLoc, loc_iter, (*stdseg)->GetLoc() ) {
            const CSeq_loc& loc = **loc_iter;
            
            if ( !IsOneBioseq(loc, m_Scope) ) {
                // !!! should probably be an error
                continue;
            }
            CConstRef<CSeq_id> id(&GetId(loc, m_Scope));
            ENa_strand strand = GetStrand(loc, m_Scope);

            if ( strand == eNa_strand_unknown  || 
                 strand == eNa_strand_other ) {
                continue;
            }

            if ( strands[id] == eNa_strand_unknown  ||
                 strands[id] == eNa_strand_other ) {
                strands[id] = strand;
            }

            if ( strands[id] != strand ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StrandRev,
                    "Strands for SeqId " + id->AsFastaString() + 
                    " are inconsistent across the alignment", align);
            }
        }
    }
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

    for ( size_t id = 0; id < dim; ++id ) {
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
                break;
            } 

            if ( seg == numseg - 1  &&  gap ) {
                // if no more positive start value are found after the initial
                // -1 start value, it's fasta like
                fasta_like.push_back(denseg.GetIds()[id]->AsFastaString());
            }
        }
    }

    if ( !fasta_like.empty() ) {
        string fasta_like_ids;
        bool first = true;
        ITERATE( vector<string>, idstr, fasta_like ) {
            if ( first ) {
                first = false;
            } else {
                fasta_like_ids += ", ";
            }

            fasta_like_ids += *idstr;
        }
        fasta_like_ids += ".";

        PostErr(eDiag_Error, eErr_SEQ_ALIGN_FastaLike,
            "This may be a fasta-like alignment for SeqIds: " + 
            fasta_like_ids, align);
    }

}           


void CValidError_align::x_ValidateFastaLike
(const TPacked& packed,
 const CSeq_align& align)
{
    // check only global or partial type
    if ( align.GetType() == CSeq_align::eType_global  ||
         align.GetType() == CSeq_align::eType_partial ||
         packed.GetDim() <= 2 ) {
        return;
    }

    static Uchar bits[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    
    size_t dim = packed.GetDim();
    size_t numseg = packed.GetNumseg();

    CPacked_seg::TIds::const_iterator id_iter = packed.GetIds().begin();
    const CPacked_seg::TPresent& present = packed.GetPresent();

    for ( size_t id = 0; id < dim;  ++id, ++id_iter ) {
        bool gap = false;
        for ( size_t seg = 0; seg < numseg; ++ seg ) {
            // if a segment is not present, set gap flag to true
            size_t i = id + (dim * seg);
            if ( !(present[i / 8] & bits[i % 8]) ) {
                gap = true;
            } else if ( gap ) {
                // if a segment is found later, then it's not fasta like, 
                // no need to check this sequence further.
                break;
            } 

            if ( seg == numseg - 1  &&  gap ) {
                // if no more segments are found for this sequence, 
                // then it's  fasta like.
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_FastaLike,
                    "This may be a fasta-like alignment for SeqId: " + 
                    (*id_iter)->AsFastaString(), align);
            }
        }
    }
}


void CValidError_align::x_ValidateFastaLike
(const TStd& std_segs,
 const CSeq_align& align)
{
    // check only global or partial type
    if ( align.GetType() == CSeq_align::eType_global  ||
         align.GetType() == CSeq_align::eType_partial ) {
        return;
    }
    // suspected seq-ids
    typedef map< CConstRef<CSeq_id>, bool > TIdGapMap;
    TIdGapMap fasta_like;

    ITERATE ( TStd, stdseg, std_segs ) {
        ITERATE ( CStd_seg::TLoc, loc_iter, (*stdseg)->GetLoc() ) {
            const CSeq_loc& loc = **loc_iter;
            
            if ( !IsOneBioseq(loc, m_Scope) ) {
                // !!! should probably be an error
                continue;
            }
            CConstRef<CSeq_id> id(&GetId(loc, m_Scope));
            bool gap = loc.IsEmpty()  ||  loc.IsNull();
            if ( gap  &&  fasta_like.find(id) == fasta_like.end() ) {
                fasta_like[id] = true;
            }
            if ( !gap  &&  fasta_like.find(id) != fasta_like.end() ) {
                fasta_like[id] = false;
            }
        }
    }
    
    ITERATE ( TIdGapMap, iter, fasta_like ) {
        if ( iter->second ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_FastaLike,
                "This may be a fasta-like alignment for SeqId: " + 
                iter->first->AsFastaString(), align);
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
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                "Segment " + NStr::UIntToString(seg) + " contains only gaps.",
                align);
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
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                "Segment " + NStr::UIntToString(seg) + "contains only gaps.",
                align);
        }
    }       
}


void CValidError_align::x_ValidateSegmentGap
(const TStd& std_segs,
 const CSeq_align& align)
{
    size_t seg = 0;
    ITERATE ( TStd, stdseg, std_segs ) {
        bool gap = true;
        ITERATE ( CStd_seg::TLoc, loc, (*stdseg)->GetLoc() ) {
            if ( !(*loc)->IsEmpty()  ||  !(*loc)->IsEmpty() ) {
                gap = false;
                break;
            }
        }
        if ( gap ) {
            // no sequence is present in this segment
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SegmentGap,
                "Segment " + NStr::UIntToString(seg) + "contains only gaps.",
                align);
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

        // verify start
        if ( start > bslen ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_StartMorethanBiolen,
                    "Start (" + NStr::UIntToString(start) +
                    ") exceeds bioseq length (" +
                    NStr::UIntToString(bslen) +
                    ") for seq-id " + ids[id]->AsFastaString() +
                    "in dendiag " + NStr::UIntToString(dendiag_num),
                    align);
        }
        
        // verify length
        if ( start + len > bslen ) {
            PostErr(eDiag_Error, eErr_SEQ_ALIGN_SumLenStart,
                    "Start + length (" + NStr::UIntToString(start + len) +
                    ") exceeds bioseq length (" +
                    NStr::UIntToString(bslen) +
                    ") for seq-id " + ids[id]->AsFastaString() +
                    "in dendiag " + NStr::UIntToString(dendiag_num),
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

    for ( int id = 0; id < dim; ++id ) {
        TSeqPos bslen = GetLength(*(ids[id]), m_Scope);
        minus = denseg.IsSetStrands()  &&
            denseg.GetStrands()[id] == eNa_strand_minus;
        
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
                    "Start + segment length (" + 
                    NStr::UIntToString(starts[curr_index] + lens[lens_index]) +
                    ") exceeds bioseq length (" +
                    NStr::UIntToString(bslen) + ")", align);
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
                    "Start + segment length (" + 
                    NStr::UIntToString(starts[curr_index] + lens[lens_index]) +
                    ") exceeds next start (" +
                    NStr::UIntToString(starts[next_index]) + ")", align);
            }
        }
    }
}


void CValidError_align::x_ValidateSeqLength
(const TPacked& packed,
 const CSeq_align& align)
{
    static Uchar bits[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

    size_t dim = packed.GetDim();
    size_t numseg = packed.GetNumseg();
    //CPacked_seg::TStarts::const_iterator start =
    //    packed.GetStarts().begin();

    const CPacked_seg::TPresent& present = packed.GetPresent();

    for ( size_t id = 0; id < dim; ++id ) {
        for ( size_t seg = 0; seg < numseg; ++seg ) {
            size_t i = id + seg * dim;
            if ( (present[i / 8] & bits[i % 8]) ) {
                // !!!
            }
        }
    }
}


void CValidError_align::x_ValidateSeqLength
(const TStd& std_segs,
 const CSeq_align& align)
{
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
            loc.GetLabel(&label);

            if ( from > bslen - 1 ) { 
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_StartMorethanBiolen,
                    "Loaction: " + label + ". From (" + 
                    NStr::UIntToString(from) + 
                    ") is more than bioseq length (" + bslen_str + ")", 
                    align);
            }

            if ( to > bslen - 1 ) { 
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_EndMorethanBiolen,
                    "Loaction: " + label + ". To (" + NStr::UIntToString(to) +
                    ") is more than bioseq length (" + bslen_str + ")", align);
            }

            if ( loclen > bslen ) {
                PostErr(eDiag_Error, eErr_SEQ_ALIGN_LenMorethanBiolen,
                    "Loaction: " + label + ". Length (" + 
                    NStr::UIntToString(loclen) + 
                    ") is more than bioseq length (" + bslen_str + ")", align);
            }
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2003/09/30 18:30:38  shomrat
* changed unsigned to signed to eliminate infinite loop
*
* Revision 1.6  2003/06/02 16:06:43  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.5  2003/05/28 16:24:40  shomrat
* Report a single FastaLike error from each alignment
*
* Revision 1.4  2003/04/29 14:58:07  shomrat
* Implemented SeqAlign validation
*
* Revision 1.3  2003/03/31 14:40:52  shomrat
* $id: -> $id$
*
* Revision 1.2  2002/12/24 16:52:42  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:15:59  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
