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


CGFFGatherer::CGFFGatherer(void)
{
}


void CGFFGatherer::x_DoSingleSection(CBioseqContext& ctx) const
{
    ItemOS() << new CStartSectionItem(ctx);

    ItemOS() << new CDateItem(ctx);  // for UpdateDate
    ItemOS() << new CLocusItem(ctx); // for strand
    if ( !ctx.Config().HideSourceFeats() ) {
        x_GatherSourceFeatures();
    }
    x_GatherFeatures();
    ItemOS() << new CBaseCountItem(ctx);
    x_GatherSequence();

    ItemOS() << new CEndSectionItem(ctx);
}



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2004/04/22 16:02:23  shomrat
* Changes in context
*
* Revision 1.6  2004/03/31 17:17:47  shomrat
* Active bioseq set outside method
*
* Revision 1.5  2004/03/25 20:43:28  shomrat
* Use handles
*
* Revision 1.4  2004/02/19 18:15:25  shomrat
* supress source-features if flag is set
*
* Revision 1.3  2004/02/11 22:53:33  shomrat
* using types in flag file
*
* Revision 1.2  2004/02/11 16:53:57  shomrat
* separate gather of features and source-features
*
* Revision 1.1  2004/01/14 16:07:18  shomrat
* Initial Revision
*
*
* ===========================================================================
*/
