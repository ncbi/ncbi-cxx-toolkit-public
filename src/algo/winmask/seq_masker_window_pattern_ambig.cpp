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
 *   CSeqMaskerWindowPatternAmbig class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <string>

#include <algo/winmask/seq_masker_util.hpp>
#include <algo/winmask/seq_masker_window_pattern_ambig.hpp>

BEGIN_NCBI_SCOPE

//-------------------------------------------------------------------------
CSeqMaskerWindowPatternAmbig::CSeqMaskerWindowPatternAmbig( 
                                                           const string & arg_data, Uint1 arg_unit_size, Uint1 arg_window_size,
                                                           Uint4 window_step, Uint4 arg_pattern, TUnit arg_ambig_unit,
                                                           Uint4 window_start, Uint1 arg_unit_step )
: CSeqMaskerWindowPattern( arg_data, arg_unit_size, arg_window_size,
                           window_step, arg_pattern, arg_unit_step ),
ambig_unit( arg_ambig_unit ), ambig( false )
{ FillWindow( window_start ); }

//-------------------------------------------------------------------------
void CSeqMaskerWindowPatternAmbig::Advance( Uint4 step )
{
    FillWindow( start + step );
}

//-------------------------------------------------------------------------
void CSeqMaskerWindowPatternAmbig::FillWindow( Uint4 winstart )
{
    first_unit = 0;
    TUnit unit = 0;
    Int4 iter = 0;
    end = winstart + unit_size - 1;

    for( ; iter < NumUnits() && end < data.size(); 
         ++iter, end += unit_step, winstart += unit_step )
        if( MakeUnit( winstart, unit ) ) units[iter] = unit;
        else units[iter] = ambig_unit;

        end -= unit_step;
        end += (window_size - unit_size)%unit_step;
        start = end - window_size + 1;
        state = (iter == NumUnits());
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

