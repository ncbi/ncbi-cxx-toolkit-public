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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objtools/format/items/genome_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGenbankGatherer::CGenbankGatherer(void)
{
}


bool s_ShowBaseCount(const CFlatFileConfig& cfg)
{ 
    return (cfg.IsModeDump()  ||  cfg.IsModeGBench());
}


bool s_ShowContig(CBioseqContext& ctx)
{
    if ( (ctx.IsSegmented()  &&  ctx.HasParts())  ||
         (ctx.IsDelta()  &&  !ctx.IsDeltaLitOnly()) ) {
        return true;
    }
    return false;
}


// FOR_EACH_SEQID_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with CSeq_id_Handle sid = *Itr;

#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

void CGenbankGatherer::x_DoSingleSection(CBioseqContext& ctx) const
{
    CConstRef<IFlatItem> item;
    const CFlatFileConfig& cfg = ctx.Config();

    // these macros make the code easier to read and less repetitive
#define GATHER_BLOCK(BlockType, ItemClassName) \
    if( cfg.IsShownGenbankBlock(CFlatFileConfig::fGenbankBlocks_##BlockType) ) { \
        item.Reset( new ItemClassName(ctx) ); \
        ItemOS() << item; \
    }

#define GATHER_ANCHOR(BlockType, block_str) \
    if( cfg.IsShownGenbankBlock(CFlatFileConfig::fGenbankBlocks_##BlockType) ) { \
        item.Reset( new CHtmlAnchorItem(ctx, (block_str) ) ); \
        ItemOS() << item; \
    }

#define GATHER_VIA_FUNC(BlockType, FuncName) \
    if( cfg.IsShownGenbankBlock(CFlatFileConfig::fGenbankBlocks_##BlockType) ) { \
        FuncName(); \
    }

    // if there's a callback, let it know we've entered another bioseq
    if( cfg.GetGenbankBlockCallback() ) {
        CFlatFileConfig::CGenbankBlockCallback::EBioseqSkip eBioseqSkip =
            cfg.GetGenbankBlockCallback()->notify_bioseq( ctx );
        if( eBioseqSkip == CFlatFileConfig::CGenbankBlockCallback::eBioseqSkip_Yes ) {
            return;
        }
    }

    // gather needed blocks
    GATHER_BLOCK(Head, CStartSectionItem);
    GATHER_ANCHOR(Locus, "locus");
    GATHER_BLOCK(Locus, CLocusItem);
    GATHER_BLOCK(Defline, CDeflineItem);
    GATHER_BLOCK(Accession, CAccessionItem);
    GATHER_BLOCK(Version, CVersionItem);
    GATHER_BLOCK(Project, CGenomeProjectItem);

    if ( ctx.IsProt() ) {
        bool show_dbsource = true;
        CBioseq_Handle& bsh = ctx.GetHandle();
        FOR_EACH_SEQID_ON_BIOSEQ_HANDLE (sid_itr, bsh) {
            CSeq_id_Handle sid = *sid_itr;
            switch (sid.Which()) {
                case NCBI_SEQID(Other):
                {
                    CConstRef<CSeq_id> id = sid.GetSeqId();
                    const CTextseq_id& tsid = *id->GetTextseq_Id ();
                    if (tsid.IsSetAccession()) {
                        const string& acc = tsid.GetAccession ();
                        if (NStr::StartsWith (acc, "WP_")) {
                            show_dbsource = false;
                        }
                    }
                    break;
                }
                default:
                    break;
           }
        }
        if (show_dbsource) {
            GATHER_BLOCK(Dbsource, CDBSourceItem);
        }
    }
    GATHER_BLOCK(Keywords, CKeywordsItem);
    if ( ctx.IsPart() ) {
        GATHER_BLOCK(Segment, CSegmentItem);
    }
    // GATHER_BLOCK(Source, CSourceItem);
    GATHER_VIA_FUNC(Sourcefeat, x_GatherSourceOrganism);
    GATHER_VIA_FUNC(Reference, x_GatherReferences);
    GATHER_ANCHOR(Comment, "comment");
    GATHER_VIA_FUNC(Comment, x_GatherComments);
    GATHER_BLOCK(Primary, CPrimaryItem);
    GATHER_ANCHOR(Featheader, "feature");
    GATHER_BLOCK(Featheader, CFeatHeaderItem);
    if ( !cfg.HideSourceFeatures() ) {
        GATHER_VIA_FUNC(Sourcefeat, x_GatherSourceFeatures);
    }
    const bool bIsMap = (ctx.GetRepr() == CSeq_inst::eRepr_map);
    if ( ctx.IsWGSMaster()  &&  ctx.GetTech() == CMolInfo::eTech_wgs ) {
        GATHER_VIA_FUNC(Wgs, x_GatherWGS);
    } else if( ctx.IsTSAMaster()  &&
               ctx.GetTech() == CMolInfo::eTech_tsa &&
               ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) 
    {
        // Yes, the TSA info is considered a kind of PRIMARY block
        GATHER_VIA_FUNC(Tsa, x_GatherTSA);
    } else if ( ctx.DoContigStyle() ) {
        if ( cfg.ShowContigFeatures() ) {
            GATHER_VIA_FUNC(FeatAndGap, x_GatherFeatures);
        }
        else if ( cfg.IsModeEntrez() ) {
            size_t size = sequence::GetLength( m_Current->GetLocation(), &m_Current->GetScope() );
            if ( size <= cfg.SMARTFEATLIMIT ) {
                GATHER_VIA_FUNC(FeatAndGap, x_GatherFeatures);
            }
        }
        GATHER_ANCHOR(Contig, "contig");
        GATHER_BLOCK(Contig, CContigItem);
        if ( cfg.ShowContigAndSeq() ) {
            if ( ctx.IsNuc() && ! bIsMap && s_ShowBaseCount(cfg) )
            {
                GATHER_BLOCK(Basecount, CBaseCountItem);
            }
            if( ! bIsMap ) {
                GATHER_BLOCK(Origin, COriginItem);
                GATHER_VIA_FUNC(Sequence, x_GatherSequence);
            }
        }
    } else {
        GATHER_VIA_FUNC(FeatAndGap, x_GatherFeatures);
        if ( cfg.ShowContigAndSeq()  &&  s_ShowContig(ctx) ) {
            GATHER_ANCHOR(Contig, "contig");
            GATHER_BLOCK(Contig, CContigItem);
        }
        if ( ctx.IsNuc()  && ! bIsMap && s_ShowBaseCount(cfg) ) 
        {
            GATHER_BLOCK(Basecount, CBaseCountItem);
        }
        if( ! bIsMap ) {
            GATHER_BLOCK(Origin, COriginItem);
            GATHER_VIA_FUNC(Sequence, x_GatherSequence);
        }
    }
    GATHER_BLOCK(Slash, CEndSectionItem);
}


void CGenbankGatherer::x_GatherWGS(void) const
{
    CBioseqContext& ctx = *m_Current;

    const string* first = 0;
    const string* last  = 0;

    bool bFirstWgsItem = true;
    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();
        CWGSItem::EWGSType wgs_type = CWGSItem::eWGS_not_set;

        if ( !uo.GetType().IsStr() ) {
            continue;
        }
        const string& type = uo.GetType().GetStr();
        if ( NStr::CompareNocase(type, "WGSProjects") == 0 ) {
            wgs_type = CWGSItem::eWGS_Projects;
        } else if ( NStr::CompareNocase(type, "WGS-Scaffold-List") == 0 ) {
            wgs_type = CWGSItem::eWGS_ScaffoldList;
        } else if ( NStr::CompareNocase(type, "WGS-Contig-List") == 0 ) {
            wgs_type = CWGSItem::eWGS_ContigList;
        }

        if ( wgs_type == CWGSItem::eWGS_not_set ) {
            continue;
        }

        ITERATE (CUser_object::TData, it, uo.GetData()) {
            if ( !(*it)->GetLabel().IsStr() ) {
                continue;
            }
            const string& label = (*it)->GetLabel().GetStr();
            if ( NStr::CompareNocase(label, "WGS_accession_first") == 0  ||
                 NStr::CompareNocase(label, "Accession_first") == 0 ) {
                first = &((*it)->GetData().GetStr());
            } else if ( NStr::CompareNocase(label, "WGS_accession_last") == 0 ||
                        NStr::CompareNocase(label, "Accession_last") == 0 ) {
                last = &((*it)->GetData().GetStr());
            }
        }

        if ( (first != 0)  &&  (last != 0) ) {
            if( bFirstWgsItem ) {
                CConstRef<IFlatItem> anchor_item( new CHtmlAnchorItem(ctx, "wgs" ) );
                ItemOS() << anchor_item; 
                bFirstWgsItem = false;
            }
            CConstRef<IFlatItem> item( new CWGSItem(wgs_type, *first, *last, uo, ctx) );  
            ItemOS() << item;
        }
    }    
}

void CGenbankGatherer::x_GatherTSA(void) const
{
    CBioseqContext& ctx = *m_Current;

    const string* first = 0;
    const string* last  = 0;

    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();
        CTSAItem::ETSAType tsa_type = CTSAItem::eTSA_not_set;

        if ( !uo.GetType().IsStr() ) {
            continue;
        }
        const string& type = uo.GetType().GetStr();
        if ( NStr::EqualNocase(type, "TSA-mRNA-List") ||
             NStr::EqualNocase(type, "TSA-RNA-List") )
        {
            tsa_type = CTSAItem::eTSA_Projects;
        }

        if ( tsa_type == CTSAItem::eTSA_not_set ) {
            continue;
        }

        ITERATE (CUser_object::TData, it, uo.GetData()) {
            if ( !(*it)->GetLabel().IsStr() ) {
                continue;
            }
            const string& label = (*it)->GetLabel().GetStr();
            if ( NStr::CompareNocase(label, "TSA_accession_first") == 0  ||
                 NStr::CompareNocase(label, "Accession_first") == 0 ) {
                first = &((*it)->GetData().GetStr());
            } else if ( NStr::CompareNocase(label, "TSA_accession_last") == 0 ||
                        NStr::CompareNocase(label, "Accession_last") == 0 ) {
                last = &((*it)->GetData().GetStr());
            }
        }

        if ( (first != 0)  &&  (last != 0) ) {
            CConstRef<IFlatItem> item( new CTSAItem(tsa_type, *first, *last, uo, ctx) );  
            ItemOS() << item;
        }
    }    
}

END_SCOPE(objects)
END_NCBI_SCOPE
