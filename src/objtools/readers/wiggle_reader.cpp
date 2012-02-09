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

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>

#include "reader_data.hpp"
#include "wiggle_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

const string CWiggleReader::s_WiggleDelim = " \t";

//  ----------------------------------------------------------------------------
CWiggleReader::CWiggleReader(
    TFlags flags ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags),
    m_uCurrentRecordType( TYPE_DATA_BED )
{
    m_uLineNumber = 0;
    m_pControlData = new CWiggleRecord;
}

//  ----------------------------------------------------------------------------
CWiggleReader::~CWiggleReader()
//  ----------------------------------------------------------------------------
{
    delete m_pControlData;
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
    CWiggleTrack* pTrack = 0;
    CRef< CSeq_annot > annot;

    x_ParseSequence( lr, pTrack, pErrorContainer );
    if ( 0 == pTrack ) {
        return annot;
    }
    annot.Reset( new CSeq_annot );

    try {
        x_AssignBrowserData( annot );
        x_AssignTrackData( annot );
        pTrack->MakeAsn( m_iFlags,
            m_pTrackDefaults->Name(), m_pTrackDefaults->Description(), *annot );
    }
    catch( CObjReaderLineException& err ) {
        ProcessError( err, pErrorContainer );
    }
    x_AddConversionInfo( annot, pErrorContainer );
    delete pTrack;
    return annot;
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
        CRef<CSeq_annot> pAnnot = ReadSeqAnnot( lr, pErrorContainer );
        if ( pAnnot ) {
            annots.push_back( pAnnot );
        }
    }
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::x_ParseSequence(
    ILineReader& lr,
    CWiggleTrack*& pTrack,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef< CSeq_annot > annot( new CSeq_annot );
    m_pControlData->Reset();
    m_uCurrentRecordType = TYPE_NONE;

    vector<string> parts;
    while (x_ReadLineData(lr, parts)) {
        
        try {
            if (!x_ProcessLineData(parts, pTrack)) {
                lr.UngetLine();
                --m_uLineNumber;
                if (0 != pTrack) {
                    //if we got any track data, return it
                    break;
                }
                else {
                    //otherwise, reinitialize:
                    m_pControlData->Reset();
                    m_uCurrentRecordType = TYPE_NONE;
                }
            }
        }
        catch (CLineError& err) {
            if (err.Line() == 0) {
                err.PatchLineNumber(m_uLineNumber);
            }
            ProcessError(err, pErrorContainer);

            CLineError warn( 
                ILineError::eProblem_MissingContext,
                eDiag_Warning, 
                "", 
                0);
            while (x_ReadLineData(lr, parts)) {
                //flush all data lines until we reach firm ground:
                unsigned int uType = x_GetLineType(parts);
                if (uType == TYPE_TRACK  ||  uType == TYPE_DECLARATION_VARSTEP  ||
                        uType == TYPE_DECLARATION_FIXEDSTEP) {
                    lr.UngetLine();
                    --m_uLineNumber;
                    break;
                }
                else if (uType != TYPE_COMMENT) {
                    warn.PatchLineNumber(m_uLineNumber);
                    ProcessError(warn, pErrorContainer);
                }
            }
        }
    }
    return (0 != pTrack);
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ReadLineData(
    ILineReader& lr,
    vector<string>& linedata )
//  ----------------------------------------------------------------------------
{
    if ( lr.AtEOF() ) {
        return false;
    }
    ++m_uLineNumber;
    linedata.clear();
    CReadUtil::Tokenize( *++lr, s_WiggleDelim, linedata );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ProcessLineData(
    const vector<string>& linedata,
    CWiggleTrack*& pTrack )
//  ----------------------------------------------------------------------------
{
    unsigned int uLineType = x_GetLineType( linedata );
    switch( uLineType ) {
        default: {
            x_ParseDataRecord( linedata );
            if (m_pControlData->SeqStart() < 0) {
                //we regard negative positions as legal but out of the range where
                // we collect data
                return true;
            }
            if ( 0 == pTrack ) {
                pTrack = new CWiggleTrack( *m_pControlData );
            }
            else {
                pTrack->AddRecord( *m_pControlData );
            }
            return true;
        }
        case TYPE_COMMENT: {
            return true;
        }
        case TYPE_BROWSER: {
            return true;
        }
        case TYPE_TRACK: {
            if ( m_uCurrentRecordType == TYPE_NONE ) {
                m_uCurrentRecordType = TYPE_TRACK;
                m_pTrackDefaults->ParseLine( linedata );
                return true;
            }
            else {
                // must belong to the next track- put it back and bail
                return false;
            }
        }
        case TYPE_DECLARATION_VARSTEP: {
            m_uCurrentRecordType = TYPE_DATA_VARSTEP;

            CWiggleRecord temp;
            temp.ParseDeclarationVarstep( linedata );
            if ( ! m_pControlData->Chrom().empty() && m_pControlData->Chrom() != temp.Chrom() ) {
                return false;
            }
            if ( ! m_pControlData->Chrom().empty() && m_pControlData->SeqSpan() != temp.SeqSpan() ) {
                return false;
            }
            *m_pControlData = temp;
            return true;
        }
        case TYPE_DECLARATION_FIXEDSTEP: {
            m_uCurrentRecordType = TYPE_DATA_FIXEDSTEP;

            CWiggleRecord temp;
            temp.ParseDeclarationFixedstep( linedata );
            if ( ! m_pControlData->Chrom().empty() && m_pControlData->Chrom() != temp.Chrom() ) {
                return false;
            }
            if ( ! m_pControlData->Chrom().empty() && m_pControlData->SeqSpan() != temp.SeqSpan() ) {
                return false;
            }
            *m_pControlData = temp;
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_ParseDataRecord(
    const vector<string>& parts )
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
    unsigned int uLineType = x_GetLineType( parts );
    if ( m_uCurrentRecordType != uLineType ) {
                CLineError err( 
                    ILineError::eProblem_GeneralParsingError,
                    eDiag_Error, 
                    "", 
                    m_uLineNumber);
                throw err;
    }
    switch ( uLineType ) {

        default: {
                CLineError err( 
                    ILineError::eProblem_GeneralParsingError,
                    eDiag_Error, 
                    "", 
                    m_uLineNumber);
            throw err;
        }
        case TYPE_DATA_BED:
            m_pControlData->ParseDataBed( parts );
            break;

        case TYPE_DATA_VARSTEP:
            m_pControlData->ParseDataVarstep( parts );
            break;

        case TYPE_DATA_FIXEDSTEP:
            m_pControlData->ParseDataFixedstep( parts );
            break;
    }
}

//  ----------------------------------------------------------------------------
unsigned int CWiggleReader::x_GetLineType(
    const vector<string>& parts)
//  ----------------------------------------------------------------------------
{
    if ( parts.empty() || NStr::StartsWith( parts[0], "#" ) ) {
        return TYPE_COMMENT;
    }
    if ( parts[0] == "browser" ) {
        return TYPE_BROWSER;
    }
    if ( parts[0] == "track" ) {
        return TYPE_TRACK;
    }
    if ( parts[0] == "variableStep" ) {
        return TYPE_DECLARATION_VARSTEP;
    }
    if ( parts[0] == "fixedStep" ) {
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
    
    CLineError err(
        ILineError::eProblem_GeneralParsingError, eDiag_Error, "", m_uLineNumber);
    throw err;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_AssignBrowserData(
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void CWiggleReader::x_DumpStats(
    CNcbiOstream& out,
    CWiggleTrack* pTrack )
//  ----------------------------------------------------------------------------
{
    out << pTrack->Chrom() << ": " << pTrack->Count() << endl;      
}

END_objects_SCOPE
END_NCBI_SCOPE
