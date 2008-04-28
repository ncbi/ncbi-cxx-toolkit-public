/* $Id$
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
* File Name: repeats_filter.hpp
*
* Author: Ilya Dondoshansky
*
* Version Creation Date:  12/12/2003
*
 */

/** @file repeats_filter.hpp
 *     C++ implementation of repeats filtering for C++ BLAST.
 */

#ifndef ALGO_BLAST_API___REPEATS_FILTER_HPP 
#define ALGO_BLAST_API___REPEATS_FILTER_HPP 

#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** Finds repeats locations for a given set of sequences. The locations are
 * saved in the respective fields of the SSeqLoc structures. If previous masks
 * exist, they are combined with the new masks.
 * opts_handle will be downcast to CBlastNucleotideOptionsHandle and if that succeeds
 * and repeat filtering is specified then it will be run, otherwise it just returns.
 * @param query_loc Vector of sequence locations. [in] [out]
 * @param opts_handle options handle for blast search [in]
 */
NCBI_XBLAST_EXPORT
void
Blast_FindRepeatFilterLoc(TSeqLocVector& query_loc, 
                          const CBlastOptionsHandle* opts_handle);

/** Overloaded version of the function above which takes the name of the
 * repeats filtering database to use, and a TSeqLocVector.
 * @param query Vector of sequence locations. [in] [out]
 * @param filter_db Name of the BLAST database with repeats to use for
 * filtering [in]
 */
NCBI_XBLAST_EXPORT
void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, const char* filter_db);


/** Overloaded version of the function above which takes the name of the
 * repeats filtering database to use, and a CBlastQueryVector.
 * @param query Vector of sequence locations. [in] [out]
 * @param filter_db Name of the BLAST database with repeats to use for
 * filtering [in]
 */
NCBI_XBLAST_EXPORT
void
Blast_FindRepeatFilterLoc(CBlastQueryVector& query, const char* filter_db);


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___BLAST_OPTION__HPP */
