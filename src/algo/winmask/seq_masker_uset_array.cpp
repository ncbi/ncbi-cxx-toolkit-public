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
 *   Implementation for CSeqMaskerUsetArray class.
 *
 */

#include <ncbi_pch.hpp>

#include <algorithm>

#include <algo/winmask/seq_masker_uset_array.hpp>
#include <algo/winmask/seq_masker_util.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * CSeqMaskerUsetArray::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eSizeOdd:      return "wrong array size";
        default:            return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
void CSeqMaskerUsetArray::add_info( const Uint4 * arg_unit_data, Uint4 sz )
{
    if( sz%2 != 0 )
        NCBI_THROW( Exception, eSizeOdd, 
                    "unit counts info must contain even number of words" );

    unit_data = reinterpret_cast< const entry * >( arg_unit_data );
    asize = sz/2;
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerUsetArray::get_info( Uint4 unit ) const
{
    const entry * ud = unit_data.get();

    if( ud == 0 )
        return 0;

    Uint4 runit = CSeqMaskerUtil::reverse_complement( unit, unit_size );

    if( runit < unit )
        unit = runit;

    entry target = { unit, 0 };
    const entry * r = lower_bound( ud, ud + asize, target, less< entry >() );
    
    if( r == ud + asize || r->u != unit )
        return 0;
    else return r->c;
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/04/12 13:35:34  morgulis
 * Support for binary format of unit counts file.
 *
 * ========================================================================
 */
