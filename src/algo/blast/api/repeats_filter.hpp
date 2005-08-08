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

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2005/08/08 16:28:17  dondosha
* Removed unused header
*
* Revision 1.7  2005/05/24 12:56:57  camacho
* Doxygen fixes
*
* Revision 1.6  2005/03/31 20:43:14  madden
* Blast_FindRepeatFilterLoc now takes CBlastOptionsHandle rather than char*
*
* Revision 1.5  2005/03/29 15:58:54  dondosha
* Added blast scope
*
* Revision 1.4  2005/02/08 20:34:49  dondosha
* Moved auxiliary functions and definitions for repeats filtering from C++ api into core; renamed FindRepeatFilterLoc into Blast_FindRepeatFilterLoc
*
* Revision 1.3  2004/08/11 11:59:07  ivanov
* Added export specifier NCBI_XBLAST_EXPORT
*
* Revision 1.2  2004/07/02 19:52:51  dondosha
* Added doxygen comments
*
* Revision 1.1  2004/06/15 19:09:10  dondosha
* Repeats filtering code, moved from internal/blast/SplitDB/blastd
*
* ===========================================================================
*/

#endif /* ALGO_BLAST_API___BLAST_OPTION__HPP */
