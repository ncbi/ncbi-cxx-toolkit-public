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
*   new (early 2003) flat-file generator -- location representation
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_formatter.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CFlatLoc::CFlatLoc(const CSeq_loc& loc, CFlatContext& ctx)
{
    if (&loc == ctx.m_CachedLoc  &&  ctx.m_CachedFlatLoc != 0) {
        *this = *ctx.m_CachedFlatLoc;
        return;
    }

    CNcbiOstrstream oss;
    x_Add(loc, oss, ctx);
    m_String = CNcbiOstrstreamToString(oss);
    
    if (CanBeDeleted()) {
        // Set m_CachedFlatLoc twice to maintain the invariant that it
        // either corresponds to m_CachedLoc or is null
        ctx.m_CachedFlatLoc.Reset();
        ctx.m_CachedLoc.Reset(&loc);
        ctx.m_CachedFlatLoc.Reset(this);
    }
}


void CFlatLoc::x_Add(const CSeq_loc& loc, CNcbiOstrstream& oss,
                     CFlatContext& ctx)
{
    string accn;
    switch (loc.Which()) {
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        oss << "gap()";
        // Add anything to m_Intervals?
        break;

    case CSeq_loc::e_Whole:
    {
        x_AddID(loc.GetWhole(), oss, ctx, &accn);
        TSeqPos l;
        if (accn == ctx.GetAccession()) {
            l = ctx.GetHandle().GetBioseqCore()->GetInst().GetLength();
        } else {
            l = sequence::GetLength(loc.GetWhole());
        }
        oss << "1.." << l;
        x_AddInt(0, l - 1, accn);
        break;
    }

    case CSeq_loc::e_Int:
        x_Add(loc.GetInt(), oss, ctx);
        break;

    case CSeq_loc::e_Packed_int:
    {
        string delim("join(");
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            oss << delim;
            x_Add(**it, oss, ctx);
            delim = ", \b";
        }
        oss << ')';
        break;
    }

    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& pnt = loc.GetPnt();
        TSeqPos           pos = pnt.GetPoint();
        x_AddID(pnt.GetId(), oss, ctx, &accn);
        if (pnt.IsSetStrand() && IsReverse(pnt.GetStrand())) {
            oss << "complement(";
            x_AddPnt(pos, pnt.IsSetFuzz() ? &pnt.GetFuzz() : 0, oss, ctx);
            oss << ')';
            x_AddInt(pos, pos, accn, SInterval::fReversed);
        } else {
            x_AddPnt(pos, pnt.IsSetFuzz() ? &pnt.GetFuzz() : 0, oss, ctx);
            x_AddInt(pos, pos, accn);
        }
        break;
    }

    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& ppnt  = loc.GetPacked_pnt();
        SInterval::TFlags     flags = 0;
        x_AddID(ppnt.GetId(), oss, ctx, &accn);
        if (ppnt.IsSetStrand() && IsReverse(ppnt.GetStrand())) {
            oss << "complement(";
            flags = SInterval::fReversed;
        }
        string delim("join(");
        ITERATE (CPacked_seqpnt::TPoints, it, ppnt.GetPoints()) {
            oss << delim;
            x_AddPnt(*it, ppnt.IsSetFuzz() ? &ppnt.GetFuzz() : 0, oss, ctx);
            x_AddInt(*it, *it, accn, flags);
            delim = ", \b";
        }
        if (ppnt.IsSetStrand() && IsReverse(ppnt.GetStrand())) {
            oss << ")";
        }
        break;
    }

    case CSeq_loc::e_Mix:
    {
        string delim("join(");
        ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            oss << delim;
            x_Add(**it, oss, ctx);
            delim = ", \b";
        }
        oss << ')';
        break;
    }

    case CSeq_loc::e_Equiv:
        // XXX - just take the first
        x_Add(*loc.GetEquiv().Get().front(), oss, ctx);
        break;

    // XXX - bond? feat?

    default:
        NCBI_THROW(CException, eUnknown,
                   "CFlatLoc::CFlatloc: unsupported (sub)location type "
                   + NStr::IntToString(loc.Which()));
    }
}


void CFlatLoc::x_Add(const CSeq_interval& si, CNcbiOstrstream& oss,
                     CFlatContext& ctx)
{
    string            accn;
    TSeqPos           from = si.GetFrom(), to = si.GetTo();
    SInterval::TFlags flags = 0;
    x_AddID(si.GetId(), oss, ctx, &accn);
    if (si.IsSetStrand() && IsReverse(si.GetStrand())) {
        oss << "complement(";
    }
    flags |= (x_AddPnt(from, si.IsSetFuzz_from() ? &si.GetFuzz_from() : 0,
                       oss, ctx)
              & SInterval::fPartialLeft);
    oss << "..";
    flags |= (x_AddPnt(to, si.IsSetFuzz_to() ? &si.GetFuzz_to() : 0, oss, ctx)
              & SInterval::fPartialRight);
    if (si.IsSetStrand() && IsReverse(si.GetStrand())) {
        oss << ')';
        x_AddInt(to, from, accn, flags | SInterval::fReversed);
    } else {
        x_AddInt(from, to, accn, flags);
    }
}


CFlatLoc::SInterval::TFlags CFlatLoc::x_AddPnt(TSeqPos pnt,
                                               const CInt_fuzz* fuzz,
                                               CNcbiOstrstream& oss,
                                               CFlatContext& ctx)
{
    CFlatLoc::SInterval::TFlags flags = 0;
    // need to convert to 1-based coordinates
    if (fuzz == 0) {
        oss << pnt + 1;
    } else if (fuzz->IsLim()) {
        bool h = ctx.GetFormatter().DoHTML();
        switch (fuzz->GetLim()) {
        case CInt_fuzz::eLim_gt:
            flags = SInterval::fPartialRight;
            oss << (h ? "&gt;" : ">") << pnt + 1;
            break;
        case CInt_fuzz::eLim_lt:
            flags = SInterval::fPartialLeft;
            oss << (h ? "&lt;" : "<") << pnt + 1;
            break;
        case CInt_fuzz::eLim_tr: oss << pnt + 1 << '^' << pnt + 2;     break;
        case CInt_fuzz::eLim_tl: oss << pnt     << '^' << pnt + 1;     break;
        default:                 oss << pnt + 1;                       break;
        }
    } else {
        TSeqPos from = pnt, to = pnt;
        switch (fuzz->Which()) {
        case CInt_fuzz::e_P_m:
            from = pnt - fuzz->GetP_m();
            to   = pnt + fuzz->GetP_m();
            break;

        case CInt_fuzz::e_Range:
            from = fuzz->GetRange().GetMin();
            to   = fuzz->GetRange().GetMax();
            break;

        case CInt_fuzz::e_Pct: // actually per thousand...
        {
            // calculate in floating point to avoid overflow
            TSeqPos delta = static_cast<TSeqPos>(0.001 * pnt * fuzz->GetPct());
            from = pnt - delta;
            to   = pnt + delta;
            break;
        }

        case CInt_fuzz::e_Alt:
            ITERATE (CInt_fuzz::TAlt, it, fuzz->GetAlt()) {
                if (*it < from) {
                    from = *it;
                } else if (*it > to) {
                    to   = *it;
                }
            }
            break;

        default:
            break;
        }
        if (from == to) {
            oss << from + 1;
        } else {
            oss << '(' << from + 1 << '.' << to + 1 << ')';
        }
    }

    return flags;
}


void CFlatLoc::x_AddInt(TSeqPos from, TSeqPos to, const string& accn,
                        CFlatLoc::SInterval::TFlags flags)
{
    // need to convert to 1-based coordinates
    SInterval ival;
    ival.m_Accession = accn;
    ival.m_Range.SetFrom(from + 1);
    ival.m_Range.SetTo  (to + 1);
    ival.m_Flags = flags;
    m_Intervals.push_back(ival);
}


void CFlatLoc::x_AddID(const CSeq_id& id, CNcbiOstrstream& oss,
                       CFlatContext& ctx, string* s)
{
    const CSeq_id& id2 = ctx.GetPreferredSynonym(id);
    string         acc = id2.GetSeqIdString(true);
    if (&id2 != &ctx.GetPrimaryID()) {
        oss << acc << ':';
    }
    if (s) {
        *s = acc;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.9  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2003/07/17 20:27:30  vasilche
* Added check for IsSetStrand().
*
* Revision 1.7  2003/06/02 16:06:42  dicuccio
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
* Revision 1.6  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.5  2003/03/28 19:03:40  ucko
* Add flags to intervals.
*
* Revision 1.4  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.3  2003/03/11 17:00:21  ucko
* Delimit join(...) elements with ", \b" as a hint to NStr::Wrap.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
