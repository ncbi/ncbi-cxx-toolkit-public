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
 * Author:  Frank Ludwig
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/idmapper/ucscid.hpp>
#include <objtools/idmapper/idmapper.hpp>
#include "idmap.hpp"
#include "idmapper_user.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
CIdMapperUser::CIdMapperUser()
//  ============================================================================
{
};

//  ============================================================================
void CIdMapperUser::Setup(
    const CArgs& args )
//  ============================================================================
{
    if ( args[ "usermap" ] ) {
        ReadUserMap( args[ "usermap" ].AsInputFile() );
    }
};

//  ============================================================================
void CIdMapperUser::Setup(
    const string& strUserMap )
//  ============================================================================
{
    CNcbiIfstream in( strUserMap.c_str() );
    if ( ! in.is_open() ) {
        // maybe, get more vocal about this
        return;
    }
    ReadUserMap( in );
};
    
//  ============================================================================
bool
CIdMapperUser::MapID(
    const string& strKey,
    CRef<CSeq_id>& value,
    unsigned int& uLength )
//  ============================================================================
{
    if ( ! m_Map.GetMapping( strKey, value, uLength ) ) {
        return CIdMapper::MapID( strKey, value, uLength );
    }
    return true;
};

//  ============================================================================
void
CIdMapperUser::Dump(
    CNcbiOstream& out,
    const string& strPrefix )
//  ============================================================================
{
    out << strPrefix << "[CIdMapperSite:" << endl;
    m_Map.Dump( out, strPrefix + "\t" );
    out << strPrefix << "]" << endl;
};

//  ============================================================================
void CIdMapperUser::ReadUserMap(
    CNcbiIstream& in )
//  ============================================================================
{
    string strLine;

    while ( ! in.eof() ) {
        NcbiGetlineEOL( in, strLine );
        
        NStr::TruncateSpacesInPlace( strLine );
        if ( NStr::StartsWith( strLine, "#" ) ) {
            continue;
        }
        vector<string> parts;
        NStr::Tokenize( strLine, " \t", parts, NStr::eMergeDelims );
        if ( parts.size() >= 2 ) {
            string strSource;
            MakeSourceId( parts[0], strSource );
            CRef<CSeq_id> target;
            MakeTargetId( parts[1], target );
            m_Map.AddMapping( strSource, target );
        }
    }
};

//  ============================================================================
bool
CIdMapperUser::MakeSourceId(
    const string& strRaw,
    string& strFinished )
//  ============================================================================
{
    strFinished = UcscID::Label( "", strRaw );
    return true;
};

//  ============================================================================
bool
CIdMapperUser::MakeTargetId(
    const string& strRaw,
    CRef<CSeq_id>& target )
//  ============================================================================
{
    list< CRef< CSeq_id > > ids;
    CSeq_id::ParseFastaIds( ids, strRaw, true );
    if ( ids.empty() ) {
        return false;
    }
    target.Reset( ids.begin()->Release() );
    return true;
};
   
END_NCBI_SCOPE

