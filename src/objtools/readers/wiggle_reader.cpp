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
 *   WIGGLE file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>   
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>

#include "wiggle_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CWiggleReader::CWiggleReader(
    TFlags flags ) :
//  ----------------------------------------------------------------------------
    m_uCurrentRecordType( TYPE_NONE ),
    m_Flags( flags )
{
    m_pSet = new CWiggleSet;
}

//  ----------------------------------------------------------------------------
CWiggleReader::~CWiggleReader()
//  ----------------------------------------------------------------------------
{
    delete m_pSet;
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CWiggleReader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pErrorContainer ).ReleaseOrNull() );
    return object; 
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CWiggleReader::ReadSeqAnnot(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{
    if (m_Flags & fAsGraph) {
        return ReadSeqAnnotGraph( lr, pErrorContainer );
    }
    else {
        return ReadSeqAnnotTable( lr, pErrorContainer );
    } 
}
    
//  --------------------------------------------------------------------------- 
void
CWiggleReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pErrorContainer );
}
 
//  ---------------------------------------------------------------------------                       
void
CWiggleReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    while ( ! lr.AtEOF() ) {
        annots.push_back( ReadSeqAnnot( lr, pErrorContainer ) );
    }
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CWiggleReader::ReadSeqAnnotGraph(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    m_uLineNumber = 0;
    
    CRef< CSeq_annot > annot( new CSeq_annot );
    string pending;
    vector<string> parts;
    CWiggleRecord record;
    bool bTrackFound( false );
    
    CSeq_annot::TData::TGraph& graphset = annot->SetData().SetGraph();
    while ( x_ReadLine( lr, pending ) ) {
        try {
            if ( x_ParseBrowserLine( pending, annot ) ) {
                continue;
            }
            if ( x_ParseTrackData( pending, annot, record ) ) {
                if ( ! bTrackFound ) {
                    bTrackFound = true;
                    continue;
                }
                else {
                    // must belong to the next track- put it back and bail
                    lr.UngetLine();
                    break;
                }
            }
            x_ParseGraphData( lr, pending, parts, record );
            m_pSet->AddRecord( record );
        }
        catch( CObjReaderLineException& err ) {
            ProcessError( err, pErrorContainer );
        }
    }
    try {
        m_pSet->MakeGraph( graphset );
    }
    catch( CObjReaderLineException& err ) {
        ProcessError( err, pErrorContainer );
    }
    x_AddConversionInfo( annot, pErrorContainer );
    if ( m_iFlags & fDumpStats ) {
        x_DumpStats( cerr );
    }
    return annot; 
}
    
//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CWiggleReader::ReadSeqAnnotTable(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    m_uLineNumber = 0;
    
    CRef< CSeq_annot > annot( new CSeq_annot );
    string pending;
    vector<string> parts;
    CWiggleRecord record;
    bool bTrackFound( false );
    
    CSeq_table& table = annot->SetData().SetSeq_table();
    while ( x_ReadLine( lr, pending ) ) {
        try {
            if ( x_ParseBrowserLine( pending, annot ) ) {
                continue;
            }
            if ( x_ParseTrackData( pending, annot, record ) ) {
                if ( ! bTrackFound ) {
                    bTrackFound = true;
                    continue;
                }
                else {
                    // must belong to the next track- put it back and bail
                    lr.UngetLine();
                    break;
                }
            }
            x_ParseGraphData( lr, pending, parts, record );
            m_pSet->AddRecord( record );
        }
        catch( CObjReaderLineException& err ) {
            ProcessError( err, pErrorContainer );
        }
    }
    try {
        m_pSet->MakeTable( table, 0!=(m_Flags & fJoinSame), 
            0!=(m_Flags & fAsByte) );
    }
    catch( CObjReaderLineException& err ) {
        ProcessError( err, pErrorContainer );
    }
    x_AddConversionInfo( annot, pErrorContainer );
    if ( m_iFlags & fDumpStats ) {
        x_DumpStats( cerr );
    }
    return annot; 
}
    
static string s_WiggleDelim = " \t";

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ParseTrackData(
    const string& pending,
    CRef<CSeq_annot>& annot,
    CWiggleRecord& record )
//
//  Note:   Expecting a line that starts with "track". With comments already
//          weeded out coming in, everything else triggers an error.
//  ----------------------------------------------------------------------------
{
    if ( ! CReaderBase::x_ParseTrackLine( pending, annot ) ) {
        return false;
    }
    vector<string> parts;
    Tokenize( pending, s_WiggleDelim, parts );
    record.ParseTrackDefinition( parts );
    return true;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_ParseGraphData(
    ILineReader& lr,
    string& pending,
    vector<string>& parts,
    CWiggleRecord& record )
//
//  Note:   Several possibilities here for the pending line:
//          (1) The line is a "variableStep" declaration. Such a declaration
//              initializes some data but does not represent a complete graph.
//              The following lines then in turn give the missing pieces and
//              complete the record.
//          (2) The line is a "fixedStep" declaration. Similar to (1).
//          (3) BED data. Self contained graph data.
//          (4) "variableStep" data. Completes graph record that was started in
//              the last "variableStep" declaration.
//          (5) "fixedStep" data. Completes graph record that was started in the
//              last "fixedStep" declaration.
//  ----------------------------------------------------------------------------
{
    parts.clear();
    Tokenize( pending, s_WiggleDelim, parts );
    
    switch ( x_GetLineType( pending, parts ) ) {

        default: {
            CObjReaderLineException err( 
                eDiag_Critical, 
                0, 
                "Internal error --- please report and submit input file for "
                "inspection" );
            throw err;
        }
        case TYPE_DECLARATION_VARSTEP:
            m_uCurrentRecordType = TYPE_DATA_VARSTEP;
            record.ParseDeclarationVarstep( parts );
            x_ReadLine( lr, pending );
            x_ParseGraphData( lr, pending, parts, record );
            break;

        case TYPE_DECLARATION_FIXEDSTEP:
            m_uCurrentRecordType = TYPE_DATA_FIXEDSTEP;
            record.ParseDeclarationFixedstep( parts );
            x_ReadLine( lr, pending );
            x_ParseGraphData( lr, pending, parts, record );
            break;

        case TYPE_DATA_BED:
            record.ParseDataBed( parts );
            break;

        case TYPE_DATA_VARSTEP:
            if ( m_uCurrentRecordType != TYPE_DATA_VARSTEP ) {
                CObjReaderLineException err( 
                    eDiag_Error, 
                    0, 
                    "Invalid data line --- VarStep data not expected here" );
                throw err;
            }
            record.ParseDataVarstep( parts );
            break;

        case TYPE_DATA_FIXEDSTEP:
            if ( m_uCurrentRecordType != TYPE_DATA_FIXEDSTEP ) {
                CObjReaderLineException err( 
                    eDiag_Error, 
                    0, 
                    "Invalid data line --- FixedStep data not expected here" );
                throw err;
            }
            record.ParseDataFixedstep( parts );
            break;
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ReadLine(
    ILineReader& lr,
    string& line )
//  ----------------------------------------------------------------------------
{
    line.clear();
    while ( ! lr.AtEOF() ) {
        line = *++lr;
        ++m_uLineNumber;
        NStr::TruncateSpacesInPlace( line );
        if ( ! x_IsCommentLine( line ) ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_IsCommentLine(
    const string& line )
//  ----------------------------------------------------------------------------
{
    return line.empty() || line[0] == '#';
}

namespace {
template<size_t blen>
inline bool s_HasPrefix(const string& line, const char (&prefix)[blen])
{
    size_t len = blen-1;
    if ( line.size() > len &&
         NStr::StartsWith(line.c_str(), prefix) &&
         (line[len] == ' ' || line[len] == '\t') ) {
        return true;
    }
    return false;
}
}

//  ----------------------------------------------------------------------------
unsigned int CWiggleReader::x_GetLineType(
    const string& line,
    const vector<string>& parts)
//  ----------------------------------------------------------------------------
{
    //
    //  Note: blank lines and comments should have been weeded out before we
    //  we even get here ...
    //
    if ( s_HasPrefix(line, "track") ) {
        return TYPE_TRACK;
    }
    if ( s_HasPrefix(line, "variableStep") ) {
        return TYPE_DECLARATION_VARSTEP;
    }
    if ( s_HasPrefix(line, "fixedStep") ) {
        return TYPE_DECLARATION_FIXEDSTEP;
    }

    if ( parts.size() == 4 ) {
        return TYPE_DATA_BED;
    }
    if ( parts.size() == 2 ) {
        return TYPE_DATA_VARSTEP;
    }
    if ( parts.size() == 1 ) {
        return TYPE_DATA_FIXEDSTEP;
    }
    
    CObjReaderLineException err( eDiag_Error, 0, "Unrecognizable line type" );
    throw err;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::Tokenize(
    const string& str,
    const string& delim,
    vector< string >& parts )
//  ----------------------------------------------------------------------------
{
    string temp;
    bool in_quote( false );
    const char joiner( '#' );

    for ( size_t i=0; i < str.size(); ++i ) {
        switch( str[i] ) {

            default:
                break;
            case '\"':
                in_quote = in_quote ^ true;
                break;

            case ' ':
                if ( in_quote ) {
                    if ( temp.empty() )
                        temp = str;
                    temp[i] = joiner;
                }
                break;
        }
    }
    if ( temp.empty() ) {
        NStr::Tokenize(str, delim, parts, NStr::eMergeDelims);
    }
    else {
        NStr::Tokenize(temp, delim, parts, NStr::eMergeDelims);
        for ( size_t j=0; j < parts.size(); ++j ) {
            for ( size_t i=0; i < parts[j].size(); ++i ) {
                if ( parts[j][i] == joiner ) {
                    parts[j][i] = ' ';
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_SetTrackData(
    CRef<CSeq_annot>& annot,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CAnnot_descr& desc = annot->SetDesc();

    if ( strKey == "name" ) {
        CRef<CAnnotdesc> name( new CAnnotdesc() );
        name->SetName( strValue );
        desc.Set().push_back( name );
        return;
    }
    if ( strKey == "description" ) {
        CRef<CAnnotdesc> title( new CAnnotdesc() );
        title->SetTitle( strValue );
        desc.Set().push_back( title );
        return;
    }
    if ( strKey == "type" ) {
        return;
    }
    CReaderBase::x_SetTrackData( annot, trackdata, strKey, strValue );
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_DumpStats(
    CNcbiOstream& out )
//  ----------------------------------------------------------------------------
{
    m_pSet->DumpStats( out );
}

END_objects_SCOPE
END_NCBI_SCOPE
