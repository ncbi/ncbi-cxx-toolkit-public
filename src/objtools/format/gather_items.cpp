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
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/annot_selector.hpp>

#include <algorithm>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/items/contig_item.hpp>
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
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/gather_iter.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/embl_gather.hpp>
#include <objtools/format/gff_gather.hpp>
#include <objtools/format/ftable_gather.hpp>
#include <objtools/format/feature_gather.hpp>
#include <objtools/format/context.hpp>
#include <objtools/error_codes.hpp>
#include "utils.hpp"

#include <connect/ncbi_socket.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Fmt_Gather


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);

class CSubtypeEquals {
public:
    bool operator()( const CRef< CSubSource > & obj1, const CRef< CSubSource > & obj2 ) {
        if( obj1.IsNull() != obj2.IsNull() ) {
            return false;
        }
        if( ! obj1.IsNull() ) {
            CSubSource_Base::TSubtype subtypevalue1 = ( obj1->CanGetSubtype() ? obj1->GetSubtype() : 0 );
            CSubSource_Base::TSubtype subtypevalue2 = ( obj2->CanGetSubtype() ? obj2->GetSubtype() : 0 );
            if( subtypevalue1 != subtypevalue2 ) {
                return false;
            }

            const CSubSource_Base::TName &name1 = ( obj1->CanGetName() ? obj1->GetName() : kEmptyStr );
            const CSubSource_Base::TName &name2 = ( obj2->CanGetName() ? obj2->GetName() : kEmptyStr );
            if( name1 != name2 ) {
                return false;
            }
        }

        return true;
    }
};

class CDbEquals {
public:
    bool operator()( const CRef< CDbtag > & obj1, const CRef< CDbtag > & obj2 ) {
        if( obj1.IsNull() != obj2.IsNull() ) {
            return false;
        }
        if( ! obj1.IsNull() ) {
            return obj1->Match( *obj2 );
        }
        return true;
    }
};


class COrgModEquals {
public:
    bool operator()( const CRef< COrgMod > & obj1, const CRef< COrgMod > & obj2 ) {
        if( obj1.IsNull() != obj2.IsNull() ) {
            return false;
        }
        if( ! obj1.IsNull() ) {
            return obj1->Equals( *obj2 );
        }
        return true;
    }
};


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

    case CFlatFileConfig::eFormat_FeaturesOnly:
        return new CFeatureGatherer;

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

    CRef<CTopLevelSeqEntryContext> topLevelSeqEntryContext( new CTopLevelSeqEntryContext(ctx.GetEntry()) );

    // See if there even are any Bioseqs to print
    // (If we don't do this test, we might print a CStartItem
    // and CEndItem with nothing in between )
    CGather_Iter seq_iter(ctx.GetEntry(), Config());
    if( ! seq_iter ) {
        return;
    }

    CConstRef<IFlatItem> item;
    item.Reset( new CStartItem() );
    os << item;
    x_GatherSeqEntry(ctx.GetEntry(), topLevelSeqEntryContext);
    item.Reset( new CEndItem() );
    os << item;
}


CFlatGatherer::~CFlatGatherer(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//
// Protected:

void CFlatGatherer::x_GatherSeqEntry(const CSeq_entry_Handle& entry,
    CRef<CTopLevelSeqEntryContext> topLevelSeqEntryContext) const
{
    // visit bioseqs in the entry (excluding segments)
    CGather_Iter seq_iter(entry, Config());
    CBioseq_Handle prev_seq;
    CBioseq_Handle this_seq;
    CBioseq_Handle next_seq;
    for ( ; seq_iter; ++seq_iter ) {

        if( this_seq ) {
            x_GatherBioseq(prev_seq, this_seq, next_seq, topLevelSeqEntryContext);
        }

        // move everything over by one
        prev_seq = this_seq;
        this_seq = next_seq;
        next_seq = *seq_iter;
    }

    // we don't process the last ones, so we do that now
    if( this_seq ) {
        x_GatherBioseq(prev_seq, this_seq, next_seq, topLevelSeqEntryContext);
    }
    if( next_seq ) {
        x_GatherBioseq(this_seq, next_seq, CBioseq_Handle(), topLevelSeqEntryContext);
    }
} 


static bool s_LocationsTouch( const CSeq_loc& loc1, const CSeq_loc& loc2 )
{
    CRange<TSeqPos> rg1, rg2;
    try {
        rg1 = loc1.GetTotalRange();
        rg2 = loc2.GetTotalRange();
    }
    catch( ... ) {
        return false;
    }
    return (rg1.GetFrom() == rg2.GetTo() + 1) || (rg1.GetTo() + 1 == rg2.GetFrom());
};


static bool s_LocationsOverlap( const CSeq_loc& loc1, const CSeq_loc& loc2, CScope *p_scope )
{
    return ( -1 != TestForOverlap( loc1, loc2, eOverlap_Simple, kInvalidSeqPos, p_scope ) );
};


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

bool s_BioSeqHasContig( 
    const CBioseq_Handle& seq, 
    CFlatFileContext& ctx )
{
    CBioseqContext* pbsc = new CBioseqContext(seq, ctx );
    CContigItem* pContig = new CContigItem( * pbsc );
    CSeq_loc::E_Choice choice = pContig->GetLoc().Which();
    delete pContig;
    delete pbsc;
        
    return ( choice != CSeq_loc::e_not_set );
}

// a default implementation for GenBank /  DDBJ formats
void CFlatGatherer::x_GatherBioseq(
    const CBioseq_Handle& prev_seq, const CBioseq_Handle& seq, const CBioseq_Handle& next_seq, 
    CRef<CTopLevelSeqEntryContext> topLevelSeqEntryContext ) const
{
    const CFlatFileConfig& cfg = Config();
    if ( cfg.IsModeRelease() && cfg.IsStyleContig() && 
      ! s_BioSeqHasContig( seq, *m_Context ) ) {
        NCBI_THROW(
            CFlatException, 
            eInvalidParam, 
            "Release mode failure: Given sequence is not contig" );
        return;
    }

    if( m_pCanceledCallback && m_pCanceledCallback->IsCanceled() ) {
        NCBI_THROW(CFlatException, eHaltRequested, 
                   "FlatFileGeneration canceled by ICancel callback");
    }

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
        m_Current.Reset(new CBioseqContext(prev_seq, seq, next_seq, *m_Context, 0, 
            (topLevelSeqEntryContext ? &*topLevelSeqEntryContext : NULL)));
        m_Context->AddSection(m_Current);
        x_DoSingleSection(*m_Current);
    }
}


void CFlatGatherer::x_DoMultipleSections(const CBioseq_Handle& seq) const
{
    CRef<CMasterContext> mctx(new CMasterContext(seq));

    CScope* scope = &seq.GetScope();
    const CSeqMap& seqmap = seq.GetSeqMap();

    CSeqMap_CI it = seqmap.BeginResolved(scope,
                                         SSeqMapSelector()
                                         .SetResolveCount(1)
                                         .SetFlags(CSeqMap::fFindRef));
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
// SOURCE/ORGANISM

void CFlatGatherer::x_GatherSourceOrganism(void) const
{
    CBioseqContext& ctx = *m_Current;

    CBioseq_Handle& hnd = ctx.GetHandle();
    const CFlatFileConfig& cfg = ctx.Config();

    CConstRef<IFlatItem> item;
    for (CSeqdesc_CI dit(hnd, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.IsSetOrg()) {
            if( cfg.IsShownGenbankBlock(CFlatFileConfig::fGenbankBlocks_Source) ) {
                item.Reset( new CSourceItem(ctx, bsrc, *dit) );
                ItemOS() << item;
                if (! ctx.IsCrossKingdom()) break;
                if (! ctx.IsRSUniqueProt()) break;
            }
        }
    }
}
    
/////////////////////////////////////////////////////////////////////////////
//
// REFERENCES

bool s_IsJustUids( const CPubdesc& pubdesc ) 
{
    const CPubdesc::TPub& pub = pubdesc.GetPub();
    ITERATE ( CPub_equiv::Tdata, it, pub.Get() ) {
        
        switch( (*it)->Which() ) {

        case CPub::e_Gen:
        case CPub::e_Sub:
        case CPub::e_Article:
        case CPub::e_Journal:
        case CPub::e_Book:
        case CPub::e_Proc:
        case CPub::e_Patent:
        case CPub::e_Man:
            return false;
        default:
            /* placate gcc */
            break;
        }
    }
    return true;
}

bool s_FilterPubdesc(const CPubdesc& pubdesc, CBioseqContext& ctx)
{
    if ( ( ! ctx.CanGetTLSeqEntryCtx() || ctx.GetTLSeqEntryCtx().GetCanSourcePubsBeFused() ) && s_IsJustUids(pubdesc) ) {
        return true;
    }
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

static bool s_IsDuplicatePmid(const CPubdesc& pubdesc,
                              set<int>& included_pmids)
{
    bool is_duplicate = false;
    ITERATE (CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
        const CPub& pub = **it;
        if (pub.IsPmid()) {
            if ( !included_pmids.insert
                 (pub.GetPmid()).second) {
                is_duplicate = true;
            }
            break;
        }
    }
    return is_duplicate;
}


void CFlatGatherer::x_GatherReferences(const CSeq_loc& loc, TReferences& refs) const
{
    CScope& scope = m_Current->GetScope();
    
    CBioseq_Handle seq = GetBioseqFromSeqLoc(loc, scope);
    if (!seq) {
        return;
    }

    set<int> included_pmids;

    // gather references from descriptors (top-level first)
    // (Since CSeqdesc_CI doesn't currently support bottom-to-top iteration,
    //  we approximate this by iterating over top-level, then non-top-level seqs )
    for (CSeqdesc_CI it(seq.GetTopLevelEntry(), CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        if (s_IsDuplicatePmid(pubdesc, included_pmids)) {
            continue;
        }
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }
    for (CSeqdesc_CI it(seq, CSeqdesc::e_Pub); it; ++it) {
        // check for dups from last for-loop
        if( ! it.GetSeq_entry_Handle().HasParentEntry() ) {
            continue;
        }
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        if (s_IsDuplicatePmid(pubdesc, included_pmids)) {
            continue;
        }
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }

    // also gather references from annotations
    {{
         SAnnotSelector sel = m_Current->SetAnnotSelector();
         for (CAnnot_CI annot_it(seq, sel);
              annot_it; ++annot_it) {
             if ( !annot_it->Seq_annot_IsSetDesc() ) {
                 continue;
             }
             ITERATE (CSeq_annot::TDesc::Tdata, it,
                      annot_it->Seq_annot_GetDesc().Get()) {
                 if ( !(*it)->IsPub() ) {
                     continue;
                 }
                 const CPubdesc& pubdesc = (*it)->GetPub();
                 if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
                     continue;
                 }
                 if (s_IsDuplicatePmid(pubdesc, included_pmids)) {
                     continue;
                 }
                 CRef<CSeqdesc> desc(new CSeqdesc);
                 desc->SetPub(const_cast<CPubdesc&>((*it)->GetPub()));
                 refs.push_back(CBioseqContext::TRef
                                (new CReferenceItem(*desc, *m_Current)));
             }
         }
     }}

    // if near segmented, collect pubs from segments under location
    CSeq_entry_Handle segset =
        seq.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if (segset  &&  seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
        CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForSeq_loc(loc, &scope);
        if (seqmap) {
            SSeqMapSelector mapsel;
            mapsel.SetFlags(CSeqMap::eSeqRef)
                  .SetResolveCount(1)
                  .SetLimitTSE(m_Current->GetTopLevelEntry());
            for (CSeqMap_CI smit(seqmap, &scope, mapsel); smit; ++smit) {
                // NB: search already limited to TSE ...
                CBioseq_Handle part;
                try {
                    // ... but not necessarily to just references, it seems.
                    // The following line has been observed to throw almost
                    // every time when run against a pool of sample files.
                    part = scope.GetBioseqHandle(smit.GetRefSeqid());
                }
                catch ( ... ) {
                    // Seemingly not a reference. Nothing to do in this 
                    // iteration.
                    continue;
                }
                if (part) {
                    for (CSeqdesc_CI dit(CSeq_descr_CI(part, 1), CSeqdesc::e_Pub); dit; ++dit) {
                        const CPubdesc& pubdesc = dit->GetPub();
                        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
                            continue;
                        }
                        if (s_IsDuplicatePmid(pubdesc, included_pmids)) {
                            continue;
                        }

                        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*dit, *m_Current)));
                    }
                }
            }
        }
    }

    // gather references from features
    CFeat_CI fci(scope, loc, CSeqFeatData::e_Pub);
    for ( ; fci; ++fci) {
        CBioseqContext::TRef ref(new CReferenceItem( fci->GetOriginalFeature(),
            *m_Current));
        refs.push_back(ref);
    }

    // add seq-submit citation
    if (m_Current->GetSubmitBlock() != NULL) {
        CBioseqContext::TRef ref(new CReferenceItem(*m_Current->GetSubmitBlock(),
            *m_Current));
        refs.push_back(ref);
    }
}


void CFlatGatherer::x_GatherCDSReferences(TReferences& refs) const
{
    _ASSERT(m_Current->IsProt());

    const CSeq_feat* cds = GetCDSForProduct(m_Current->GetHandle());
    if (cds == NULL) {
        return;
    }
    const CSeq_loc& cds_loc = cds->GetLocation();
    const CSeq_loc& cds_prod = cds->GetProduct();

    CScope& scope = m_Current->GetScope();

    CBioseq_Handle cds_seq = GetBioseqFromSeqLoc(cds_loc, scope);
    if (!cds_seq) {
        return;
    }

    // Used for, e.g., AAB59639
    // Note: This code should NOT trigger for, e.g., AAA02896
    if( m_Current->GetRepr() == CSeq_inst::eRepr_raw && 
        cds_seq.GetParentBioseq_set().CanGetClass() && 
        cds_seq.GetParentBioseq_set().GetClass() == CBioseq_set::eClass_parts ) {
        CSeq_id* primary_seq_id = m_Current->GetPrimaryId();
        if( primary_seq_id ) {
            CBioseq_Handle potential_cds_seq = scope.GetBioseqHandle( *primary_seq_id );
            if( potential_cds_seq ) {
                cds_seq = potential_cds_seq;
            }
        }
    }

    // needed for, e.g., AAB59378
    if( ! cds_seq.GetInitialSeqIdOrNull() ) {
        CConstRef<CBioseq_set> coreBioseqSet = cds_seq.GetParentBioseq_set().GetBioseq_setCore();
        if( coreBioseqSet && coreBioseqSet->CanGetSeq_set() ) {
            ITERATE( CBioseq_set_Base::TSeq_set, coreSeqSet_iter, coreBioseqSet->GetSeq_set() ) {
                if( (*coreSeqSet_iter)->IsSeq() ) {
                    const CSeq_id* coreSeqId = (*coreSeqSet_iter)->GetSeq().GetFirstId();
                    if( coreSeqId ) {
                        CBioseq_Handle potential_cds_seq = scope.GetBioseqHandle( *coreSeqId );
                        if( potential_cds_seq ) {
                            cds_seq = potential_cds_seq;
                            break;
                        }
                    }
                }
            }
        }
    }

    for (CFeat_CI it(m_Current->GetScope(), cds_loc, CSeqFeatData::e_Pub); it; ++it) {
        const CSeq_feat& feat = it->GetOriginalFeature();
        if (TestForOverlap(cds_loc, feat.GetLocation(), eOverlap_SubsetRev, kInvalidSeqPos, &scope) >= 0) {
            CBioseqContext::TRef ref(new CReferenceItem(feat, *m_Current, &cds_prod));
            refs.push_back(ref);
        }
    }

    // gather references from descriptors (top-level first)
    // (Since CSeqdesc_CI doesn't currently support bottom-to-top iteration,
    //  we approximate this by iterating over top-level, then non-top-level cds_seqs )
    for (CSeqdesc_CI it(cds_seq.GetTopLevelEntry(), CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }
    for (CSeqdesc_CI it(cds_seq, CSeqdesc::e_Pub); it; ++it) {
        // check for dups from last for-loop
        if( ! it.GetSeq_entry_Handle().HasParentEntry() ) {
            continue;
        }
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }
}

static bool
s_IsCircularTopology(CBioseqContext &ctx)
{
    const CBioseq_Handle &handle = ctx.GetHandle();
    return( handle && 
        handle.CanGetInst_Topology() && 
        handle.GetInst_Topology() == CSeq_inst::eTopology_circular );
}


void CFlatGatherer::x_GatherReferences(void) const
{
    TReferences& refs = m_Current->SetReferences();

    x_GatherReferences(m_Current->GetLocation(), refs);

    // if protein with no pubs, get pubs applicable to DNA location of CDS
    if (refs.empty()  &&  m_Current->IsProt()) {
        x_GatherCDSReferences(refs);
    }

    // re-sort references and merge/remove duplicates
    CReferenceItem::Rearrange(refs, *m_Current);

    CConstRef<IFlatItem> item;
    ITERATE (TReferences, ref, refs) {
        item.Reset( *ref );
        *m_ItemOS << item;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// COMMENTS

static bool s_NsAreGaps(const CBioseq_Handle& seq, CBioseqContext& ctx)
{
    if (!seq.IsSetInst()  ||  !seq.IsSetInst_Ext()) {
        return false;
    }

    if (ctx.IsDelta()  &&  ctx.IsWGS() &&  seq.GetInst_Ext().IsDelta()) {
        ITERATE (CDelta_ext::Tdata, iter, seq.GetInst_Ext().GetDelta().Get()) {
            const CDelta_seq& dseg = **iter;
            if (dseg.IsLiteral()) {
                const CSeq_literal& lit = dseg.GetLiteral();
                if (!lit.CanGetSeq_data()  &&  lit.CanGetLength()  &&
                     lit.GetLength() > 0 ) {
                    return true;
                }
            }
        }
    }

    return false;
}


void CFlatGatherer::x_GatherComments(void) const
{
    CBioseqContext& ctx = *m_Current;

    // There are some comments that we want to know the existence of right away, but we don't
    // want to add until later:
    CConstRef<CUser_object> firstGenAnnotSCAD = x_PrepareAnnotDescStrucComment(ctx);

    x_UnverifiedComment(ctx);

    x_AuthorizedAccessComment(ctx);

    x_MapComment(ctx);

    // Gather comments related to the seq-id
    x_IdComments(ctx, 
        ( firstGenAnnotSCAD ? eGenomeAnnotComment_No : eGenomeAnnotComment_Yes ) );
    x_RefSeqComments(ctx,
        ( firstGenAnnotSCAD ? eGenomeAnnotComment_No : eGenomeAnnotComment_Yes ) );

    if ( s_NsAreGaps(ctx.GetHandle(), ctx) ) {
        x_AddComment(new CCommentItem(CCommentItem::GetNsAreGapsStr(), ctx));
    }

    x_HistoryComments(ctx);
    x_RefSeqGenomeComments(ctx);
    x_WGSComment(ctx);
    x_TSAComment(ctx);
    if ( ctx.ShowGBBSource() ) {
        x_GBBSourceComment(ctx);
    }
    x_DescComments(ctx);
    x_MaplocComments(ctx);
    x_RegionComments(ctx);
    x_NameComments(ctx);
    x_StructuredComments(ctx);
    x_HTGSComments(ctx);
    if( ctx.ShowAnnotCommentAsCOMMENT() ) {
        x_AnnotComments(ctx);
    }
    if( firstGenAnnotSCAD ) {
        x_AddComment(new CCommentItem(*firstGenAnnotSCAD, ctx));
    }
//    x_FeatComments(ctx);

    x_RemoveDupComments();
    x_RemoveExcessNewlines();

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

void CFlatGatherer::x_RemoveDupComments(void) const
{
    // Note: we want to remove duplicate comments WITHOUT changing the order

    // holds the comments we've seen so far
    set< list<string> > setCommentsSeen;

    TCommentVec newComments;
    ERASE_ITERATE(TCommentVec, com_iter, m_Comments) {
        // add to newComments only if not seen before
        if( setCommentsSeen.find((*com_iter)->GetCommentList()) == setCommentsSeen.end() ) {
            // hasn't been seen before
            setCommentsSeen.insert((*com_iter)->GetCommentList());
            newComments.push_back(*com_iter);
        }
    }

    // swap is faster than assignment
    m_Comments.swap(newComments);
}

void CFlatGatherer::x_RemoveExcessNewlines(void) const
{
    // between each set of comments, we only want at most one line, so we compare the end
    // of one comment with the beginning of the next and trim the first as
    // necessary
    if( m_Comments.empty() ) {
        return;
    }

    for( size_t idx = 0; idx < (m_Comments.size() - 1); ++idx ) { // The "-1" is because the last comment has no comment after it
        CCommentItem & comment = *m_Comments[idx];
        const CCommentItem & next_comment = *m_Comments[idx+1];

        comment.RemoveExcessNewlines(next_comment);
    }
}

void CFlatGatherer::x_FlushComments(void) const
{
    if ( m_Comments.empty() ) {
        return;
    }
    // add a period to the last comment (if needed)
    if (m_Comments.back()->NeedPeriod()) {
        m_Comments.back()->AddPeriod();
    }

    // Remove periods after URLs
    NON_CONST_ITERATE (TCommentVec, it, m_Comments) {
        (*it)->RemovePeriodAfterURL();
    }
    
    // add a period to a GSDB comment (if exist and not last)
    TCommentVec::iterator last = m_Comments.end();
    --last;
    
    CConstRef<IFlatItem> item;
    NON_CONST_ITERATE (TCommentVec, it, m_Comments) {
        CGsdbComment* gsdb = dynamic_cast<CGsdbComment*>(it->GetPointerOrNull());
        if ( gsdb != 0   &&  it != last ) {
            gsdb->AddPeriod();
        }
        item.Reset( *it );
        *m_ItemOS << item;
    }

    m_Comments.clear();
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

void CFlatGatherer::x_UnverifiedComment(CBioseqContext& ctx) const
{
    if( ctx.GetUnverifiedType() == CBioseqContext::fUnverified_None ) {
        return;
    }

    static const string kUnverifiedPrefix = "GenBank staff is unable to verify ";
    static const string kUnverifiedSuffix = " provided by the submitter.";

    typedef pair<CBioseqContext::TUnverified, const string>  TUnverifiedElem;
    static const TUnverifiedElem sc_unverified_map[] = {
        TUnverifiedElem(CBioseqContext::fUnverified_Organism,              "source organism"),
        TUnverifiedElem(CBioseqContext::fUnverified_SequenceOrAnnotation,  "sequence and/or annotation")
    };
    typedef CStaticArrayMap<CBioseqContext::TUnverified, const string> TUnverifiedMap;
    DEFINE_STATIC_ARRAY_MAP(TUnverifiedMap, sc_UnverifiedMap, sc_unverified_map);

    vector<string> arr_type_string;
    ITERATE( TUnverifiedMap, map_iter, sc_UnverifiedMap ) {
        if( (ctx.GetUnverifiedType() & map_iter->first) != 0 ) {
            arr_type_string.push_back(map_iter->second);
        }
    }

    string type_string;
    for( size_t ii = 0; ii < arr_type_string.size(); ++ii ) {
        if( ii == 0 ) {
            // do nothing; no prefix
        } else if( ii == (arr_type_string.size() - 1) ) {
            type_string += " and ";
        } else {
            type_string += ", ";
        }
        type_string += arr_type_string[ii];
    }

    if( type_string.empty() ) {
        type_string = "[ERROR:what?]";
    }

    x_AddComment( new CCommentItem(kUnverifiedPrefix + type_string + kUnverifiedSuffix, ctx) );
}

void CFlatGatherer::x_MapComment(CBioseqContext& ctx) const
{
    const CPacked_seqpnt * pSeqpnts = ctx.GetOpticalMapPoints();
    if( ! pSeqpnts || RAW_FIELD_IS_EMPTY_OR_UNSET(*pSeqpnts, Points) ) {
        return;
    }

    string sOpticalMapComment = CCommentItem::GetStringForOpticalMap(ctx);
    if ( ! NStr::IsBlank(sOpticalMapComment) ) {
        CRef<CCommentItem> item(new CCommentItem(sOpticalMapComment, ctx));
        item->SetNeedPeriod(false);
        x_AddComment(item);
    }
}

void CFlatGatherer::x_AuthorizedAccessComment(CBioseqContext& ctx) const
{
    string sAuthorizedAccess = 
        CCommentItem::GetStringForAuthorizedAccess(ctx);
    if ( ! NStr::IsBlank(sAuthorizedAccess) ) {
        x_AddComment(new CCommentItem(sAuthorizedAccess, ctx));
    }
}

void CFlatGatherer::x_IdComments(CBioseqContext& ctx, 
    EGenomeAnnotComment eGenomeAnnotComment) const
{
    const CObject_id* local_id = 0;
    const CObject_id* file_id = 0;

    string genome_build_number =
        CGenomeAnnotComment::GetGenomeBuildNumber(ctx.GetHandle());
    bool has_ref_track_status = s_HasRefTrackStatus(ctx.GetHandle());
    CCommentItem::ECommentFormat format = ctx.Config().DoHTML() ?
        CCommentItem::eFormat_Html : CCommentItem::eFormat_Text;

    ITERATE( CBioseq::TId, id_iter, ctx.GetBioseqIds() ) {
        const CSeq_id& id = **id_iter;

        switch ( id.Which() ) {
        case CSeq_id::e_Other:
            {{
                if ( ctx.IsRSCompleteGenomic() ) {  // NC
                    if ( !genome_build_number.empty()   &&
                         !has_ref_track_status &&
                         eGenomeAnnotComment == eGenomeAnnotComment_Yes ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                else if ( ctx.IsRSContig()  ||  ctx.IsRSIntermedWGS() ) {
                    if ( ctx.IsEncode() ) {
                        string encode = CCommentItem::GetStringForEncode(ctx);
                        if ( !NStr::IsBlank(encode) ) {
                            x_AddComment(new CCommentItem(encode, ctx));
                        }
                    } else if ( !has_ref_track_status && eGenomeAnnotComment == eGenomeAnnotComment_Yes ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSPredictedProtein()  ||
                     ctx.IsRSPredictedMRna()     ||
                     ctx.IsRSPredictedNCRna()    ||
                     ctx.IsRSWGSProt() ) 
                {
                    SModelEvidance me;
                    if ( GetModelEvidance(ctx.GetHandle(), me) ) {
                        string str = CCommentItem::GetStringForModelEvidance(me, format);
                        if ( !str.empty() ) {
                            CRef<CCommentItem> item(new CCommentItem(str, ctx));
                            item->SetNeedPeriod(false);
                            x_AddComment(item);
                        }
                    }
                }
                if( ctx.IsRSUniqueProt() ) {
                    string str = CCommentItem::GetStringForUnique(ctx);
                    if( ! str.empty() ) {
                        x_AddComment(new CCommentItem(str, ctx));
                    }
                }
            }}
            break;
        case CSeq_id::e_General:
            {{
                const CDbtag& dbtag = id.GetGeneral();
                if ( STRING_FIELD_MATCH(dbtag, Db, "GSDB")  &&
                    FIELD_IS_SET_AND_IS(dbtag, Tag, Id) ) 
                {
                    x_AddGSDBComment(dbtag, ctx);
                }
                if( STRING_FIELD_MATCH(dbtag, Db, "NCBIFILE") ) {
                    file_id = &(id.GetGeneral().GetTag());
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

    if ( ctx.IsTPA()  ||  ctx.IsGED() ) {
        if ( ctx.Config().IsModeGBench()  ||  ctx.Config().IsModeDump() ) {
            if ( local_id != 0 ) {
                x_AddComment(new CLocalIdComment(*local_id, ctx));
            }
            if( file_id != 0 ) {
                x_AddComment(new CFileIdComment(*file_id, ctx));
            }
        }
    }
}


void CFlatGatherer::x_RefSeqComments(CBioseqContext& ctx,
    EGenomeAnnotComment eGenomeAnnotComment) const
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
                CCommentItem::ECommentFormat format = ctx.Config().DoHTML() ?
                    CCommentItem::eFormat_Html : CCommentItem::eFormat_Text;
                string str = 
                    CCommentItem::GetStringForRefTrack(uo, ctx.GetHandle(), format, 
                    ( eGenomeAnnotComment == eGenomeAnnotComment_Yes ? 
                      CCommentItem::eGenomeBuildComment_Yes : 
                      CCommentItem::eGenomeBuildComment_No ) );
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

static bool
s_GiInCSeq_hist_ids( const TGi gi, const CSeq_hist_rec_Base::TIds & ids )
{
    ITERATE( CSeq_hist_rec_Base::TIds, hist_iter, ids ) {
        if( (*hist_iter) && (*hist_iter)->IsGi() && (*hist_iter)->GetGi() == gi ) {
            return true;
        }
    }
    return false;
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
        if ( r.CanGetDate()  &&  !r.GetIds().empty() && 
            ! s_GiInCSeq_hist_ids( ctx.GetGI(), r.GetIds()  ) ) 
        {
            x_AddComment(new CHistComment(CHistComment::eReplaced_by,
                hist, ctx));
        }
    }

    if ( hist.IsSetReplaces()  &&  !ctx.Config().IsModeGBench() ) {
        const CSeq_hist::TReplaces& r = hist.GetReplaces();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() &&
            ! s_GiInCSeq_hist_ids( ctx.GetGI(), r.GetIds() ) ) 
        {
            x_AddComment(new CHistComment(CHistComment::eReplaces,
                hist, ctx));
        }
    }
}

void CFlatGatherer::x_RefSeqGenomeComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();

        string str = CCommentItem::GetStringForRefSeqGenome(uo);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx, &uo));
            break;
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

void CFlatGatherer::x_TSAComment(CBioseqContext& ctx) const
{
    if ( !ctx.IsTSAMaster()  ||  ctx.GetTSAMasterName().empty() ) {
        return;
    }

    if ( ctx.GetTech() == CMolInfo::eTech_tsa &&
         ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) 
    {
        string str = CCommentItem::GetStringForTSA(ctx);
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
            string comment = "Original source text: " + gbb.GetSource();
            ncbi::objects::AddPeriod(comment);
            x_AddComment(new CCommentItem(comment, ctx, &(*it)));
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


void CFlatGatherer::x_NameComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Name); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}

void CFlatGatherer::x_StructuredComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
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
            AddPeriod(str);
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
        string tech_str = GetTechString(tech);
        if (!NStr::IsBlank(tech_str)) {
            objects::AddPeriod(tech_str);
            x_AddComment(new CCommentItem("Method: " + tech_str, ctx, &(*desc)));
        }
    }
}

void CFlatGatherer::x_AnnotComments(CBioseqContext& ctx) const
{
    CAnnot_CI annot_ci(ctx.GetHandle());
    for( ; annot_ci; ++annot_ci ) {
        if( ! annot_ci->Seq_annot_IsSetDesc() ) {
            continue;
        }

         const CAnnot_descr & descr = annot_ci->Seq_annot_GetDesc();
         if( ! descr.IsSet() ) {
             continue;
         }

         const CAnnot_descr::Tdata & vec_desc = descr.Get();
         ITERATE(CAnnot_descr::Tdata, desc_iter, vec_desc) {
             const CAnnotdesc & desc = **desc_iter;
             if( ! desc.IsComment() ) {
                 continue;
             }
             x_AddComment(new CCommentItem(desc.GetComment(), ctx));
         }
    }
}

CConstRef<CUser_object> CFlatGatherer::x_PrepareAnnotDescStrucComment(CBioseqContext& ctx) const
{
    // get structured comments from Seq-annot descr user objects
    CConstRef<CUser_object> firstGenAnnotSCAD( x_GetAnnotDescStrucCommentFromBioseqHandle(ctx.GetHandle()) );

    // if not found, fall back on first far sequence component of NCBI_GENOMES records, if possible
    if( ! firstGenAnnotSCAD && ctx.IsNcbiGenomes() && 
        ctx.GetRepr() == CSeq_inst::eRepr_delta && 
        ctx.GetHandle() &&
        ctx.GetHandle().IsSetInst_Ext() &&
        ctx.GetHandle().GetInst_Ext().IsDelta() &&
        ctx.GetHandle().GetInst_Ext().GetDelta().IsSet() )
    {
        const CDelta_ext::Tdata & delta_ext = ctx.GetHandle().GetInst_Ext().GetDelta().Get();
        ITERATE(CDelta_ext::Tdata, ext_iter, delta_ext) {
            if( ! (*ext_iter)->IsLoc() ) {
                continue;
            }

            const CSeq_loc & loc = (*ext_iter)->GetLoc();
            const CSeq_id *seq_id = loc.GetId();
            if( ! seq_id ) {
                continue;
            }

            CBioseq_Handle far_bsh = ctx.GetScope().GetBioseqHandle(*seq_id);
            if( ! far_bsh ) {
                continue;
            }

            firstGenAnnotSCAD.Reset( x_GetAnnotDescStrucCommentFromBioseqHandle(far_bsh) );
            if( firstGenAnnotSCAD ) {
                return firstGenAnnotSCAD;
            }
        }
    }

    return firstGenAnnotSCAD;
}

CConstRef<CUser_object> CFlatGatherer::x_GetAnnotDescStrucCommentFromBioseqHandle( CBioseq_Handle bsh ) const
{
    CSeq_entry_Handle curr_entry_h = bsh.GetParentEntry();

    for( ; curr_entry_h ; curr_entry_h = curr_entry_h.GetParentEntry() ) { // climbs up tree

        // look on the annots
        CSeq_annot_CI annot_ci( curr_entry_h, CSeq_annot_CI::eSearch_entry ); 
        for( ; annot_ci; ++annot_ci ) {
            if( ! annot_ci->Seq_annot_CanGetDesc() ) {
                continue;
            }

            const CAnnot_descr & annot_descr = annot_ci->Seq_annot_GetDesc();
            if( ! annot_descr.IsSet() ) {
                continue;
            }

            const CAnnot_descr::Tdata & descrs = annot_descr.Get();
            ITERATE( CAnnot_descr::Tdata, descr_iter, descrs  ) {
                if( ! (*descr_iter)->IsUser() ) {
                    continue;
                }

                const CUser_object & descr_user = (*descr_iter)->GetUser();
                if( STRING_FIELD_CHOICE_MATCH(descr_user, Type, Str, "StructuredComment") )
                {
                    CConstRef<CUser_field> prefix_field = descr_user.GetFieldRef("StructuredCommentPrefix");

                    // note: case sensitive
                    if( prefix_field && 
                        FIELD_CHOICE_EQUALS(*prefix_field, Data, Str, "##Genome-Annotation-Data-START##") )
                    {
                        // we found our first match
                        return CConstRef<CUser_object>( &descr_user );
                    }
                }
            }
        }

        // not found in annots, so try the Seqdescs
        for (CSeqdesc_CI it(curr_entry_h, CSeqdesc::e_User, 1); it; ++it) {
            const CUser_object & descr_user = (*it).GetUser();
            if( STRING_FIELD_CHOICE_MATCH(descr_user, Type, Str, "StructuredComment") ) 
            {
                CConstRef<CUser_field> prefix_field = descr_user.GetFieldRef("StructuredCommentPrefix");
                if( prefix_field && 
                    FIELD_CHOICE_EQUALS(*prefix_field, Data, Str, "##Genome-Annotation-Data-START##") )
                {
                    // we found our first match
                    return CConstRef<CUser_object>( &descr_user );
                }
            }
        }
    }

    // not found
    return CConstRef<CUser_object>();
}

// add comment features that are full length on appropriate segment
void CFlatGatherer::x_FeatComments(CBioseqContext& ctx) const
{
    CScope *scope = &ctx.GetScope();
    const CSeq_loc& loc = ctx.GetLocation();

    for (CFeat_CI it(ctx.GetScope(), loc, CSeqFeatData::e_Comment);
        it; ++it) {
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
    CConstRef<IFlatItem> item;

    item.Reset( new CHtmlAnchorItem( *m_Current, "sequence") );
    *m_ItemOS << item;

    static const TSeqPos kChunkSize = 4800;

    TSeqPos size = GetLength( m_Current->GetLocation(), &m_Current->GetScope() );
    TSeqPos from = GetStart( m_Current->GetLocation(), &m_Current->GetScope() ) + 1;
    TSeqPos to = GetStop( m_Current->GetLocation(), &m_Current->GetScope() ) + 1;

    from = ( from >= 1 ? from : 1 );
    to = ( to <= size ? to : size );

    bool first = true;
    for ( TSeqPos pos = 1; pos <= to; pos += kChunkSize ) {
        TSeqPos end = min( pos + kChunkSize - 1, to );
        item.Reset( new CSequenceItem( pos, end, first, *m_Current ) );
        *m_ItemOS << item;
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

    // if SWISS-PROT, may have multiple source descriptors
    bool loop = (bool) (ctx.IsSP() || (ctx.IsCrossKingdom() && ctx.IsRSUniqueProt()));
    bool okay = false;

    // collect biosources on bioseq
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.IsSetOrg()) {
            sf.Reset(new CSourceFeatureItem(bsrc, print_range, ctx));
            srcs.push_back(sf);
            okay = true;
        }
        if(!loop  &&  okay) {
            break;
        }
    }

    // if segmented collect descriptors from local segments
    if (bh.GetInst_Repr() == CSeq_inst::eRepr_seg) {
        CTSE_Handle tse = bh.GetTSE_Handle();
        CSeqMap_CI smit(bh, SSeqMapSelector(CSeqMap::fFindRef));
        for (; smit; ++smit) {
            // biosource descriptors only on parts
            CBioseq_Handle segh = 
                scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(), tse);
            if (!segh) {
                continue;
            }

            CSeqdesc_CI src_it(CSeq_descr_CI(segh, 1), CSeqdesc::e_Source);
            for (; src_it; ++src_it) {
                CRange<TSeqPos> seg_range(smit.GetPosition(), smit.GetEndPosition());
                // collect descriptors only from the segment 
                const CBioSource& bsrc = src_it->GetSource();
                if (bsrc.IsSetOrg()) {
                    sf.Reset(new CSourceFeatureItem(bsrc, seg_range, ctx));
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
    as.SetFeatType(CSeqFeatData::e_Biosrc)
      .SetOverlapIntervals()
      .SetResolveDepth(1) // in case segmented
      .SetNoMapping(false)
      .SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());

    for ( CFeat_CI fi(bh, range, as); fi; ++fi ) {
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
            CBioseq_Handle bioseq_h;
            ITERATE( CSeq_loc, cds_loc_ci, cds_loc ) {
                bioseq_h = scope->GetBioseqHandle(cds_loc_ci.GetSeq_id());
                if( bioseq_h ) {
                    break;
                }
            }
            if( bioseq_h ) {
                x_CollectBioSourcesOnBioseq(
                    bioseq_h,
                    cds_loc.GetTotalRange(),
                    ctx,
                    srcs);
            }
        }
    }

    // if no source found create one (only if not FTable format or Dump mode)
    if ( srcs.empty()  &&  ! cfg.IsFormatFTable()  &&  ! cfg.IsModeDump() ) {
        CRef<CBioSource> bsrc(new CBioSource);
        bsrc->SetOrg();
        CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*bsrc, CRange<TSeqPos>::GetWhole(), ctx));
        srcs.push_back(sf);
    }
}

// If the loc contains NULLs between any parts, put NULLs between
// *every* part.
// If no normalization occurred, we return the original loc.
static
CConstRef<CSeq_loc> s_NormalizeNullsBetween( CConstRef<CSeq_loc> loc, bool force_adding_nulls = false )
{
    if( ! loc ) {
        return loc;
    }

    if( ! loc->IsMix() || ! loc->GetMix().IsSet() ) {
        return loc;
    }

    if( loc->GetMix().Get().size() < 2 ) {
        return loc;
    }

    bool need_to_normalize = false;
    if( force_adding_nulls ) {
        // user forces us to add NULLs
        need_to_normalize = true;
    } else {
        // first check for the common cases of not having to normalize anything
        CSeq_loc_CI loc_ci( *loc, CSeq_loc_CI::eEmpty_Allow );
        bool saw_multiple_non_nulls_in_a_row = false;
        bool last_was_null = true; // edges considered NULL for our purposes here
        bool any_null_seen = false; // edges don't count here, though
        for ( ; loc_ci ; ++loc_ci ) {
            if( loc_ci.IsEmpty() ) {
                last_was_null = true;
                any_null_seen = true;
            } else {
                if( last_was_null ) {
                    last_was_null = false;
                } else {
                    // two non-nulls in a row
                    saw_multiple_non_nulls_in_a_row = true;
                }
            }
        }

        need_to_normalize = ( any_null_seen && saw_multiple_non_nulls_in_a_row );
    }

    if( ! need_to_normalize ) {
        return loc;
    }

    // normalization is needed
    // it's very rare that we actually have to do the normalization.
    CRef<CSeq_loc> null_loc( new CSeq_loc );
    null_loc->SetNull();

    CRef<CSeq_loc> new_loc( new CSeq_loc );
    CSeq_loc_mix::Tdata &mix_data = new_loc->SetMix().Set();
    CSeq_loc_CI loc_ci( *loc, CSeq_loc_CI::eEmpty_Skip );
    for( ; loc_ci ; ++loc_ci ) {
        if( ! mix_data.empty() ) {
            mix_data.push_back( null_loc );
        }
        CRef<CSeq_loc> loc_piece( new CSeq_loc );
        loc_piece->Assign( *loc_ci.GetRangeAsSeq_loc() );
        mix_data.push_back( loc_piece );
    }

    return new_loc;
}

// assumes focus is first one in srcs
void CFlatGatherer::x_SubtractFromFocus(TSourceFeatSet& srcs ) const
{
    if ( srcs.size() < 2 ) {
        // nothing to do
        return;
    }

    CRef<CSourceFeatureItem> focus = srcs.front();
    const CSeq_loc & focus_seq_loc = focus->GetLoc();

    auto_ptr<CSeq_loc> copyOfOriginalSeqLocOfFocus( new CSeq_loc() );
    copyOfOriginalSeqLocOfFocus->Assign( focus_seq_loc );

    // check if focus is completely contained inside any other source.
    // In that case, we don't do the location subtraction from focus.
    /* ITERATE( TSourceFeatSet, it, srcs ) {
        if (it != srcs.begin()) {
            const sequence::ECompare comparison =
                sequence::Compare( focus_seq_loc, (*it)->GetLoc(), &m_Current->GetScope() );
            if( comparison == sequence::eContained || comparison == sequence::eSame ) {
                return;
            }
        }
    } */

    // subtract non-focus locations from the original focus
    NON_CONST_ITERATE(TSourceFeatSet, it, srcs) {
        if (it != srcs.begin()) {
            focus->Subtract(**it, m_Current->GetScope());
        }
    }

    // if we subtract into nothing, restore the original
    if( focus->GetLoc().GetTotalRange().GetLength() == 0 ) {
        focus->SetLoc( *copyOfOriginalSeqLocOfFocus );
        copyOfOriginalSeqLocOfFocus.release();
    }

    // if remainder is multi-interval, make it "order()" instead of "join()".
    // (We don't just test for "IsMix" because it could be a mix of one interval.
    CSeq_loc_CI focus_loc_iter = focus->GetLoc().begin();
    if( focus_loc_iter != focus->GetLoc().end() ) {
        ++focus_loc_iter;
        if( focus_loc_iter != focus->GetLoc().end() ) {
            // okay, so convert it into an order by inserting NULLs between
            CConstRef<CSeq_loc> new_focus = s_NormalizeNullsBetween( CConstRef<CSeq_loc>(&focus->GetLoc()), true );
            focus->SetLoc( *new_focus );
        }
    }
}


struct SSortSourceByLoc
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

    if (!m_Current->Config().IsModeDump()) {
        x_MergeEqualBioSources(srcs);
    }
    
    // sort by type (descriptor / feature) and location
    sort(srcs.begin(), srcs.end(), SSortSourceByLoc());

    // if the descriptor has a non-synthetic focus (by now sorted to be first),
    // subtract out all other source locations.
    if (srcs.front()->IsFocus()  &&  !srcs.front()->IsSynthetic()) {
        x_SubtractFromFocus(srcs);

        // if features completely subtracted descriptor intervals,
        // suppress in release, entrez modes.
        if ( srcs.front()->GetLoc().GetTotalRange().GetLength() == 0  &&
             m_Current->Config().HideEmptySource()  &&  srcs.size() > 1 ) {
            srcs.pop_front();
        }
    }
  
    CConstRef<IFlatItem> item;
    ITERATE( TSourceFeatSet, it, srcs ) {
        item.Reset( *it );
        *m_ItemOS << item;
    }
}


void CFlatGatherer::x_MergeEqualBioSources(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }

    // see if merging is allowed (set sourcePubFuse)
    //
    // (this code is basically copied and pasted from elsewhere.  Maybe they should all be put
    // in a shared function?)
    bool sourcePubFuse = false;
    {{
        if( m_Current->GetHandle().CanGetId() ) {
            ITERATE( CBioseq_Handle::TId, it, m_Current->GetHandle().GetId() ) {
                CConstRef<CSeq_id> seqId = (*it).GetSeqIdOrNull();
                if( ! seqId.IsNull() ) {
                    switch( seqId->Which() ) {
                        case CSeq_id_Base::e_Gibbsq:
                        case CSeq_id_Base::e_Gibbmt:
                        case CSeq_id_Base::e_Embl:
                        case CSeq_id_Base::e_Pir:
                        case CSeq_id_Base::e_Swissprot:
                        case CSeq_id_Base::e_Patent:        
                        case CSeq_id_Base::e_Ddbj:
                        case CSeq_id_Base::e_Prf:
                        case CSeq_id_Base::e_Pdb:
                        case CSeq_id_Base::e_Tpe:
                        case CSeq_id_Base::e_Tpd:
                        case CSeq_id_Base::e_Gpipe:
                            // with some types, it's okay to merge
                            sourcePubFuse = true;
                            break;                        
                        case CSeq_id_Base::e_Genbank:
                        case CSeq_id_Base::e_Tpg:
                            // Genbank allows merging only if it's the old-style 1 + 5 accessions
                            if( NULL != seqId->GetTextseq_Id() &&
                                seqId->GetTextseq_Id()->GetAccession().length() == 6 ) {
                                    sourcePubFuse = true;
                            }
                            break;
                        case CSeq_id_Base::e_not_set:
                        case CSeq_id_Base::e_Local:
                        case CSeq_id_Base::e_Other:
                        case CSeq_id_Base::e_General:
                        case CSeq_id_Base::e_Giim:                        
                        case CSeq_id_Base::e_Gi:
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }}                        

    if( ! sourcePubFuse ) {
        return;
    }

    // the following is slow ( quick eyeballing says at *least* O(n^2) ). If records
    // with lots of biosources are possible, we should consider improving it.
    // sorting, uniquing, and sorting back again would be a possible way to get O(n log(n) )
    // but you'd have to convert x_BiosourcesEqualForMergingPurposes into a "less-than" function

    // merge equal sources ( erase the later one on equality )
    // First, release the pointers of all the items we plan to remove.
    // ( because deque's erase function invalidates all iterators, so we can't erase as we go )
    TSourceFeatSet::iterator item_outer = srcs.begin();
    for( ; item_outer != srcs.end(); ++item_outer  ) {
        if( item_outer->IsNull() ) {
            continue;
        }
        TSourceFeatSet::iterator item_inner = item_outer;
        ++item_inner;
        while ( item_inner != srcs.end() ) {
            if( item_inner->IsNull() ) {
                ++item_inner;
                continue;
            }
            if( x_BiosourcesEqualForMergingPurposes( **item_outer, **item_inner ) ) {
                CRef<CSeq_loc> merged_loc = 
                    Seq_loc_Add((*item_outer)->GetLoc(), (*item_inner)->GetLoc(),
                    CSeq_loc::fMerge_All, // CSeq_loc::fSortAndMerge_All,
                    &m_Current->GetScope());
                (*item_outer)->SetLoc(*merged_loc);
                item_inner->Release(); // marked for later removal
            }
            ++item_inner;
        }
    }

    // now remove all the TSFItems that are null by copying the non-null ones to a new TSourceFeatSet
    // and swapping the deques
    TSourceFeatSet newSrcs;
    TSourceFeatSet::iterator copy_iter = srcs.begin();
    for( ; copy_iter != srcs.end(); ++copy_iter ) {
        if( ! copy_iter->IsNull() ) {
            newSrcs.push_back( *copy_iter );
        }
    }
    srcs.swap( newSrcs );
}

// "the same" means something different for merging purposes than it does
// for true equality (e.g. locations might not be the same)
// That's why we have this function.
bool CFlatGatherer::x_BiosourcesEqualForMergingPurposes( 
    const CSourceFeatureItem &src1, const CSourceFeatureItem &src2 ) const
{
    // some variables which we'll need later
    const CBioSource &biosrc1 = src1.GetSource();
    const CBioSource &biosrc2 = src2.GetSource();
    const CMappedFeat &feat1 = src1.GetFeat();
    const CMappedFeat &feat2 = src2.GetFeat();

    // focus
    if( src1.IsFocus() != src2.IsFocus() ) {
        return false;
    }

    // taxname
    const string &taxname1 = (biosrc1.IsSetTaxname() ? biosrc1.GetTaxname() : kEmptyStr);
    const string &taxname2 = (biosrc2.IsSetTaxname() ? biosrc2.GetTaxname() : kEmptyStr);
    if( taxname1 != taxname2 ) {
        return false;
    }

    // comments
    const string comment1 = ( feat1.IsSetComment() ? feat1.GetComment() : kEmptyStr );
    const string comment2 = ( feat2.IsSetComment() ? feat2.GetComment() : kEmptyStr );
    if( comment1 != comment2 ) {
        return false;
    }

    // org mods and dbs
    if( biosrc1.CanGetOrg() != biosrc2.CanGetOrg() ) {
        return false;
    }
    if( biosrc1.CanGetOrg() ) {
        const CBioSource_Base::TOrg& org1 = biosrc1.GetOrg();
        const CBioSource_Base::TOrg& org2 = biosrc2.GetOrg();

        if( org1.CanGetOrgname() != org2.CanGetOrgname() ) {
            return false;
        }
        if( org1.CanGetOrgname() ) {
            const COrg_ref_Base::TOrgname & orgname1 = org1.GetOrgname();
            const COrg_ref_Base::TOrgname & orgname2 = org2.GetOrgname();

            // check orgname mod
            if( orgname1.CanGetMod() != orgname2.CanGetMod() ) {
                return false;
            }
            if( orgname1.CanGetMod() ) {
                const COrgName_Base::TMod& orgmod1 = orgname1.GetMod();
                const COrgName_Base::TMod& orgmod2 = orgname2.GetMod();

                if( orgmod1.size() != orgmod2.size() ) {
                    return false;
                }

                if( ! equal( orgmod1.begin(), orgmod1.end(), 
                    orgmod2.begin(), COrgModEquals() ) ) {
                    return false;
                }
            }
        }

        // check dbs
        if( org1.CanGetDb() != org2.CanGetDb() ) {
            return false;
        }
        if( org1.CanGetDb() ) {
            const COrg_ref_Base::TDb& db1 = org1.GetDb();
            const COrg_ref_Base::TDb& db2 = org2.GetDb();

            if( db1.size() != db2.size() ) {
                return false;
            }

            if( ! equal( db1.begin(), db1.end(), 
                db2.begin(), CDbEquals() ) ) {
                return false;
            }
        }
    }

    // SubSources
    if( biosrc1.IsSetSubtype() != biosrc2.IsSetSubtype() ) {
        return false;
    }
    if( biosrc1.IsSetSubtype() ) { // other known to be set, too
        const CBioSource_Base::TSubtype & subtype1 = biosrc1.GetSubtype();
        const CBioSource_Base::TSubtype & subtype2 = biosrc2.GetSubtype();

        if( subtype1.size() != subtype2.size() ) {
            return false;
        }

        if( ! equal( subtype1.begin(), subtype1.end(), 
            subtype2.begin(), CSubtypeEquals() ) ) {
                return false;
        }
    }

    // for equality, make sure locations overlap or are adjacent
    // if not, they should definitely not be equal.
    const bool locations_overlap_or_touch = 
        ( s_LocationsOverlap( src1.GetLoc(), src2.GetLoc(), &src1.GetContext()->GetScope() ) ||
        s_LocationsTouch(  src1.GetLoc(), src2.GetLoc() ) );
    if( ! locations_overlap_or_touch ) {
        return false;
    }

    // no differences, so they're the same (for merging purposes)
    return true;
}

void s_SetSelection(SAnnotSelector& sel, CBioseqContext& ctx)
{
    const CFlatFileConfig& cfg = ctx.Config();

    // set feature types to be collected
    {{
        //sel.SetAnnotType(CSeq_annot::C_Data::e_Ftable);
        // source features are collected elsewhere
        sel.ExcludeFeatType(CSeqFeatData::e_Biosrc);
        // pub features are used in the REFERENCES section
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_pub);
        // some feature types are always excluded (deprecated?)
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_non_std_residue)
           .ExcludeFeatSubtype(CSeqFeatData::eSubtype_rsite)
           .ExcludeFeatSubtype(CSeqFeatData::eSubtype_seq);
        // exclude other types based on user flags
        if ( cfg.HideImpFeatures() ) {
            sel.ExcludeFeatType(CSeqFeatData::e_Imp);
        }
        if ( cfg.HideRemoteImpFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
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
        if ( cfg.HideMiscFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
        }
        if ( cfg.HideGapFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gap);
        }
        if (ctx.IsNuc()) {
            sel.ExcludeFeatType(CSeqFeatData::e_Het);
        }
    }}
    // only for non-user selector
    if (ctx.GetAnnotSelector() == NULL) {
        sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
        if (GetStrand(ctx.GetLocation(), &ctx.GetScope()) == eNa_strand_minus) {
            sel.SetSortOrder(SAnnotSelector::eSortOrder_Reverse);  // sort in reverse biological order
        } else {
            sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
        }

        if (cfg.ShowContigFeatures()) {
            sel.SetResolveAll()
                .SetAdaptiveDepth(true);
        } else {
            sel.SetLimitTSE(ctx.GetHandle().GetTSE_Handle())
                .SetResolveTSE();
        }
    }

    /// make sure we are sorting correctly
    sel.SetFeatComparator(new feature::CFeatComparatorByLabel);
}

enum EEndsOnBioseqOpt {
    // Determines whether any part of the seq-loc ends on this bioseq for it to
    // count, or that the last part must end on the seqloc.
    // There is also a little extra unexpected logic for the "last part" case.
    eEndsOnBioseqOpt_LastPartOfSeqLoc = 1,
    eEndsOnBioseqOpt_AnyPartOfSeqLoc
};

static bool s_SeqLocEndsOnBioseq(const CSeq_loc& loc, CBioseqContext& ctx, 
    EEndsOnBioseqOpt mode, CSeqFeatData::E_Choice feat_type )
{
    const bool showOutOfBoundsFeats = ctx.Config().ShowOutOfBoundsFeats();
    const bool is_part = ctx.IsPart();
    const bool is_small_genome_set = ( ctx.CanGetTLSeqEntryCtx() &&
        ctx.GetTLSeqEntryCtx().GetHasSmallGenomeSet() );

    // check certain case(s) that let us skip some work
    if( showOutOfBoundsFeats && ! is_part ) {
        return true;
    }

    const CBioseq_Handle& seq = ctx.GetHandle();
    const int seq_len = seq.GetBioseqLength();

    CSeq_loc_CI first;
    CSeq_loc_CI last;
    CSeq_loc_CI first_non_far;
    CSeq_loc_CI last_non_far;
    bool any_piece_is_on_bioseq = false;
    for ( CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological); it; ++it ) {
        if( ! any_piece_is_on_bioseq ) {
            if( seq.IsSynonym(it.GetSeq_id()) && (int)it.GetRangeAsSeq_loc()->GetStop(eExtreme_Biological) < seq_len ) {
                any_piece_is_on_bioseq = true;
                if( mode == eEndsOnBioseqOpt_AnyPartOfSeqLoc ) {
                    return true;
                }
            }
        }

        if( ! first ) {
            first = it;
        }
        last = it;

        if( ctx.IsSeqIdInSameTopLevelSeqEntry(it.GetSeq_id()) ) {
            if( ! first_non_far ) {
                first_non_far = it;
            }
            last_non_far = it;
        }
    }
    if( ! first_non_far || ! any_piece_is_on_bioseq ) {
        // no non-far pieces
        return false;
    }

    if( mode == eEndsOnBioseqOpt_AnyPartOfSeqLoc ) {
        return false;
    }

    
    if( is_small_genome_set ) {
        // if first part is on this bioseq, we're already successful
        const bool first_is_on_bioseq = (
            first == first_non_far && 
            seq.IsSynonym(first.GetSeq_id()) && 
            seq_len > (int)first.GetRangeAsSeq_loc()->GetStop(eExtreme_Biological) );
        if( first_is_on_bioseq ) {
            return true;
        }

        // for genes (and only genes), we allow the following extra laxness:
        // if first part is NOT on bioseq, but is on same TSE, then it's fine:
        if( feat_type == CSeqFeatData::e_Gene &&
            ctx.IsSeqIdInSameTopLevelSeqEntry(first.GetSeq_id()) ) 
        {
            return true;
        }

        // if first part is positive and far, last part must be on bioseq
        // and first of non-far parts must be on this bioseq.
        if( first != first_non_far && 
            first.GetStrand() != eNa_strand_minus &&
            seq.IsSynonym(last.GetSeq_id()) &&
            seq.IsSynonym(first_non_far.GetSeq_id()) )
        {
            return true;
        }

        // no test passed
        return false;
    } else {
        // first and last non-far parts must be on this bioseq
        if( ! seq.IsSynonym(first_non_far.GetSeq_id()) ||
            ! seq.IsSynonym(last_non_far.GetSeq_id()) )
        {
            return false;
        }

        // when first part is minus, then it must be on this bioseq
        // when first part is plus, then *last* piece must be on this bioseq
        const bool bMinus = (first.GetStrand() == eNa_strand_minus);
        CSeq_loc_CI part_to_check = ( bMinus ? first : last );

        const bool endsOnThisBioseq = ( part_to_check  &&  
            seq.IsSynonym(part_to_check.GetSeq_id()) );
        if( is_part ) {
            return endsOnThisBioseq;
        } else {
            if( endsOnThisBioseq ) {
                // if we're not partial, we also check that we're within range
                return seq_len > (int)part_to_check.GetRangeAsSeq_loc()->GetStop(eExtreme_Biological);
            } else {
                return false;
            }
        }
    }
}

/* gcc warning: "defined but not used"
static CSeq_loc_Mapper* s_CreateMapper(CBioseqContext& ctx)
{
    if ( ctx.GetMapper() != 0 ) {
        return ctx.GetMapper();
    }
    const CFlatFileConfig& cfg = ctx.Config();

    // do not create mapper if:
    // 1 .segmented but not doing master style.
    if (ctx.IsSegmented()  &&  !cfg.IsStyleMaster()) {
        return 0;
    } else if (!ctx.IsSegmented()) {
        // 2. not delta, or delta and supress contig featuers
        if (!ctx.IsDelta()  ||  !cfg.ShowContigFeatures()) {
            return 0;
        }
    }

    // ... otherwise
    CSeq_loc_Mapper* mapper = new CSeq_loc_Mapper(ctx.GetHandle(),
        CSeq_loc_Mapper::eSeqMap_Up);
    if (mapper != NULL) {
        mapper->SetMergeAbutting();
        mapper->KeepNonmappingRanges();
    }
    return mapper;
}
*/

static bool s_CopyCDSFromCDNA(CBioseqContext& ctx)
{
    return ctx.IsInGPS()  &&  !ctx.IsInNucProt()  &&  ctx.Config().CopyCDSFromCDNA();
}

CSeqMap_CI s_CreateGapMapIter(const CSeq_loc& loc, CBioseqContext& ctx)
{
    CSeqMap_CI gap_it;
    
    if ( !ctx.IsDelta() ) {
        return gap_it;
    }

    if (ctx.Config().HideGapFeatures()) {
        return gap_it;
    }

    CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForSeq_loc(loc, &ctx.GetScope());
    if (!seqmap) {
        ERR_POST_X(1, "Failed to create CSeqMap for gap iteration");
        return gap_it;
    }

    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindGap)   // only iterate gaps
       .SetResolveCount(1);           // starting with a Seq-loc resolve 1 level
    gap_it = CSeqMap_CI(seqmap, &ctx.GetScope(), sel);

    return gap_it;
}


static CRef<CGapItem> s_NewGapItem(CSeqMap_CI& gap_it, CBioseqContext& ctx)
{
    const static string kRegularGap  = "gap";
    const static string kAssemblyGap = "assembly_gap";

    TSeqPos pos     = gap_it.GetPosition();
    TSeqPos end_pos = gap_it.GetEndPosition();

    // attempt to find CSeq_gap info
    const CSeq_gap * pGap = NULL;
    if( gap_it.IsSetData() && gap_it.GetData().IsGap() ) {
        pGap = &gap_it.GetData().GetGap();
    } else {
        CConstRef<CSeq_literal> pSeqLiteral = gap_it.GetRefGapLiteral();
        if( pSeqLiteral && pSeqLiteral->IsSetSeq_data() )
        {
             const CSeq_data & seq_data = pSeqLiteral->GetSeq_data();
             if( seq_data.IsGap() ) {
                 pGap = &seq_data.GetGap();
             }
        }
    }


    CFastaOstream::SGapModText gap_mod_text;
    if( pGap ) {
        CFastaOstream::GetGapModText(*pGap, gap_mod_text);
    }
    const string & sType             = gap_mod_text.gap_type;
    const vector<string> & sEvidence = gap_mod_text.gap_linkage_evidences;

    // feature name depends on what quals we use
    const bool bIsAssemblyGap = ( ! sType.empty() || ! sEvidence.empty() );
    const string & sFeatName = ( bIsAssemblyGap ? kAssemblyGap : kRegularGap );

    CRef<CGapItem> retval(gap_it.IsUnknownLength() ? 
        new CGapItem(pos, end_pos, ctx, sFeatName, sType, sEvidence) :
        new CGapItem(pos, end_pos, ctx, sFeatName, sType, sEvidence, 
            gap_it.GetLength() ));
    return retval;
}


static bool s_IsDuplicateFeatures(const CSeq_feat_Handle& f1, const CSeq_feat_Handle& f2)
{
    _ASSERT(f1  &&  f2);

    const bool feats_have_same_structure =
           !f1.IsTableSNP()  &&  !f2.IsTableSNP()       &&
           f1.GetFeatSubtype() == f2.GetFeatSubtype()   &&
           f1.GetLocation().Equals(f2.GetLocation())    &&
           f1.GetSeq_feat()->Equals(*f2.GetSeq_feat());
    if( ! feats_have_same_structure ) {
        return false;
    }

    // Also need to check if on same annot (e.g. AC004755)
    const CSeq_annot_Handle &f1_annot = f1.GetAnnot();
    const CSeq_annot_Handle &f2_annot = f2.GetAnnot();
    if( f1_annot && f2_annot ) {
        if( (f1_annot == f2_annot) || 
            ( ! f1_annot.Seq_annot_CanGetDesc() && ! f2_annot.Seq_annot_CanGetDesc()  ) ) 
        {
            return true;
        }
    }

    // different Seq-annots, so they're not dups
    return false;
}


static string s_GetFeatDesc(const CSeq_feat_Handle& feat)
{
    string desc;
    feature::GetLabel(*feat.GetSeq_feat(), &desc, feature::fFGL_Both,
                      &feat.GetScope());

    // Add feature location part of label
    string loc_label;
    feat.GetLocation().GetLabel(&loc_label);
    if (loc_label.size() > 100) {
        loc_label.replace(97, NPOS, "...");
    }
    desc += loc_label;
    return desc.c_str();
}

static void s_CleanCDDFeature(const CSeq_feat& feat)
{
    /// we adjust CDD feature types based on a few simple rules
    if (feat.GetData().IsSite()  &&
        feat.GetData().GetSite() == CSeqFeatData::eSite_other  &&
        feat.GetNamedDbxref("CDD")  &&
        feat.IsSetComment()) {

        /// CDD features may have the site type encoded as a comment
        string s;
        if (feat.GetComment().find_last_not_of(" ") !=
            feat.GetComment().size() - 1) {
            s = NStr::TruncateSpaces(feat.GetComment());
        }
        const string& comment =
            (s.empty() ? feat.GetComment() : s);

        typedef pair<const char*, CSeqFeatData::ESite> TPair;
        static const TPair sc_Pairs[] = {
            TPair("acetylation site", CSeqFeatData::eSite_acetylation),
            TPair("active site", CSeqFeatData::eSite_active),
            TPair("active-site", CSeqFeatData::eSite_active),
            TPair("active_site", CSeqFeatData::eSite_active),
            TPair("binding", CSeqFeatData::eSite_binding),
            TPair("binding site", CSeqFeatData::eSite_binding),
            TPair("cleavage site", CSeqFeatData::eSite_cleavage),
            TPair("DNA binding", CSeqFeatData::eSite_dna_binding),
            TPair("DNA-binding", CSeqFeatData::eSite_dna_binding),
            TPair("DNA binding site", CSeqFeatData::eSite_dna_binding),
            TPair("DNA-binding site", CSeqFeatData::eSite_dna_binding),
            TPair("glycosylation site", CSeqFeatData::eSite_glycosylation),
            TPair("inhibitor", CSeqFeatData::eSite_inhibit),
            TPair("lipid binding site", CSeqFeatData::eSite_lipid_binding),
            TPair("lipid binding", CSeqFeatData::eSite_lipid_binding),
            TPair("metal binding", CSeqFeatData::eSite_metal_binding),
            TPair("metal-binding", CSeqFeatData::eSite_metal_binding),
            TPair("metal binding site", CSeqFeatData::eSite_metal_binding),
            TPair("metal-binding site", CSeqFeatData::eSite_metal_binding),
            TPair("modified", CSeqFeatData::eSite_modified),
            TPair("phosphorylation", CSeqFeatData::eSite_phosphorylation),
            TPair("phosphorylation site", CSeqFeatData::eSite_phosphorylation),
        };

        static const size_t kMaxPair = sizeof(sc_Pairs) / sizeof(TPair);
        for (size_t i = 0;  i < kMaxPair;  ++i) {
            if (NStr::EqualNocase(comment, sc_Pairs[i].first)) {
                //cerr << MSerial_AsnText << feat;
                CSeq_feat& f = const_cast<CSeq_feat&>(feat);
                f.SetData().SetSite(sc_Pairs[i].second);
                f.ResetComment();
            }
            else if (NStr::FindNoCase(comment, sc_Pairs[i].first) == 0) {
                //cerr << MSerial_AsnText << feat;
                CSeq_feat& f = const_cast<CSeq_feat&>(feat);
                f.SetData().SetSite(sc_Pairs[i].second);
            }
        }
    }
}

//  ============================================================================
// This determines if there are any gap features that exactly coincide over the
// given range.  This is used so we don't generate a gap twice
// (e.g. once automatically and once due to an explicit gap feature in the asn)
// Params:
// gap_start/gap_end - The range of the gap we're checking for.
// it - The iterator of features whose first feature should start at gap_start 
bool s_CoincidingGapFeatures(
    CFeat_CI it, // it's important to use a *copy* of the iterator 
                 // so we don't change the one in the caller.
    const TSeqPos gap_start, 
    const TSeqPos gap_end )
//  ============================================================================
{
    for( ; it; ++it ) {
        CConstRef<CSeq_loc> feat_loc(&it->GetLocation());

        const TSeqPos feat_start = feat_loc->GetStart(eExtreme_Positional);
        const TSeqPos feat_end   = feat_loc->GetStop (eExtreme_Positional);
        const bool featIsGap = (  it->GetFeatSubtype() == CSeqFeatData::eSubtype_gap );

        // found coinciding gap feature
        if( featIsGap && (feat_start == gap_start) && (feat_end == gap_end) ) {
            return true;
        }

        // went past the gap, so there's no coinciding gap feature after this point
        if( feat_start > gap_start ) {
            return false;
        }
    }

    return false;
}

void CFlatGatherer::x_GatherFeaturesOnLocation
(const CSeq_loc& loc,
 SAnnotSelector& sel,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();
    CFlatItemOStream& out = *m_ItemOS;

    CSeqMap_CI gap_it = s_CreateGapMapIter(loc, ctx);

    // logic to handle offsets that occur when user sets 
    // the -from and -to command-line parameters
    CRef<CSeq_loc_Mapper> slice_mapper; // NULL (unset) if no slicing
    if( ! ctx.GetLocation().IsWhole() ) {
        // build slice_mapper for mapping locations

        CSeq_id seq_id;
        seq_id.Assign( *ctx.GetHandle().GetSeqId() );

        const size_t new_len = sequence::GetLength( ctx.GetLocation(), &scope );

        CSeq_loc old_loc;
        old_loc.SetInt().SetId( seq_id );
        old_loc.SetInt().SetFrom( 0 );
        old_loc.SetInt().SetTo( new_len - 1 );

        slice_mapper.Reset( new CSeq_loc_Mapper( loc, old_loc ) );
        slice_mapper->SetFuzzOption( CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr );
        slice_mapper->TruncateNonmappingRanges();

        // skip gaps when we take slices
        gap_it = CSeqMap_CI();
    }

    // Gaps of length zero are only shown for SwissProt Genpept records
    const bool showGapsOfSizeZero = ( ctx.IsProt() && ctx.GetPrimaryId()->Which() == CSeq_id_Base::e_Swissprot );

    // cache to avoid repeated calculations
    const int loc_len = sequence::GetLength(*loc.GetId(), &ctx.GetScope() ) ;

    CSeq_feat_Handle prev_feat;
    CConstRef<IFlatItem> item;
    CFeat_CI it(scope, loc, sel);
    ctx.GetFeatTree().AddFeatures(it);
    for ( ;  it;  ++it) {
        try {
            CSeq_feat_Handle feat = it->GetSeq_feat_Handle();
            const CSeq_feat& original_feat = it->GetOriginalFeature();

            ///
            /// HACK HACK HACK
            /// we need to cleanse CDD features
            ///

            s_CleanCDDFeature(original_feat);

            ///
            /// HACK HACK HACK
            ///

            ///
            /// HACK HACK HACK
            /// we may need to assert proper product resolution
            ///

            if( slice_mapper && (feat.GetFeatSubtype() == CSeqFeatData::eSubtype_gap) && ! feat.IsPlainFeat() ) {
                // skip gaps when we take slices (i.e. "-from" and "-to" command-line args),
                // unless they're a plain feature.
                // (compare NW_001468136 (100 to 200000) and AC185591 (100 to 100000) )
                continue;
            }

            if (it->GetData().IsRna()  &&  it->IsSetProduct()) {
                vector<CMappedFeat> children =
                    ctx.GetFeatTree().GetChildren(*it);
                if (children.size() == 1  &&
                    children.front().IsSetProduct()) {

                    /// resolve sequences
                    CSeq_id_Handle rna =
                        sequence::GetIdHandle(it->GetProduct(), &scope);
                    CSeq_id_Handle prot =
                        sequence::GetIdHandle(children.front().GetProduct(),
                                              &scope);

                    CBioseq_Handle rna_bsh;
                    CBioseq_Handle prot_bsh;
                    GetResolveOrder(scope,
                                    rna, prot,
                                    rna_bsh, prot_bsh);
                }
            }

            ///
            /// HACK HACK HACK
            ///

            // supress dupliacte features
            if (prev_feat  &&  s_IsDuplicateFeatures(prev_feat, feat)) {
                continue;
            }
            prev_feat = feat;

            CConstRef<CSeq_loc> feat_loc(&it->GetLocation()); 

            // Map the feat_loc if we're using a slice (the "-from" and "-to" command-line options)
            if( slice_mapper )
            {
                feat_loc.Reset( slice_mapper->Map( *feat_loc ) );
            }

            feat_loc = s_NormalizeNullsBetween( feat_loc );
        
            // make sure location ends on the current bioseq
            if ( !s_SeqLocEndsOnBioseq(*feat_loc, ctx, eEndsOnBioseqOpt_LastPartOfSeqLoc, feat.GetData().Which() ) ) {
                // may need to map sig_peptide on a different segment
                if (feat.GetData().IsCdregion()) {
                    if (!ctx.Config().IsFormatFTable()) {
                        x_GetFeatsOnCdsProduct(original_feat, *feat_loc, ctx, slice_mapper);
                    }
                }
                continue;
            }

            // handle gaps
            const int feat_end   = feat_loc->GetStop(eExtreme_Positional);
            int feat_start = feat_loc->GetStart(eExtreme_Positional);
            if( feat_start > feat_end ) {
                feat_start -= loc_len;
            }
            while (gap_it) {
                const int gap_start = gap_it.GetPosition();
                const int gap_end   = (gap_it.GetEndPosition() - 1);

                // if feature after gap first output the gap 
                if ( feat_start >= gap_start ) {
                    // - Don't output gaps of size zero (except: see showGapsOfSizeZero's definition)
                    // - Don't output if there's an explicit gap that overlaps this one
                    const bool noGapSizeProblem = ( showGapsOfSizeZero || (gap_start <= gap_end) );
                    if( noGapSizeProblem && ! s_CoincidingGapFeatures( it, gap_start, gap_end ) ) {
                        item.Reset( s_NewGapItem(gap_it, ctx) );
                        out << item;
                    }
                    ++gap_it;
                } else {
                    break;
                }
            }

            // format feature
            item.Reset( x_NewFeatureItem(*it, ctx, feat_loc) );
            out << item;

            // Add more features depending on user preferences

            switch (feat.GetFeatSubtype()) {
                case CSeqFeatData::eSubtype_mRNA:
                {{
                    // optionally map CDS from cDNA onto genomic
                    if (s_CopyCDSFromCDNA(ctx)   &&  feat.IsSetProduct()) {
                        x_CopyCDSFromCDNA(original_feat, ctx);
                    }
                    break;
                }}
                case CSeqFeatData::eSubtype_cdregion:
                    {{  
                        // map features from protein
                        if (!ctx.Config().IsFormatFTable()) {
                            x_GetFeatsOnCdsProduct(original_feat, *feat_loc, ctx, 
                                slice_mapper,
                                CConstRef<CFeatureItem>(static_cast<const CFeatureItem*>(item.GetNonNullPointer())) );
                        }
                        break;
                    }}
                default:
                    break;
            }
        } catch (CException& e) {
            // special case: Job cancellation exceptions make us stop
            // generating features.
            if( NStr::EqualNocase(e.what(), "job cancelled") ||
                NStr::EqualNocase(e.what(), "job canceled") )
            {
                LOG_POST_X(2, Error << "Job canceled while processing feature "
                                << s_GetFeatDesc(it->GetSeq_feat_Handle())
                                << " [" << e << "]; flatfile may be truncated");
                return;
            }

            // for cases where a halt is requested, just rethrow the exception
            if( e.GetErrCodeString() == string("eHaltRequested") ) {
                throw e;
            }

            // post to log, go on to next feature
            LOG_POST_X(2, Error << "Error processing feature "
                                << s_GetFeatDesc(it->GetSeq_feat_Handle())
                                << " [" << e << "]");
        }
    }  //  end of for loop

    // when all features are done, output remaining gaps
    while (gap_it) {
        // we don't output gaps of size zero (except: see showGapsOfSizeZero)
        if( showGapsOfSizeZero || (gap_it.GetPosition() < gap_it.GetEndPosition()) ) {
            item.Reset( s_NewGapItem(gap_it, ctx) );
            out << item;
        }
        ++gap_it;
    }
}


void CFlatGatherer::x_CopyCDSFromCDNA
(const CSeq_feat& feat,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();

    CBioseq_Handle cdna;
    ITERATE( CSeq_loc, prod_loc_ci, feat.GetProduct() ) {
        cdna = scope.GetBioseqHandle( prod_loc_ci.GetSeq_id() );
        if( cdna ) {
            break;
        }
    }
    if ( !cdna ) {
        return;
    }
    // NB: There is only one CDS on an mRNA
    CFeat_CI cds(cdna, CSeqFeatData::e_Cdregion);
    if ( cds ) {
        // map mRNA location to the genomic
        CSeq_loc_Mapper mapper(feat,
                               CSeq_loc_Mapper::eProductToLocation,
                               &scope);
        CRef<CSeq_loc> cds_loc = mapper.Map(cds->GetLocation());

        CConstRef<IFlatItem> item( 
            x_NewFeatureItem(*cds, ctx, cds_loc,
                             CFeatureItem::eMapped_from_cdna) );
        *m_ItemOS << item;
    }
}

static bool
s_ContainsGaps( const CSeq_loc &loc )
{
    CSeq_loc_CI it( loc, CSeq_loc_CI::eEmpty_Allow );
    for( ; it; ++it ) {
        if( it.IsEmpty() ) {
            return true;
        }
    }
    return false;
}

void CFlatGatherer::x_GatherFeatures(void) const
{
    CBioseqContext& ctx = *m_Current;
    const CFlatFileConfig& cfg = ctx.Config();
    CFlatItemOStream& out = *m_ItemOS;
    CConstRef<IFlatItem> item;

    SAnnotSelector sel;
    SAnnotSelector* selp = &sel;
    if (ctx.GetAnnotSelector() != NULL) {
        selp = &ctx.SetAnnotSelector();
    }
    s_SetSelection(*selp, ctx);

    // optionally map gene from genomic onto cDNA
    if ( ctx.IsInGPS()  &&  cfg.CopyGeneToCDNA()  &&
         ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) {
        CMappedFeat mrna = GetMappedmRNAForProduct(ctx.GetHandle());
        if (mrna) {
            CMappedFeat gene = GetBestGeneForMrna(mrna, &ctx.GetFeatTree());
            if (gene) {
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->SetWhole(*ctx.GetPrimaryId());
                item.Reset( 
                    x_NewFeatureItem(gene, ctx, loc,
                                     CFeatureItem::eMapped_from_genomic) );
                out << item;
            }
        }
    }

    CSeq_loc loc;
    if ( ctx.GetMasterLocation() != 0 ) {
        loc.Assign(*ctx.GetMasterLocation());
    } else {
        loc.Assign(*ctx.GetHandle().GetRangeSeq_loc(0, 0));
    }

    // collect features
    if ( ctx.IsSegmented()  &&  cfg.IsStyleMaster()  &&  cfg.OldFeaturesOrder() ) {
        if (ctx.GetAnnotSelector() == NULL) {
            selp->SetResolveNone();
        }
        
        // first do the master bioeseq
        x_GatherFeaturesOnLocation(loc, *selp, ctx);

        // map the location on the segments        
        CSeq_loc_Mapper mapper(1, ctx.GetHandle(),
                               CSeq_loc_Mapper::eSeqMap_Down);
        CRef<CSeq_loc> seg_loc(mapper.Map(loc));
        if ( seg_loc ) {
            // now go over each of the segments
            for ( CSeq_loc_CI it(*seg_loc); it; ++it ) {
                x_GatherFeaturesOnLocation(it.GetEmbeddingSeq_loc(), *selp, ctx);
            }
        }
    } else {
        x_GatherFeaturesOnLocation(loc, *selp, ctx);
    }
    
    if ( ctx.IsProt() ) {
        // Also collect features which this protein is their product.
        // Currently there are only two possible candidates: Coding regions
        // and Prot features (rare).
        
        // look for the Cdregion feature for this protein
        CBioseq_Handle handle = ( ctx.CanGetMaster() ? ctx.GetMaster().GetHandle() : ctx.GetHandle() );
        CFeat_CI feat_it(handle,
            SAnnotSelector(CSeqFeatData::e_Cdregion)
            .SetByProduct()
            .SetResolveDepth(0) );
        if (feat_it) {
            CMappedFeat cds = *feat_it;

            // map CDS location to its location on the product
            CSeq_loc_Mapper mapper(*cds.GetOriginalSeq_feat(),
                CSeq_loc_Mapper::eLocationToProduct,
                &ctx.GetScope());
            mapper.SetFuzzOption( CSeq_loc_Mapper::fFuzzOption_CStyle | CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr );
            CRef<CSeq_loc> cds_prod = mapper.Map(cds.GetLocation());
            cds_prod = cds_prod->Merge( ( s_IsCircularTopology(ctx) ? CSeq_loc::fMerge_All : CSeq_loc::fSortAndMerge_All ), NULL );

            // it's a common case that we map one residue past the edge of the protein (e.g. NM_131089).
            // In that case, we shrink the cds's location back one residue.
            if( cds_prod->IsInt() && cds.GetProduct().IsWhole() ) {
                const CSeq_id *cds_prod_seq_id = cds.GetProduct().GetId();
                if( cds_prod_seq_id != NULL ) {
                    CBioseq_Handle prod_bioseq_handle = ctx.GetScope().GetBioseqHandle( *cds_prod_seq_id );
                    if( prod_bioseq_handle ) {
                        const TSeqPos bioseq_len = prod_bioseq_handle.GetBioseqLength();
                        if( cds_prod->GetInt().GetTo() >= bioseq_len ) {
                            cds_prod->SetInt().SetTo( bioseq_len - 1 );
                        }
                    }
                }
            }

            // if there are any gaps in the location, we know that there was an issue with the mapping, so
            // we fall back on the product.
            if( s_ContainsGaps(*cds_prod) ) {
                cds_prod->Assign( cds.GetProduct() );
            }

            // remove fuzz
            cds_prod->SetPartialStart( false, eExtreme_Positional );
            cds_prod->SetPartialStop ( false, eExtreme_Positional );

            item.Reset(
                x_NewFeatureItem(cds, ctx, &*cds_prod, 
                    CFeatureItem::eMapped_from_cdna) );

            out << item;
        }

        // look for Prot features (only for RefSeq records or
        // GenBank not release_mode).
        if ( ctx.IsRefSeq()  ||  !cfg.ForGBRelease() ) {
            SAnnotSelector prod_sel(CSeqFeatData::e_Prot, true);
            prod_sel.SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());
            prod_sel.SetResolveMethod(SAnnotSelector::eResolve_TSE);
            prod_sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
            CFeat_CI it(ctx.GetHandle(), prod_sel);
            ctx.GetFeatTree().AddFeatures(it);
            for ( ;  it;  ++it) {  
                item.Reset(x_NewFeatureItem(*it,
                                            ctx,
                                            &it->GetProduct(),
                                            CFeatureItem::eMapped_from_prot) );
                out << item;
            }
        }
    }
}


static bool s_IsCDD(const CSeq_feat_Handle& feat)
{
    ITERATE(CSeq_feat::TDbxref, it, feat.GetDbxref()) {
        if ( (*it)->GetType() == CDbtag::eDbtagType_CDD ) {
            return true;
        }
    }
    return false;
}


SAnnotSelector s_GetCdsProductSel(CBioseqContext& ctx)
{
    SAnnotSelector sel = ctx.SetAnnotSelector();
    sel.SetFeatSubtype(CSeqFeatData::eSubtype_region)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_site)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_bond)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_mat_peptide_aa)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_sig_peptide_aa)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_transit_peptide_aa)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_preprotein);
    return sel;
}

//  ============================================================================
void s_FixIntervalProtToCds(
    const CSeq_feat& srcFeat,
    const CSeq_loc& srcLoc,
    CRef< CSeq_loc > pDestLoc )
//  ============================================================================
{

    if ( ! pDestLoc->IsInt() ) {
        return;
    }
    CSeq_interval& destInt = pDestLoc->SetInt();

    if ( ! srcLoc.IsInt() ) {
        return;
    }
    const CSeq_interval& srcInt = srcLoc.GetInt();
    CSeq_id_Handle srcIdHandle = CSeq_id_Handle::GetHandle( srcInt.GetId());

    if ( ! srcFeat.GetData().IsCdregion() ) {
        return;
    }
    const CSeq_loc& featLoc = srcFeat.GetLocation();
    if ( ! featLoc.IsInt() ) {
        return;
    }
    const CSeq_interval& featInt = featLoc.GetInt();

    //
    //  [1] Coordinates are in peptides, need to be mapped to nucleotides.
    //  [2] Intervals are closed, i.e. [first_in, last_in].
    //  [3] Coordintates are relative to coding region + codon_start.
    //

    size_t uRawFrom = srcInt.GetFrom() * 3;
    size_t uRawTo = srcInt.GetTo() * 3 + 2;

    const CSeqFeatData::TCdregion& srcCdr = srcFeat.GetData().GetCdregion();
    if ( srcInt.CanGetStrand() ) {
        destInt.SetStrand( srcInt.GetStrand() );
    }
    if ( destInt.CanGetStrand() && destInt.GetStrand() == eNa_strand_minus ) {
        destInt.SetTo( featInt.GetTo() - uRawFrom );
        destInt.SetFrom( featInt.GetTo() - uRawTo );
    }
    else {
        destInt.SetFrom( featInt.GetFrom() + uRawFrom );
        destInt.SetTo( featInt.GetFrom() + uRawTo );
    }

    if ( srcCdr.CanGetFrame() && (srcCdr.GetFrame() != CSeqFeatData::TCdregion::eFrame_not_set) ) {
        CCdregion::TFrame frame = srcCdr.GetFrame();
        destInt.SetFrom( destInt.GetFrom() + frame -1 );
        destInt.SetTo( destInt.GetTo() + frame -1 );
    }

    if ( srcInt.CanGetFuzz_from() ) {
        if ( 3 + destInt.GetFrom() - featInt.GetFrom() < 6 ) {
            destInt.SetFrom( featInt.GetFrom() );
        }
        CRef<CInt_fuzz> pFuzzFrom( new CInt_fuzz );
        pFuzzFrom->Assign( srcInt.GetFuzz_from() );
        destInt.SetFuzz_from( *pFuzzFrom );
    }
    else {
        destInt.ResetFuzz_from();
    }

    if ( srcInt.CanGetFuzz_to() ) {
        if ( 3 + featInt.GetTo() - destInt.GetTo() < 6 ) {
            destInt.SetTo( featInt.GetTo() );
        }
        CRef<CInt_fuzz> pFuzzTo( new CInt_fuzz );
        pFuzzTo->Assign( srcInt.GetFuzz_to() );
        destInt.SetFuzz_to( *pFuzzTo );
    }
    else {
        destInt.ResetFuzz_to();
    }
}

//  ============================================================================

//  ============================================================================
void CFlatGatherer::x_GetFeatsOnCdsProduct(
    const CSeq_feat& feat,
    const CSeq_loc& mapped_loc,
    CBioseqContext& ctx,
    CRef<CSeq_loc_Mapper> slice_mapper,
    CConstRef<CFeatureItem> cdsFeatureItem ) const
//  ============================================================================
{
    const CFlatFileConfig& cfg = ctx.Config();

    if (!feat.GetData().IsCdregion()  ||  !feat.CanGetProduct()) {
        return;
    }

    if (cfg.HideCDSProdFeatures()) {
        return;
    }

    CScope& scope = ctx.GetScope();
    CConstRef<CSeq_id> prot_id(feat.GetProduct().GetId());
    if (!prot_id) {
        return;
    }
    
    CBioseq_Handle  prot;

    prot = scope.GetBioseqHandleFromTSE(*prot_id, ctx.GetHandle());
    // !!! need a flag for fetching far proteins
    if (!prot) {
        return;
    }
    CFeat_CI it(prot, s_GetCdsProductSel(ctx));
    if (!it) {
        return;
    }
    ctx.GetFeatTree().AddFeatures( it ); // !!!

    // map from cds product to nucleotide
    CSeq_loc_Mapper prot_to_cds(feat, CSeq_loc_Mapper::eProductToLocation, &scope);
    prot_to_cds.SetFuzzOption( CSeq_loc_Mapper::fFuzzOption_CStyle );
    
    CSeq_feat_Handle prev;  // keep track of the previous feature
    for ( ; it; ++it ) {
        CSeq_feat_Handle curr = it->GetSeq_feat_Handle();
        const CSeq_loc& curr_loc = curr.GetLocation();
        CSeqFeatData::ESubtype subtype = curr.GetFeatSubtype();

        if ( cfg.HideCDDFeatures()  &&
             subtype == CSeqFeatData::eSubtype_region  &&
             s_IsCDD(curr) ) {
            // passing this test prevents mapping of COG CDD region features
            continue;
        }

        // suppress duplicate features (on protein)
        if (prev  &&  s_IsDuplicateFeatures(curr, prev)) {
            continue;
        }

        ///
        /// HACK HACK HACK
        ///

        s_CleanCDDFeature(it->GetOriginalFeature());

        ///
        /// HACK HACK HACK
        ///

        // map prot location to nuc location
        CRef<CSeq_loc> loc(prot_to_cds.Map(curr_loc));
        if ( curr.IsSetData() ) {
            const CSeqFeatData& currData = curr.GetData();
            if ( currData.Which() == CSeqFeatData::e_Prot ) {
                // s_FixIntervalProtToCds( feat, curr_loc, loc ); // This line appears to be unnecessary, but I'm leaving it in in case further testing reveals I'm wrong.
            }
        }
        if (loc) {
            if (loc->IsMix()  ||  loc->IsPacked_int()) {
                // merge might turn interval into point, so we give it 2 fuzzes to prevent that
                x_GiveOneResidueIntervalsBogusFuzz(*loc);

                loc = Seq_loc_Merge(*loc, CSeq_loc::fMerge_Abutting, &scope);
                // remove the bogus fuzz we've added
                x_RemoveBogusFuzzFromIntervals(*loc);
            }
        }
        if (!loc  ||  loc->IsNull()) {
            continue;
        }
        if ( !s_SeqLocEndsOnBioseq(*loc, ctx, eEndsOnBioseqOpt_AnyPartOfSeqLoc, CSeqFeatData::e_Cdregion) ) {
            continue;
        }

        // for command-line args "-from" and "-to"
        if( slice_mapper && loc ) {
            loc = slice_mapper->Map( *loc );
            if( loc->IsNull() ) {
                continue;
            }
        }

        CConstRef<IFlatItem> item
            ( x_NewFeatureItem(*it, ctx, 
                               s_NormalizeNullsBetween(loc), CFeatureItem::eMapped_from_prot,
                               cdsFeatureItem ) );
        *m_ItemOS << item;

        prev = curr;
    }    
}

// C++ doesn't allow inner functions, so this is the best we can do
static void s_GiveOneResidueIntervalsBogusFuzz_Helper( CSeq_interval & interval )
{
    if( interval.IsSetFrom() && interval.IsSetTo() && 
        interval.GetFrom() == interval.GetTo() ) 
    {
        if( interval.IsSetFuzz_from() && ! interval.IsSetFuzz_to() ) {
            interval.SetFuzz_to().SetLim( CInt_fuzz::eLim_circle );
        } else if( ! interval.IsSetFuzz_from() && interval.IsSetFuzz_to() ) {
            interval.SetFuzz_from().SetLim( CInt_fuzz::eLim_circle );
        }
    }
}

//  ============================================================================
void CFlatGatherer::x_GiveOneResidueIntervalsBogusFuzz(CSeq_loc & loc)
//  ============================================================================
{
    if( loc.IsInt() ) {
        s_GiveOneResidueIntervalsBogusFuzz_Helper( loc.SetInt() );
    } else if ( loc.IsPacked_int() && loc.GetPacked_int().IsSet() ) {
        CPacked_seqint::Tdata & intervals = loc.SetPacked_int().Set();
        NON_CONST_ITERATE( CPacked_seqint::Tdata, int_iter, intervals ) {
            s_GiveOneResidueIntervalsBogusFuzz_Helper( **int_iter );
        }
    } else if ( loc.IsMix() && loc.GetMix().IsSet() ) {
        CSeq_loc_mix::Tdata & pieces = loc.SetMix().Set();
        NON_CONST_ITERATE(CSeq_loc_mix::Tdata, piece_iter, pieces) {
            x_GiveOneResidueIntervalsBogusFuzz(**piece_iter);
        }
    }
}

// C++ doesn't allow inner functions, so this is the best we can do
static void s_RemoveBogusFuzzFromIntervals_Helper( CSeq_interval & interval )
{
    if( interval.IsSetFuzz_from() && interval.IsSetFuzz_to() && 
        interval.IsSetFrom() && interval.IsSetTo() && 
        interval.GetFrom() == interval.GetTo() ) 
    {
        const CInt_fuzz & fuzz_from = interval.GetFuzz_from();
        const CInt_fuzz & fuzz_to   = interval.GetFuzz_to();
        if( fuzz_from.IsLim() && fuzz_from.GetLim() == CInt_fuzz::eLim_circle ) {
            interval.ResetFuzz_from();
        }
        if( fuzz_to.IsLim() && fuzz_to.GetLim() == CInt_fuzz::eLim_circle ) {
            interval.ResetFuzz_to();
        }
    }
}

//  ============================================================================
void CFlatGatherer::x_RemoveBogusFuzzFromIntervals(CSeq_loc & loc)
//  ============================================================================
{
    if( loc.IsInt() ) {
        s_RemoveBogusFuzzFromIntervals_Helper( loc.SetInt() );
    } else if ( loc.IsPacked_int() ) {
        CPacked_seqint::Tdata & intervals = loc.SetPacked_int().Set();
        NON_CONST_ITERATE( CPacked_seqint::Tdata, int_iter, intervals ) {
            s_RemoveBogusFuzzFromIntervals_Helper( **int_iter );
        }
    } else if ( loc.IsMix() && loc.GetMix().IsSet() ) {
        CSeq_loc_mix_Base::Tdata & pieces = loc.SetMix().Set();
        NON_CONST_ITERATE(CSeq_loc_mix_Base::Tdata, piece_iter, pieces) {
            x_RemoveBogusFuzzFromIntervals(**piece_iter);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// ALIGNMENTS


void CFlatGatherer::x_GatherAlignments(void) const
{
    CBioseqContext&  ctx    = *m_Current;
    CSeq_loc_Mapper* mapper = ctx.GetMapper();
    CConstRef<IFlatItem> item;
    for (CAlign_CI it(ctx.GetScope(), ctx.GetLocation());  it;  ++it) {
        if (mapper) {
            item.Reset( new CAlignmentItem(*mapper->Map(*it), ctx) );
            *m_ItemOS << item;
        } else {
            item.Reset( new CAlignmentItem(const_cast<CSeq_align&>(*it), ctx) );
            *m_ItemOS << item;
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
