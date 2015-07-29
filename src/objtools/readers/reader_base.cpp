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
 *   Basic reader interface.
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

#include <objects/seqset/Seq_entry.hpp>
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

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gvf_reader.hpp>
#include <objtools/readers/vcf_reader.hpp>
#include <objtools/readers/rm_reader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/readers/ucscregion_reader.hpp>

#include <algorithm>
#include <ctime>

#include "reader_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CReaderBase*
CReaderBase::GetReader(
    CFormatGuess::EFormat format,
    TReaderFlags flags )
//  ----------------------------------------------------------------------------
{
    switch ( format ) {
    default:
        return 0;
    case CFormatGuess::eBed:
        return new CBedReader(flags);
    case CFormatGuess::eBed15:
        return new CMicroArrayReader(flags);
    case CFormatGuess::eWiggle:
        return new CWiggleReader(flags);
    case CFormatGuess::eGtf:
    case CFormatGuess::eGtf_POISENED:
        return new CGff3Reader(flags);
    case CFormatGuess::eGff3:
        return new CGff3Reader(flags);
    case CFormatGuess::eGvf:
        return new CGvfReader(flags);
    case CFormatGuess::eVcf:
        return new CVcfReader(flags);
    case CFormatGuess::eRmo:
        return new CRepeatMaskerReader(flags);
    case CFormatGuess::eFasta:
        return new CFastaReader(flags);
    case CFormatGuess::eFiveColFeatureTable:
        return new CFeature_table_reader(flags);
    case CFormatGuess::eUCSCRegion:
        return new CUCSCRegionReader(flags);
    }
}

//  ----------------------------------------------------------------------------
CReaderBase::CReaderBase(
    TReaderFlags flags,
    const string& annotName,
    const string& annotTitle) :
//  ----------------------------------------------------------------------------
    m_uLineNumber(0),
    m_uProgressReportInterval(0),
	m_uNextProgressReport(0),
    m_iFlags(flags),
    m_AnnotName(annotName),
    m_AnnotTitle(annotTitle),
    m_pReader(0),
    m_pCanceler(0)
{
    m_pTrackDefaults = new CTrackData;
}

//  ----------------------------------------------------------------------------
CReaderBase::~CReaderBase()
//  ----------------------------------------------------------------------------
{
    delete m_pTrackDefaults;
}

//  ----------------------------------------------------------------------------
CRef< CSerialObject >
CReaderBase::ReadObject(
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadObject( lr, pMessageListener );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqAnnot( lr, pMessageListener );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* ) 
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);
    return CRef<CSeq_annot>();
}
                
//  --------------------------------------------------------------------------- 
void
CReaderBase::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}
 
//  ---------------------------------------------------------------------------                       
void
CReaderBase::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    ILineErrorListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pMessageListener);
    while (annot) {
        //xAssignTrackData(annot);
        annots.push_back(annot);
        annot = ReadSeqAnnot(lr, pMessageListener);
    }
}
                        
//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    CRef<CSeq_entry> pResult = ReadSeqEntry( lr, pMessageListener );
    return pResult;
}

//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    ILineReader& lr,
    ILineErrorListener* ) 
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);
    return CRef<CSeq_entry>();
}
               
//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessError(
    CObjReaderLineException& err,
    ILineErrorListener* pContainer )
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber( m_uLineNumber );
    if (!pContainer) {
        err.Throw();
    }
    if (!pContainer->PutError(err)) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Critical,
            0,
            "Error allowance exceeded",
            ILineError::eProblem_GeneralParsingError) );
        pErr->Throw();
        //err.Throw();
    }
}

//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessWarning(
    CObjReaderLineException& err,
    ILineErrorListener* pContainer )
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber( m_uLineNumber );
    if (!pContainer) {
        cerr << m_uLineNumber << ": " << err.SeverityStr() << err.Message() 
            << endl;
        return;
    }
    if (!pContainer->PutError(err)) {
        err.Throw();
    }
}

//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessError(
    CLineError& err,
    ILineErrorListener* pContainer )
//  ----------------------------------------------------------------------------
{
    if (!pContainer  ||  !pContainer->PutError(err)) {
        err.Throw();
    }
 }

//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessWarning(
    CLineError& err,
    ILineErrorListener* pContainer )
//  ----------------------------------------------------------------------------
{
    if (!pContainer) {
        cerr << m_uLineNumber << ": " << err.SeverityStr() << err.Message() 
            << endl;
        return;
    }
    if (!pContainer->PutError(err)) {
        err.Throw();
    }
 }

//  ----------------------------------------------------------------------------
void CReaderBase::xSetBrowserRegion(
    const string& strRaw,
    CAnnot_descr& desc,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location( new CSeq_loc );

    string strChrom;
    string strInterval;
    if ( ! NStr::SplitInTwo( strRaw, ":", strChrom, strInterval ) ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad browser line: cannot parse browser position" ) );
        ProcessError(*pErr, pEC);
    }
    CRef<CSeq_id> id( new CSeq_id( CSeq_id::e_Local, strChrom ) );

    if (NStr::Compare(strInterval, "start-stop") == 0 )
    {
        location->SetWhole(*id);
    }
    else
    {
        string strFrom;
        string strTo;
        if ( ! NStr::SplitInTwo( strInterval, "-", strFrom, strTo ) ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad browser line: cannot parse browser position" ) );
            ProcessError(*pErr, pEC);
        }  
        try
        {
            int n_from,n_to;

            n_from = NStr::StringToInt(strFrom, NStr::fAllowCommas);
            n_to   = NStr::StringToInt(strTo, NStr::fAllowCommas);

            CSeq_interval& interval = location->SetInt();
            interval.SetFrom(n_from-1);
            interval.SetTo(n_to-1);
            interval.SetStrand( eNa_strand_unknown );
            location->SetId( *id );

        }
        catch (const CStringException&)
        {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad browser line: cannot parse browser position" ) );
            ProcessError(*pErr, pEC);
            location.Release();
        }
    }

    if (location.NotEmpty())
    {
        CRef<CAnnotdesc> region( new CAnnotdesc() );
        region->SetRegion( *location );
        desc.Set().push_back( region );
    }
}
    
//  ----------------------------------------------------------------------------
bool CReaderBase::xParseBrowserLine(
    const string& strLine,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "browser" ) ) {
        return false;
    }
    CAnnot_descr& desc = annot->SetDesc();
    
    vector<string> fields;
    NStr::Tokenize( strLine, " \t", fields, NStr::eMergeDelims );
    for ( vector<string>::iterator it = fields.begin(); it != fields.end(); ++it ) {
        if ( *it == "position" ) {
            ++it;
            if ( it == fields.end() ) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Error,
                    0,
                    "Bad browser line: incomplete position directive" ) );
                ProcessError(*pErr, pEC);
            }
            xSetBrowserRegion(*it, desc, pEC);
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
void CReaderBase::xAssignTrackData(
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    if (!m_AnnotName.empty()) {
        annot->SetNameDesc(m_AnnotName);
    }
    if (!m_AnnotTitle.empty()) {
        annot->SetTitleDesc(m_AnnotTitle);
    }
    if (!m_pTrackDefaults->ContainsData()) {
        return;
    }
    CAnnot_descr& desc = annot->SetDesc();
    CRef<CUser_object> trackdata( new CUser_object() );
    trackdata->SetType().SetStr( "Track Data" );
   
    if ( !m_pTrackDefaults->Description().empty() ) {
        annot->SetTitleDesc(m_pTrackDefaults->Description());
    }
    if ( !m_pTrackDefaults->Name().empty() ) {
        annot->SetNameDesc(m_pTrackDefaults->Name());
    }
    map<string,string>::const_iterator cit = m_pTrackDefaults->Values().begin();
    while ( cit != m_pTrackDefaults->Values().end() ) {
        trackdata->AddField( cit->first, cit->second );
        ++cit;
    }
    if ( trackdata->CanGetData() && ! trackdata->GetData().empty() ) {
        CRef<CAnnotdesc> user( new CAnnotdesc() );
        user->SetUser( *trackdata );
        desc.Set().push_back( user );
    }
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xParseTrackLine(
    const string& strLine,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    vector<string> parts;
    CReadUtil::Tokenize( strLine, " \t", parts );
    if ( !CTrackData::IsTrackData( parts ) ) {
        return false;
    }
    m_pTrackDefaults->ParseLine( parts );
    return true;
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xParseBrowserLine(
    const string& strLine,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
void CReaderBase::xSetTrackData(
    CRef<CSeq_annot>& annot,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    trackdata->AddField( strKey, strValue );
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xParseComment(
    const CTempString& record,
    CRef<CSeq_annot>& annot ) /* throws CObjReaderLineException */
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(record, "#")) {
        return true;
    }
    return false;
}
 
//  ----------------------------------------------------------------------------
void CReaderBase::xAddConversionInfo(
    CRef<CSeq_annot >& annot,
    ILineErrorListener *pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (!annot || !pMessageListener) {
        return;
    }
    if (0 == pMessageListener->LevelCount(eDiag_Critical)  &&
        0 == pMessageListener->LevelCount(eDiag_Error) &&
        0 == pMessageListener->LevelCount(eDiag_Warning) &&
        0 == pMessageListener->LevelCount(eDiag_Info)) {
        return;
    }


    CRef<CAnnotdesc> user(new CAnnotdesc());
    user->SetUser(*xMakeAsnConversionInfo(pMessageListener));
    annot->SetDesc().Set().push_back(user);
}

//  ----------------------------------------------------------------------------
void CReaderBase::xAddConversionInfo(
    CRef<CSeq_entry >& entry,
    ILineErrorListener *pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (!entry || !pMessageListener) {
        return;
    }
    CRef<CSeqdesc> user(new CSeqdesc());
    user->SetUser(*xMakeAsnConversionInfo(pMessageListener));
    entry->SetDescr().Set().push_back(user);
}

//  ----------------------------------------------------------------------------
CRef<CUser_object> CReaderBase::xMakeAsnConversionInfo(
    ILineErrorListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> conversioninfo(new CUser_object());
    conversioninfo->SetType().SetStr("Conversion Info");    
    conversioninfo->AddField( 
        "critical errors", int(pMessageListener->LevelCount(eDiag_Critical)));
    conversioninfo->AddField( 
        "errors", int(pMessageListener->LevelCount(eDiag_Error)));
    conversioninfo->AddField( 
        "warnings", int(pMessageListener->LevelCount(eDiag_Warning)));
    conversioninfo->AddField( 
        "notes", int(pMessageListener->LevelCount(eDiag_Info)));
    return conversioninfo;
}

//  ----------------------------------------------------------------------------
void CReaderBase::SetProgressReportInterval(
    unsigned int intv)
//  ----------------------------------------------------------------------------
{
    m_uProgressReportInterval = intv;
    m_uNextProgressReport = (unsigned int)time(0) + intv;
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xIsReportingProgress() const
//  ----------------------------------------------------------------------------
{
    if (0 == m_uProgressReportInterval) {
        return false;
    }
    if (0 == m_pReader) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CReaderBase::xReportProgress(
    ILineErrorListener* pProgress)
//  ----------------------------------------------------------------------------
{
    if (!xIsReportingProgress()) { // progress reports disabled
        return;
    }
    unsigned int uCurrentTime = (unsigned int)time(0);
    if (uCurrentTime < m_uNextProgressReport) { // not time yet
        return;
    }
    // report something
    ios::streampos curPos = m_pReader->GetPosition();
    pProgress->PutProgress("Progress", Uint8(curPos), 0);

    m_uNextProgressReport += m_uProgressReportInterval;
}

//  ============================================================================
bool CReaderBase::xReadInit()
//  ============================================================================
{
    return true;
}

//  ============================================================================
bool CReaderBase::xProgressInit(
    ILineReader& lr)
//  ============================================================================
{
    if (0 == m_uProgressReportInterval) {
        return true;
    }
    m_pReader = &lr;
    return true;
}

//  ============================================================================
void CReaderBase::SetCanceler(
    ICanceled* pCanceler)
//  ============================================================================
{
    m_pCanceler = pCanceler;
}

//  ============================================================================
bool CReaderBase::xIsOperationCanceled() const
//  ============================================================================
{
    if (!m_pCanceler) {
        return false;
    }
    return m_pCanceler->IsCanceled();
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xIsCommentLine(
    const CTempString& strLine)
//  ----------------------------------------------------------------------------
{
    if (strLine.empty()) {
        return true;
    }
    return (strLine[0] == '#' && strLine[1] != '#');
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xIsTrackLine(
    const CTempString& strLine)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(strLine, "track ")) {
        return true;
    }
    return NStr::StartsWith(strLine, "track\t");
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xIsBrowserLine(
    const CTempString& strLine)
//  ----------------------------------------------------------------------------
{
    return NStr::StartsWith(strLine, "browser ");
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xGetLine(
    ILineReader& lr,
    string& line)
//  ----------------------------------------------------------------------------
{
    while (!lr.AtEOF()) {
        line = *++lr;
        ++m_uLineNumber;
        NStr::TruncateSpacesInPlace(line);
        if (!xIsCommentLine(line)) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xUngetLine(
    ILineReader& lr)
//  ----------------------------------------------------------------------------
{
    lr.UngetLine();
    --m_uLineNumber;
    return true;
}
END_objects_SCOPE
END_NCBI_SCOPE
