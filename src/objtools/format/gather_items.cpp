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
*          Mati Shomrat, NCBI
*
* File Description:
*   
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/align_ci.hpp>

#include <algorithm>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/embl_gather.hpp>
#include <objtools/format/gff_gather.hpp>
#include <objtools/format/ftable_gather.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// Public:

// "virtual constructor"
CFlatGatherer* CFlatGatherer::New(CFlatFileConfig::TFormat format)
{
    switch ( format ) {
    case CFlatFileConfig::eFormat_GenBank:
    case CFlatFileConfig::eFormat_GBSeq:
        //case CFlatFileGenerator<>::eFormat_Index:
        return new CGenbankGatherer;
        
    case CFlatFileConfig::eFormat_EMBL:
        return new CEmblGatherer;

    case CFlatFileConfig::eFormat_GFF:
    case CFlatFileConfig::eFormat_GFF3:
        return new CGFFGatherer;
    
    case CFlatFileConfig::eFormat_FTable:
        return new CFtableGatherer;

    case CFlatFileConfig::eFormat_DDBJ:
    default:
        NCBI_THROW(CFlatException, eNotSupported, 
            "This format is currently not supported");
    }

    return 0;
}


void CFlatGatherer::Gather(CFlatFileContext& ctx, CFlatItemOStream& os) const
{
    m_ItemOS.Reset(&os);
    m_Context.Reset(&ctx);

    os << new CStartItem();
    x_GatherSeqEntry(ctx.GetEntry());
    os << new CEndItem();
}


CFlatGatherer::~CFlatGatherer(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//
// Protected:


void CFlatGatherer::x_GatherSeqEntry(const CSeq_entry_Handle& entry) const
{
    if ( entry.IsSet()  &&  entry.GetSet().IsSetClass() ) {
        CBioseq_set::TClass clss = entry.GetSet().GetClass();
        if ( clss == CBioseq_set::eClass_genbank  ||
             clss == CBioseq_set::eClass_mut_set  ||
             clss == CBioseq_set::eClass_pop_set  ||
             clss == CBioseq_set::eClass_phy_set  ||
             clss == CBioseq_set::eClass_eco_set  ||
             clss == CBioseq_set::eClass_wgs_set  ||
             clss == CBioseq_set::eClass_gen_prod_set ) {
            for ( CSeq_entry_CI it(entry); it; ++it ) {
                x_GatherSeqEntry(*it);
            }
            return;
        }
    }

    // visit each bioseq in the entry (excluding segments)
    CBioseq_CI seq_iter(entry, CSeq_inst::eMol_not_set,
        CBioseq_CI::eLevel_Mains);
    for ( ; seq_iter; ++seq_iter ) {
        if ( x_DisplayBioseq(entry, *seq_iter) ) {
            x_GatherBioseq(*seq_iter);
        }
    }
} 


bool CFlatGatherer::x_DisplayBioseq
(const CSeq_entry_Handle& entry,
 const CBioseq_Handle& seq) const
{
    const CFlatFileConfig& cfg = Config();

    const CSeq_id& id = GetId(seq, eGetId_Best);
    if ( id.IsLocal()  &&  cfg.SuppressLocalId() ) {
        return false;
    }

    if ( (CSeq_inst::IsNa(seq.GetInst_Mol())  &&  cfg.IsViewNuc())  ||
         (CSeq_inst::IsAa(seq.GetInst_Mol())  &&  cfg.IsViewProt()) ) {
        return true;
    }

    return false;
}


static bool s_IsSegmented(const CBioseq_Handle& seq)
{
    return seq  &&
           seq.IsSetInst()  &&
           seq.IsSetInst_Repr()  &&
           seq.GetInst_Repr() == CSeq_inst::eRepr_seg;
}


static bool s_HasSegments(const CBioseq_Handle& seq)
{
    CSeq_entry_Handle h =
        seq.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if (h) {
        for (CSeq_entry_CI it(h); it; ++it) {
            if (it->IsSet()  &&  it->GetSet().IsSetClass()  &&
                it->GetSet().GetClass() == CBioseq_set::eClass_parts) {
                return true;
            }
        }
    }
    return false;
}


// a defualt implementation for GenBank /  DDBJ formats
void CFlatGatherer::x_GatherBioseq(const CBioseq_Handle& seq) const
{
    const CFlatFileConfig& cfg = Config();

    // Do multiple sections (segmented style) if:
    // a. the bioseq is segmented and has near parts
    // b. style is normal or segmented (not master)
    // c. user didn't specify a location
    // d. not FTable format
    if ( s_IsSegmented(seq)  &&  s_HasSegments(seq)       &&
         (cfg.IsStyleNormal()  ||  cfg.IsStyleSegment())  &&
         (m_Context->GetLocation() == 0)                  &&
         !cfg.IsFormatFTable() ) {
        x_DoMultipleSections(seq);
    } else {
        // display as a single bioseq (single section)
        m_Current.Reset(new CBioseqContext(seq, *m_Context));
        m_Context->AddSection(m_Current);
        x_DoSingleSection(*m_Current);
    }   
}


void CFlatGatherer::x_DoMultipleSections(const CBioseq_Handle& seq) const
{
    CRef<CMasterContext> mctx(new CMasterContext(seq));

    CScope* scope = &seq.GetScope();
    const CSeqMap& seqmap = seq.GetSeqMap();

    CSeqMap_CI it = seqmap.BeginResolved(scope, 1, CSeqMap::fFindRef);
    while ( it ) {
        CSeq_id_Handle id = it.GetRefSeqid();
        CBioseq_Handle part = scope->GetBioseqHandleFromTSE(id, seq);
        if (part) {
            // do only non-virtual parts
            CSeq_inst::TRepr repr = part.IsSetInst_Repr() ?
                part.GetInst_Repr() : CSeq_inst::eRepr_not_set;
            if (repr != CSeq_inst::eRepr_virtual) {
                m_Current.Reset(new CBioseqContext(part, *m_Context, mctx));
                m_Context->AddSection(m_Current);
                x_DoSingleSection(*m_Current);
            }
        }
        ++it;
    }
}

    
/////////////////////////////////////////////////////////////////////////////
//
// REFERENCES

bool s_FilterPubdesc(const CPubdesc& pubdesc, CBioseqContext& ctx)
{
    if ( pubdesc.CanGetComment() ) {
        const string& comment = pubdesc.GetComment();
        bool is_gene_rif = NStr::StartsWith(comment, "GeneRIF", NStr::eNocase);

        const CFlatFileConfig& cfg = ctx.Config();
        if ( (cfg.HideGeneRIFs()  &&  is_gene_rif)  ||
             ((cfg.OnlyGeneRIFs()  ||  cfg.LatestGeneRIFs())  &&  !is_gene_rif) ) {
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_GatherReferences(void) const
{
    CBioseqContext::TReferences& refs = m_Current->SetReferences();

    // gather references from descriptors
    for (CSeqdesc_CI it(m_Current->GetHandle(), CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }

    // gather references from features
    SAnnotSelector sel(CSeqFeatData::e_Pub);
    CFeat_CI it(m_Current->GetScope(), m_Current->GetLocation(), sel);
    for ( ; it; ++it) {
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(it->GetData().GetPub(),
                                        *m_Current, &it->GetLocation())));
    }
    CReferenceItem::Rearrange(refs, *m_Current);
    ITERATE (CBioseqContext::TReferences, ref, refs) {
        *m_ItemOS << *ref;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// COMMENTS


void CFlatGatherer::x_GatherComments(void) const
{
    CBioseqContext& ctx = *m_Current;

    // Gather comments related to the seq-id
    x_IdComments(ctx);
    x_RefSeqComments(ctx);

    if ( CCommentItem::NsAreGaps(ctx.GetHandle(), ctx) ) {
        x_AddComment(new CCommentItem(CCommentItem::GetNsAreGapsStr(), ctx));
    }

    x_HistoryComments(ctx);
    x_WGSComment(ctx);
    if ( ctx.ShowGBBSource() ) {
        x_GBBSourceComment(ctx);
    }
    x_DescComments(ctx);
    x_MaplocComments(ctx);
    x_RegionComments(ctx);
    x_HTGSComments(ctx);
    x_FeatComments(ctx);

    x_FlushComments();
}


void CFlatGatherer::x_AddComment(CCommentItem* comment) const
{
    CRef<CCommentItem> com(comment);
    if ( !com->Skip() ) {
        m_Comments.push_back(com);
    }
}


void CFlatGatherer::x_AddGSDBComment
(const CDbtag& dbtag,
 CBioseqContext& ctx) const
{
    CRef<CCommentItem> gsdb_comment(new CGsdbComment(dbtag, ctx));
    if ( !gsdb_comment->Skip() ) {
        m_Comments.push_back(gsdb_comment);
    }
}


void CFlatGatherer::x_FlushComments(void) const
{
    if ( m_Comments.empty() ) {
        return;
    }
    // add a period to the last comment (if not local id)
    if ( dynamic_cast<CLocalIdComment*>(&*m_Comments.back()) == 0 ) {
        m_Comments.back()->AddPeriod();
    }
    
    // add a period to a GSDB comment (if exist and not last)
    TCommentVec::iterator last = m_Comments.end();
    --last;
    NON_CONST_ITERATE (TCommentVec, it, m_Comments) {
        CGsdbComment* gsdb = dynamic_cast<CGsdbComment*>(it->GetPointerOrNull());
        if ( gsdb != 0   &&  it != last ) {
            gsdb->AddPeriod();
        }
        *m_ItemOS << *it;
    }

    m_Comments.clear();
}


string s_GetGenomeBuildNumber(const CBioseq_Handle& bsh)
{
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        if ( uo.IsSetType()  &&  uo.GetType().IsStr()  &&
             uo.GetType().GetStr() == "GenomeBuild" ) {
            if ( uo.HasField("NcbiAnnotation") ) {
                const CUser_field& uf = uo.GetField("NcbiAnnotation");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                     !uf.GetData().GetStr().empty() ) {
                    return uf.GetData().GetStr();
                }
            } else if ( uo.HasField("Annotation") ) {
                const CUser_field& uf = uo.GetField("Annotation");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                     !uf.GetData().GetStr().empty() ) {
                    static const string prefix = "NCBI build ";
                    if ( NStr::StartsWith(uf.GetData().GetStr(), prefix) ) {
                        return uf.GetData().GetStr().substr(prefix.length());
                    }
                }
            }
        }
    }

    return kEmptyStr;
}


bool s_HasRefTrackStatus(const CBioseq_Handle& bsh) {
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        CCommentItem::TRefTrackStatus status = 
            CCommentItem::GetRefTrackStatus(it->GetUser());
        if ( status != CCommentItem::eRefTrackStatus_Unknown ) { 
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_IdComments(CBioseqContext& ctx) const
{
    const CObject_id* local_id = 0;

    string genome_build_number = s_GetGenomeBuildNumber(ctx.GetHandle());
    bool has_ref_track_status = s_HasRefTrackStatus(ctx.GetHandle());

    ITERATE( CBioseq::TId, id_iter, ctx.GetBioseqIds() ) {
        const CSeq_id& id = **id_iter;

        switch ( id.Which() ) {
        case CSeq_id::e_Other:
            {{
                if ( ctx.IsRSCompleteGenomic() ) {
                    if ( !genome_build_number.empty() ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSContig()  ||  ctx.IsRSIntermedWGS() ) {
                    if ( !has_ref_track_status ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSPredictedProtein()  ||
                     ctx.IsRSPredictedMRna()     ||
                     ctx.IsRSPredictedNCRna()    ||
                     ctx.IsRSWGSProt() ) {
                    SModelEvidance me;
                    if ( GetModelEvidance(ctx.GetHandle(), me) ) {
                        string str = CCommentItem::GetStringForModelEvidance(me);
                        if ( !str.empty() ) {
                            x_AddComment(new CCommentItem(str, ctx));
                        }
                    }
                }
            }}
            break;
        case CSeq_id::e_General:
            {{
                const CDbtag& dbtag = id.GetGeneral();
                if ( dbtag.CanGetDb()  &&  dbtag.GetDb() == "GSDB"  &&
                     dbtag.CanGetTag()  &&  dbtag.GetTag().IsId() ) {
                    x_AddGSDBComment(dbtag, ctx);
                }
            }}
            break;
        case CSeq_id::e_Local:
            {{
                local_id = &(id.GetLocal());
            }}
            break;
        default:
            break;
        }
    }

    if ( local_id != 0 ) {
        if ( ctx.IsTPA()  ||  ctx.IsGED() ) {
            if ( ctx.Config().IsModeGBench()  ||  ctx.Config().IsModeDump() ) {
                x_AddComment(new CLocalIdComment(*local_id, ctx));
            }
        }
    }
}


void CFlatGatherer::x_RefSeqComments(CBioseqContext& ctx) const
{
    bool did_tpa = false, did_ref_track = false, did_genome = false;

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();

        // TPA
        {{
            if ( !did_tpa ) {
                string str = CCommentItem::GetStringForTPA(uo, ctx);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                    did_tpa = true;
                }
            }
        }}

        // BankIt
        {{
            if ( !ctx.Config().HideBankItComment() ) {
                string str = CCommentItem::GetStringForBankIt(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                }
            }
        }}

        // RefTrack
        {{
            if ( !did_ref_track ) {
                string str = CCommentItem::GetStringForRefTrack(uo);
                if ( !str.empty() ) {

                    x_AddComment(new CCommentItem(str, ctx, &uo));
                    did_ref_track = true;
                }
            }
        }}

        // Genome
        {{
            if ( !did_genome ) {
                // !!! Not implememnted in the C version. should it be?
            }
        }}
    }
}


void CFlatGatherer::x_HistoryComments(CBioseqContext& ctx) const
{
    const CBioseq_Handle& seq = ctx.GetHandle();
    if ( !seq.IsSetInst_Hist() ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst_Hist();

    if ( hist.CanGetReplaced_by() ) {
        const CSeq_hist::TReplaced_by& r = hist.GetReplaced_by();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaced_by,
                hist, ctx));
        }
    }

    if ( hist.IsSetReplaces()  &&  !ctx.Config().IsModeGBench() ) {
        const CSeq_hist::TReplaces& r = hist.GetReplaces();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaces,
                hist, ctx));
        }
    }
}


void CFlatGatherer::x_WGSComment(CBioseqContext& ctx) const
{
    if ( !ctx.IsWGSMaster()  ||  ctx.GetWGSMasterName().empty() ) {
        return;
    }

    if ( ctx.GetTech() == CMolInfo::eTech_wgs ) {
        string str = CCommentItem::GetStringForWGS(ctx);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx));
        }
    }
}


void CFlatGatherer::x_GBBSourceComment(CBioseqContext& ctx) const
{
    if (!ctx.ShowGBBSource()) {
        return;
    }

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Genbank); it; ++it) {
        const CGB_block& gbb = it->GetGenbank();
        if ( gbb.CanGetSource()  &&  !gbb.GetSource().empty() ) {
            x_AddComment(new CCommentItem(
                "Original source text: " + gbb.GetSource(),
                ctx,
                &(*it)));
        }
    }

}


void CFlatGatherer::x_DescComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Comment); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_MaplocComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Maploc); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_RegionComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Region); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_HTGSComments(CBioseqContext& ctx) const
{
    CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    if ( !desc ) {
        return;
    }
    const CMolInfo& mi = *ctx.GetMolinfo();

    if ( ctx.IsRefSeq()  &&  
         mi.GetCompleteness() != CMolInfo::eCompleteness_unknown ) {
        string str = CCommentItem::GetStringForMolinfo(mi, ctx);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx, &(*desc)));
        }
    }

    CMolInfo::TTech tech = mi.GetTech();
    if ( tech == CMolInfo::eTech_htgs_0  ||
         tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2 ) {
        x_AddComment(new CCommentItem(
            CCommentItem::GetStringForHTGS(ctx), ctx, &(*desc)));
    } else {
        const string& tech_str = GetTechString(tech);
        if ( !tech_str.empty() ) {
            x_AddComment(new CCommentItem("Method: " + tech_str, ctx, &(*desc)));
        }
    }
}


// add comment features that are full length on appropriate segment
void CFlatGatherer::x_FeatComments(CBioseqContext& ctx) const
{
    CScope *scope = &ctx.GetScope();
    const CSeq_loc& loc = ctx.GetLocation();

    for (CFeat_CI it(ctx.GetScope(), loc, CSeqFeatData::e_Comment); it; ++it) {
        ECompare comp = Compare(it->GetLocation(), loc, scope);

        if ((comp == eSame)  ||  (comp == eContains)) {
            x_AddComment(new CCommentItem(it->GetOriginalFeature(), ctx));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

// We use multiple items to represent the sequence.
void CFlatGatherer::x_GatherSequence(void) const
{
    static const TSeqPos kChunkSize = 2400; // 20 lines
    
    bool first = true;
    TSeqPos size = GetLength(m_Current->GetLocation(), &m_Current->GetScope());
    for ( TSeqPos start = 0; start < size; start += kChunkSize ) {
        TSeqPos end = min(start + kChunkSize, size);
        *m_ItemOS << new CSequenceItem(start, end, first, *m_Current);
        first = false;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// FEATURES


// source

void CFlatGatherer::x_CollectSourceDescriptors
(const CBioseq_Handle& bh,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    CRef<CSourceFeatureItem> sf;
    CScope* scope = &ctx.GetScope();
    const CSeq_loc& loc = ctx.GetLocation();

    TRange print_range(0, GetLength(loc, scope) - 1);

    for ( CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        sf.Reset(new CSourceFeatureItem(dit->GetSource(), print_range, ctx));
        srcs.push_back(sf);
    }
    
    // if segmented collect descriptors from segments
    if ( ctx.IsSegmented() ) {
        const CBioseq& seq = *ctx.GetHandle().GetBioseqCore();
        CConstRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(seq);
              
        // iterate over segments
        CSeqMap_CI smit(seq_map->BeginResolved(scope, 1, CSeqMap::fFindRef));
        for ( ; smit; ++smit ) {
            CBioseq_Handle segh = scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(), ctx.GetHandle());
            if ( !segh  ||  !segh.IsSetDescr() ) {
                continue;
            }

            CRange<TSeqPos> seg_range(smit.GetPosition(), smit.GetEndPosition());
            // collect descriptors only from the segment 
            ITERATE(CBioseq_Handle::TDescr::Tdata, it, segh.GetDescr().Get()) {
                if ( (*it)->IsSource() ) {
                    sf.Reset(new CSourceFeatureItem((*it)->GetSource(), seg_range, ctx));
                    srcs.push_back(sf);
                }
            }
        }
    }
}


void CFlatGatherer::x_CollectSourceFeatures
(const CBioseq_Handle& bh,
 const TRange& range,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    SAnnotSelector as;
    as.SetFeatType(CSeqFeatData::e_Biosrc);
    as.SetOverlapIntervals();
    as.SetResolveDepth(1);  // in case segmented
    as.SetNoMapping(false);

    for ( CFeat_CI fi(bh, range.GetFrom(), range.GetTo(), as); fi; ++fi ) {
        TSeqPos stop = fi->GetLocation().GetTotalRange().GetTo();
        if ( stop >= range.GetFrom()  &&  stop  <= range.GetTo() ) {
            CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*fi, ctx));
            srcs.push_back(sf);
        }
    }
}


void CFlatGatherer::x_CollectBioSourcesOnBioseq
(const CBioseq_Handle& bh,
 const TRange& range,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    const CFlatFileConfig& cfg = ctx.Config();

    // collect biosources descriptors on bioseq
    if ( !cfg.IsFormatFTable()  ||  cfg.IsModeDump() ) {
        x_CollectSourceDescriptors(bh, ctx, srcs);
    }

    // collect biosources features on bioseq
    if ( !ctx.DoContigStyle()  ||  cfg.ShowContigSources() ) {
        x_CollectSourceFeatures(bh, range, ctx, srcs);
    }
}


void CFlatGatherer::x_CollectBioSources(TSourceFeatSet& srcs) const
{
    CBioseqContext& ctx = *m_Current;
    CScope* scope = &ctx.GetScope();
    const CFlatFileConfig& cfg = ctx.Config();

    x_CollectBioSourcesOnBioseq(ctx.GetHandle(),
                                ctx.GetLocation().GetTotalRange(),
                                ctx,
                                srcs);
    
    // if protein with no sources, get sources applicable to DNA location of CDS
    if ( srcs.empty()  &&  ctx.IsProt() ) {
        const CSeq_feat* cds = GetCDSForProduct(ctx.GetHandle());
        if ( cds != 0 ) {
            const CSeq_loc& cds_loc = cds->GetLocation();
            x_CollectBioSourcesOnBioseq(
                scope->GetBioseqHandle(cds_loc),
                cds_loc.GetTotalRange(),
                ctx,
                srcs);
        }
    }

    // if no source found create one (only if not FTable format or Dump mode)
    if ( srcs.empty()  &&  !cfg.IsFormatFTable()  ||  cfg.IsModeDump() ) {
        CRef<CBioSource> bsrc(new CBioSource);
        CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*bsrc, CRange<TSeqPos>::GetWhole(), ctx));
        srcs.push_back(sf);
    }
}



void CFlatGatherer::x_MergeEqualBioSources(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }
    // !!! To Do:
    // !!! * sort based on biosource
    // !!! * merge equal biosources (merge locations)
}


void CFlatGatherer::x_SubtractFromFocus(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }
    // !!! To Do:
    // !!! * implement SeqLocSubtract
}


struct SSortByLoc
{
    bool operator()(const CRef<CSourceFeatureItem>& sfp1,
                    const CRef<CSourceFeatureItem>& sfp2) 
    {
        // descriptor always goes first
        if (sfp1->WasDesc()  &&  !sfp2->WasDesc()) {
            return true;
        } else if (!sfp1->WasDesc()  &&  sfp2->WasDesc()) {
            return false;
        }
        
        CSeq_loc::TRange range1 = sfp1->GetLoc().GetTotalRange();
        CSeq_loc::TRange range2 = sfp2->GetLoc().GetTotalRange();
        // feature with smallest left extreme is first
        if ( range1.GetFrom() != range2.GetFrom() ) {
            return range1.GetFrom() < range2.GetFrom();
        }
        
        // shortest first (just for flatfile)
        if ( range1.GetToOpen() != range2.GetToOpen() ) {
            return range1.GetToOpen() < range2.GetToOpen();
        }
        
        return false;
    }
};


void CFlatGatherer::x_GatherSourceFeatures(void) const
{
    TSourceFeatSet srcs;
    
    x_CollectBioSources(srcs);
    if ( srcs.empty() ) {
        return;
    }
    x_MergeEqualBioSources(srcs);
    
    // sort by type (descriptor / feature) and location
    sort(srcs.begin(), srcs.end(), SSortByLoc());
    
    // if the descriptor has a focus, subtract out all other source locations.
    if ( srcs.front()->GetSource().IsSetIs_focus() ) {
        x_SubtractFromFocus(srcs);

        // if features completely subtracted descriptor intervals,
        // suppress in release, entrez modes.
        if ( srcs.front()->GetLoc().GetTotalRange().GetLength() == 0  &&
             m_Current->Config().HideEmptySource()  &&  srcs.size() > 1 ) {
            srcs.pop_front();
        }
    }
  
    ITERATE( TSourceFeatSet, it, srcs ) {
        *m_ItemOS << *it;
    }
}


void s_SetSelection(SAnnotSelector& sel, CBioseqContext& ctx)
{
    const CFlatFileConfig& cfg = ctx.Config();

    // set feature types to be collected
    {{
        sel.SetAnnotType(CSeq_annot::C_Data::e_Ftable);
        // source features are collected elsewhere
        sel.ExcludeFeatType(CSeqFeatData::e_Biosrc);
        // pub features are used in the REFERENCES section
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_pub);
        // some feature types are always excluded (deprecated?)
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_non_std_residue);
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_rsite);
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_seq);
        // exclude other types based on user flags
        if ( cfg.HideImpFeats() ) {
            sel.ExcludeFeatType(CSeqFeatData::e_Imp);
        }
        if ( cfg.HideRemoteImpFeats() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation);
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon);
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron);
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
        }
        if ( cfg.HideSNPFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation);
        }
        if ( cfg.HideExonFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon);
        }
        if ( cfg.HideIntronFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron);
        }
    }}
    sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
    if ( GetStrand(ctx.GetLocation(), &ctx.GetScope()) == eNa_strand_minus ) {
        sel.SetSortOrder(SAnnotSelector::eSortOrder_Reverse);  // sort in reverse biological order
    } else {
        sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    }
    sel.SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());
    sel.SetResolveTSE();
}


static bool s_SeqLocEndsOnBioseq(const CSeq_loc& loc, const CBioseq_Handle& seq)
{
    CSeq_loc_CI last;
    for ( CSeq_loc_CI it(loc); it; ++it ) {
        last = it;
    }
    return (last  &&  seq.IsSynonym(last.GetSeq_id()));
}


static bool s_FeatEndsOnBioseq(const CSeq_feat& feat, const CBioseq_Handle& seq)
{
    return s_SeqLocEndsOnBioseq(feat.GetLocation(), seq);
}


static CSeq_loc_Mapper* s_CreateMapper(CBioseqContext& ctx)
{
    if ( ctx.GetMapper() != 0 ) {
        return ctx.GetMapper();
    }

    // do not create mapper if not segmented or segmented but not doing master style.
    const CFlatFileConfig& cfg = ctx.Config();
    if ( !ctx.IsSegmented()  || !(cfg.IsStyleMaster()  ||  cfg.IsFormatFTable()) ) {
        return 0;
    }

    CSeq_loc_Mapper* mapper = new CSeq_loc_Mapper(ctx.GetHandle());
    if ( mapper != 0 ) {
        mapper->SetMergeAbutting();
        mapper->PreserveDestinationLocs();
        mapper->KeepNonmappingRanges();
    }
    return mapper;
}


static bool s_CopyCDSFromCDNA(CBioseqContext& ctx)
{
    return ctx.IsInGPS()  &&  !ctx.IsInNucProt()  &&  ctx.Config().CopyCDSFromCDNA();
}


static void s_FixLocation(CConstRef<CSeq_loc>& feat_loc, CBioseqContext& ctx)
{
    if ( !feat_loc->IsMix() ) {
        return;
    }

    bool partial5 = feat_loc->IsPartialLeft();
    bool partial3 = feat_loc->IsPartialRight();

    CRef<CSeq_loc> loc(SeqLocMerge(ctx.GetHandle(), feat_loc->GetMix().Get(),
        fFuseAbutting | fMergeIntervals));
    loc->SetPartialLeft(partial5);
    loc->SetPartialRight(partial3);

    feat_loc.Reset(loc);
}


void CFlatGatherer::x_GatherFeaturesOnLocation
(const CSeq_loc& loc,
 SAnnotSelector& sel,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();
    CFlatItemOStream& out = *m_ItemOS;

    CRef<CSeq_loc_Mapper> mapper(s_CreateMapper(ctx));

    CSeq_feat_Handle prev_feat;
    for ( CFeat_CI it(scope, loc, sel); it; ++it ) {
        const CSeq_feat& feat = it->GetOriginalFeature();

        // supress dupliacte features
        CSeq_annot_Handle annot = it->GetSeq_feat_Handle().GetAnnot();
        if (prev_feat.GetSeq_feat()) {            
            if (prev_feat.GetFeatSubtype() == feat.GetData().GetSubtype()) {
                if (annot == prev_feat.GetAnnot()) {
                    if (prev_feat.GetSeq_feat()->Equals(feat)) {
                        continue;
                    }
                }
            }
        }
        prev_feat = it->GetSeq_feat_Handle();

        // if part show only features ending on that part
        if (ctx.IsPart()  &&  !s_FeatEndsOnBioseq(feat, ctx.GetHandle()) ) {
            // may need to map sig_peptide on a different segment
            if (feat.GetData().IsCdregion()) {
                if (!ctx.Config().IsFormatFTable()) {
                    x_GetFeatsOnCdsProduct(feat, ctx, mapper);
                }
            }
            continue;
        }

        CConstRef<CSeq_loc> feat_loc(&feat.GetLocation());
        if ( mapper ) {
            feat_loc.Reset(mapper->Map(*feat_loc));
            s_FixLocation(feat_loc, ctx);
        }
                
        out << new CFeatureItem(feat, ctx, feat_loc);

        // Add more features depending on user preferences
        switch ( feat.GetData().GetSubtype() ) {
        case CSeqFeatData::eSubtype_mRNA:
            {{
                // optionally map CDS from cDNA onto genomic
                if ( s_CopyCDSFromCDNA(ctx)   &&  feat.IsSetProduct() ) {
                    x_CopyCDSFromCDNA(feat, ctx);
                }
                break;
            }}
        case CSeqFeatData::eSubtype_cdregion:
            {{  
                if ( !ctx.Config().IsFormatFTable() ) {
                    x_GetFeatsOnCdsProduct(feat, ctx, mapper);
                }
                break;
            }}
        default:
            break;
        }
    }
}


void CFlatGatherer::x_CopyCDSFromCDNA
(const CSeq_feat& feat,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();

    CBioseq_Handle cdna = scope.GetBioseqHandle(feat.GetProduct());
    if ( !cdna ) {
        return;
    }
    // NB: There is only one CDS on an mRNA
    CFeat_CI cds(cdna, 0, 0, CSeqFeatData::e_Cdregion);
    if ( cds ) {
        // map mRNA location to the genomic
        CSeq_loc_Mapper mapper(feat,
                               CSeq_loc_Mapper::eProductToLocation,
                               &scope);
        CRef<CSeq_loc> cds_loc = mapper.Map(cds->GetLocation());

        *m_ItemOS << new CFeatureItem(cds->GetOriginalFeature(), ctx, cds_loc,
                                      CFeatureItem::eMapped_from_cdna);
    }
}


void CFlatGatherer::x_GatherFeatures(void) const
{
    CBioseqContext& ctx = *m_Current;
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();
    CFlatItemOStream& out = *m_ItemOS;

    SAnnotSelector sel;
    const SAnnotSelector* selp = ctx.GetAnnotSelector();
    if ( selp == 0 ) {
        s_SetSelection(sel, ctx);
        selp = &sel;
    }

    // optionally map gene from genomic onto cDNA
    if ( ctx.IsInGPS()  &&  cfg.CopyGeneToCDNA()  &&
         ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) {
        const CSeq_feat* mrna = GetmRNAForProduct(ctx.GetHandle());
        if ( mrna != 0 ) {
            CConstRef<CSeq_feat> gene = 
                GetOverlappingGene(mrna->GetLocation(), scope);
            if ( gene != 0 ) {
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->SetWhole(*ctx.GetPrimaryId());
                out << new CFeatureItem(*gene, ctx, loc, 
                                        CFeatureItem::eMapped_from_genomic);
            }
        }
    }

    CSeq_loc loc;
    if ( ctx.GetMasterLocation() != 0 ) {
        loc.Assign(*ctx.GetMasterLocation());
    } else {
        loc.SetWhole().Assign(*ctx.GetHandle().GetSeqId());
    }

    // collect features
    if ( ctx.IsSegmented()  &&  cfg.IsStyleMaster()  &&  cfg.OldFeatsOrder() ) {
        if ( ctx.GetAnnotSelector() == 0 ) {
            sel.SetResolveNone();
        }
        
        // first do the master bioeseq
        x_GatherFeaturesOnLocation(loc, sel, ctx);

        // map the location on the segments        
        CSeq_loc_Mapper mapper(1, ctx.GetHandle());
        CRef<CSeq_loc> seg_loc(mapper.Map(loc));
        if ( seg_loc ) {
            // now go over each of the segments
            for ( CSeq_loc_CI it(*seg_loc); it; ++it ) {
                x_GatherFeaturesOnLocation(it.GetSeq_loc(), sel, ctx);
            }
        }
    } else {
        x_GatherFeaturesOnLocation(loc, sel, ctx);
    }
    
    if ( ctx.IsProt() ) {
        // Also collect features which this protein is their product.
        // Currently there are only two possible candidates: Coding regions
        // and Prot features (rare).
        
        // look for the Cdregion feature for this protein
        const CSeq_feat* cds = GetCDSForProduct(ctx.GetHandle());
        if ( cds != 0 ) {
            out << new CFeatureItem(*cds, ctx, &cds->GetProduct(), 
                    CFeatureItem::eMapped_from_cdna);
        }

        // look for Prot features (only for RefSeq records or
        // GenBank not release_mode).
        if ( ctx.IsRefSeq()  ||  !cfg.ForGBRelease() ) {
            SAnnotSelector sel(CSeqFeatData::e_Prot, true);
            sel.SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());
            sel.SetResolveMethod(SAnnotSelector::eResolve_TSE);
            sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
            for ( CFeat_CI it(ctx.GetHandle(), 0, 0, sel); it; ++it ) {  
                out << new CFeatureItem(it->GetOriginalFeature(),
                                        ctx,
                                        &it->GetProduct(),
                                        CFeatureItem::eMapped_from_prot);
            }
        }
    }
}


static bool s_IsCDD(const CSeq_feat& feat)
{
    ITERATE(CSeq_feat::TDbxref, it, feat.GetDbxref()) {
        if ( (*it)->GetType() == CDbtag::eDbtagType_CDD ) {
            return true;
        }
    }
    return false;
}


void CFlatGatherer::x_GetFeatsOnCdsProduct
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 CRef<CSeq_loc_Mapper>& mapper) const
{
    if (!feat.GetData().IsCdregion()) {
        return;
    }

    const CFlatFileConfig& cfg = ctx.Config();
    
    if (cfg.HideCDSProdFeatures()) {
        return;
    }
    
    if (!feat.CanGetProduct()) {
        return;
    }

    CScope& scope = ctx.GetScope();
    CBioseq_Handle  prot = scope.GetBioseqHandle(feat.GetProduct());
    if (!prot) {
        return;
    }
    
    CFeat_CI prev;
    bool first = true;
    CSeq_loc_Mapper prot_to_cds(feat, CSeq_loc_Mapper::eProductToLocation, &scope);
    // explore mat_peptides, sites, etc.
    for ( CFeat_CI it(prot, 0, 0); it; ++it ) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        if ( !(subtype == CSeqFeatData::eSubtype_region)              &&
             !(subtype == CSeqFeatData::eSubtype_site)                &&
             !(subtype == CSeqFeatData::eSubtype_bond)                &&
             !(subtype == CSeqFeatData::eSubtype_mat_peptide_aa)      &&
             !(subtype == CSeqFeatData::eSubtype_sig_peptide_aa)      &&
             !(subtype == CSeqFeatData::eSubtype_transit_peptide_aa)  &&
             !(subtype == CSeqFeatData::eSubtype_preprotein) ) {
            continue;
        }

        if ( cfg.HideCDDFeats()  &&
             subtype == CSeqFeatData::eSubtype_region  &&
             s_IsCDD(it->GetOriginalFeature()) ) {
            // passing this test prevents mapping of COG CDD region features
            continue;
        }

        // suppress duplicate features (on protein)
        if (!first) {
            const CSeq_loc& loc_curr = it->GetLocation();
            const CSeq_loc& loc_prev = prev->GetLocation();
            const CSeq_feat& feat_curr = it->GetOriginalFeature();
            const CSeq_feat& feat_prev = prev->GetOriginalFeature();

            if ( feat_prev.Compare(feat_curr, loc_curr, loc_prev) == 0 ) {
                continue;
            }
        }

        // map prot location to nuc location
        CRef<CSeq_loc> loc(prot_to_cds.Map(it->GetLocation()));
        // possibly map again (e.g. from part to master)
        if ( loc.NotEmpty()  &&  mapper.NotEmpty() ) {
            loc.Reset(mapper->Map(*loc));
        }
        if (!loc  ||  loc->IsNull()) {
            continue;
        }
        if (ctx.IsPart()  &&  !s_SeqLocEndsOnBioseq(*loc, ctx.GetHandle())) {
            continue;
        }

        // make sure feature is within sublocation
        if ( ctx.GetMasterLocation() != 0 ) {
            const CSeq_loc& mloc = *ctx.GetMasterLocation();
            if ( Compare(mloc, *loc, &scope) != eContains ) {
                continue;
            }
        }
        
        *m_ItemOS << new CFeatureItem(it->GetOriginalFeature(), ctx, 
            loc, CFeatureItem::eMapped_from_prot);

        prev = it;
        first = false;
    }    
}


/////////////////////////////////////////////////////////////////////////////
//
// ALIGNMENTS


void CFlatGatherer::x_GatherAlignments(void) const
{
    CBioseqContext&  ctx    = *m_Current;
    CSeq_loc_Mapper* mapper = ctx.GetMapper();
    for (CAlign_CI it(ctx.GetScope(), ctx.GetLocation());  it;  ++it) {
        if (mapper) {
            *m_ItemOS << new CAlignmentItem(*mapper->Map(*it), ctx);
        } else {
            *m_ItemOS << new CAlignmentItem(const_cast<CSeq_align&>(*it), ctx);
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.26  2004/08/30 13:43:05  shomrat
*  supress duplicate features;fixed mapping from prot to nuc
*
* Revision 1.25  2004/08/25 17:11:26  grichenk
* Removed obsolete SAnnotSelector::SetCombineMethod()
*
* Revision 1.24  2004/08/25 15:03:56  grichenk
* Removed duplicate methods from CSeqMap
*
* Revision 1.23  2004/08/19 16:31:29  shomrat
* Do only non-virtual parts; gather feature comments
*
* Revision 1.22  2004/06/21 18:52:56  ucko
* +x_GatherAlignments
*
* Revision 1.21  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/05/06 17:52:21  shomrat
* Fixed feature location
*
* Revision 1.19  2004/04/27 15:12:16  shomrat
* Added logic for partial range formatting
*
* Revision 1.18  2004/04/22 16:00:25  shomrat
* Changes in context
*
* Revision 1.17  2004/04/13 16:47:15  shomrat
* Added GBSeq format
*
* Revision 1.16  2004/04/07 14:51:24  shomrat
* Fixed typo
*
* Revision 1.15  2004/04/07 14:27:47  shomrat
* FTable format always on master bioseq
*
* Revision 1.14  2004/03/31 17:16:04  shomrat
* Set current bioseq once in calling function
*
* Revision 1.13  2004/03/30 20:31:09  shomrat
* Bug fix
*
* Revision 1.12  2004/03/26 17:24:55  shomrat
* Changes to comment gathering
*
* Revision 1.11  2004/03/25 20:39:47  shomrat
* Use handles
*
* Revision 1.10  2004/03/18 15:39:40  shomrat
* + Filtering of displayed records
*
* Revision 1.9  2004/03/12 16:57:54  shomrat
* Filter viewable bioseqs; Use new location mapping
*
* Revision 1.8  2004/03/10 16:22:44  shomrat
* Use reference to object
*
* Revision 1.7  2004/03/05 18:45:19  shomrat
* changes to feature gathering
*
* Revision 1.6  2004/02/19 18:11:25  shomrat
* Set feature iterator selector based on user flags
*
* Revision 1.5  2004/02/11 22:52:41  shomrat
* using types in flag file
*
* Revision 1.4  2004/02/11 16:52:12  shomrat
* completed implementation of featture gathering
*
* Revision 1.3  2004/01/14 16:15:03  shomrat
* minor changes to accomodate for GFF format
*
* Revision 1.2  2003/12/18 17:43:34  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:21:48  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
