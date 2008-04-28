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
* File Name: winmask_filter.hpp
*
* Author: Kevin Bealer
*
* File Creation Date:  04/18/2008
*
*/

/** @file winmask_filter.hpp
 *     Blast wrappers for WindowMasker filtering.
 */

#ifndef ALGO_BLAST_API___WINMASK_FILTER_HPP 
#define ALGO_BLAST_API___WINMASK_FILTER_HPP 

#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


/// Find Window Masker filtered locations by database name.
/// @param query These queries will be masked. [in|out]
/// @param lstat Filename of the WindowMasker database. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(CBlastQueryVector& query, const string & lstat);


/// Find Window Masker filtered locations on TSeqLocVector by database name.
/// @param query These queries will be masked. [in|out]
/// @param lstat Filename of the WindowMasker database. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(TSeqLocVector & query, const string & lstat);


/// Find Window Masker filtered locations using a BlastOptionsHandle.
/// @param query These queries will be masked. [in|out]
/// @param opts_handle This provides the database name or taxid. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(CBlastQueryVector& query,
                          const CBlastOptionsHandle* opts_handle);


/// Find Window Masker filtered locations using a BlastOptionsHandle.
/// @param query These queries will be masked. [in|out]
/// @param opts_handle This provides the database name or taxid. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(TSeqLocVector & query,
                          const CBlastOptionsHandle* opts_handle);


/// Find Window Masker filtered locations using a BlastOptions.
/// @param query These queries will be masked. [in|out]
/// @param opts_handle This provides the database name or taxid. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(CBlastQueryVector   & query,
                          const CBlastOptions * opts_handle);


/// Find Window Masker filtered locations using BlastOptions.
/// @param query These queries will be masked. [in|out]
/// @param opts This provides the database name or taxid. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLoc(TSeqLocVector       & query,
                          const CBlastOptions * opts);


/// Find Window Masker filtered locations by taxonomic ID.
/// @param query These queries will be masked. [in|out]
/// @param taxid This taxid will be used to find a database. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLocTaxId(CBlastQueryVector& query, int taxid);


/// Find Window Masker filtered locations on a TSeqLocVector by Taxid.
/// @param query These queries will be masked. [in|out]
/// @param taxid This taxid will be used to find a database. [in]
NCBI_XBLAST_EXPORT
void
Blast_FindWindowMaskerLocTaxId(TSeqLocVector & query, int taxid);


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___WINMASK_FILTER__HPP */

