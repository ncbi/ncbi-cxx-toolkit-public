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
 *   CSeqMaskerScoreMin class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>

#include <algo/winmask/seq_masker_window.hpp>
#include <algo/winmask/seq_masker_score_min.hpp>

BEGIN_NCBI_SCOPE

//-------------------------------------------------------------------------
Uint4 CSeqMaskerScoreMin::operator()()
{
    list< Uint4 > stats;
    Uint4 num = window->NumUnits();

    for( Uint1 i = 0; i < num; ++i )
    {
        Uint4 result = lstat[(*window)[i]];
        list< Uint4 >::iterator j = stats.begin();

        while( j != stats.end() && result > *j ) ++j;

        stats.insert( j, result );

        if( stats.size() > num - count + 1 ) stats.pop_back();
    }

    return stats.back();
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

