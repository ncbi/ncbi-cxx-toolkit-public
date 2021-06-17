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
 *   Implementation for CSeqMaskerIstatAscii class.
 *
 */

#include <ncbi_pch.hpp>

#include "algo/winmask/seq_masker_istat_ascii.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CSeqMaskerIstatAscii::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eStreamOpenFail:   return "open failed";
        case eSyntax:           return "syntax error";
        case eParam:            return "bad parameter value";
        default:                return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CSeqMaskerIstatAscii::CSeqMaskerIstatAscii( const string & name,
                                            Uint4 arg_threshold,
                                            Uint4 arg_textend,
                                            Uint4 arg_max_count,
                                            Uint4 arg_use_max_count,
                                            Uint4 arg_min_count,
                                            Uint4 arg_use_min_count,
                                            Uint4 start_line )
    :   CSeqMaskerIstat(    arg_threshold, arg_textend, 
                            arg_max_count, arg_use_max_count,
                            arg_min_count, arg_use_min_count )
{
    CNcbiIfstream input_stream( name.c_str() );

    if( !input_stream )
        NCBI_THROW( Exception, eStreamOpenFail,
                    string( "could not open " ) + name );

    bool start = true;
    Uint4 linenum = start_line;
    Uint4 ambig_len = kMax_UI4;
    string line;
    for( Uint4 i( 0 ); i < start_line; ++i ) getline( input_stream, line );

    while( input_stream )
    {
        line.erase();
        getline( input_stream, line );
        ++linenum;
        if( line.length() == 0 ) continue;
        if( line[0] == '#' ) continue;

        // Check if we have a precomputed parameter.
        if( line[0] == '>' )
        {
            SIZE_TYPE name_end = line.find_first_of( " \t", 0 );
            SIZE_TYPE val_start = line.find_first_not_of( " \t", name_end );

            if( name_end == NPOS || val_start == NPOS )
            {
                CNcbiOstrstream str;
                str << "at line " << linenum;
                string msg = CNcbiOstrstreamToString(str);
                NCBI_THROW( Exception, eSyntax, msg);
            }

            string name = line.substr( 1, name_end - 1 );

            if( name == "t_threshold" && get_threshold() == 0 )
                set_threshold( 
                    NStr::StringToUInt(line.substr(val_start, NPOS), 0, 0));

            if( name == "t_extend" && get_textend() == 0 )
                set_textend(
                    NStr::StringToUInt(line.substr(val_start, NPOS), 0, 0));

            if( name == "t_low" )
                set_min_count(
                    NStr::StringToUInt(line.substr(val_start, NPOS), 0, 0));

            if( name == "t_high" && get_max_count() == 0 )
                set_max_count(
                    NStr::StringToUInt(line.substr(val_start, NPOS), 0, 0));

            continue;
        }

        if( start )
        {
            start = false;
            uset.set_unit_size( 
                static_cast< Uint1 >( NStr::StringToUInt( line ) ) );
            continue;
        }

        SIZE_TYPE unit_start = line.find_first_not_of( " \t", 0 );
        SIZE_TYPE unit_end   = line.find_first_of( " \t", unit_start );
        SIZE_TYPE cnt_start  = line.find_first_not_of( " \t", unit_end );

        if( unit_start == NPOS || unit_end == NPOS || cnt_start == NPOS )
        {
            CNcbiOstrstream str;
            str << "at line " << linenum;
            string msg = CNcbiOstrstreamToString( str );
            NCBI_THROW( Exception, eSyntax, msg );
        }

        Uint4 unit = NStr::StringToUInt(line.substr(unit_start, 
                                                    unit_end - unit_start),
                                        0, 16);
        Uint4 cnt = NStr::StringToUInt(line.substr(cnt_start));

        if( cnt < ambig_len ) {
            ambig_len = cnt;
            set_ambig_unit( unit );
        }

        if( cnt >= get_min_count() ) 
            uset.add_info( unit, cnt );
    }

    string bad_param;

    if( get_threshold() == 0 )
        bad_param += "t_threhold ";

    if( get_textend() == 0 )
        bad_param += "t_extend ";

    if( get_max_count() == 0 )
        bad_param += "t_high ";

    if( get_min_count() == 0 )
        bad_param += "t_low ";

    if( !bad_param.empty() )
        NCBI_THROW( Exception, eParam, bad_param );

    if( get_use_min_count() == 0 )
      set_use_min_count( (get_min_count() + 1)/2 );

    if( get_use_max_count() == 0 )
      set_use_max_count( get_max_count() );
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatAscii::trueat( Uint4 unit ) const
{ return uset.get_info( unit ); }

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatAscii::at( Uint4 unit ) const
{
  Uint4 res = uset.get_info( unit );

  if( res == 0 || res < get_min_count() )
    return get_use_min_count();

  return (res > get_max_count()) ? get_use_max_count() : res;
}

END_NCBI_SCOPE
