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
CSeqMaskerScoreMean::CSeqMaskerScoreMean( const CSeqMasker::LStat & lstat )
: CSeqMaskerScore( lstat ), sum( 0 ), start( 0 ), num( 0 )
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
        sum -= lstat[(*window)[0]];
    }
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::PostAdvance( Uint4 step )
{
    if(    step == 1 
           && window->UnitStep() == 1 
           && window->Start() - start == 1 )
        sum += lstat[(*window)[num - 1]];
    else 
    {
        sum = 0;

        for( Uint1 i = 0; i < num; ++i ) sum += lstat[(*window)[i]];
    }
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMean::Init()
{
    start = window->Start();
    num = window->NumUnits();
    sum = 0;

    for( Uint1 i = 0; i < num; ++i ) sum += lstat[(*window)[i]];
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

