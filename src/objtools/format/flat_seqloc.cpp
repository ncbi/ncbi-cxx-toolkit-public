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
        CBioseq_Handle bsh = seq.GetScope().GetBioseqHandle(id, CScope::eGetBioseq_Loaded);
        return bsh ? bsh.GetInst_Repr() == CSeq_inst::eRepr_virtual : false;
    }
    return false;
}


static bool s_IsVirtualSeqInt
(const CSeq_interval& seqint,
 const CBioseq_Handle& seq)
{
    return seqint.IsSetId() ?
        s_IsVirtualId(CSeq_id_Handle::GetHandle(seqint.GetId()), seq) :
        false;
}


static bool s_IsVirtualLocation(const CSeq_loc& loc, const CBioseq_Handle& seq)
{
    const CSeq_id* id = loc.GetId();
    return (id != NULL) ?
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

    const char* prefix = "join(";

    // deal with complement of entire location
    if ( type == eType_location ) {
        if ( show_comp  &&  GetStrand(loc, &scope) == eNa_strand_minus ) {
            CRef<CSeq_loc> rev_loc(SeqLocRevCmp(loc, &scope));
            oss << "complement(";
            x_Add(*rev_loc, oss, ctx, type, false);
            oss << ')';
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
        const char* delim = "";
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp)) {
                delim = "";
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
        ENa_strand strand = ppnt.IsSetStrand() ? ppnt.GetStrand() : eNa_strand_unknown;
        x_AddID(ppnt.GetId(), oss, ctx, type);
        if (strand == eNa_strand_minus  &&  show_comp) {
            oss << "complement(";
        }
        oss << "join(";
        const char* delim = "";
        ITERATE (CPacked_seqpnt::TPoints, it, ppnt.GetPoints()) {
            oss << delim;
            const CInt_fuzz* fuzz = ppnt.CanGetFuzz() ? &ppnt.GetFuzz() : 0;
            if (!x_Add(*it, fuzz, oss, ctx.Config().DoHTML())) {
                delim = "";
            } else {
                delim = ", \b";
            }
        }
        if (strand == eNa_strand_minus  &&  show_comp) {
            oss << ")";
        }
        break;
    }}
    case CSeq_loc::e_Mix:
    {{
        const char* delim = "";
        oss << prefix;
        CSeq_loc_CI::EEmptyFlag empty = ((type == eType_location) ?
            CSeq_loc_CI::eEmpty_Skip : CSeq_loc_CI::eEmpty_Allow);
        for ( CSeq_loc_CI it(loc, empty); it; ++it ) {
            oss << delim;
            if (!x_Add(it.GetSeq_loc(), oss, ctx, type, show_comp)) {
                delim = "";
            } else {
                delim = ", \b";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Equiv:
    {{
        const char* delim = "";
        oss << "one-of(";
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp)) {
                delim = "";
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

    if (type == eType_location  &&
        s_IsVirtualId(CSeq_id_Handle::GetHandle(si.GetId()), ctx.GetHandle()) ) {
        return false;
    }
    if (comp) {
        oss << "complement(";
    }
    x_AddID(si.GetId(), oss, ctx, type);
    x_Add(from, si.IsSetFuzz_from() ? &si.GetFuzz_from() : 0, oss, do_html);
    if (to > 0  &&
        (from != to  ||  si.IsSetFuzz_from()  ||  si.IsSetFuzz_to())) {
        oss << "..";
        x_Add(to, si.IsSetFuzz_to() ? &si.GetFuzz_to() : 0, oss, do_html);
    }
    if (comp) {
        oss << ')';
    }

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
                    oss << (html ? "&lt" : "<") << ((pnt > 3) ? pnt : 1);
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
        idp.Reset();
    }
    if (!idp) {
        idp.Reset(&id);
    }
    switch ( idp->Which() ) {
    default:
        oss << idp->GetSeqIdString(true) << ':';
        break;
    case CSeq_id::e_Gi:
        oss << "gi|" << idp->GetSeqIdString(true) << ':';
        break;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
