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
 *   CDustMasker class implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <vector>
#include <algorithm>

#include <algo/blast/core/blast_dust.h>

#include <algo/winmask/dust_masker.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
static inline char iupacna_to_blastna( char c )
{
    switch( c )
    {
    case 'a': case 'A': return 0; 
    case 'c': case 'C': return 1;
    case 'g': case 'G': return 2;
    case 't': case 'T': return 3;
    case 'r': case 'R': return 4;
    case 'y': case 'Y': return 5;
    case 'm': case 'M': return 6;
    case 'k': case 'K': return 7;
    case 'w': case 'W': return 8;
    case 's': case 'S': return 9;
    case 'b': case 'B': return 10;
    case 'd': case 'D': return 11;
    case 'h': case 'H': return 12;
    case 'v': case 'V': return 13;
    case 'n': case 'N': 
    default:            return 14;
    }
}

//------------------------------------------------------------------------------
CDustMasker::CDustMasker( Uint4 arg_window, Uint4 arg_level, Uint4 arg_linker )
: window( arg_window ), level( arg_level ), linker( arg_linker )
{}

//------------------------------------------------------------------------------
CDustMasker::~CDustMasker(){}

//------------------------------------------------------------------------------
CDustMasker::TMaskList * CDustMasker::operator()( const string & data )
{
    // Transform to BLASTNA.
    string data_blastna;
    data_blastna.reserve( data.size() );
    transform( data.begin(), data.end(), 
                    back_inserter< string >( data_blastna ), 
                    iupacna_to_blastna );

    // Now dust.
    BlastSeqLoc * blast_loc( 0 );
    SeqBufferDust( reinterpret_cast< Uint1 * >( 
                                               const_cast< char * >( data_blastna.c_str() ) ), 
                   data_blastna.length(), 0, 
                   level, window, linker, &blast_loc );

    // Convert to the output type.
    TMaskList * result( new TMaskList );

    while( blast_loc )
    {
        result->push_back(TMaskedInterval( blast_loc->ssr->left, 
                                           blast_loc->ssr->right ) );
        blast_loc = blast_loc->next;
    }

    // Results are not necessarily sorted, so sort and remove duplicates.
    sort( result->begin(), result->end() );
    result->erase( unique( result->begin(), result->end() ), 
                   result->end() );

    return result;
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/03/01 16:07:42  ucko
 * Fix 64-bit builds by using TMaskedInterval rather than a hard-coded
 * type that may not be correct.
 *
 * Revision 1.3  2005/02/12 20:24:39  dicuccio
 * Dropped use of std:: (not needed)
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

