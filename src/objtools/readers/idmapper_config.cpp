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
//#include <corelib/ncbistd.hpp>
//#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/idmapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
void
CIdMapperConfig::InitializeCache()
//  ============================================================================
{
    string strLine( "" );
    string strCurrentContext( m_strContext );
    
    while( !m_Istr.eof() ) {
        NcbiGetlineEOL( m_Istr, strLine );
        NStr::TruncateSpacesInPlace( strLine );
        if ( strLine.empty() || NStr::StartsWith( strLine, "#" ) ) {
            //comment
            continue;
        }
        if ( NStr::StartsWith( strLine, "[" ) ) {
            //start of new build section
            SetCurrentContext( strLine, strCurrentContext );
            continue;
        }
        if ( m_strContext == strCurrentContext ) {
            AddMapEntry( strLine );
        }
    }
};

//  ============================================================================
void
CIdMapperConfig::SetCurrentContext(
    const string& strLine,
    string& strContext )
//  ============================================================================
{
    vector<string> columns;
    NStr::Tokenize( strLine, " \t[]|:", columns, NStr::eMergeDelims );
    
    //sanity check: only a single columns remaining
    if ( columns.size() != 1 ) {
        return;
    }
    
    strContext = columns[0];
};

//  ============================================================================
void
CIdMapperConfig::AddMapEntry(
    const string& strLine )
//  ============================================================================
{
    vector<string> columns;
    NStr::Tokenize( strLine, " \t", columns, NStr::eMergeDelims );
    
    //sanity check: two or three columns. If three columns, the last better be
    //integer
    if ( columns.size() != 2 && columns.size() != 3 ) {
        return;
    }
    if ( columns.size() == 3 ) {
        string strLength = columns[2];
        try {
            NStr::StringToLong( strLength );
        }
        catch( CException& ) {
            return;
        }
    }
    
    CSeq_id_Handle hSource = SourceHandle( columns[0] );
    CSeq_id_Handle hTarget = TargetHandle( columns[1] );
    if ( hSource && hTarget ) {
        AddMapping( SourceHandle( columns[0] ), TargetHandle( columns[1] ) );
    }
};

//  ============================================================================
CSeq_id_Handle
CIdMapperConfig::SourceHandle(
    const string& strId )
//  ============================================================================
{
    CSeq_id source( CSeq_id::e_Local, strId );
    return CSeq_id_Handle::GetHandle( source );
};

//  ============================================================================
CSeq_id_Handle
CIdMapperConfig::TargetHandle(
    const string& strId )
//  ============================================================================
{
    //maybe it's a straight GI number ...
    try {
        CSeq_id target( CSeq_id::e_Gi, NStr::StringToInt( strId ) );
        return CSeq_id_Handle::GetHandle( target );
    }
    catch( CException& ) {
        //or, maybe not ...
    }

    //if not, assume a fasta string of one or more IDs. If more than one, pick 
    // the first
    list< CRef< CSeq_id > > ids;
    CSeq_id::ParseFastaIds( ids, strId, true );
    if ( ids.empty() ) {
        //nothing to work with ...
        return CSeq_id_Handle();
    }
    
    list< CRef< CSeq_id > >::iterator idit;
    CSeq_id_Handle hTo;
    
    for ( idit = ids.begin(); idit != ids.end(); ++idit ) {
    
        //we favor GI numbers over everything else. In the absence of a GI number
        // go for a Genbank accession. If neither is available, we use the first
        // id we find.
        const CSeq_id& current = **idit;
        switch ( current.Which() ) {
        
        case CSeq_id::e_Gi:
            return CSeq_id_Handle::GetHandle( current );
        
        case CSeq_id::e_Genbank:
            hTo = CSeq_id_Handle::GetHandle( current );
            break;
                
        default:
            if ( !hTo ) {
                hTo = CSeq_id_Handle::GetHandle( current );
            }
            break;
        }
    }
    
    //don't know what else to do...
    return CSeq_id_Handle();
};
   
END_NCBI_SCOPE

