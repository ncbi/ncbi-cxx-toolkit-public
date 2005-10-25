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
#include "dust_filter.hpp"
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
Blast_FindDustFilterLoc(TSeqLocVector& queries, 
                        const CBlastNucleotideOptionsHandle* nucl_handle)
{
    // Either non-blastn search or dust filtering not desired.
    if (nucl_handle == NULL || nucl_handle->GetDustFiltering() == false)
       return;

    NON_CONST_ITERATE(TSeqLocVector, query, queries)
    {
        CSeqVector data(*query->seqloc, *query->scope, 
                        CBioseq_Handle::eCoding_Iupac);

        CSymDustMasker duster(nucl_handle->GetDustFilteringLevel(),
                          nucl_handle->GetDustFilteringWindow(),
                          nucl_handle->GetDustFilteringLinker());
        CSeq_id& query_id = const_cast<CSeq_id&>(*query->seqloc->GetId());

        CRef<CPacked_seqint> masked_locations =
            duster.GetMaskedInts(query_id, data);
        CPacked_seqint::Tdata locs = masked_locations->Get();

        if (locs.size() > 0)
        {
           CRef<CSeq_loc> entire_slp(new CSeq_loc);
           entire_slp->SetWhole().Assign(query_id);
           CSeq_loc_Mapper mapper(*entire_slp, *query->seqloc, query->scope);
           CRef<CSeq_loc> tmp;
           tmp = NULL;
         
           const int kTopFlags = CSeq_loc::fStrand_Ignore|CSeq_loc::fMerge_All;
           ITERATE(CPacked_seqint::Tdata, masked_loc, locs)
           {
               CRef<CSeq_loc> seq_interval
                   (new CSeq_loc(query_id,
                                 (*masked_loc)->GetFrom(), 
                                 (*masked_loc)->GetTo()));
               CRef<CSeq_loc> tmp1 = mapper.Map(*seq_interval);

               if (!tmp)
               {
                   if (query->mask)
                      tmp = query->mask->Add(*tmp1, kTopFlags, NULL);
                   else
                      tmp = tmp1;
               }
               else
               {
                  tmp = tmp->Add(*tmp1, kTopFlags, NULL);
               }
           }

           CSeq_loc* tmp2 = new CSeq_loc();
           for(CSeq_loc_CI loc_it(*tmp); loc_it; ++loc_it)
           {
                 tmp2->SetPacked_int().AddInterval(query_id,
                                                   loc_it.GetRange().GetFrom(), 
                                                   loc_it.GetRange().GetTo());
           }
           query->mask.Reset(tmp2);
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
 *  Revision 1.4  2005/10/25 14:19:17  camacho
 *  dust_filter.hpp is now a private header
 *
 *  Revision 1.3  2005/09/28 18:53:06  camacho
 *  Updated to use CSymDustMasker::GetMaskedInts() interface
 *
 *  Revision 1.2  2005/08/03 18:55:21  jcherry
 *  #include dust_filter.hpp (needed for export specifier to work)
 *
 *  Revision 1.1  2005/07/19 13:46:17  madden
 *  Wrapper for calls to symmetric dust lib
 *
 *
 * ===========================================================================
 */
