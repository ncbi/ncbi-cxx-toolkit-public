#ifndef ALGO_BLAST_API___SEQSRC_MULTISEQ__HPP
#define ALGO_BLAST_API___SEQSRC_MULTISEQ__HPP

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
* Author:  Ilya Dondoshansky
*
*/

/// @file seqsrc_multiseq.hpp
/// Implementation of the BlastSeqSrc interface for a vector of sequence 
/// locations.


#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** Initialize the sequence source structure.
 * @param seq_vector Vector of sequence locations [in]
 * @param program Type of BLAST to be performed [in]
 */
NCBI_XBLAST_EXPORT BlastSeqSrc* 
MultiSeqBlastSeqSrcInit(const TSeqLocVector& seq_vector, 
                        EBlastProgramType program);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/07/06 16:10:43  camacho
 * Remove out-dated comments
 *
 * Revision 1.14  2005/04/06 21:06:30  dondosha
 * Use EBlastProgramType instead of EProgram in non-user-exposed functions
 *
 * Revision 1.13  2005/03/28 20:42:44  jcherry
 * Added export specifier
 *
 * Revision 1.12  2005/01/26 21:02:57  dondosha
 * Made internal functions static, moved internal class to .cpp file
 *
 * Revision 1.11  2004/11/18 16:23:21  camacho
 * Remove unneeded header
 *
 * Revision 1.10  2004/11/17 20:20:13  camacho
 * 1. Removed GetErrorMessage function as it is no longer needed
 * 2. Moved SMultiSeqSrcNewArgs to implementation file
 * 3. Made initialization function name consistent with other BlastSeqSrc
 *    implementations.
 *
 * Revision 1.9  2004/10/06 14:50:13  dondosha
 * Removed unnecessary member field from the data structure
 *
 * Revision 1.8  2004/07/19 14:57:57  dondosha
 * Corrected file name
 *
 * Revision 1.7  2004/07/19 13:53:49  dondosha
 * Removed GetSeqLoc method
 *
 * Revision 1.6  2004/03/25 17:18:28  camacho
 * typedef not needed for C++ structs
 *
 * Revision 1.5  2004/03/23 14:14:12  camacho
 * Moved comment from source file to header
 *
 * Revision 1.4  2004/03/19 18:56:48  camacho
 * Change class & structure names to follow C++ toolkit conventions
 *
 * Revision 1.3  2004/03/15 22:34:50  dondosha
 * Added extern "C" for 2 functions to eliminate Sun compiler warnings
 *
 * Revision 1.2  2004/03/15 18:34:19  dondosha
 * Made doxygen comments and top #ifndef adhere to toolkit standard
 *
 * ===========================================================================
 */

#endif /* ALGO_BLAST_API___SEQSRC_MULTISEQ__HPP */
