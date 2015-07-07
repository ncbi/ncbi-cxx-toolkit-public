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
#include <corelib/ncbireg.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/idmapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CIdMapperConfig::CIdMapperConfig(CNcbiIstream& istr,
                                 const std::string& strContext,
                                 bool bInvert,
                                 ILineErrorListener* pErrors)
    : CIdMapper(strContext, bInvert, pErrors)
{
    Initialize(istr);
}


CIdMapperConfig::CIdMapperConfig(const std::string& strContext,
                                 bool bInvert,
                                 ILineErrorListener* pErrors)
    : CIdMapper(strContext, bInvert, pErrors)
{
}


void CIdMapperConfig::Initialize(CNcbiIstream& istr)
{
    string buffer;
    {{
         CNcbiOstrstream os;
         NcbiStreamCopy(os, istr);
         buffer = string(CNcbiOstrstreamToString(os));
     }}

    CMemoryRegistry reg;
    try {
        CNcbiIstrstream is(buffer.data(), buffer.size());
        reg.Read(is);
    }
    catch (CException& e) {
        ERR_POST(Warning << "CIdMapperConfig: "
                 "error reading config file in registry format: " << e <<
                 "; trying to read in old format...");

        //
        // older config file support
        // consider dropping this
        //
        CNcbiIstrstream is(buffer.data(), buffer.size());

        string strLine( "" );
        string strCurrentContext( m_strContext );

        while( !is.eof() ) {
            NcbiGetlineEOL( is, strLine );
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

        /// done here
        return;
    }

    ///
    /// enumerate the fields required for the mapping
    ///
    list<string> entries;
    reg.EnumerateEntries(m_strContext, &entries);
    NON_CONST_ITERATE (list<string>, iter, entries) {
        if (*iter == "map_from"  ||
            *iter == "map_to") {
            /// reserved keys
            continue;
        }
        string id_set = reg.Get(m_strContext, *iter);
        list<string> ids;
        NStr::Split(id_set, " \t\n\r", ids);

        ///
        /// id_from and id_to are naturally reversed, since we use a format
        /// that contains 'gi| -> aliases' mapping
        ///

        CSeq_id id_to;
        try {
            id_to.Set(*iter);
        }
        catch (CException&) {
            id_to.SetLocal().SetStr(*iter);
        }
        CSeq_id_Handle idh_to = CSeq_id_Handle::GetHandle(id_to);

        ITERATE (list<string>, id_iter, ids) {
            CSeq_id id_from;
            try {
                id_from.Set(*id_iter);
            }
            catch (CException&) {
                id_from.SetLocal().SetStr(*id_iter);
            }
            CSeq_id_Handle idh_from = CSeq_id_Handle::GetHandle(id_from);

            AddMapping(idh_from, idh_to);
            if (m_bInvert) {
                /// inversion honors *ONLY* the first token to preserve 1:1
                /// mapping
                break;
            }
        }
    }
};


void CIdMapperConfig::DescribeContexts(CNcbiIstream& istr,
                                       list<SMappingContext>& contexts)
{
    CMemoryRegistry reg;
    reg.Read(istr);

    list<string> sections;
    reg.EnumerateSections(&sections);
    ITERATE (list<string>, iter, sections) {
        SMappingContext ctx;
        ctx.context = *iter;
        ctx.map_from = reg.Get(*iter, "map_from");
        ctx.map_to   = reg.Get(*iter, "map_to");
        contexts.push_back(ctx);
    }
}

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
        AddMapping( hSource, hTarget );
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
    return hTo;
};
   
END_NCBI_SCOPE

