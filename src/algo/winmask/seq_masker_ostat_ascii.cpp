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
 *   Implementation of CSeqMaskerUStatAscii class.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>

#include <algo/winmask/seq_masker_ostat_ascii.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CSeqMaskerOstatAscii::CSeqMaskerOstatAsciiException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadOrder: return "bad unit order";
        default:        return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CSeqMaskerOstatAscii::CSeqMaskerOstatAscii( const string & name )
    : CSeqMaskerOstat( name.empty() ? static_cast<CNcbiOstream&>(NcbiCout)
                                  : static_cast<CNcbiOstream&>(*new CNcbiOfstream( name.c_str() )) )
{}

//------------------------------------------------------------------------------
CSeqMaskerOstatAscii::~CSeqMaskerOstatAscii()
{
    if( &out_stream != &NcbiCout )
        delete &out_stream;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetUnitSize( Uint4 us )
{ out_stream << us << endl; }

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetUnitCount( Uint4 unit, Uint4 count )
{ 
    static Uint4 punit = 0;

    if( unit != 0 && unit <= punit )
    {
        CNcbiOstrstream ostr;
        ostr << "current unit " << hex << unit << "; "
          << "previous unit " << hex << punit;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatAsciiException, eBadOrder, s );
    }

    out_stream << hex << unit << " " << dec << count << "\n"; 
    punit = unit;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetComment( const string & msg )
{ out_stream << "#" << msg << "\n"; }

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetParam( const string & name, Uint4 value )
{ out_stream << ">" << name << " " << value << "\n"; }

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetBlank() { out_stream << "\n"; }

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/03/29 13:30:17  dicuccio
 * Use <> for includes.  Use CNcbiOstrstream instead of bare ostrstream.
 * Compilation fox for Win32: cast CNcbiOfstream to CNcbiOstream in ctor
 *
 * Revision 1.2  2005/03/29 01:37:53  ucko
 * Tweak constructor to avoid confusing GCC 2.95.
 *
 * Revision 1.1  2005/03/28 22:41:06  morgulis
 * Moved win_mask_ustat* files to library and renamed them.
 *
 * Revision 1.1  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * ========================================================================
 */
