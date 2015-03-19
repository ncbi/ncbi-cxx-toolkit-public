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
 *   Implementation of CSeqMaskerUStat class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_ostat.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

#define STAT_ALGO_VER_MAJOR 1
#define STAT_ALGO_VER_MINOR 0
#define STAT_ALGO_VER_PATCH 0

//------------------------------------------------------------------------------
char const * CSeqMaskerOstat::STAT_ALGO_COMPONENT_NAME = 
    "windowmasker-statistics-algorithm-version";


//------------------------------------------------------------------------------
CSeqMaskerVersion CSeqMaskerOstat::StatAlgoVersion(
        STAT_ALGO_COMPONENT_NAME, 
        STAT_ALGO_VER_MAJOR,
        STAT_ALGO_VER_MINOR,
        STAT_ALGO_VER_PATCH 
);

//------------------------------------------------------------------------------
const char * CSeqMaskerOstat::CSeqMaskerOstatException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadState:     return "bad state";
        default:            return CException::GetErrCodeString();
    }
}
            
//------------------------------------------------------------------------------
string CSeqMaskerOstat::FormatParameters() const {
    std::ostringstream os;
    Uint4 t_low( pvalues[0] == 0 ? 1 : pvalues[0] ), t_high( pvalues[3] );
    os << "##parameters:unit=" << (Uint4)unit_size  << ' '
       << "t_low=" << t_low << ' ' << "t_high=" << t_high;
    return os.str();
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::WriteBinMetaData( std::ostream & os ) const {
    Uint4 sz( 0 );
    string s1( "##" );
    s1 += GetStatFmtVersion().Print();
    sz += s1.size() + 1;
    string s2( "##" );
    s2 += fmt_gen_algo_ver.Print();
    sz += s2.size() + 1;
    string s3( FormatParameters() ),
           s4;
    sz += s3.size() + 1;

    if( !metadata.empty() ) {
        s4 = string( "##note:" ) + metadata;
        sz += s4.size() + 1;
    }

    char zero( 0 );
    os.write( (char *)&sz, sizeof( sz ) );
    os.write( s1.c_str(), s1.size() );
    os.write( &zero, 1 );
    os.write( s2.c_str(), s2.size() );
    os.write( &zero, 1 );
    os.write( s3.c_str(), s3.size() );
    os.write( &zero, 1 );
    
    if( !s4.empty() ) { 
        os.write( s4.c_str(), s4.size() );
        os.write( &zero, 1 );
    }
}

//------------------------------------------------------------------------------
string CSeqMaskerOstat::FormatMetaData() const {
    std::ostringstream os;
    os << "##" << GetStatFmtVersion().Print() << endl;
    os << "##" << fmt_gen_algo_ver.Print() << endl;
    os << FormatParameters() << endl;
    if( !metadata.empty() ) os << "##note:" << metadata << endl;
    return os.str();
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::setUnitSize( Uint1 us )
{
    if( state != start )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set unit size in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetUnitSize( us );
    state = ulen;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::setUnitCount( Uint4 unit, Uint4 count )
{
    if( state != ulen && state != udata )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set unit count data in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetUnitCount( unit, count );
    state = udata;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::setParam( const string & name, Uint4 value )
{
    if( state != udata && state != thres )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set masking parameters in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetParam( name, value );
    state = thres;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::finalize()
{
    if( state != udata && state != thres )
    {
        CNcbiOstrstream ostr;
        ostr << "can not finalize data structure in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    state = final;
    doFinalize();
}

END_NCBI_SCOPE
