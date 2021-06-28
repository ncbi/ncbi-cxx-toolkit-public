#ifndef ALGO_BLAST_VDB___SEQSRC_VDB__H
#define ALGO_BLAST_VDB___SEQSRC_VDB__H

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
 * Author:  Vahram Avagyan
 *
 */

/// @file blastseqsrc_sra.h
/// Blast Sequence Source implementation for the Sequence Read Archive databases.
///
/// BlastSeqSrc is an abstract data type used by Blast as a layer between
/// the Blast engine and the physical source of biological sequence data.
/// This implementation of BlastSeqSrc uses the SRA (Sequence Read Archive)
/// databases to extract and convert the nucleotide sequence data and
/// provide it to Blast through the standard BlastSeqSrc interface.
///
/// This is the top-level header file for the SRA BlastSeqSrc library.
/// It contains the main function for initializing the SRA BlastSeqSrc
/// object, which takes care of setting up the BlastSeqSrc with all the
/// correct function pointers and data structures.

// ==========================================================================//

// Blast includes
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_seqsrc.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// BlastSeqSrc initialization

/// Allocate and initialize the SRA BlastSeqSrc object.
/// @param sraRunAccessions Array of SRA run accessions to open. [in]
/// @param numRuns Number of SRA run accessions to open. [in]
/// @return Pointer to a properly initialized BlastSeqSrc object.
NCBI_VDB2BLAST_EXPORT
BlastSeqSrc*
SRABlastSeqSrcInit(const char** sraRunAccessions, Uint4 numRuns,
		           Boolean isProtein,  Boolean* excluded_runs, Uint4 * status,
		           Boolean isCSRA);

// ==========================================================================//

#ifdef __cplusplus
}
#endif

#endif /* ALGO_BLAST_VDB___SEQSRC_VDB__H */
