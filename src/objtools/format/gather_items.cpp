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
*   
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <set>

#include <objtools/format/flat_file_generator.hpp>
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
#include <objtools/format/items/endsection_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/embl_gather.hpp>
#include "context.hpp"
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// Public:

// "virtual constructor"
CFlatGatherer* CFlatGatherer::New(TFormat format)
{
    switch ( format ) {
    case CFlatFileGenerator::eFormat_GenBank:
        //case CFlatFileGenerator<>::eFormat_GBSeq:
        //case CFlatFileGenerator<>::eFormat_Index:
        return new CGenbankGatherer;
        break;
        
    case CFlatFileGenerator::eFormat_EMBL:
        return new CEmblGatherer;
        break;
        
    case CFlatFileGenerator::eFormat_DDBJ:
        //return new CDdbjGatherItems;
        break;
        
    default:
        // Throw exception - unrecognized format
        break;
    }

    return 0;
}


void CFlatGatherer::Gather(CFFContext& ctx, CFlatItemOStream& os) const
{
    m_ItemOS.Reset(&os);
    m_Context.Reset(&ctx);

    x_GatherSeqEntry(ctx.GetTSE());
}


CFlatGatherer::~CFlatGatherer(void)
{
}







/////////////////////////////////////////////////////////////////////////////
//
// Protected:


void CFlatGatherer::x_GatherHeader(CFFContext& ctx, CFlatItemOStream& item_os) const
{
    //item_os << new CLocusItem(ctx);
    //item_os << new CKeywordsItem(ctx);
}


/////////////////////////////////////////////////////////////////////////////
//
// Private:

void CFlatGatherer::x_GatherSeqEntry(const CSeq_entry& entry) const
{
    if ( entry.IsSet() ) {
        CBioseq_set::TClass clss = entry.GetSet().GetClass();
        if ( clss == CBioseq_set::eClass_genbank  ||
             clss == CBioseq_set::eClass_mut_set  ||
             clss == CBioseq_set::eClass_pop_set  ||
             clss == CBioseq_set::eClass_phy_set  ||
             clss == CBioseq_set::eClass_eco_set  ||
             clss == CBioseq_set::eClass_wgs_set  ||
             clss == CBioseq_set::eClass_gen_prod_set ) {
            ITERATE(CBioseq_set::TSeq_set, iter, entry.GetSet().GetSeq_set()) {
                x_GatherSeqEntry(**iter);
            }
            return;
        }
    }

    // visit each bioseq in the entry (excluding segments)
    CBioseq_CI seq_iter(m_Context->GetScope(),
        entry,
        CSeq_inst::eMol_not_set,
        CBioseq_CI::eLevel_Mains);
    for ( ; seq_iter; ++seq_iter ) {
        x_GatherBioseq(seq_iter->GetBioseq());
    }
} 


// a defualt implementation for GenBank /  DDBJ formats
void CFlatGatherer::x_GatherBioseq(const CBioseq& seq) const
{
    if ( !seq.CanGetInst() ) {
        return;
    }

    bool segmented = seq.GetInst().CanGetRepr()  &&
        seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg;
    CFlatFileGenerator::TStyle style = m_Context->GetStyle();
    if ( segmented  &&
         (style == CFlatFileGenerator::eStyle_Normal  ||
          style == CFlatFileGenerator::eStyle_Segment) ) {
        // display individual segments (multiple sections)
        x_DoMultipleSections(seq);
    } else {
        // display as a single bioseq (single section)
        x_DoSingleSection(seq);
    }   
}


void CFlatGatherer::x_DoMultipleSections(const CBioseq& seq) const
{
    m_Context->SetMasterBioseq(seq);

    // we need to remove the const because of the SeqMap creation interface.
    CBioseq& non_const_seq = const_cast<CBioseq&>(seq);

    CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForBioseq(non_const_seq);
    SSeqMapSelector selector;
    selector.SetResolveCount(1); // do not resolve segments

    CScope& scope = m_Context->GetScope();

    // iterate over the segments, gathering a segment at a time.
    ITERATE (CSeg_ext::Tdata, seg, seq.GetInst().GetExt().GetSeg().Get()) {
        // skip gaps
        if ( (*seg)->IsNull() ) {
            continue;
        }

        // !!! set the context location

        const CSeq_id& seg_id = sequence::GetId(**seg, &scope);
        CBioseq_Handle bsh = scope.GetBioseqHandle(seg_id);
        if ( bsh ) {
            x_DoSingleSection(bsh.GetBioseq());
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// REFERENCES

bool s_FilterPubdesc(const CPubdesc& pubdesc, CFFContext& ctx)
{
    if ( pubdesc.CanGetComment() ) {
        const string& comment = pubdesc.GetComment();
        bool is_gene_rif = NStr::StartsWith(comment, "GeneRIF", NStr::eNocase);

        if ( (ctx.HideGeneRIFs()  &&  is_gene_rif)  ||
             ((ctx.OnlyGeneRIFs()  ||  ctx.LatestGeneRIFs())  &&  !is_gene_rif) ) {
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_GatherReferences(void) const
{
    typedef CRef<CReferenceItem> TRef;
    vector<TRef> references;

    // gather references from descriptors
    for (CSeqdesc_CI it(m_Context->GetHandle(), CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Context) ) {
            continue;
        }
        
        references.push_back(TRef(new CReferenceItem(*it, *m_Context)));
    }

    // gather references from features
    CFeat_CI it(m_Context->GetHandle(), 0, 0, CSeqFeatData::e_Pub);
    for ( ; it; ++it) {
        references.push_back(TRef(new CReferenceItem(it->GetData().GetPub(),
                                        *m_Context, &it->GetLocation())));
    }
    CReferenceItem::Rearrange(references, *m_Context);
    ITERATE (vector<TRef>, it, references) {
        *m_ItemOS << *it;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// COMMENTS


void CFlatGatherer::x_GatherComments(void) const
{
    CFFContext& ctx = *m_Context;

    // Gather comments related to the seq-id
    x_IdComments(ctx);
    x_RefSeqComments(ctx);

    if ( CCommentItem::NsAreGaps(ctx.GetActiveBioseq(), ctx) ) {
        x_AddComment(new CCommentItem(CCommentItem::kNsAreGaps, ctx));
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
        if ( !m_Comments.empty() ) {
            m_Comments.push_back(com);
        } else {
            *m_ItemOS << com;
        }
    }
}


void CFlatGatherer::x_AddGSDBComment
(const CDbtag& dbtag,
 CFFContext& ctx) const
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

    // the first one is GSDB comment
    if ( m_Comments.size() > 1 ) {
        // if there are subsequent comments, add period after GSDB id
        CRef<CGsdbComment> gsdb_comment(dynamic_cast<CGsdbComment*>(m_Comments.front().GetPointer()));
        if ( gsdb_comment ) {
            gsdb_comment->AddPeriod();
        }
    }

    ITERATE (vector< CRef<CCommentItem> >, it, m_Comments) {
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
        if ( !CCommentItem::GetStatusForRefTrack(it->GetUser()).empty() ) {
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_IdComments(CFFContext& ctx) const
{
    const CObject_id* local_id = 0;

    string genome_build_number = s_GetGenomeBuildNumber(ctx.GetHandle());
    bool has_ref_track_status = s_HasRefTrackStatus(ctx.GetHandle());

    ITERATE( CBioseq::TId, id_iter, ctx.GetActiveBioseq().GetId() ) {
        const CSeq_id& id = **id_iter;

        switch ( id.Which() ) {
        case CSeq_id::e_Other:
            {{
                if ( ctx.IsNC() ) {
                    if ( !genome_build_number.empty() ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsNT()  ||  ctx.IsNW() ) {
                    if ( !has_ref_track_status ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsXP()  ||  ctx.IsXM()  ||  ctx.IsXR()  ||  ctx.IsZP() ) {
                    SModelEvidance me;
                    if ( GetModelEvidance(ctx.GetHandle(), ctx.GetScope(), me) ) {
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
            CFlatFileGenerator::TMode mode = ctx.GetMode();
            if ( mode == CFlatFileGenerator::eMode_GBench  ||  
                 mode == CFlatFileGenerator::eMode_Dump ) {
                // !!! create a CLocalIdComment(*local_id, ctx)
            }
        }
    }
}


void CFlatGatherer::x_RefSeqComments(CFFContext& ctx) const
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
            if ( !ctx.HideBankItComment() ) {
                string str = CCommentItem::GetStringForBankIt(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                }
            }
        }}

        // RefTrack
        {{
            if ( !did_ref_track ) {
                string str = CCommentItem::GetStatusForRefTrack(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
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


void CFlatGatherer::x_HistoryComments(CFFContext& ctx) const
{
    const CBioseq& seq = ctx.GetActiveBioseq();
    if ( !seq.GetInst().CanGetHist() ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst().GetHist();

    if ( hist.CanGetReplaced_by() ) {
        const CSeq_hist::TReplaced_by& r = hist.GetReplaced_by();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaced_by,
                hist, ctx));
        }
    }

    if ( hist.IsSetReplaces()  &&
         ctx.GetMode() == CFlatFileGenerator::eMode_GBench) {
        const CSeq_hist::TReplaces& r = hist.GetReplaced_by();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaces,
                hist, ctx));
        }
    }
}


void CFlatGatherer::x_WGSComment(CFFContext& ctx) const
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


void CFlatGatherer::x_GBBSourceComment(CFFContext& ctx) const
{
    _ASSERT(ctx.ShowGBBSource());

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


void CFlatGatherer::x_DescComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Comment); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_MaplocComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Maploc); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_RegionComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Region); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_HTGSComments(CFFContext& ctx) const
{
    CSeqdesc_CI mi_iter(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    const CMolInfo& mi = mi_iter->GetMolinfo();

    if ( ctx.IsRefSeq()  &&  mi.CanGetCompleteness()  &&
         mi.GetCompleteness() != CMolInfo::eCompleteness_unknown ) {
        string str = CCommentItem::GetStringForMolinfo(mi, ctx);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx, &(*mi_iter)));
        }
    }

    CMolInfo::TTech tech = ctx.GetTech();
    if ( tech == CMolInfo::eTech_htgs_0  ||
         tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2 ) {
        x_AddComment(new CCommentItem(
            CCommentItem::GetStringForHTGS(mi, ctx), ctx, &(*mi_iter)));
    } else {
        const string& tech_str = GetTechString(tech);
        if ( !tech_str.empty() ) {
            x_AddComment(new CCommentItem("Method: " + tech_str, ctx, &(*mi_iter)));
        }
    }
}


void CFlatGatherer::x_FeatComments(CFFContext& ctx) const
{

}


/////////////////////////////////////////////////////////////////////////////
//
// ORIGIN

// We use multiple items to represent the sequence.
void CFlatGatherer::x_GatherSequence(void) const
{
    static const TSeqPos kChunkSize = 2400; // 20 lines
    
    bool first = true;
    TSeqPos size = m_Context->GetHandle(). GetBioseqLength();
    for ( TSeqPos start = 0; start < size; start += kChunkSize ) {
        TSeqPos end = min(start + kChunkSize, size);
        *m_ItemOS << new CSequenceItem(start, end, first, *m_Context);
        first = false;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// FEATURES


// source

static void s_CollectBioSources
(CFFContext& ctx,
 set< CConstRef<CBioSource> >& bsrcs)
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source);  it;  ++it) {
        bsrcs.insert(CConstRef<CBioSource>(&it->GetSource()));
    }
}


void CFlatGatherer::x_GatherSourceFeatures(void) const
{
    set< CConstRef<CBioSource> > bsrcs;
  
    s_CollectBioSources(*m_Context, bsrcs);
}


void CFlatGatherer::x_GatherFeatures(void) const
{
    CFFContext& ctx = *m_Context;
    CScope& scope = ctx.GetScope();
    typedef CConstRef<CFeatureItemBase> TFFRef;
    list<TFFRef> l, l2;
    bool source = true;
    CFlatItemOStream& out = *m_ItemOS;
    
    // XXX -- should select according to flags; may require merging/re-sorting.
    // (Generally needs lots of additional logic, basically...)
    if (source) {
        for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
            switch (it->Which()) {
            case CSeqdesc::e_Org:
                out << new CSourceFeatureItem(it->GetOrg(), ctx);
                break;
            case CSeqdesc::e_Source:
                out << new CSourceFeatureItem(it->GetSource(), ctx);
                break;
            default:
                break;
            }
        }
    } else if (ctx.IsProt()) { // broaden condition?
        for (CFeat_CI it(scope, *ctx.GetLocation(), CSeqFeatData::e_not_set,
                         SAnnotSelector::eOverlap_Intervals,
                         CFeat_CI::eResolve_All, CFeat_CI::e_Product);
             it;  ++it) {
            l.push_back(TFFRef(new CFeatureItem
                (*it, ctx, &it->GetProduct(), true)));
        }
    }
    if (source) {
        for (CFeat_CI it(scope, *ctx.GetLocation(), CSeqFeatData::e_not_set,
                         SAnnotSelector::eOverlap_Intervals,
                         CFeat_CI::eResolve_All);
             it;  ++it) {
            switch (it->GetData().Which()) {
            case CSeqFeatData::e_Org:
            case CSeqFeatData::e_Biosrc:
                out << new CSourceFeatureItem(*it, ctx);
                break;
            default:
                break;
            }
        }
    }
    for (CFeat_CI it(scope, *ctx.GetLocation(), CSeqFeatData::e_not_set,
                     SAnnotSelector::eOverlap_Intervals,
                     CFeat_CI::eResolve_All);
         it;  ++it) {
        switch (it->GetData().Which()) {
        case CSeqFeatData::e_Pub:  // done as REFERENCEs
            break;
        case CSeqFeatData::e_Org:
        case CSeqFeatData::e_Biosrc: // sone as source
            break;
        default:
            out << new CFeatureItem(*it, ctx);
            break;
        }
    }
    /*
    !!!
    CFFContext& ctx = *m_Context;
    
    *m_ItemOS << new CFeatHeaderItem(ctx);

    CFeat_CI feat_it(ctx.GetScope(),
                     *ctx.GetLocation(),
                     CSeqFeatData::e_not_set,
                     SAnnotSelector::eOverlap_Intervals,
                     CFeat_CI::eResolve_All);
    for ( ; feat_it; ++feat_it ) {
        if ( x_SkipFeature(feat_it->GetOriginalFeature(), ctx) ) {
            continue;
        }
    }
    */
}


bool CFlatGatherer::x_SkipFeature
(const CSeq_feat& feat,
 const CFFContext& ctx) const
{
    CSeqFeatData::E_Choice type = feat.GetData().Which();
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();        
   
    if ( subtype == CSeqFeatData::eSubtype_pub              ||
        subtype == CSeqFeatData::eSubtype_non_std_residue  ||
        subtype == CSeqFeatData::eSubtype_biosrc           ||
        subtype == CSeqFeatData::eSubtype_rsite            ||
        subtype == CSeqFeatData::eSubtype_seq ) {
        return true;
    }
    
    // check feature customization flags
    if ( ctx.ValidateFeats()  &&
        (subtype == CSeqFeatData::eSubtype_bad  ||
         subtype == CSeqFeatData::eSubtype_virion) ) {
        return true;
    }
    
    if ( ctx.IsNa()  &&  subtype == CSeqFeatData::eSubtype_het ) {
        return true;
    }
    
    if ( ctx.HideImpFeat()  &&  type == CSeqFeatData::e_Imp ) {
        return true;
    }
    
    if ( ctx.HideSnpFeat()  &&  subtype == CSeqFeatData::eSubtype_variation ) {
        return true;
    }

    if ( ctx.HideExonFeat()  &&  subtype == CSeqFeatData::eSubtype_exon ) {
        return true;
    }

    if ( ctx.HideIntronFeat()  &&  subtype == CSeqFeatData::eSubtype_intron ) {
        return true;
    }

    if ( ctx.HideRemImpFeat()  &&  subtype == CSeqFeatData::eSubtype_imp ) {
        if ( subtype == CSeqFeatData::eSubtype_variation  ||
             subtype == CSeqFeatData::eSubtype_exon  ||
             subtype == CSeqFeatData::eSubtype_intron  ||
             subtype == CSeqFeatData::eSubtype_misc_feature ) {
            return true;
        }
    }

    // skip genes in DDBJ format
    if ( ctx.GetFormat() == CFlatFileGenerator::eFormat_DDBJ  &&  
         type == CSeqFeatData::e_Gene ) {
        return true;
    }

    // supress full length comment features
    /*
    if ( type == CSeqFeatData::e_Comment ) {
        CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
        if ( range.IsWhole()  ||  
            (range.GetFrom() == ctx.Start()  &&  range.GetTo() == ctx.End()) ) {
            return true;
        }
    }
    */
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/12/17 20:21:48  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
