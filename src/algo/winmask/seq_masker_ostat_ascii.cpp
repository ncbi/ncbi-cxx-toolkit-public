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

#define STAT_FMT_COMPONENT_NAME "windowmasker-statistics-format-version"
#define STAT_FMT_VER_MAJOR 1
#define STAT_FMT_VER_MINOR 0
#define STAT_FMT_VER_PATCH 0
#define STAT_FMT_VER_PFX "ascii "

//------------------------------------------------------------------------------
CSeqMaskerVersion CSeqMaskerOstatAscii::FormatVersion(
        STAT_FMT_COMPONENT_NAME, 
        STAT_FMT_VER_MAJOR,
        STAT_FMT_VER_MINOR,
        STAT_FMT_VER_PATCH,
        STAT_FMT_VER_PFX
);

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

    counts.push_back( std::make_pair( unit, count ) );
    punit = unit;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doSetComment( const string & msg )
{ 
    comments.push_back( msg );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatAscii::doFinalize() {
    out_stream << FormatMetaData();
    out_stream << (Uint4)unit_size << endl; 

    for( size_t i( 0 ); i < counts.size(); ++i ) {
        out_stream << hex << counts[i].first << ' ' 
                   << dec << counts[i].second << '\n';
    }

    out_stream << '\n';

    for( size_t i( 0 ); i < comments.size(); ++i ) {
        out_stream << '#' << comments[i] << '\n';
    }

    out_stream << '\n';
    out_stream << '>' << PARAMS[0] << ' ' << pvalues[0] << '\n';
    out_stream << '>' << PARAMS[1] << ' ' << pvalues[1] << '\n';
    out_stream << '>' << PARAMS[2] << ' ' << pvalues[2] << '\n';
    out_stream << '>' << PARAMS[3] << ' ' << pvalues[3] << '\n';
    out_stream << endl;
}

END_NCBI_SCOPE
