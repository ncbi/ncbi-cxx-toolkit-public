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
 * Authors:  Aleksandr Morgulis
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_istat.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
bool CSeqMaskerIstat::ParseVersionString( string const & vs ) {
    if( vs.size() < 2 ) return false;
    if( vs[0] != '(' ) return false;
    string::size_type p( vs.find( ')' ) ), pp;
    if( p == string::npos ) return false;
    string enc( vs.substr( 1, p - 1 ) );

    if( enc != "ascii" && 
        enc != "optimized ascii" &&
        enc != "binary" &&
        enc != "optimized binary" ) return false;

    pp = p + 1;
    p = vs.find( ':', pp );
    if( p == string::npos ) return false;
    string name( vs.substr( pp, p - pp ) );
    pp = p + 1;
    p = vs.find( '.', pp );
    if( p == string::npos ) return false;
    string major_str( vs.substr( pp, p - pp ) );

    if( major_str.find_first_not_of( "0123456789" ) != string::npos ) {
        return false;
    }

    pp = p + 1;
    p = vs.find( '.', pp );
    if( p == string::npos ) return false;
    string minor_str( vs.substr( pp, p - pp ) );

    if( minor_str.find_first_not_of( "0123456789" ) != string::npos ) {
        return false;
    }

    string patch_str( vs.substr( p + 1 ) );

    if( patch_str.find_first_not_of( "0123456789" ) != string::npos ) {
        return false;
    }

    SetFmtEncoding( enc );
    SetFmtVersion( 
            name, stoi( major_str ), stoi( minor_str ), stoi( patch_str ) );
    return true;
}

//------------------------------------------------------------------------------
void CSeqMaskerIstat::ParseMetaDataString( string const & md ) {
    if( md.size() >= 2 && md.substr( 0, 2 ) == "##" ) {
        string d( md.substr( 2 ) );
        string::size_type comma_pos( d.find( ',' ) );

        if( comma_pos == string::npos ) {
            if( !ParseVersionString( d ) ) SetMetaData( d );
            return;
        }

        if( !ParseVersionString( d.substr( 0, comma_pos ) ) ) {
            SetMetaData( d );
            return;
        }

        SetMetaData( d.substr( comma_pos + 1 ) );
    }
}

END_NCBI_SCOPE

