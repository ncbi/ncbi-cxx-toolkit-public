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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Code to handle Concise Idiosyncratic Gapped Alignment Report notation.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/cigar.hpp>

#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <ctype.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

SCigarAlignment::SCigarAlignment(const string& s)
{
    SSegment seg = { eNotSet, 0 };

    for (SIZE_TYPE pos = 0;  pos < s.size();  ++pos) {
        if (isalpha(s[pos])) {
            seg.op = static_cast<EOperation>(toupper(s[pos]));
            if (seg.len) {
                x_AddAndClear(seg);
            }
        } else if (isdigit(s[pos])) {
            SIZE_TYPE pos2 = s.find_first_not_of("0123456789");
            seg.len = NStr::StringToInt(s.substr(pos, pos2));
            if (seg.op) {
                x_AddAndClear(seg);
            }
            pos = pos2 - 1;
        }
        // ignore other characters, particularly space and plus.
    }

    if (seg.op  &&  seg.len) {
        x_AddAndClear(seg);
    }
}


CRef<CSeq_loc> SCigarAlignment::x_NextChunk(const CSeq_id& id, TSeqPos pos,
                                            TSignedSeqPos len)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetId(id);
    if (len >= 0) {
        loc->SetInt().SetFrom(pos);
        loc->SetInt().SetTo(pos + len - 1);
    } else {
        loc->SetInt().SetFrom(pos + len + 1);
        loc->SetInt().SetTo(pos);
        loc->SetInt().SetStrand(eNa_strand_minus);
    }
    return loc;
}


CRef<CSeq_align> SCigarAlignment::operator()(const CSeq_interval& ref,
                                             const CSeq_interval& tgt)
{
    int refsign = 1, refscale = 1, tgtsign = 1, tgtscale = 1;
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial);
    align->SetDim(2);

    {{
        // Figure out whether we're looking at a homogeneous or a
        // nuc->prot alignment.  (It's not clear that the format
        // supports prot->nuc alignments.)
        TSeqPos count = 0, shifts = 0;
        bool    shifty = false;
        ITERATE (TSegments, it, segments) {
            switch (it->op) {
            case eMatch:
            case eDeletion:
            case eIntron:
                count += it->len;
                break;
            case eInsertion:
                break;
            case eForwardShift:
                shifts += it->len;
                shifty = true;
                break;
            case eReverseShift:
                shifts -= it->len;
                shifty = true;
                break;
            default:
                // x_Warn(string("Bad segment type ") + raw_seg.type);
                break;
            }
        }
        if (3 * count + shifts == ref.GetLength()) {
            refscale = 3; // nuc -> prot
        } else if (count + shifts == ref.GetLength()) {
            // warn if shifty?
        } else if (shifty) {
            refscale = 3; // assume N->P despite mismatch
        } else {
            // assume homogenous despite mismatch
        }
    }}

    if (ref.IsSetStrand()  &&  IsReverse(ref.GetStrand())) {
        refsign = -1;
    }
    if (tgt.IsSetStrand()  &&  IsReverse(tgt.GetStrand())) {
        tgtsign = -1;
    }

    CRef<CSeq_id> refid(new CSeq_id), tgtid(new CSeq_id);
    refid->Assign(ref.GetId());
    tgtid->Assign(tgt.GetId());

    TSeqPos refpos = (refsign > 0) ? ref.GetFrom() : ref.GetTo();
    TSeqPos tgtpos = (tgtsign > 0) ? tgt.GetFrom() : tgt.GetTo();
    iterate (TSegments, it, segments) {
        CRef<CSeq_loc> refseg = x_NextChunk(*refid, refpos,
                                            it->len * refscale * refsign);
        CRef<CSeq_loc> tgtseg = x_NextChunk(*tgtid, tgtpos,
                                            it->len * tgtscale * tgtsign);
        switch (it->op) {
        case eIntron:
            // refseg->SetEmpty(*refid);
            // tgtseg->SetEmpty(*tgtid);
            // fall through
        case eMatch:
            refpos += it->len * refscale * refsign;
            tgtpos += it->len * tgtscale * tgtsign;
            break;
        case eInsertion:
            refseg->SetEmpty(*refid);
            tgtpos += it->len * tgtscale * tgtsign;
            break;
        case eDeletion:
            refpos += it->len * refscale * refsign;
            tgtseg->SetEmpty(*tgtid);
            break;
        case eForwardShift:
            refpos += refsign;
            continue;
        case eReverseShift:
            refpos -= refsign;
            continue;
        case eNotSet:
            break;
        }
        CRef<CStd_seg> seg(new CStd_seg);
        seg->SetLoc().push_back(refseg);
        seg->SetLoc().push_back(tgtseg);
        align->SetSegs().SetStd().push_back(seg);
    }

    if (refscale == tgtscale) {
        align.Reset(align->CreateDensegFromStdseg());
    }
    return align;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/06/07 20:42:35  ucko
* Add a reader for CIGAR alignments, as used by GFF 3.
*
*
* ===========================================================================
*/
