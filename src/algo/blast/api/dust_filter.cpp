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
 * Author: Tom Madden
 *
 * Initial Version Creation Date:  June 20, 2005
 *
 *
 * */

/// @file dust_filter.cpp
/// Calls sym dust lib in algo/dustmask and returns CSeq_locs for use by BLAST.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/api/dust_filter.hpp>
#include <serial/iterator.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>

#include <objmgr/seq_vector.hpp>

#include <algo/dustmask/symdust.hpp>

#include <string.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void
Blast_FindDustFilterLoc(TSeqLocVector& query, const CBlastNucleotideOptionsHandle* nucl_handle)
{
    // Either non-blastn search or dust filtering not desired.
    if (nucl_handle == NULL || nucl_handle->GetDustFiltering() == false)
       return;

    for (unsigned int index = 0; index<query.size(); index++)
    {
        CSeqVector data(*query[index].seqloc, *query[index].scope, CBioseq_Handle::eCoding_Iupac);

        CSymDustMasker duster(nucl_handle->GetDustFilteringLevel(),
                          nucl_handle->GetDustFilteringWindow(),
                          nucl_handle->GetDustFilteringLinker());

        std::vector< CConstRef< objects::CSeq_loc > > locs;
        duster.GetMaskedLocs(const_cast< objects::CSeq_id & > (*(*query[index].seqloc).GetId()), data, locs);


        if (locs.size() > 0)
        {
           CRef<CSeq_loc> entire_slp(new CSeq_loc);
           entire_slp->SetWhole().Assign(*(*query[index].seqloc).GetId());
           CSeq_loc_Mapper mapper(*entire_slp, *query[index].seqloc, query[index].scope);
           CRef<CSeq_loc> tmp;
           tmp = NULL;
         
           const int kTopFlags = CSeq_loc::fStrand_Ignore|CSeq_loc::fMerge_All;
           for (unsigned int loc_index = 0; loc_index<locs.size(); loc_index++)
           {
                CRef<CSeq_loc> tmp1 = mapper.Map(*locs[loc_index]);

                if (!tmp)
                {
                    if (query[index].mask)
                       tmp = (*query[index].mask).Add(*tmp1, kTopFlags, NULL);
                    else
                       tmp = tmp1;
                }
                else
                {
                   tmp = (*tmp).Add(*tmp1, kTopFlags, NULL);
                }
           }

           CSeq_loc* tmp2 = new CSeq_loc();
           for(CSeq_loc_CI loc_it(*tmp); loc_it; ++loc_it)
           {
                 tmp2->SetPacked_int().AddInterval(
                      *(*query[index].seqloc).GetId(),
                      loc_it.GetRange().GetFrom(),
                      loc_it.GetRange().GetTo());
           }
           query[index].mask.Reset(tmp2);
        }
    }

}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
 *  $Log$
 *  Revision 1.2  2005/08/03 18:55:21  jcherry
 *  #include dust_filter.hpp (needed for export specifier to work)
 *
 *  Revision 1.1  2005/07/19 13:46:17  madden
 *  Wrapper for calls to symmetric dust lib
 *
 *
 * ===========================================================================
 */
