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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   CSeqMaskerScoreMean class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <algo/winmask/seq_masker_window.hpp>
#include <algo/winmask/seq_masker_score_mean.hpp>

BEGIN_NCBI_SCOPE


//-------------------------------------------------------------------------
CSeqMaskerScoreMean::CSeqMaskerScoreMean( 
    const CRef< CSeqMaskerIstat > & ustat )
: CSeqMaskerScore( ustat ), sum( 0 ), start( 0 ), num( 0 )
{
}

//-------------------------------------------------------------------------
Uint4 CSeqMaskerScoreMean::operator()()
{
    return sum/num;
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::PreAdvance( Uint4 step )
{
    if( step == 1 && window->UnitStep() == 1 )
    {
        start = window->Start();
        sum -= *scores_start;
    }
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::PostAdvance( Uint4 step )
{
    if(    step == 1 
           && window->UnitStep() == 1 
           && window->Start() - start == 1 )
    {
        *scores_start = (*ustat)[(*window)[num - 1]];
        sum += *scores_start;
        scores_start = (scores_start - &scores[0] == (int)(num - 1) ) 
	             ? &scores[0]
                     : scores_start + 1;
    }
    else{ FillScores(); }
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::Init()
{
    start = window->Start();
    num = window->NumUnits();
    scores.resize( num, 0 );
  
    FillScores();
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::FillScores()
{
  sum = 0;
  scores_start = &scores[0];

  for( Uint1 i = 0; i < num; ++i )
  {
    scores[i] = (*ustat)[(*window)[i]];
    sum += scores[i];
  }
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * Revision 1.3  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

