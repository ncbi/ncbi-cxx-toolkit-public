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
* Author:  Aaron Ucko, Mati Shomrat
*
* File Description:
*   
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/format/item_ostream.hpp>
#include <objtools/format/gff_gather.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/feature_item.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//  ============================================================================
CGFFGatherer::CGFFGatherer()
//  ============================================================================
{
}

//  ============================================================================
void CGFFGatherer::Gather(
    CFlatFileContext& ctx, 
    CFlatItemOStream& os ) const
//  ============================================================================
{
    const CSeq_entry_Handle& seh = ctx.GetEntry();
    
    m_ItemOS.Reset(&os);
    m_Context.Reset(&ctx);

    CConstRef<IFlatItem> item;
    item.Reset( new CStartItem(seh) );
    os << item;
    x_GatherSeqEntry(ctx);
    item.Reset( new CEndItem() );
    os << item;
}

//  ============================================================================
void CGFFGatherer::x_DoSingleSection(
    CBioseqContext& ctx ) const
//  ============================================================================
{
    CConstRef<IFlatItem> item;

    item.Reset( new CStartSectionItem(ctx) );
    ItemOS() << item;

    item.Reset( new CDateItem(ctx) );
    ItemOS() << item;  // for UpdateDate

    item.Reset( new CLocusItem(ctx) );
    ItemOS() << item; // for strand

    if ( !ctx.Config().HideSourceFeatures() ) {
        x_GatherSourceFeatures();
    }
    x_GatherFeatures();
    if ( ctx.Config().IsFormatGFF3() ) {
        x_GatherAlignments();
    }
    item.Reset( new CBaseCountItem(ctx) );
    ItemOS() << item;

    item.Reset( new CEndSectionItem(ctx) );
    ItemOS() << item;
}

END_SCOPE(objects)
END_NCBI_SCOPE
