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
CSeqMaskerOstatAscii::CSeqMaskerOstatAscii( 
        const string & name, string const & metadata )
    : CSeqMaskerOstat( 
            name.empty() ? 
            static_cast<CNcbiOstream&>(NcbiCout) : 
            static_cast<CNcbiOstream&>(*new CNcbiOfstream( name.c_str() )),
            name.empty() ? false : true, metadata )
{}

//------------------------------------------------------------------------------
CSeqMaskerOstatAscii::CSeqMaskerOstatAscii( 
        CNcbiOstream & os, string const & metadata )
    : CSeqMaskerOstat( os, false, metadata )
{}

//------------------------------------------------------------------------------
CSeqMaskerOstatAscii::~CSeqMaskerOstatAscii()
{
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetUnitSize( Uint4 us )
{ 
    out_stream << us << endl; 
    string md( FormatMetaData( "ascii" ) );
    if( !md.empty() ) out_stream << md << endl;
}

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
