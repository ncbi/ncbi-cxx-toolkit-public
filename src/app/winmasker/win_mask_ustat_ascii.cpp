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
 *   Implementation of CWinMaskUStatAscii class.
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>

#include "win_mask_ustat_ascii.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CWinMaskUstatAscii::CWinMaskUstatAsciiException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadOrder: return "bad unit order";
        default:        return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CWinMaskUstatAscii::CWinMaskUstatAscii( const string & name )
    : CWinMaskUstat( name.empty() ? NcbiCout 
                                  : *new CNcbiOfstream( name.c_str() ) )
{}

//------------------------------------------------------------------------------
CWinMaskUstatAscii::~CWinMaskUstatAscii()
{
    if( &out_stream != &NcbiCout )
        delete &out_stream;
}

//------------------------------------------------------------------------------
void CWinMaskUstatAscii::doSetUnitSize( Uint4 us )
{ out_stream << us << endl; }

//------------------------------------------------------------------------------
void CWinMaskUstatAscii::doSetUnitCount( Uint4 unit, Uint4 count )
{ 
    static Uint4 punit = 0;

    if( unit != 0 && unit <= punit )
    {
        ostringstream s;
        s << "current unit " << hex << unit << "; "
          << "previous unit " << hex << punit;
        NCBI_THROW( CWinMaskUstatAsciiException, eBadOrder, s.str() );
    }

    out_stream << hex << unit << " " << dec << count << "\n"; 
    punit = unit;
}

//------------------------------------------------------------------------------
void CWinMaskUstatAscii::doSetComment( const string & msg )
{ out_stream << "#" << msg << "\n"; }

//------------------------------------------------------------------------------
void CWinMaskUstatAscii::doSetParam( const string & name, Uint4 value )
{ out_stream << ">" << name << " " << value << "\n"; }

//------------------------------------------------------------------------------
void CWinMaskUstatAscii::doSetBlank() { out_stream << "\n"; }

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * ========================================================================
 */
