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
 *   Implementation for CSeqMaskerUsetSimple class.
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>
#include <algorithm>

#include "algo/winmask/seq_masker_uset_simple.hpp"
#include "algo/winmask/seq_masker_util.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * CSeqMaskerUsetSimple::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadOrder:     return "bad unit order";
        case eSizeMismatch: return "size mismatch";
        default:            return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
void CSeqMaskerUsetSimple::add_info( Uint4 unit, Uint4 count )
{
    if( !units.empty() && unit <= units[units.size() - 1] )
    {
        ostringstream s;
        s << "last unit: " << hex << units[units.size() - 1]
          << " ; adding " << hex << unit;
        NCBI_THROW( Exception, eBadOrder, s.str() );
    }

    units.push_back( unit );
    counts.push_back( count );
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerUsetSimple::get_info( Uint4 unit ) const
{
    Uint4 runit = CSeqMaskerUtil::reverse_complement( unit, unit_size );
    
    if( runit < unit )
        unit = runit;

    vector< Uint4 >::const_iterator res 
        = lower_bound( units.begin(), units.end(), unit );

    if( res == units.end() || *res != unit )
        return 0;
    else return counts[res - units.begin()];
}

END_NCBI_SCOPE
