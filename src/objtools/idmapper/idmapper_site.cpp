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
#include <objtools/readers/reader_exception.hpp>
#include "idmap.hpp"
#include "idmapper_site.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
CIdMapperSite::CIdMapperSite()
//  ============================================================================
{
};

//  ============================================================================
void CIdMapperSite::Setup(
    const CArgs& args )
//  ============================================================================
{
    if ( args[ "sitemap" ] ) {
        ReadSiteMap( args[ "sitemap" ].AsInputFile() );
    }
};

//  ============================================================================
void CIdMapperSite::Setup(
    const string& strUserMap )
//  ============================================================================
{
    CNcbiIfstream in( strUserMap.c_str() );
    if ( ! in.is_open() ) {
        // maybe, get more vocal about this
        return;
    }
    ReadSiteMap( in );
};
    
//  ============================================================================
bool
CIdMapperSite::MapID(
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
CIdMapperSite::Dump(
    CNcbiOstream& out,
    const string& strPrefix )
//  ============================================================================
{
    out << strPrefix << "[CIdMapperUser:" << endl;
    m_Map.Dump( out, strPrefix + "\t" );
    out << strPrefix << "]" << endl;
};

//  ============================================================================
void CIdMapperSite::ReadSiteMap(
    CNcbiIstream& in )
//  ============================================================================
{
    string strLine;
    string strBuild;
    
    while ( ! in.eof() ) {
        NcbiGetlineEOL( in, strLine );
        
        NStr::TruncateSpacesInPlace( strLine );
        if ( NStr::StartsWith( strLine, "#" ) ) {
            continue;
        }
        vector<string> parts;
        NStr::Tokenize( strLine, " \t", parts, NStr::eMergeDelims );
        if ( parts.size() == 1 ) {
            if ( NStr::StartsWith( parts[0], "[" ) && 
                NStr::EndsWith( parts[0], "]" ) ) 
            {
                strBuild = parts[0].substr( 1, parts[0].size() - 2 );
            }
        }
        if ( parts.size() >= 2 ) {
            if ( strBuild.empty() ) {
                NCBI_THROW2( CObjReaderParseException, eFormat,
                    "Site Map: Build specification missing", 0 );
            }
            string strSource;
            MakeSourceId( strBuild, parts[0], strSource );
            CRef<CSeq_id> target;
            MakeTargetId( parts[1], target );
            m_Map.AddMapping( strSource, target );
        }
    }
};

//  ============================================================================
bool
CIdMapperSite::MakeSourceId(
    const string& strBuild,
    const string& strChrom,
    string& strFinished )
//  ============================================================================
{
    strFinished = UcscID::Label( strBuild, strChrom );
    return true;
};

//  ============================================================================
bool
CIdMapperSite::MakeTargetId(
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

