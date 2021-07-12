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
#include <objtools/readers/track_data.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/reader_message.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/readers/gvf_reader.hpp>
#include <objtools/readers/vcf_reader.hpp>
#include <objtools/readers/rm_reader.hpp>
#include <objtools/readers/psl_reader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/readers/ucscregion_reader.hpp>

#include <algorithm>
#include <ctime>

#include "reader_data.hpp"
#include "reader_message_handler.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CReaderBase*
CReaderBase::GetReader(
    CFormatGuess::EFormat format,
    TReaderFlags flags,
    CReaderListener* pRL )
//  ----------------------------------------------------------------------------
{
    switch ( format ) {
    default:
        return 0;
    case CFormatGuess::eBed:
        return new CBedReader(flags);
    case CFormatGuess::eBed15:
        return new CMicroArrayReader(flags, pRL);
    case CFormatGuess::eWiggle:
        return new CWiggleReader(flags);
    case CFormatGuess::eGtf:
    case CFormatGuess::eGtf_POISENED:
        return new CGtfReader(flags);
    case CFormatGuess::eGff3:
        return new CGff3Reader(flags);
    case CFormatGuess::eGvf:
        return new CGvfReader(flags);
    case CFormatGuess::eVcf:
        return new CVcfReader(flags, pRL);
    case CFormatGuess::eRmo:
        return new CRepeatMaskerReader(flags);
    case CFormatGuess::eFasta:
        return new CFastaReader(flags);
    case CFormatGuess::eFiveColFeatureTable:
        return new CFeature_table_reader(flags);
    case CFormatGuess::eUCSCRegion:
        return new CUCSCRegionReader(flags);
    case CFormatGuess::ePsl:
        return new CPslReader(flags, pRL);
    }
}

//  ----------------------------------------------------------------------------
CReaderBase::CReaderBase(
    TReaderFlags flags,
    const string& annotName,
    const string& annotTitle,
    SeqIdResolver seqidresolver,
    CReaderListener* pListener) :
//  ----------------------------------------------------------------------------
    m_uLineNumber(0),
    m_uProgressReportInterval(0),
    m_uNextProgressReport(0),
    m_iFlags(flags),
    m_AnnotName(annotName),
    m_AnnotTitle(annotTitle),
    m_pTrackDefaults(new CTrackData),
    m_pReader(nullptr),
    m_pCanceler(nullptr),
    mSeqIdResolve(seqidresolver),
    m_pMessageHandler(new CReaderMessageHandler(pListener))
{
}


//  ----------------------------------------------------------------------------
CReaderBase::~CReaderBase()
//  ----------------------------------------------------------------------------
{
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
CRef<CSerialObject>
CReaderBase::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    CRef<CSerialObject> object(
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );
    return object;
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
    ILineErrorListener* pEL)
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);

    m_uDataCount = 0;
    CRef<CSeq_annot> pAnnot = xCreateSeqAnnot();

    TReaderData readerData;
    xGuardedGetData(lr, readerData, pEL);
    if (readerData.empty()) {
        pAnnot.Reset();
        return pAnnot;
    }
    while (!readerData.empty()) {
        if (IsCanceled()) {
            CReaderMessage cancelled(
                eDiag_Fatal,
                m_uLineNumber,
                "Data import interrupted by user.");
            xProcessReaderMessage(cancelled, pEL);
        }
        xReportProgress();

        xGuardedProcessData(readerData, *pAnnot, pEL);
        xGuardedGetData(lr, readerData, pEL);
    }
    xPostProcessAnnot(*pAnnot);
    return pAnnot;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CReaderBase::xCreateSeqAnnot()
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pAnnot(new CSeq_annot);
    if (!m_AnnotName.empty()) {
        pAnnot->SetNameDesc(m_AnnotName);
    }
    if (!m_AnnotTitle.empty()) {
        pAnnot->SetTitleDesc(m_AnnotTitle);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CReaderBase::xGuardedGetData(
    ILineReader& lr,
    TReaderData& readerData,
    ILineErrorListener* pEL)
//  ----------------------------------------------------------------------------
{
    try {
        xGetData(lr, readerData);
    }
    catch (CReaderMessage& err) {
        xProcessReaderMessage(err, pEL);
    }
    catch (ILineError& err) {
        xProcessLineError(err, pEL);
    }
    catch (CException& err) {
        xProcessUnknownException(err);
    }
}

//  ----------------------------------------------------------------------------
void
CReaderBase::xGuardedProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot,
    ILineErrorListener* pEL)
//  ----------------------------------------------------------------------------
{
    try {
        xProcessData(readerData, annot);
    }
    catch (CReaderMessage& err) {
        xProcessReaderMessage(err, pEL);
    }
    catch (ILineError& err) {
        xProcessLineError(err, pEL);
    }
    catch (CException& err) {
        xProcessUnknownException(err);
    }
}

//  ----------------------------------------------------------------------------
void
CReaderBase::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    readerData.clear();
    string line;
    if (xGetLine(lr, line)) {
        readerData.push_back(TReaderLine{m_uLineNumber, line});
    }
    ++m_uDataCount;
}

//  ----------------------------------------------------------------------------
void
CReaderBase::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
}

//  ---------------------------------------------------------------------------
void
CReaderBase::ReadSeqAnnots(
    TAnnots& annots,
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
    TAnnots& annots,
    ILineReader& lr,
    ILineErrorListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    xReadInit();
    xProgressInit(lr);
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pMessageListener);
    while (annot) {
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
    CAnnot_descr& desc)
//  ----------------------------------------------------------------------------
{
    CReaderMessage error(
        eDiag_Error,
        m_uLineNumber,
        "Bad browser line: cannot parse browser position.");

    CRef<CSeq_loc> location( new CSeq_loc );

    string strChrom;
    string strInterval;
    if ( ! NStr::SplitInTwo( strRaw, ":", strChrom, strInterval ) ) {
        throw error;
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
            throw error;
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
        catch (const CStringException&) {
            location.Reset();
            throw error;
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
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    CReaderMessage error(
        eDiag_Error,
        m_uLineNumber,
        "Bad browser line: incomplete position directive.");

    if ( ! NStr::StartsWith( strLine, "browser" ) ) {
        return false;
    }
    CAnnot_descr& desc = annot.SetDesc();

    vector<string> fields;
    NStr::Split(strLine, " \t", fields, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    for ( vector<string>::iterator it = fields.begin(); it != fields.end(); ++it ) {
        if ( *it == "position" ) {
            ++it;
            if ( it == fields.end() ) {
               throw error;
            }
            xSetBrowserRegion(*it, desc);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CReaderBase::xAssignTrackData(
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if (!m_AnnotName.empty()) {
        annot.SetNameDesc(m_AnnotName);
    }
    if (!m_AnnotTitle.empty()) {
        annot.SetTitleDesc(m_AnnotTitle);
    }
    m_pTrackDefaults->WriteToAnnot(annot);
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xParseTrackLine(
    const string& strLine)
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
bool CReaderBase::xParseComment(
    const CTempString& record,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(record, "#")) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
void CReaderBase::xPostProcessAnnot(
    CSeq_annot&)
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void CReaderBase::xAddConversionInfo(
    CSeq_annot& annot,
    ILineErrorListener* pML)
//  ----------------------------------------------------------------------------
{
    size_t countInfos = m_pMessageHandler->LevelCount(eDiag_Info);
    size_t countWarnings = m_pMessageHandler->LevelCount(eDiag_Warning);
    size_t countErrors = m_pMessageHandler->LevelCount(eDiag_Error);
    size_t countCritical = m_pMessageHandler->LevelCount(eDiag_Critical);
    if (pML) {
        countCritical += pML->LevelCount(eDiag_Critical);
        countErrors += pML->LevelCount(eDiag_Error);
        countWarnings += pML->LevelCount(eDiag_Warning);
        countInfos += pML->LevelCount(eDiag_Info);
    }
    if (countInfos + countWarnings + countErrors + countCritical == 0) {
        return;
    }
    CRef<CUser_object> conversioninfo(new CUser_object());
    conversioninfo->SetType().SetStr("Conversion Info");
    conversioninfo->AddField( "critical errors", static_cast<int>(countCritical));
    conversioninfo->AddField( "errors", static_cast<int>(countErrors));
    conversioninfo->AddField( "warnings", static_cast<int>(countWarnings));
    conversioninfo->AddField( "notes", static_cast<int>(countInfos));

    CRef<CAnnotdesc> user(new CAnnotdesc());
    user->SetUser(*conversioninfo);
    annot.SetDesc().Set().push_back(user);
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
    int curPos = static_cast<int>(m_pReader->GetPosition());
    m_pMessageHandler->Progress(CReaderProgress(curPos, 0));
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
    if (strLine == "track") {
        return true;
    }
    if (NStr::StartsWith(strLine, "track ")) {
        return true;
    }
    return NStr::StartsWith(strLine, "track\t");
}

//  ----------------------------------------------------------------------------
bool CReaderBase::xIsTrackTerminator(
    const CTempString& strLine)
//  ----------------------------------------------------------------------------
{
    auto line = NStr::TruncateSpaces_Unsafe(strLine);
    return (line == "###");
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
    if (!m_PendingLine.empty()) {
        line = m_PendingLine;
        m_PendingLine.clear();
        return true;
    }
    CTempString temp;
    while (!lr.AtEOF()) {
        temp = *++lr;
        ++m_uLineNumber;
        temp = NStr::TruncateSpaces_Unsafe(temp);
        if (!xIsCommentLine(temp)) {
            line = temp;
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

//  ----------------------------------------------------------------------------
void
CReaderBase::xProcessReaderMessage(
    CReaderMessage& readerMessage,
    ILineErrorListener* pEL)
//
//  Strategy:
//  (0) Above all, don't swallow FATAl errors as they are guaranteed to stop
//      the program on the spot.
//  (1) Give readerMessage to internal message handler. If configured properly
//      it will handle readerMessage.Otherwise, it will emit an ILineError.
//  (2) If an ILineError is emitted and we have an actual ILineErrorListener
//      then give it a shot.
//  (3) If we don't have an ILineErrorListener or it the ILineErrorListener
//      does not want the ILineError then rethrow the ILineError and hope for
//      the best.
//  ----------------------------------------------------------------------------
{
    readerMessage.SetLineNumber(m_uLineNumber);
    try {
        m_pMessageHandler->Report(readerMessage);
        if (readerMessage.Severity() == eDiag_Fatal) {
            throw;
        }
    }
    catch(ILineError& lineError) {
        xProcessLineError(lineError, pEL);
    }
};

//  ----------------------------------------------------------------------------
void
CReaderBase::xProcessLineError(
    const ILineError& lineError,
    ILineErrorListener* pEL)
//
//  This is to deal with legacy format readers that may throw ILIneError instead
//  of the preferred CReaderMessage.
//
//  Strategy:
//  (1) If pEL is good, then give the lineError to pEL.
//  (2) If pEL doesn't accept the lineError then throw it (and hope some upper
//      layer knows what to do with it).
//  ----------------------------------------------------------------------------
{
    if (!pEL  ||  !pEL->PutMessage(lineError)) {
        throw;
    }
}

//  ----------------------------------------------------------------------------
void
CReaderBase::xProcessUnknownException(
    const CException& error)
//
//  If we get errors outside of the established type system then there is no way
//  of knowing what happened, how it happened, how much data is bad, or even
//  whether there is any way of continuing at all.
//  We therefore turn all such errors into a FATAL CReaderMessage and rethrow
//  it.
//  ----------------------------------------------------------------------------
{
    CReaderMessage terminator(
        eDiag_Fatal,
        m_uLineNumber,
        "Exception: " + error.GetMsg());
    throw(terminator);
}

END_objects_SCOPE
END_NCBI_SCOPE
