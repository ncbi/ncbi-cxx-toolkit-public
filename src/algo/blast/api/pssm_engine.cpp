static char const rcsid[] =
    "$Id$";
/* ===========================================================================
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

/** @file blast_psi_cxx.cpp
 * Implementation of the C++ API for the PSI-BLAST PSSM generation engine.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_psi.hpp>
#include "blast_setup.hpp"

// Object includes
#include <objects/general/Object_id.hpp>
#include <objects/scoremat/Score_matrix.hpp>
#include <objects/scoremat/Score_matrix_parameters.hpp>

// Core BLAST includes
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_setup.h>
#include "../core/blast_psi_priv.h"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPssmEngine::CPssmEngine(IPssmInputData* input)
    : m_PssmInput(input), m_ScoreBlk(NULL)
{
    m_ScoreBlk = x_InitializeScoreBlock(m_PssmInput->GetQuery(),
                                        m_PssmInput->GetQueryLength());
}

CPssmEngine::~CPssmEngine()
{
    m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
}

// This method is the core of this class. It delegates the extraction of
// information from the pairwise alignments into a multiple sequence alignment
// structure to its IPsiAlignmentProcessor strategy.
// Creating the PSSM is then delegated to the core PSSM engine.
// Afterwards the results are packaged in a scoremat (structure defined from
// the ASN.1 specification).
CRef<CScore_matrix_parameters>
CPssmEngine::Run()
{
    m_PssmInput->Process();

    // @todo need protocol to request diagnostics structure contents
    PsiDiagnosticsResponse* diagnostics = NULL;
    PsiMatrix* pssm = PSICreatePSSM(m_PssmInput->GetData(),
                                    m_PssmInput->GetOptions(),
                                    m_ScoreBlk, diagnostics);

    // Convert core BLAST matrix structure into ASN.1 score matrix object
    CRef<CScore_matrix_parameters> retval(NULL);
    if (pssm) {
        retval = x_PsiMatrix2ScoreMatrix(pssm, diagnostics);
        pssm = PSIMatrixFree(pssm);
    }

    return retval;
}

unsigned char*
CPssmEngine::x_GuardProteinQuery(const unsigned char* query,
                                 unsigned int query_length)
{
    ASSERT(query);

    unsigned char* retval = NULL;
    retval = (unsigned char*) malloc(sizeof(unsigned char)*(query_length + 2));
    if ( !retval ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query with sentinels");
    }

    retval[0] = retval[query_length+1] = GetSentinelByte(BLASTP_ENCODING);
    memcpy((void*) &retval[1], (void*) query, query_length);
    return retval;
}

BlastQueryInfo*
CPssmEngine::x_InitializeQueryInfo(unsigned int query_length)
{
    BlastQueryInfo* retval = NULL;

    retval = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo));
    if ( !retval ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BlastQueryInfo");
    }

    retval->num_queries             = 1;
    retval->first_context           = 0;
    retval->last_context            = 0;
    retval->context_offsets         = (Int4*) calloc(2, sizeof(Int4));
    retval->context_offsets[1]      = query_length + 1;
    retval->eff_searchsp_array      = (Int8*) calloc(1, sizeof(Int8));
    retval->length_adjustments      = (Int4*) calloc(1, sizeof(Int4));
    retval->max_length              = query_length;

    return retval;
}

BlastScoreBlk*
CPssmEngine::x_InitializeScoreBlock(const unsigned char* query,
                                    unsigned int query_length)
{
    ASSERT(query);

    // This program type will need to be changed when psi-tblastn is
    // implemented
    const EBlastProgramType kProgramType = eBlastTypeBlastp;
    short status = 0;

    AutoPtr<unsigned char, CDeleter<unsigned char> > guarded_query;
    guarded_query.reset(x_GuardProteinQuery(query, query_length));

    // Setup the scoring options
    CBlastScoringOptions opts;
    status = BlastScoringOptionsNew(kProgramType, &opts);
    if (status != 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BlastScoringOptions");
    }
    opts->matrix = strdup("BLOSUM62"); // FIXME: shouldn't use hard coded value
    opts->matrix_path = strdup(FindMatrixPath(opts->matrix, true).c_str());

    // Setup the sequence block structure
    CBLAST_SequenceBlk query_blk;
    status = BlastSeqBlkNew(&query_blk);
    if (status != 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BLAST_SequenceBlk");
    }
    
    // Populate the sequence block structure, transferring ownership of the
    // guarded protein sequence
    status = BlastSeqBlkSetSequence(query_blk, guarded_query.release(),
                                    query_length);
    if (status != 0) {
        // should never happen, previous function only performs assignments
        abort();    
    }

    // Setup the query info structure
    CBlastQueryInfo query_info(x_InitializeQueryInfo(query_length));

    BlastScoreBlk* retval = NULL;
    Blast_Message* errors = NULL;
    const double kScaleFactor = 1.0;
    const bool kIsPhiBlast = false;
    status = BlastSetup_GetScoreBlock(query_blk,
                                      query_info,
                                      opts,
                                      kProgramType,
                                      kIsPhiBlast,
                                      &retval,
                                      kScaleFactor,
                                      &errors);
    if (status != 0) {
        NCBI_THROW(CBlastException, eInternal, errors->message);
    }

    /*********************************************************************/
    // MUST INITIALIZE THIS: should be moved to setup code
    retval->kbp_ideal = Blast_KarlinBlkIdealCalc(retval);

    return retval;
}

CRef<CScore_matrix_parameters>
CPssmEngine::x_PsiMatrix2ScoreMatrix(const PsiMatrix* pssm,
                                     const PsiDiagnosticsResponse* diagnostics)
{
    ASSERT(pssm);
    //ASSERT(diagnostics);
    if (!diagnostics) {
        NCBI_THROW(CBlastException, eInternal, "Unimplemented");
    }

    CRef<CScore_matrix_parameters> retval(new CScore_matrix_parameters);

    retval->SetLambda(diagnostics->lambda);
    retval->SetKappa(diagnostics->kappa);
    retval->SetH(diagnostics->h);

    CRef<CObject_id> pssm_id(new CObject_id);
    pssm_id->SetId(-1);     // FIXME: needs final decision on what goes here

    CScore_matrix& score_mat = retval->SetMatrix();
    score_mat.SetIs_protein(true);
    score_mat.SetIdentifier(*pssm_id);
    score_mat.SetNrows(BLASTAA_SIZE);
    score_mat.SetNcolumns(pssm->ncols);
    score_mat.SetByrow(false);

    for (unsigned int i = 0; i < pssm->ncols; i++) {
        for (unsigned int j = 0; j < BLASTAA_SIZE; j++) {
            score_mat.SetScores().push_back(pssm->pssm[i][j]);
        }
    }

    if (diagnostics->res_freqs) {
        for (unsigned int i = 0; i < pssm->ncols; i++) {
            for (unsigned int j = 0; j < BLASTAA_SIZE; j++) {
                score_mat.SetPosFreqs().push_back(pssm->res_freqs[i][j]);
            }
        }
    }
 
    /// FIXME: need to move sequence weights to diagnostics structure
    if (diagnostics->sequence_weights) {

        for (unsigned int i = 0; 
             i < diagnostics->dimensions->num_seqs + 1; 
             i++) {
            score_mat.SetWeights().push_back(diagnostics->sequence_weights[i]);
        }

    }

#if 0
    /// FIXME: need to move raw residue counts (frequencies) from
    /// PsiAlignmentData structure to diagnostics structure
    if (diagnostics->res_freqs) {

        for (unsigned int i = 0; 
             i < diagnostics->dimensions->num_seqs + 1; 
             i++) {
            score_mat.SetRawFreqs().push_back(diagnostics->res_counts[i]);
        }

    }
#endif

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.8  2004/08/02 13:42:41  camacho
 * Fix warning
 *
 * Revision 1.7  2004/08/02 13:29:00  camacho
 * +Initialization of BlastScoreBlk and population of Score_matrix structure
 *
 * Revision 1.6  2004/07/29 17:54:29  camacho
 * Redesigned to use strategy pattern
 *
 * Revision 1.5  2004/06/21 12:53:05  camacho
 * Replace PSI_ALPHABET_SIZE for BLASTAA_SIZE
 *
 * Revision 1.4  2004/06/09 14:32:23  camacho
 * Added use_best_alignment option
 *
 * Revision 1.3  2004/05/28 17:42:02  camacho
 * Fix compiler warning
 *
 * Revision 1.2  2004/05/28 17:15:43  camacho
 * Fix NCBI {BEGIN,END}_SCOPE macro usage, remove warning
 *
 * Revision 1.1  2004/05/28 16:41:39  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
