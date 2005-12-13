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
 * Author: Alejandro Schaffer
 *
 */

/** @file blast_kappa.h
 * Header file for composition-based statistics
 * @todo FIXME needs doxygen comments
 */

#ifndef __BLAST_KAPPA__
#define __BLAST_KAPPA__

#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_hspstream.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Top level routine to recompute alignments for each
 *  match found by the gapped BLAST algorithm
 *  A linked list of alignments is returned (param hitList); the alignments 
 *  are sorted according to the lowest E-value of the best alignment for each
 *  matching sequence; alignments for the same matching sequence
 *  are in the list consecutively regardless of the E-value of the
 *  secondary alignments. Ties in sorted order are much rarer than
 *  for the standard BLAST method, but are broken deterministically
 *  based on the index of the matching sequences in the database.
 * @param program_number the type of blast search being performed [in]
 * @param queryBlk query sequence [in]
 * @param query_info query information [in]
 * @param sbp (Karlin-Altschul) information for search [in]
 * @param seqSrc used to fetch database/match sequences [in]
 * @param gen_code_string the genetic code for translated queries [in]
 * @param hsp_stream used to fetch hits for further processing [in]
 * @param scoringParams parameters used for scoring (matrix, gap costs etc.) [in]
 * @param extendParams parameters used for extension [in]
 * @param hitParams parameters used for saving hits [in]
 * @param psiOptions options related to psi-blast [in]
 * @param results All HSP results from previous stages of the search [in] [out]
 * @return 0 on success, otherwise failure.
*/

Int2
Blast_RedoAlignmentCore(EBlastProgramType program_number,
                  BLAST_SequenceBlk * queryBlk,
                  BlastQueryInfo* query_info,
                  BlastScoreBlk* sbp,
                  BlastHSPStream* hsp_stream,
                  const BlastSeqSrc* seqSrc,
                  const Uint1* gen_code_string,
                  BlastScoringParameters* scoringParams,
                  const BlastExtensionParameters* extendParams,
                  const BlastHitSavingParameters* hitParams,
                  const PSIBlastOptions* psiOptions,
                  BlastHSPResults* results);

#ifdef __cplusplus

}
#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.12  2005/12/13 17:57:21  gertz
 * Revert accidental checkin of the previous revision (cvs commit was run in
 * the wrong directory.)
 *
 * Revision 1.10  2005/12/01 14:47:40  madden
 * Renamed Kappa_RedoAlignmentCore as Blast_RedoAlignmentCore
 *
 * Revision 1.9  2004/11/23 21:46:03  camacho
 * Brought up to date with current version of kappa.c [by Mike Gertz]
 *
 * Revision 1.8  2004/06/21 14:53:09  madden
 * Doxygen fixes
 *
 * Revision 1.7  2004/06/16 14:53:03  dondosha
 * Moved extern "C" after the #includes
 *
 * Revision 1.6  2004/06/08 15:09:33  dondosha
 * Use BlastHSPStream interface in the engine instead of saving hits directly
 *
 * Revision 1.5  2004/05/24 17:27:37  madden
 * Doxygen fix
 *
 * Revision 1.4  2004/05/20 15:20:57  madden
 * Doxygen compliance fixes
 *
 * Revision 1.3  2004/05/19 17:03:20  madden
 * Remove (to blast_kappa.c) #defines for Xchar and StarChar
 *
 * Revision 1.2  2004/05/19 14:52:01  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.1  2004/05/18 13:22:33  madden
 * SmithWaterman and composition-based stats code
 *
 * ===========================================================================
 */

#endif /* __BLAST_KAPPA__ */

