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
*   new (early 2003) flat-file generator -- base formatter class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_items.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void IFlatFormatter::Format(const CSeq_entry& entry, IFlatItemOStream& out,
                            IFlatFormatter::TFilterFlags flags,
                            CFlatContext* ctx)
{
    CRef<CFlatContext> ctx0;
    if (ctx == 0) {
        ctx0.Reset(new CFlatContext);
        ctx = ctx0;
        ctx->SetFlags(entry, true);
    } else {
        ctx->SetFlags(entry, false);
    }

    if (entry.IsSeq()) {
        const CBioseq& seq   = entry.GetSeq();
        if (flags & (seq.IsAa() ? fSkipProteins : fSkipNucleotides)) {
            return;
        }
        Format(seq, out, ctx);
    } else {
        const CBioseq_set& bss = entry.GetSet();
        ITERATE (CBioseq_set::TSeq_set, it, bss.GetSeq_set()) {
            if (ctx->InSegSet()  &&  (*it)->IsSet()
                &&  (*it)->GetSet().GetClass() == CBioseq_set::eClass_parts) {
                // skip internal parts sets -- covered indirectly
                continue;
            }
            CRef<CFlatContext> ctx2(new CFlatContext(*ctx));
            Format(**it, out, flags, ctx2);
            if (ctx->GetSegmentCount() > 0) {
                ++ctx->m_SegmentNum;
            }
        }
    }
}



void IFlatFormatter::Format(const CBioseq& seq, IFlatItemOStream& out,
                            CFlatContext* ctx)
{
    CRef<CFlatContext> ctx0;
    if (ctx == 0) {
        ctx0.Reset(new CFlatContext);
        ctx = ctx0;
        if (seq.GetParentEntry()) {
            ctx->SetFlags(*seq.GetParentEntry(), true);
        }
    }

    // XXX - also count deltas containing external references
    bool contig = false; // put in ctx instead?
    if (seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
        if (x_FormatSegments(seq, out, *ctx)) {
            return;
        } else if (m_Style != eStyle_Master
                   ||  (m_Flags & fShowContigInMaster)) {
            contig = true;
        }
    }

    ctx->m_Formatter = const_cast<IFlatFormatter*>(this);
    ctx->m_Handle    = m_Scope->GetBioseqHandle(seq);
    ctx->m_References.clear();
    ctx->m_Mol       = seq.GetInst().GetMol();
    ctx->m_IsProt    = seq.IsAa();

    if ( !ctx->m_Location ) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetWhole().Assign(*ctx->m_Handle.GetSeqId());
        ctx->m_Location = loc;
    }

    out << new CFlatForehead(*ctx);
    out << new CFlatHead(*ctx);
    out << new CFlatKeywords(*ctx);
    if (ctx->GetSegmentCount()) {
        out << new CFlatSegment(*ctx);
    }
    out << new CFlatSource(*ctx);
    x_FormatReferences(*ctx, out);
    out << new CFlatComment(*ctx);
    if (ctx->IsTPA()) { // also some types of refseq...
        out << new CFlatPrimary(*ctx);
    }
    out << new CFlatFeatHeader;
    x_FormatFeatures(*ctx, out, true);
    if (ctx->IsWGSMaster()) {
        out << new CFlatWGSRange(*ctx);
    } else if (ctx->IsRefSeqGenome()) { // NS_
        out << new CFlatGenomeInfo(*ctx);
    } else {
        if ( !contig  ||  (m_Flags & fShowContigFeatures)
            ||  m_Style == eStyle_Master) {
            x_FormatFeatures(*ctx, out, false);
        }
        if (contig) {
            out << new CFlatContig(*ctx);
        }
        if ( !contig  ||  m_Style == eStyle_Master) {
            out << new CFlatDataHeader(*ctx);
            out << new CFlatData(*ctx);
        }
    }
    out << new CFlatTail;
}



void IFlatFormatter::Format(const CSeq_loc& loc, bool adjust_coords,
                            IFlatItemOStream& out, CFlatContext* ctx)
{
    if ( !adjust_coords ) {
        _ASSERT(sequence::IsOneBioseq(loc)); // otherwise, should split...
    }

    CBioseq_Handle h = m_Scope->GetBioseqHandle(loc);
    CRef<CFlatContext> ctx0;
    if (ctx == 0) {
        ctx0.Reset(new CFlatContext);
        ctx = ctx0;
        ctx->SetFlags(*h.GetBioseqCore()->GetParentEntry(), true);
    }
    ctx->m_Location.Reset(&loc);
    ctx->m_AdjustCoords = adjust_coords;
    Format(h.GetBioseq(), out, ctx);
}


string IFlatFormatter::ExpandTildes(const string& s, ETildeStyle style)
{
    if (style == eTilde_tilde) {
        return s;
    }
    SIZE_TYPE start = 0, tilde, length = s.size();
    string result;
    while (start < length  &&  (tilde = s.find('~', start)) != NPOS) {
        result += s.substr(start, tilde - start);
        start = tilde + 1;
        char next = start < length ? s[start] : 0;
        switch (style) {
        case eTilde_space:
            if ((start < length  &&  isdigit(next))
                ||  (start + 1 < length  &&  (next == ' '  ||  next == '(')
                     &&  isdigit(s[start + 1]))) {
                result += '~';
            } else {
                result += ' ';
            }
            break;

        case eTilde_newline:
            if (next == '~') {
                result += '~';
                ++start;
            } else {
                result += '\n';
            }
            break;

        default: // just keep it, for lack of better ideas
            result += '~';
            break;
        }
    }
    result += s.substr(start);
    return result;
}


bool IFlatFormatter::x_FormatSegments(const CBioseq& seq,
                                      IFlatItemOStream& out, CFlatContext& ctx)
{
    // Proceed iff either the style is segmented or the style is
    // normal and we have near segments
    if (m_Style != eStyle_Segment
        &&  (m_Style != eStyle_Normal  ||  !ctx.InSegSet())) {
        return false; // just treat as a normal sequence
    }

    const CSeg_ext::Tdata& segs = seq.GetInst().GetExt().GetSeg().Get();
    ctx.m_SegmentCount = 0;
    ITERATE (CSeg_ext::Tdata, it, segs) {
        if ( !(*it)->IsNull() ) {
            ++ctx.m_SegmentCount;
        }
    }

    ctx.m_SegmentNum = 1;
    ITERATE (CSeg_ext::Tdata, it, segs) {
        CRef<CFlatContext> ctx2(new CFlatContext(ctx));
        if ( !(*it)->IsNull() ) {
            Format(**it, true, out, ctx2);
            ++ctx.m_SegmentNum;
        }
    }
    return true;
}


void IFlatFormatter::x_FormatReferences(CFlatContext& ctx,
                                        IFlatItemOStream& out)
{
    typedef CRef<CFlatReference> TRefRef;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Pub);  it;  ++it) {
        ctx.m_References.push_back
            (TRefRef(new CFlatReference(it->GetPub(), 0, ctx)));
    }
    for (CFeat_CI it(ctx.GetHandle().GetScope(), ctx.GetLocation(),
                     CSeqFeatData::e_Pub);
         it;  ++it) {
        ctx.m_References.push_back
            (TRefRef(new CFlatReference(it->GetData().GetPub(),
                                        &it->GetLocation(), ctx)));
    }
    CFlatReference::Sort(ctx.m_References, ctx);
    ITERATE (vector<TRefRef>, it, ctx.m_References) {
        out << *it;
    }
}


inline
static bool operator <(const CConstRef<IFlattishFeature>& f1,
		       const CConstRef<IFlattishFeature>& f2)
{
    return *f1 < *f2;
}


void IFlatFormatter::x_FormatFeatures(CFlatContext& ctx,
                                      IFlatItemOStream& out, bool source)
{
    CScope& scope = ctx.GetHandle().GetScope();
    typedef CConstRef<IFlattishFeature> TFFRef;
    list<TFFRef> l, l2;
    // XXX -- should select according to flags; may require merging/re-sorting.
    // (Generally needs lots of additional logic, basically...)
    if (source) {
        for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
            switch (it->Which()) {
            case CSeqdesc::e_Org:
                out << new CFlattishSourceFeature(it->GetOrg(), ctx);
                break;
            case CSeqdesc::e_Source:
                out << new CFlattishSourceFeature(it->GetSource(), ctx);
                break;
            default:
                break;
            }
        }
    } else if (ctx.IsProt()) { // broaden condition?
        for (CFeat_CI it(scope, ctx.GetLocation(), CSeqFeatData::e_not_set,
                         SAnnotSelector::eOverlap_Intervals,
                         SAnnotSelector::eResolve_All, CFeat_CI::e_Product);
             it;  ++it) {
            l.push_back(TFFRef(new CFlattishFeature
                               (*it, ctx, &it->GetProduct(), true)));
        }
    }
    for (CFeat_CI it(scope, ctx.GetLocation(), CSeqFeatData::e_not_set,
                     SAnnotSelector::eOverlap_Intervals,
                     SAnnotSelector::eResolve_All);
         it;  ++it) {
        switch (it->GetData().Which()) {
        case CSeqFeatData::e_Pub:  // done as REFERENCEs
            break;
        case CSeqFeatData::e_Org:
        case CSeqFeatData::e_Biosrc:
            if (source) {
                out << new CFlattishSourceFeature(*it, ctx);
            }
            break;
        default:
            if ( !source ) {
                if (l.empty()) { // no merging to worry about
                    out << new CFlattishFeature(*it, ctx);
                } else {
                    l2.push_back(TFFRef(new CFlattishFeature(*it, ctx)));
                }
            }
            break;
        }
    }
    if ( !l.empty() ) {
        l.merge(l2);
        ITERATE (list<TFFRef>, it, l) { 
            out << *it;
        }
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
* Revision 1.8  2004/04/05 15:56:15  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.7  2003/12/02 19:21:26  ucko
* Fix a potential infinite loop in tilde expansion.
*
* Revision 1.6  2003/06/02 16:06:42  dicuccio
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
* Revision 1.5  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.4  2003/03/18 21:56:06  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.3  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2003/03/10 22:02:14  ucko
* Rename s_FFLess to operator <, due to bogus MSVC pickiness.
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
