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
void CSeqMaskerUsetSimple::add_info( vector< Uint4 > & arg_units,
                                     vector< Uint4 > & arg_counts )
{
    if( arg_units.size() != arg_counts.size() )
    {
        ostringstream s;
        s << "units vector size is " << arg_units.size()
          << " ; counts vector size is " << arg_counts.size();
        NCBI_THROW( Exception, eSizeMismatch, s.str() );
    }

    if( arg_units.size() > 1 )
        for( vector< Uint4 >::size_type i = 0 ; i < arg_units.size() - 1 ; ++i )
            if( arg_units[i] >= arg_units[i + 1] )
            {
                ostringstream s;
                s << "last unit: " << hex << arg_units[i]
                  << " ; adding " << hex << arg_units[i + 1];
                NCBI_THROW( Exception, eBadOrder, s.str() );
            }

    units.swap( arg_units );
    counts.swap( arg_counts );
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

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * ========================================================================
 */
