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

    ItemOS() << new CStartSectionItem(ctx);

    // The ID Line
    ItemOS() << new CLocusItem(ctx);
    // The AC Line
    ItemOS() << new CAccessionItem(ctx);
    // The SV Line
    if ( ctx.IsNuc() ) {
        ItemOS() << new CVersionItem(ctx);
    }
    // The DT Line
    ItemOS() << new CDateItem(ctx);
    // The DE Line
    ItemOS() << new CDeflineItem(ctx);
    // The KW Line
    ItemOS() << new CKeywordsItem(ctx);
    // The OS, OC, OG Lines
    ItemOS() << new CSourceItem(ctx);
    // The Reference (RN, RC, RP, RX, RG, RA, RT, RL) lines
    x_GatherReferences();
    x_GatherComments();

    // Features
    ItemOS() << new CFeatHeaderItem(ctx);
    if ( !cfg.HideSourceFeatures() ) {
        x_GatherSourceFeatures();
    }
    x_GatherFeatures();
    // Base count
    if ( ctx.IsNuc()  &&  (cfg.IsModeGBench()  ||  cfg.IsModeDump()) ) {
        ItemOS() << new CBaseCountItem(ctx);
    }
    // Sequenece
    x_GatherSequence();
    
    ItemOS() << new CEndSectionItem(ctx);
}


END_SCOPE(objects)
END_NCBI_SCOPE
