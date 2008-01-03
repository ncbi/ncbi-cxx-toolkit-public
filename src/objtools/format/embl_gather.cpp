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

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/embl_gather.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CEmblGatherer::CEmblGatherer(void)
{
}


void CEmblGatherer::x_DoSingleSection(CBioseqContext& ctx) const
{
    const CFlatFileConfig& cfg = ctx.Config();
    CConstRef<IFlatItem> item;

    item.Reset( new CStartSectionItem(ctx) );
    ItemOS() << item;

    // The ID Line
    item.Reset( new CLocusItem(ctx) );
    ItemOS() << item;
    // The AC Line
    item.Reset( new CAccessionItem(ctx) );
    ItemOS() << item;
    // The SV Line
    if ( ctx.IsNuc() ) {
        item.Reset( new CVersionItem(ctx) );
        ItemOS() << item;
    }
    // The DT Line
    item.Reset( new CDateItem(ctx) );
    ItemOS() << item;
    // The DE Line
    item.Reset( new CDeflineItem(ctx) );
    ItemOS() << item;
    // The KW Line
    item.Reset( new CKeywordsItem(ctx) );
    ItemOS() << item;
    // The OS, OC, OG Lines
    item.Reset( new CSourceItem(ctx) );
    ItemOS() << item;
    // The Reference (RN, RC, RP, RX, RG, RA, RT, RL) lines
    x_GatherReferences();
    x_GatherComments();

    // Features
    item.Reset( new CFeatHeaderItem(ctx) );
    ItemOS() << item;
    if ( !cfg.HideSourceFeatures() ) {
        x_GatherSourceFeatures();
    }
    x_GatherFeatures();
    // Base count
    if ( ctx.IsNuc()  &&  (cfg.IsModeGBench()  ||  cfg.IsModeDump()) ) {
        item.Reset( new CBaseCountItem(ctx) );
        ItemOS() << item;
    }
    // Sequenece
    x_GatherSequence();
    
    item.Reset( new CEndSectionItem(ctx) );
    ItemOS() << item
;
}


END_SCOPE(objects)
END_NCBI_SCOPE
