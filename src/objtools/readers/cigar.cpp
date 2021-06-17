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
#include <objtools/readers/reader_exception.hpp>

#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <ctype.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

SCigarAlignment::SCigarAlignment(const string& s, EFormat fmt)
    : format(GuessFormat(s, fmt))
{
    SSegment seg = { eNotSet, 1 };

    for (SIZE_TYPE pos = 0;  pos < s.size();  ++pos) {
        if (isalpha((unsigned char) s[pos])) {
            if (format == eOpFirst  &&  seg.op != eNotSet) {
                _ASSERT(seg.len == 1);
                x_AddAndClear(seg);
            }
            seg.op = static_cast<EOperation>(toupper((unsigned char) s[pos]));
            if (format == eLengthFirst) {
                x_AddAndClear(seg);
            }
        } else if (isdigit((unsigned char) s[pos])) {
            SIZE_TYPE pos2 = s.find_first_not_of("0123456789", pos + 1);
            seg.len = NStr::StringToInt(s.substr(pos, pos2 - pos));
            if (format == eOpFirst) {
                _ASSERT(seg.op != eNotSet);
                x_AddAndClear(seg);
            }
            pos = pos2 - 1;
        }
        // ignore other characters, particularly space and plus.
    }

    if (seg.op != eNotSet) {
        _ASSERT(format == eOpFirst);
        _ASSERT(seg.len == 1);
        x_AddAndClear(seg);
    }
}


SCigarAlignment::EFormat SCigarAlignment::GuessFormat(const string& s,
                                                      EFormat fmt)
{
    static const char* const kAlnum
        = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    SIZE_TYPE first_alnum = s.find_first_of(kAlnum);
    SIZE_TYPE last_alnum  = s.find_last_of(kAlnum);
    EFormat   result      = fmt;

    if (first_alnum == last_alnum) {
        if (first_alnum != NPOS  &&  isdigit((unsigned char) s[first_alnum])) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "SCigarAlignment: no operations found", first_alnum);
        } else {
            result = eLengthFirst; // arbitrary
        }
    } else {
        _ASSERT(first_alnum != NPOS);
        _ASSERT(last_alnum  != NPOS);
        _ASSERT(first_alnum < last_alnum);
        if (isdigit((unsigned char) s[first_alnum])) {
            if (fmt == eOpFirst) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            "SCigarAlignment: expected operation-first syntax",
                            first_alnum);
            } else if (isdigit((unsigned char) s[last_alnum])) {
                NCBI_THROW2
                    (CObjReaderParseException, eFormat,
                     "SCigarAlignment: must start or end with an operation",
                     first_alnum);
            } else {
                result = eLengthFirst;
            }
        } else if (isdigit((unsigned char) s[last_alnum])) {
            if (fmt == eLengthFirst) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            "SCigarAlignment: expected length-first syntax",
                            first_alnum);
            } else {
                result = eOpFirst;
            }
        } else if (s.find_first_of("0123456789") == NPOS) {
            result = eLengthFirst; // arbitrary
        } else {
            switch (fmt) {
            case eConservativeGuess:
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            "SCigarAlignment: ambiguous syntax", first_alnum);
            case eLengthFirst:
            case eLengthFirstIfAmbiguous:
                result = eLengthFirst;
                break;
            case eOpFirst:
            case eOpFirstIfAmbiguous:
                result = eOpFirst;
            }
        }
    }

    return result;
}


CRef<CSeq_loc> SCigarAlignment::x_NextChunk(const CSeq_id& id, TSeqPos pos,
                                            TSignedSeqPos len) const
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId().Assign(id);
    if (len >= 0) {
        loc->SetInt().SetFrom(pos);
        loc->SetInt().SetTo(pos + len - 1);
        loc->SetInt().SetStrand(eNa_strand_plus);
    } else {
        loc->SetInt().SetFrom(pos + len + 1);
        loc->SetInt().SetTo(pos);
        loc->SetInt().SetStrand(eNa_strand_minus);
    }
    return loc;
}


CRef<CSeq_align> SCigarAlignment::operator()(const CSeq_interval& ref,
                                             const CSeq_interval& tgt) const
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
    ITERATE (TSegments, it, segments) {
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
