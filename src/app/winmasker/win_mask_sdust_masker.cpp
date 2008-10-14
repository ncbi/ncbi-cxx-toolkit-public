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

#include "win_mask_sdust_masker.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

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
CSDustMasker::CSDustMasker( Uint4 arg_window, Uint4 arg_level, Uint4 arg_linker )
: window( arg_window ), level( arg_level ), linker( arg_linker ),
  duster_( arg_level, arg_window, arg_linker )
{}

//------------------------------------------------------------------------------
CSDustMasker::~CSDustMasker(){}

//------------------------------------------------------------------------------
CSDustMasker::TMaskList * CSDustMasker::operator()( 
    const objects::CSeqVector & data, const TMaskList & exclude_ranges )
{
    TMaskList * res( new TMaskList );
    TMaskList::const_iterator e_it = exclude_ranges.begin();
    TMaskList::const_iterator e_end = exclude_ranges.end();
    CSeqVector::const_iterator start_it = data.begin();
    CSeqVector::const_iterator current_it = data.begin();
    CSeqVector::const_iterator end_it = data.end();

    if( e_it != e_end && e_it->first == 0 && e_it->second + 1 > window )
    {
        current_it = start_it + e_it->second - window + 2;
        ++e_it;
    }

    do
    {
        Uint4 start_offset = current_it - start_it;

        while( e_it != e_end && e_it->second - e_it->first + 1 <= window )
            ++e_it;

        end_it = (e_it == e_end) ? data.end() 
                                   : start_it + e_it->first + window;
        Uint4 stop_offset = end_it - start_it;

        // Now dust.
        std::auto_ptr< TMaskList > result = duster_( 
            data, start_offset, stop_offset );

        res->insert( res->end(), result->begin(), result->end() );

        if( e_it != e_end )
        {
            current_it = start_it + e_it->second - window + 2;
            ++e_it;
        }
    } while( end_it != data.end() );

    return res;
}


END_NCBI_SCOPE
