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
 *   CSeqMaskerWindow class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <string>

#include <corelib/ncbi_limits.h>

#include <algo/winmask/seq_masker_window.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//-------------------------------------------------------------------------
Uint1 CSeqMaskerWindow::LOOKUP[kMax_UI1];

//-------------------------------------------------------------------------
CSeqMaskerWindow::CSeqMaskerWindow( const CSeqVector & arg_data, 
                                    Uint1 arg_unit_size, 
                                    Uint1 arg_window_size,
                                    Uint4 arg_window_step,
                                    Uint1 arg_unit_step )
    : data(arg_data), state( false ), 
      unit_size( arg_unit_size ), unit_step( arg_unit_step ),
      window_size( arg_window_size ), window_step( arg_window_step ),
      end( 0 ), first_unit( 0 ), unit_mask( 0 )
{
    static bool first_call = true;

    if( first_call )
    {
        LOOKUP[unsigned('A')] = 1;
        LOOKUP[unsigned('C')] = 2;
        LOOKUP[unsigned('G')] = 3;
        LOOKUP[unsigned('T')] = 4;
        first_call = false;
    }

    if( data.size() < window_size ); // TODO Throw an exception.
    if( unit_size > window_size );   // TODO Throw an exception.

    units.resize( NumUnits(), 0 );
    unit_mask = (1 << (unit_size << 1)) - 1;
    FillWindow( 0 );
}

CSeqMaskerWindow::~CSeqMaskerWindow()
{
}

//-------------------------------------------------------------------------
void CSeqMaskerWindow::Advance( Uint4 step )
{
    if( step >= window_size || unit_step > 1 ) 
    {
        FillWindow( start + step );
        return;
    }

    Uint1 num_units = NumUnits();
    Uint1 last_unit = first_unit ? first_unit - 1 : num_units - 1;
    Uint4 unit = units[last_unit];
    Uint4 iter = 0;

    for( ; ++end < data.size() && iter < step ; ++iter, ++start )
    {
        Uint1 letter = LOOKUP[unsigned(data[end])];

        if( !(letter--) )
        { 
            FillWindow( end );
            return;
        }

        unit = ((unit<<2)&unit_mask) + letter;

        if( ++first_unit == num_units ) first_unit = 0;

        if( ++last_unit == num_units ) last_unit = 0;

        units[last_unit] = unit;
    }

    --end;

    if( iter != step ) state = false;
}

//-------------------------------------------------------------------------
void CSeqMaskerWindow::FillWindow( Uint4 winstart )
{
    first_unit = 0;
    TUnit unit = 0;
    Int4 iter = 0;
    end = winstart;

    for( ; iter < window_size && end < data.size(); ++iter, ++end )
    {
        Uint1 letter = LOOKUP[unsigned(data[end])];

        if( !(letter--) )
        {
            iter = -1;
            continue;
        }

        unit = ((unit<<2)&unit_mask) + letter;

        if( iter >= unit_size - 1 ) 
            if( !((iter + 1 - unit_size)%unit_step) )
                units[(iter + 1- unit_size)/unit_step] = unit;
    }

    start = (end--) - window_size;
    state = (iter == window_size);
}


END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
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

