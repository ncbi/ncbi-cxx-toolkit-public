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
 *   CSeqMaskerScoreMeanGlob class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <algo/winmask/seq_masker_score_mean_glob.hpp>
#include <algo/winmask/seq_masker_window.hpp>

BEGIN_NCBI_SCOPE

//-------------------------------------------------------------------------
void CSeqMaskerScoreMeanGlob::PostAdvance( Uint4 step )
{
    if( step%window->UnitStep() )
    {
        _TRACE( "ERROR: window must advance in multiples of unit step." );
        exit( 1 );
    }

    step /= window->UnitStep();
    Uint1 num_units = window->NumUnits();
    Uint4 n = (step >= num_units) ? num_units : step;
    n = num_units - n;

    for( Uint4 i = n; i < num_units; ++i )
        update((*window)[n]);
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMeanGlob::Init()
{
    avg = 0.0;
    num = window->NumUnits();

    for( Uint1 i = 0; i < num; ++i ) avg += lstat[(*window)[i]];

    avg /= num;
}

//-------------------------------------------------------------------------
void CSeqMaskerScoreMeanGlob::update( Uint4 unit )
{
    ++num;
    avg += (lstat[unit] - avg)/num;
}


END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

