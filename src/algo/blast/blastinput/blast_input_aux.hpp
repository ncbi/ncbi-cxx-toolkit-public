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
 * Author: Christiam Camacho
 *
 */

/** @file blast_input_aux.hpp
 *  Auxiliary functions for BLAST input library
 */

#ifndef ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP
#define ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP

#include <algo/blast/api/sseqloc.hpp>   /* for CBlastQueryVector */
#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** 
 * @brief Create a CArgDescriptions object and invoke SetArgumentDescriptions
 * for each of the TBlastCmdLineArgs in its argument list
 * 
 * @param args arguments to configure the return value [in]
 * 
 * @return a CArgDescriptions object with the command line options set
 */
NCBI_XBLAST_EXPORT
CArgDescriptions* 
SetUpCommandLineArguments(TBlastCmdLineArgs& args);

/** Retrieve the appropriate batch size for the specified task 
 * @program BLAST task [in]
 */
NCBI_XBLAST_EXPORT
int
GetQueryBatchSize(EProgram program);

/** Read sequence input for BLAST 
 * @param in input stream from which to read [in]
 * @param read_proteins expect proteins or nucleotides as input [in]
 * @param range range restriction to apply to sequences read [in]
 * @param sequences output will be placed here [in|out]
 * @return CScope object which contains all the sequences read
 */
NCBI_XBLAST_EXPORT
CRef<objects::CScope>
ReadSequencesToBlast(CNcbiIstream& in, 
                     bool read_proteins, 
                     const TSeqRange& range, 
                     CRef<CBlastQueryVector>& sequences);

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP */

