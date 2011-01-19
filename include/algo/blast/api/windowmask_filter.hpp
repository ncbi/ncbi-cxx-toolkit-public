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
*/

/** @file windowmask_filter.hpp
 *  Interface to retrieve list of available windowmasker filtering
 */

#ifndef ALGO_BLAST_API___WINDOWMASK_FILTER_HPP 
#define ALGO_BLAST_API___WINDOWMASK_FILTER_HPP 

#include <algo/blast/core/blast_export.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// This function returns a list of NCBI taxonomy IDs for which there exists
/// windowmasker masking data to support organism specific filtering.
NCBI_XBLAST_EXPORT
void GetTaxIdWithWindowMaskerSupport(set<int>& supported_taxids);

/// Get the windowmasker file path for a given taxid
/// @param taxid NCBI taxonomy ID to get windowmasker files for [in]
/// @return empty string if not found
NCBI_XBLAST_EXPORT string WindowMaskerTaxidToDb(int taxid);

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___WINDOWMASK_FILTER_HPP */
