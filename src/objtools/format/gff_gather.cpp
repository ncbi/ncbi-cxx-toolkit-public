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


void CGFFGatherer::x_DoSingleSection
(const CBioseq& seq) const
{
    CFFContext& ctx = Context();

    ctx.SetActiveBioseq(seq);

    if ( (ctx.IsNa()  &&  
          ((ctx.GetFilterFlags() & CFlatFileGenerator::fSkipNucleotides) != 0))  ||
         (ctx.IsProt()  &&
          ((ctx.GetFilterFlags() & CFlatFileGenerator::fSkipProteins) != 0)) ) {
        return;
    }

    ItemOS() << new CStartSectionItem(ctx);

    ItemOS() << new CDateItem(ctx);  // for UpdateDate
    ItemOS() << new CLocusItem(ctx); // for strand
    //x_GatherSourceFeatures();
    x_GatherFeatures(true);  // !!!temporary
    x_GatherFeatures(false);
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
* Revision 1.1  2004/01/14 16:07:18  shomrat
* Initial Revision
*
*
* ===========================================================================
*/
