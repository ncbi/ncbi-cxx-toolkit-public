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
 * Author:  Christiam Camacho
 *
 */

/** @file psiblast_aux_priv.hpp
 * Auxiliary setup functions for Blast objects interface
 */

#ifndef ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP
#define ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP

#include <corelib/ncbiobj.hpp>
#include <list>

/** @addtogroup AlgoBlast
 *
 * @{
 */

struct BlastScoreBlk;
struct PSIBlastOptions;

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class CBlastOptions;

/** Setup CORE BLAST score block structure with data from the scoremat PSSM.
 * @param score_blk BlastScoreBlk structure to set up [in|out]
 * @param pssm scoremat PSSM [in]
 */
void PsiBlastSetupScoreBlock(BlastScoreBlk* score_blk,
                             CConstRef<objects::CPssmWithParameters> pssm);

/** Given a PSSM with frequency ratios and options, invoke the PSSM engine to
 * compute the scores.
 * @param pssm object containing the PSSM's frequency ratios [in|out]
 * @param opts PSSM engine options [in]
 */
void PsiBlastComputePssmScores(CRef<objects::CPssmWithParameters> pssm,
                               const CBlastOptions& opts);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP */
