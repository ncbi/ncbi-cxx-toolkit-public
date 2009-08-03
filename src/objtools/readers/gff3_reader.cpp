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
 *   GFF file reader
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
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

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
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    int iFlags ):
//  ----------------------------------------------------------------------------
    m_iReaderFlags( iFlags ),
    m_pErrors( 0 )
{
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff3Reader::ReadSeqEntry(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
//    m_pErrors = pErrorContainer;
//    CRef< CSeq_entry > entry = CGFFReader::Read( lr, m_iReaderFlags );
//    m_pErrors = 0;
//    return entry;

    x_Reset();
    m_TSE->SetSet();
    
    string strLine;
    while( x_ReadLine( lr, strLine ) ) {
    
        try {
            if ( x_ParseStructuredComment( strLine ) ) {
                continue;
            }
            if ( x_ParseBrowserLineToSeqEntry( strLine, m_TSE ) ) {
                continue;
            }
            if ( x_ParseTrackLineToSeqEntry( strLine, m_TSE ) ) {
                continue;
            }
            // -->
            CRef<SRecord> record = x_ParseFeatureInterval(strLine);
            if (record) {
                
                record->line_no = m_LineNumber;
                string id = x_FeatureID(*record);
                record->id = id;

                if (id.empty()) {
                    x_ParseAndPlace(*record);
                } else {
                    CRef<SRecord>& match = m_DelayedRecords[id];
                    // _TRACE(id << " -> " << match.GetPointer());
                    if (match) {
                        x_MergeRecords(*match, *record);
                    } else {
                        match.Reset(record);
                    }
                }
            }
        }
        catch( CObjReaderLineException& err ) {
            ProcessError( err, pErrorContainer );
        }
        // <--
    }
    // -->
    NON_CONST_ITERATE (TDelayedRecords, it, m_DelayedRecords) {
        SRecord& rec = *it->second;
        /// merge mergeable ranges
        NON_CONST_ITERATE (SRecord::TLoc, loc_iter, rec.loc) {
            ITERATE (set<TSeqRange>, src_iter, loc_iter->merge_ranges) {
                TSeqRange range(*src_iter);
                set<TSeqRange>::iterator dst_iter =
                    loc_iter->ranges.begin();
                for ( ;  dst_iter != loc_iter->ranges.end();  ) {
                    TSeqRange r(range);
                    r += *dst_iter;
                    if (r.GetLength() <=
                        range.GetLength() + dst_iter->GetLength()) {
                        range += *dst_iter;
                        _TRACE("merging overlapping ranges: "
                               << range.GetFrom() << " - "
                               << range.GetTo() << " <-> "
                               << dst_iter->GetFrom() << " - "
                               << dst_iter->GetTo());
                        loc_iter->ranges.erase(dst_iter++);
                        break;
                    } else {
                        ++dst_iter;
                    }
                }
                loc_iter->ranges.insert(range);
            }
        }

        if (rec.key == "exon") {
            rec.key = "mRNA";
        }
        x_ParseAndPlace(rec);
    }

    x_RemapGeneRefs( m_TSE, m_GeneRefs );

    CRef<CSeq_entry> tse(m_TSE); // need to save before resetting.
    x_Reset();
    
    // promote transcript_id and protein_id to products
    if ( m_iReaderFlags & fSetProducts ) {
        x_SetProducts( tse );
    }

    if ( m_iReaderFlags & fCreateGeneFeats ) {
        x_CreateGeneFeatures( tse );
    }
    // <--
    return tse;
}
    
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CGff3Reader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
//    m_pErrors = pErrorContainer;
//    CRef< CSeq_entry > entry = CGFFReader::Read( lr, m_iReaderFlags );
//    m_pErrors = 0;
//    CRef<CSerialObject> object( entry.ReleaseOrNull() );
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pErrorContainer ).ReleaseOrNull() );
    return object;
}
    
//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Info(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Info( message, line );
    }
    CObjReaderLineException err( eDiag_Info, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Warn(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Warn( message, line );
    }
    CObjReaderLineException err( eDiag_Warning, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Error(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Error( message, line );
    }
    CObjReaderLineException err( eDiag_Error, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_ReadLine(
    ILineReader& lr,
    string& strLine )
//  ----------------------------------------------------------------------------
{
    strLine.clear();
    while ( ! lr.AtEOF() ) {
        strLine = *++lr;
        ++m_uLineNumber;
        NStr::TruncateSpacesInPlace( strLine );
        if ( ! x_IsCommentLine( strLine ) ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_IsCommentLine(
    const string& strLine )
//  ----------------------------------------------------------------------------
{
    if ( strLine.empty() ) {
        return true;
    }
    return (strLine[0] == '#' && strLine[1] != '#');
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_ParseBrowserLineToSeqEntry(
    const string& strLine,
    CRef<CSeq_entry>& entry )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "browser" ) ) {
        return false;
    }
    CSeq_descr& descr = entry->SetDescr();
    
    vector<string> fields;
    NStr::Tokenize( strLine, " \t", fields, NStr::eMergeDelims );
    for ( vector<string>::iterator it = fields.begin(); it != fields.end(); ++it ) {
        if ( *it == "position" ) {
            ++it;
            if ( it == fields.end() ) {
                CObjReaderLineException err(
                    eDiag_Error,
                    0,
                    "Bad browser line: incomplete position directive" );
                throw( err );
            }
            CRef<CSeqdesc> region( new CSeqdesc() );
            region->SetRegion( *it );
            descr.Set().push_back( region );
            continue;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_ParseTrackLineToSeqEntry(
    const string& strLine,
    CRef<CSeq_entry>& entry )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "track" ) ) {
        return false;
    }
    CSeq_descr& descr = entry->SetDescr();

    CRef<CUser_object> trackdata( new CUser_object() );
    trackdata->SetType().SetStr( "Track Data" );    
    
    map<string, string> values;
    x_GetTrackValues( strLine, values );
    for ( map<string,string>::iterator it = values.begin(); it != values.end(); ++it ) {
        x_SetTrackDataToSeqEntry( entry, trackdata, it->first, it->second );
    }
    if ( trackdata->CanGetData() && ! trackdata->GetData().empty() ) {
        CRef<CSeqdesc> user( new CSeqdesc() );
        user->SetUser( *trackdata );
        descr.Set().push_back( user );
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3Reader::x_SetTrackDataToSeqEntry(
    CRef<CSeq_entry>& entry,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CSeq_descr& descr = entry->SetDescr();

    if ( strKey == "name" ) {
        CRef<CSeqdesc> name( new CSeqdesc() );
        name->SetName( strValue );
        descr.Set().push_back( name );
        return;
    }
    if ( strKey == "description" ) {
        CRef<CSeqdesc> title( new CSeqdesc() );
        title->SetTitle( strValue );
        descr.Set().push_back( title );
        return;
    }
    trackdata->AddField( strKey, strValue );
}

END_objects_SCOPE
END_NCBI_SCOPE
