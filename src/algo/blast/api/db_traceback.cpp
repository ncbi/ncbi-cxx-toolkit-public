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
 * File Description:
 *   Class interface for running BLAST traceback only, given the precomputed
 *   preliminary HSP results.
 *
 * ===========================================================================
 */
#include <algo/blast/api/db_traceback.hpp>
#include "blast_setup.hpp"
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_message.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CDbBlastTraceback::CDbBlastTraceback(const TSeqLocVector& queries, 
                       BlastSeqSrc* seq_src, EProgram p, 
                       BlastHSPResults* results)
    : CDbBlast(queries, seq_src, p)
{
    BLAST_ResultsFree(mi_pResults);
    mi_pResults = results;
}

CDbBlastTraceback::CDbBlastTraceback(const TSeqLocVector& queries, 
                       BlastSeqSrc* seq_src, CBlastOptionsHandle& opts,
                       BlastHSPResults* results)
    : CDbBlast(queries, seq_src, opts)
{
    BLAST_ResultsFree(mi_pResults);
    mi_pResults = results;
}

int CDbBlastTraceback::SetupSearch()
{
    int status = 0;
    EProgram x_eProgram = GetOptionsHandle().GetOptions().GetProgram();
    
    if ( !mi_bQuerySetUpDone ) {
        x_ResetQueryDs();
        
        SetupQueryInfo(GetQueries(), GetOptionsHandle().GetOptions(), 
                       &mi_clsQueryInfo);
        SetupQueries(GetQueries(), GetOptionsHandle().GetOptions(), 
                     mi_clsQueryInfo, &mi_clsQueries);

        mi_pScoreBlock = 0;
        
        Blast_Message* blast_message = NULL;
        
        /* Pass NULL lookup segments and output filtering locations pointers
           in the next call, to indicate that we don't need them here. */
        BLAST_MainSetUp(x_eProgram, GetQueryOpts(), GetScoringOpts(),
            GetHitSaveOpts(), mi_clsQueries, mi_clsQueryInfo, NULL, NULL,
            &mi_pScoreBlock, &blast_message);

        if (blast_message)
            GetErrorMessage().push_back(blast_message);

        BLAST_GapAlignSetUp(x_eProgram, GetSeqSrc(), 
            GetScoringOpts(), GetEffLenOpts(), GetExtnOpts(), GetHitSaveOpts(),
            mi_clsQueries, mi_clsQueryInfo, mi_pScoreBlock, 0,
            &mi_pExtParams, &mi_pHitParams, &mi_pGapAlign);

    }
    return status;
}

void
CDbBlastTraceback::RunSearchEngine()
{
    Int2 status;

    status = 
        BLAST_ComputeTraceback(GetOptionsHandle().GetOptions().GetProgram(), 
            mi_pResults, mi_clsQueries, mi_clsQueryInfo,
            GetSeqSrc(), mi_pGapAlign, GetScoringOpts(), mi_pExtParams, 
            mi_pHitParams, GetDbOpts(), GetProtOpts());
}

/// Resets query data structures; does only part of the work in the base 
/// CDbBlast class
void
CDbBlastTraceback::x_ResetQueryDs()
{
    mi_bQuerySetUpDone = false;
    // should be changed if derived classes are created
    mi_clsQueries.Reset(NULL);
    mi_clsQueryInfo.Reset(NULL);
    mi_pScoreBlock = BlastScoreBlkFree(mi_pScoreBlock);
    
    sfree(mi_pReturnStats);
    NON_CONST_ITERATE(TBlastError, itr, mi_vErrors) {
        *itr = Blast_MessageFree(*itr);
    }

}


END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/02/24 18:20:42  dondosha
 * Class derived from CDbBlast to do only traceback, given precomputed HSP results
 *
 *
 * ===========================================================================
 */
