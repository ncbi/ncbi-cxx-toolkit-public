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
*/

/*****************************************************************************

File name: blast_kappa.h

Author: Alejandro Schaffer

Contents: header file for composition-based statistics

$Revision$

*****************************************************************************/
/*
* $Log$
* Revision 1.1  2004/05/18 13:22:33  madden
* SmithWaterman and composition-based stats code
*
 *
*/

#ifndef __BLAST_KAPPA__
#define __BLAST_KAPPA__

#ifdef __cplusplus
extern "C" {
#endif

#define Xchar   21    /*character for low-complexity columns*/
#define StarChar   25    /*character for stop codons*/

Int2
Kappa_RedoAlignmentCore(BLAST_SequenceBlk * query_blk,
                  BlastQueryInfo* query_info,
                  BlastScoreBlk* sbp,
                  BlastHitList* hit_list,
                  const BlastSeqSrc* seq_src,
                  BlastScoringParameters* scoringParams,
                  const BlastExtensionParameters* extendParams,
                  const BlastHitSavingParameters* hitsavingParams,
                  const PSIBlastOptions* psi_options);

#ifdef __cplusplus

}
#endif

#endif /* __BLAST_KAPPA__ */

