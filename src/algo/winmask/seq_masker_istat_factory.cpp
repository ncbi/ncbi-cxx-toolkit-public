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
 *   Implementation of CSeqMaskerIstatFactory class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_istat_factory.hpp>
#include <algo/winmask/seq_masker_istat_ascii.hpp>
#include <algo/winmask/seq_masker_istat_oascii.hpp>
#include <algo/winmask/seq_masker_istat_bin.hpp>
#include <algo/winmask/seq_masker_istat_obinary.hpp>

#include <corelib/ncbifile.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * CSeqMaskerIstatFactory::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadFormat:    return "unknown format";
        case eCreateFail:   return "creation failure";
        case eOpen:         return "open failed";
        default:            return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
static bool CheckBin( 
        string const & name, vector< string > & md, size_t & skip ) {
    skip = 0;
    CNcbiIfstream input_stream( name.c_str(), IOS_BASE::binary );

    if( input_stream ) {
        Uint4 word( 0 );
        input_stream.read( (char *)&word, sizeof( word ) );

        if( word == 3 ) { 
            skip = 8;
            input_stream.read( (char *)&word, sizeof( word ) );
            skip += word;
            char * data( new char[word + 2] );
            data[word] = data[word + 1] = 0;
            input_stream.read( data, word );

            for( char * d( data ); *d != 0; d += strlen( d ) + 1 ) {
                md.push_back( string( d ) );
            }

            delete[] data;
            return true;
        }
        else if( word < 3 ) return true;
        else return false;
    }

    return false;
}

//------------------------------------------------------------------------------
static void GetMetaDataLines( 
        string const & name, vector< string > & md, size_t & skip ) {
    if( !CFile( name ).Exists() ) return;
    if( CheckBin( name, md, skip ) ) return;
    CNcbiIfstream check( name.c_str() );
    string line;

    while( check ) {
        getline( check, line );

        if( line.size() >= 2 && line[0] == '#' && line[1] == '#' ) {
            md.push_back( line );
        }
        else break;
    }
}

//------------------------------------------------------------------------------
void ExtractStatAlgoVersion( 
        vector< string > const & md, CSeqMaskerVersion & ver ) {
    static char const * DIGITS = "0123456789";
    string t;

    for( vector< string >::const_iterator i( md.begin() ); 
            i != md.end(); ++i ) {
        string::size_type p( i->find( ':', 2 ) ), pp;

        if( p != string::npos ) {
            if( i->substr( 2, p -2 ) == 
                    CSeqMaskerOstat::STAT_ALGO_COMPONENT_NAME ) {
                int major = 0, minor = 0, patch = 0;
                pp = p + 1;
                p = i->find( '.', pp );
                if( p == string::npos ) continue;
                t = i->substr( pp, p - pp );
                if( t.find_first_not_of( DIGITS ) != string::npos ) continue;
                major = NStr::StringToInt( t );
                pp = p + 1;
                p = i->find( '.', pp );
                if( p == string::npos ) continue;
                t = i->substr( pp, p - pp );
                if( t.find_first_not_of( DIGITS ) != string::npos ) continue;
                minor = NStr::StringToInt( t );
                t = i->substr( p + 1 );
                if( t.find_first_not_of( DIGITS ) != string::npos ) continue;
                patch = NStr::StringToInt( t );
                ver = CSeqMaskerVersion( 
                        CSeqMaskerOstat::STAT_ALGO_COMPONENT_NAME,
                        major, minor, patch );
                return;
            }
        }
    }
}

//------------------------------------------------------------------------------
string ExtractMetaDataStr( vector< string > const & md ) {
    for( vector< string >::const_iterator i( md.begin() ); 
            i != md.end(); ++i ) {
        string::size_type pos( i->find( ':', 2 ) );

        if( pos != string::npos ) {
            if( i->substr( 2, pos - 2 ) == "note" ) {
                return i->substr( pos + 1 );
            }
        }
    }

    return "";
}

//------------------------------------------------------------------------------
CSeqMaskerIstatFactory::EStatType CSeqMaskerIstatFactory::DiscoverStatType(
        string const & name, vector< string > & md, size_t & skip ) {
    GetMetaDataLines( name, md, skip );
    CNcbiIfstream check( name.c_str() );

    if( !check ) {
        NCBI_THROW( Exception, eOpen, string( "could not open " ) + name );
    }

    if( skip > 0 ) {
        char * buf( new char[skip] );
        check.read( buf, skip );
        delete[] buf;
    }
    else if( !md.empty() ) {
        string line;
        for( size_t i( 0 ); i < md.size(); ++i ) getline( check, line );
    }

    if( check ) {
        Uint4 data = 1;

        if( check.read( (char *)&data, sizeof( Uint4 ) ) ) {
            if( data == 0 ) return eBinary;
            if( data == 0x41414141 ) return eOAscii;
            if( data == 1 || data == 2 ) return eOBinary;
            return eAscii;
        }
    }

    return eUnknown;
}

//------------------------------------------------------------------------------
CSeqMaskerIstatFactory::EStatType CSeqMaskerIstatFactory::DiscoverStatType(
        string const & name ) {
    size_t skip( 0 );
    vector< string > md;
    return DiscoverStatType( name, md, skip );
}

//------------------------------------------------------------------------------
CSeqMaskerIstat * CSeqMaskerIstatFactory::create( const string & name,
                                                  Uint4 threshold,
                                                  Uint4 textend,
                                                  Uint4 max_count,
                                                  Uint4 use_max_count,
                                                  Uint4 min_count,
                                                  Uint4 use_min_count,
                                                  bool use_ba )
{
    try
    {
        size_t skip( 0 );
        vector< string > md;
        EStatType stat_type( DiscoverStatType( name , md, skip ) );
        CSeqMaskerIstat * res( 0 );

        switch( stat_type ) {
            case eAscii: res = new CSeqMaskerIstatAscii( 
                                 name,
                                 threshold, textend,
                                 max_count, use_max_count,
                                 min_count, use_min_count,
                                 md.size() );
                         break;

            case eBinary: res = new CSeqMaskerIstatBin( 
                                  name, threshold, textend,
                                  max_count, use_max_count,
                                  min_count, use_min_count,
                                  skip );
                          break;

            case eOAscii: res = new CSeqMaskerIstatOAscii( 
                                  name,
                                  threshold, textend,
                                  max_count, use_max_count,
                                  min_count, use_min_count,
                                  md.size() );
                          break;

            case eOBinary: res = new CSeqMaskerIstatOBinary( 
                                   name,
                                   threshold, textend,
                                   max_count, use_max_count,
                                   min_count, use_min_count,
                                   use_ba, skip );
                           break;

			default: NCBI_THROW(Exception, eCreateFail, "unrecognized unit counts format");
        }

        {
            string md_str( ExtractMetaDataStr( md ) );
            CSeqMaskerVersion ver( 
                    CSeqMaskerOstat::STAT_ALGO_COMPONENT_NAME, 1, 0, 0 );
            ExtractStatAlgoVersion( md, ver );
            res->SetStatAlgoVersion( ver );
            if( !md_str.empty() ) res->SetMetaData( md_str );
        }

        return res;
    }
    catch( CException & e)
    {
        NCBI_RETHROW( e, Exception, eCreateFail,
                      "could not create a unit counts container" );
    }
    catch( std::exception & e )
    {
        NCBI_THROW( Exception, eCreateFail,
                    std::string( "could not create a unit counts container: " ) +
                    e.what() );
    }
}

END_NCBI_SCOPE
