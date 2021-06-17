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
*   5-Column feature table data gathering  
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/ftable_gather.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFtableGatherer::CFtableGatherer(void)
{
}


void CFtableGatherer::x_DoSingleSection(CBioseqContext& ctx) const
{
    CConstRef<IFlatItem> item;

    item.Reset( new CStartSectionItem(ctx) );
    ItemOS() << item;

    item.Reset( new CFeatHeaderItem(ctx) );
    ItemOS() << item;
    if ( ctx.Config().ShowFtableRefs() ) {
        x_GatherReferences();
    }
    if ( !ctx.Config().HideSourceFeatures() ) {
        x_GatherSourceFeatures();
    }
    x_GatherFeatures();
    
    item.Reset( new CEndSectionItem(ctx) );
    ItemOS() << item;
}


END_SCOPE(objects)
END_NCBI_SCOPE
