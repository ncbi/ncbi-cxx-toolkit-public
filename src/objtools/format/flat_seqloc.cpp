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
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objtools/format/items/flat_seqloc.hpp>
#include <objtools/format/context.hpp>
#include <algorithm>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


static bool s_IsVirtualId(const CSeq_id_Handle& id, const CBioseq_Handle& seq)
{
    if (!id  ||  !seq) {
        return true;
    }
    CBioseq_Handle::TId ids = seq.GetId();
    if (find(ids.begin(), ids.end(), id) == ids.end()) {
        CBioseq_Handle bsh = seq.GetScope().GetBioseqHandleFromTSE(id, seq);
        return bsh ? bsh.GetInst_Repr() == CSeq_inst::eRepr_virtual : false;
    }
    return false;
}


static bool s_IsVirtualSeqInt
(const CSeq_interval& seqint,
 const CBioseq_Handle& seq)
{
    return seqint.CanGetId() ?
        s_IsVirtualId(CSeq_id_Handle::GetHandle(seqint.GetId()), seq) :
        false;
}


static bool s_IsVirtualLocation(const CSeq_loc& loc, const CBioseq_Handle& seq)
{
    const CSeq_id* id = 0;
    try {
        switch (loc.Which()) {
        case CSeq_loc::e_Whole:
            id = &loc.GetWhole();
            break;
        case CSeq_loc::e_Int:
            id = &loc.GetInt().GetId();
            break;
        case CSeq_loc::e_Pnt:
            id = &loc.GetPnt().GetId();
            break;
        default:
            break;
        }
    } catch (CException&) {
        id = 0;
    }

    return (id != 0) ?
        s_IsVirtualId(CSeq_id_Handle::GetHandle(*id), seq) :
        false;
}


CFlatSeqLoc::CFlatSeqLoc
(const CSeq_loc& loc,
 CBioseqContext& ctx,
 TType type)
{
    CNcbiOstrstream oss;
    x_Add(loc, oss, ctx, type, true);
    m_String = CNcbiOstrstreamToString(oss);
}


bool CFlatSeqLoc::x_Add
(const CSeq_loc& loc,
 CNcbiOstrstream& oss,
 CBioseqContext& ctx,
 TType type,
 bool show_comp)
{
    CScope& scope = ctx.GetScope();
    const CBioseq_Handle& seq = ctx.GetHandle();

    string prefix = "join(";

    // deal with complement of entire location
    if ( type == eType_location ) {
        if ( show_comp  &&  GetStrand(loc, &scope) == eNa_strand_minus ) {
            CRef<CSeq_loc> rev_loc(SeqLocRevCmp(loc, &scope));
            oss << "complement(";
            x_Add(*rev_loc, oss, ctx, type, false);
            oss << ")";
            return true;
        }

        if ( loc.IsMix() ) {
            ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
                if ( (*it)->IsNull()  ||  s_IsVirtualLocation(**it, seq) ) {
                    prefix = "order(";
                    break;
                }
            }
        } else if ( loc.IsPacked_int() ) {
            ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
                if ( s_IsVirtualSeqInt(**it, seq) ) {
                    prefix = "order(";
                    break;
                }
            }
        }
    }

    // handle each location component
    switch ( loc.Which() ) {
    case CSeq_loc::e_Null:
    {{
        const CFlatGapLoc* gap = dynamic_cast<const CFlatGapLoc*>(&loc);
        if (gap == 0) {
            oss << "gap()";
        } else {
            oss << "gap(" << gap->GetLength() << ")";
        }
        break;
    }}
    case CSeq_loc::e_Empty:
    {{
        oss << "gap()";
        break;
    }}
    case CSeq_loc::e_Whole:
    {{
        x_AddID(loc.GetWhole(), oss, ctx, type);
        TSeqPos len = sequence::GetLength(loc, &scope);
        oss << "1";
        if (len > 1) {
            oss << ".." << len;
        }
        break;
    }}
    case CSeq_loc::e_Int:
    {{
        return x_Add(loc.GetInt(), oss, ctx, type, show_comp);
    }}
    case CSeq_loc::e_Packed_int:
    {{
        oss << prefix;
        string delim;
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp)) {
                delim = kEmptyStr;
            } else {
                delim = ", \b";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Pnt:
    {{
        return x_Add(loc.GetPnt(), oss, ctx, type, show_comp);
    }}
    case CSeq_loc::e_Packed_pnt:
    {{
        const CPacked_seqpnt& ppnt  = loc.GetPacked_pnt();
        x_AddID(ppnt.GetId(), oss, ctx, type);
        if (ppnt.IsSetStrand() && ppnt.GetStrand() == eNa_strand_minus  &&  show_comp) {
            oss << "complement(";
        }
        oss << "join(";
        string delim;
        ITERATE (CPacked_seqpnt::TPoints, it, ppnt.GetPoints()) {
            oss << delim;
            const CInt_fuzz* fuzz = ppnt.CanGetFuzz() ? &ppnt.GetFuzz() : 0;
            if (!x_Add(*it, fuzz, oss, ctx.Config().DoHTML())) {
                delim = kEmptyStr;
            } else {
                delim = ", \b";
            }
        }
        if (ppnt.IsSetStrand() && IsReverse(ppnt.GetStrand())  &&  show_comp) {
            oss << ")";
        }
        break;
    }}
    case CSeq_loc::e_Mix:
    {{
        string delim;
        oss << prefix;
        CSeq_loc_CI::EEmptyFlag empty = ((type == eType_location) ?
            CSeq_loc_CI::eEmpty_Skip : CSeq_loc_CI::eEmpty_Allow);
        for ( CSeq_loc_CI it(loc, empty); it; ++it ) {
            oss << delim;
            if (!x_Add(it.GetSeq_loc(), oss, ctx, type, show_comp)) {
                delim = kEmptyStr;
            } else {
                delim = ", \b";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Equiv:
    {{
        string delim;
        oss << "one-of(";
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp)) {
                delim = kEmptyStr;
            } else {
                delim = ", \b";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Bond:
    {{
        const CSeq_bond& bond = loc.GetBond();
        if ( !bond.CanGetA() ) {
            return false;
        }
        oss << "bond(";
        x_Add(bond.GetA(), oss, ctx, type, show_comp);
        if ( bond.CanGetB() ) {
            oss << ", \b";
            x_Add(bond.GetB(), oss, ctx, type, show_comp);
        }
        oss << ")";
    }}
    case CSeq_loc::e_Feat:
    default:
        return false;
        /*NCBI_THROW(CException, eUnknown,
                   "CFlatSeqLoc::CFlatSeqLoc: unsupported (sub)location type "
                   + NStr::IntToString(loc.Which()));*/
    } // end of switch statement

    return true;
}


bool CFlatSeqLoc::x_Add
(const CSeq_interval& si,
 CNcbiOstrstream& oss,
 CBioseqContext& ctx,
 TType type,
 bool show_comp)
{
    bool do_html = ctx.Config().DoHTML();

    TSeqPos from = si.GetFrom(), to = si.GetTo();
    ENa_strand strand = si.CanGetStrand() ? si.GetStrand() : eNa_strand_unknown;
    bool comp = show_comp  &&  (strand == eNa_strand_minus);

    if ( type == eType_location ) {
        if ( s_IsVirtualId(CSeq_id_Handle::GetHandle(si.GetId()), ctx.GetHandle()) ) {
            return false;
        }
        oss << (comp ? "complement(" : kEmptyStr);
        x_AddID(si.GetId(), oss, ctx, type);
    } else {
        oss << (comp ? "complement(" : kEmptyStr);
        x_AddID(si.GetId(), oss, ctx, type);
    }
    x_Add(from, si.IsSetFuzz_from() ? &si.GetFuzz_from() : 0, oss, do_html);
    if (to > 0  &&
        (from != to  ||  si.IsSetFuzz_from()  ||  si.IsSetFuzz_to())) {
        oss << "..";
        x_Add(to, si.IsSetFuzz_to() ? &si.GetFuzz_to() : 0, oss, do_html);
    }
    oss << (comp ? ")" : kEmptyStr);

    return true;
}


bool CFlatSeqLoc::x_Add
(const CSeq_point& pnt,
 CNcbiOstrstream& oss,
 CBioseqContext& ctx,
 TType type,
 bool show_comp)
{
    if ( !pnt.CanGetPoint() ) {
        return false;
    }

    bool do_html = ctx.Config().DoHTML();

    TSeqPos pos = pnt.GetPoint();
    x_AddID(pnt.GetId(), oss, ctx, type);
    if ( pnt.IsSetStrand()  &&  IsReverse(pnt.GetStrand())  &&  show_comp ) {
        oss << "complement(";
        x_Add(pos, pnt.IsSetFuzz() ? &pnt.GetFuzz() : 0, oss, do_html);
        oss << ')';
    } else {
        x_Add(pos, pnt.IsSetFuzz() ? &pnt.GetFuzz() : 0, oss, do_html);
    }

    return true;
}


bool CFlatSeqLoc::x_Add
(TSeqPos pnt,
 const CInt_fuzz* fuzz,
 CNcbiOstrstream& oss,
 bool html)
{
    // need to convert to 1-based coordinates
    pnt += 1;

    if ( fuzz != 0 ) {
        switch ( fuzz->Which() ) {
        case CInt_fuzz::e_P_m:
            {
                oss << '(' << pnt - fuzz->GetP_m() << '.' 
                    << pnt + fuzz->GetP_m() << ')';
                break;
            }
        case CInt_fuzz::e_Range:
            {
                oss << '(' << (fuzz->GetRange().GetMin() + 1)
                    << '.' << (fuzz->GetRange().GetMax() + 1) << ')';
                break;
            }
        case CInt_fuzz::e_Pct: // actually per thousand...
            {
                // calculate in floating point to avoid overflow
                TSeqPos delta = 
                    static_cast<TSeqPos>(0.001 * pnt * fuzz->GetPct());
                oss << '(' << pnt - delta << '.' << pnt + delta << ')';
                break;
            }
        case CInt_fuzz::e_Lim:
            {
                switch ( fuzz->GetLim() ) {
                case CInt_fuzz::eLim_gt:
                    oss << (html ? "&gt" : ">") << pnt;
                    break;
                case CInt_fuzz::eLim_lt:
                    oss << (html ? "&lt" : "<") << pnt;
                    break;
                case CInt_fuzz::eLim_tr:
                    oss << pnt << '^' << pnt + 1;
                    break;
                case CInt_fuzz::eLim_tl:
                    oss << pnt - 1 << '^' << pnt;
                    break;
                default:
                    oss << pnt;
                    break;
                }
                break;
            }
        default:
            {
                oss << pnt;
                break;
            }
        } // end of switch statement
    } else {
        oss << pnt;
    }

    return true;
}


void CFlatSeqLoc::x_AddID
(const CSeq_id& id,
 CNcbiOstrstream& oss,
 CBioseqContext& ctx,
 TType type)
{
    if (ctx.GetHandle().IsSynonym(id)) {
        if ( type == eType_assembly ) {
            oss << ctx.GetAccession() << ':';
        }
        return;
    }

    CConstRef<CSeq_id> idp;
    try {
        idp = GetId(id, ctx.GetScope(), eGetId_ForceAcc).GetSeqId();
    } catch (CException&) {
        idp.Reset(NULL);
    }
    if (!idp) {
        idp.Reset(&id);
    }

    oss << idp->GetSeqIdString(true) << ':';
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.17  2005/02/17 15:58:42  grichenk
* Changes sequence::GetId() to return CSeq_id_Handle
*
* Revision 1.16  2004/11/30 17:39:13  shomrat
* Handle whole locations of length 1
*
* Revision 1.15  2004/11/15 20:08:08  shomrat
* Cleanup of ID formatting
*
* Revision 1.14  2004/10/18 18:47:36  shomrat
* Use sequence::GetId
*
* Revision 1.13  2004/10/05 15:40:02  shomrat
* Use CScope::GetIds
*
* Revision 1.12  2004/08/30 13:39:13  shomrat
* get accession for GI
*
* Revision 1.11  2004/08/19 16:30:24  shomrat
* Fixed formatting
*
* Revision 1.10  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.9  2004/05/20 13:45:43  shomrat
* fixed formatting of fuzz-range
*
* Revision 1.8  2004/04/22 15:58:01  shomrat
* Changes in context
*
* Revision 1.7  2004/03/26 17:24:14  shomrat
* Skip gaps when formatting a location
*
* Revision 1.6  2004/03/02 17:50:14  shomrat
* Bug fix (missing break)
*
* Revision 1.5  2004/02/19 18:10:34  shomrat
* added genome assembly formmating and some fixes
*
* Revision 1.4  2004/02/11 16:51:06  shomrat
* use lenght from bioseq-handle
*
* Revision 1.3  2004/01/14 16:14:01  shomrat
* minor fixes
*
* Revision 1.2  2003/12/18 17:43:33  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:21:15  shomrat
* Initial Revision (adapted from flat lib)
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
