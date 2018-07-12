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
 *   BED file reader
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
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
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
#include <objtools/readers/track_data.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <deque>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CLinePreBuffer
//  ============================================================================
{
public:
    using LinePreIt = deque<string>::const_iterator;

    CLinePreBuffer(
        ILineReader& lineReader):
        mLineReader(lineReader),
        mLineNumber(0)
    {};

    virtual ~CLinePreBuffer() {};

    bool FillBuffer(
        size_t numLines)
    {
        CTempString line;
        while (numLines  &&  !mLineReader.AtEOF()) {
            line = *++mLineReader;
            NStr::TruncateSpacesInPlace(line);
            mBuffer.push_back(line);
            if (!IsCommentLine(line)) {
                --numLines;
            }
        }
        return true;
    }
            
    virtual bool IsCommentLine(
        const CTempString& line)
    {
        if (NStr::StartsWith(line, "#")) {
            return true;
        }
        if (NStr::IsBlank(line)) {
            return true;
        }
        return false;
    };

    bool GetLine(
        string& line)
    {
        while (!mBuffer.empty()  ||  !mLineReader.AtEOF()) {
            string temp;
            if (!mBuffer.empty()) {
                temp = mBuffer.front();
                mBuffer.pop_front();
            }
            else {
                temp = *++mLineReader;
                NStr::TruncateSpacesInPlace(temp);
            }
            if (!IsCommentLine(temp)) {
                line = temp;
                ++mLineNumber;
                return true;
            }
        }
        return false;   
    };

    bool UngetLine(
        const string& line)
    {
        mBuffer.push_front(line);
        --mLineNumber;
        return true;
    }

    size_t LineNumber() const
    {
        return mLineNumber;
    };

    LinePreIt begin()
    {
        return mBuffer.begin();
    };

    LinePreIt end()
    {
        return mBuffer.end();
    };

protected:
    ILineReader& mLineReader;
    deque<string> mBuffer;
    size_t mLineNumber;
};


//  ----------------------------------------------------------------------------
CBedReader::CBedReader(
    int flags,
    const string& annotName,
    const string& annotTitle ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags, annotName, annotTitle),
    m_currentId(""),
    mRealColumnCount(0),
    mValidColumnCount(0),
    mAssumeErrorsAreRecordLevel(true),
    m_CurrentFeatureCount(0),
    m_usescore(false),
    m_CurBatchSize(0),
    m_MaxBatchSize(10000)
{
}


//  ----------------------------------------------------------------------------
CBedReader::~CBedReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CBedReader::ReadSeqAnnot(
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqAnnot( lr, pMessageListener );
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CBedReader::ReadSeqAnnot(
    ILineReader& lineReader,
    ILineErrorListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    xProgressInit(lineReader);

    CRef<CSeq_annot> annot;
    CRef<CAnnot_descr> desc;

    annot.Reset(new CSeq_annot);
    desc.Reset(new CAnnot_descr);
    annot->SetDesc(*desc);

    m_CurrentFeatureCount = 0;

    xParseTrackLine("track", pEC);

    string line;
    CLinePreBuffer preBuffer(lineReader);

    try {
        xDetermineLikelyColumnCount(preBuffer, pEC);
        while (preBuffer.GetLine(line)) {
            m_uLineNumber = preBuffer.LineNumber();
            // interact with calling party
            if (IsCanceled()) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Info,
                    0,
                    "Reader stopped by user.",
                    ILineError::eProblem_ProgressInfo));
                ProcessError(*pErr, pEC);
                return CRef<CSeq_annot>();
            }
            xReportProgress(pEC);

            if (xIsTrackLine(line)) {
                if (!m_CurrentFeatureCount) {
                    xParseTrackLine(line, pEC);
                    continue;
                }
                preBuffer.UngetLine(line);
                xDetermineLikelyColumnCount(preBuffer, pEC);
                break;
            }
            if (xParseBrowserLine(line, annot, pEC)) {
                continue;
            }
            if (xParseFeature(line, annot, pEC)) {
                continue;
            }
        }
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return CRef<CSeq_annot>();
    }

    //  Only return a valid object if there was at least one feature
    if (0 == m_CurrentFeatureCount) {
        return CRef<CSeq_annot>();
    }
    xPostProcessAnnot(annot, pEC);
    return annot;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xDetermineLikelyColumnCount(
    CLinePreBuffer& preBuffer,
    ILineErrorListener* pEc)
//  ----------------------------------------------------------------------------
{
    using LineIt = CLinePreBuffer::LinePreIt;

    AutoPtr<CObjReaderLineException> pErrColumnCount(
        CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Bad data line: Inconsistent column count." ) );
    AutoPtr<CObjReaderLineException> pErrChromBounds(
        CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Bad data line: Invalid chrom boundaries." ) );

    const size_t MIN_SAMPLE_SIZE = 50;
    preBuffer.FillBuffer(MIN_SAMPLE_SIZE);

    mRealColumnCount = mValidColumnCount = 0;
    vector<string>::size_type realColumnCount = 0;
    vector<string>::size_type validColumnCount = 0;
    for (LineIt lineIt = preBuffer.begin(); lineIt != preBuffer.end(); ++lineIt) {
        const auto& line = *lineIt;
        if (preBuffer.IsCommentLine(line)) {
            continue;
        }
        if (this->xIsTrackLine(line)) {
            continue;
        }
        if (this->xIsBrowserLine(line)) {
            continue;
        }
        vector<string> columns;
        NStr::Split(line, " \t", columns, NStr::fSplit_MergeDelimiters);
        xCleanColumnValues(columns);
        if (realColumnCount == 0 ) {
            realColumnCount = columns.size();
        }
        if (realColumnCount != columns.size()) {
            pErrColumnCount->Throw();
        }
        
        if (validColumnCount == 0) {
            validColumnCount = realColumnCount;
            if (validColumnCount > 12) {
                validColumnCount = 12;
            }
        }
        unsigned long chromStart = 0, chromEnd = 0;
        try {
            chromStart = NStr::StringToULong(columns[1]);
            chromEnd = NStr::StringToULong(columns[2]);
        }
        catch (CException&) {
            pErrChromBounds->Throw();
        }
        if (validColumnCount >= 7) {
            try {
                auto thickStart = NStr::StringToULong(columns[6]);
                if (thickStart < chromStart  ||  chromEnd < thickStart) {
                    validColumnCount = 6;
                }
            }
            catch(CException&) {
                validColumnCount = 6;
            }
        }
        if (validColumnCount >= 8) {
            try {
                auto thickEnd = NStr::StringToULong(columns[7]);
                if (thickEnd < chromStart  ||  chromEnd < thickEnd) {
                    validColumnCount = 6;
                }
            }
            catch(CException&) {
                validColumnCount = 6;
            }
        }

        int blockCount;
        if (validColumnCount >= 10) {
            try {
                blockCount = NStr::StringToInt(
                    columns[9], NStr::fDS_ProhibitFractions);
                if (blockCount < 1) {
                    validColumnCount = 9;
                }
            }
            catch(CException&) {
                validColumnCount = 9;
            }
        }
        if (validColumnCount >= 11) {
            vector<string> blockSizes;
            auto col10 = columns[10];
            if (NStr::EndsWith(col10, ",")) {
                col10 = col10.substr(0, col10.size()-1);
            }
            NStr::Split(col10, ",", blockSizes, NStr::fSplit_MergeDelimiters);
            if (blockSizes.size() != blockCount) {
                validColumnCount = 9;
            }
            else {
                try {
                    for (auto blockSize: blockSizes) {
                        NStr::StringToULong(blockSize);
                    }
                }
                catch(CException&) {
                    validColumnCount = 9;
                }
            }
        }
        if (validColumnCount >= 12) {
            vector<string> blockStarts;
            auto col11 = columns[11];
            if (NStr::EndsWith(col11, ",")) {
                col11 = col11.substr(0, col11.size()-1);
            }
            NStr::Split(col11, ",", blockStarts, NStr::fSplit_MergeDelimiters);
            if (blockStarts.size() != blockCount) {
                validColumnCount = 9;
            }
            else {
                try {
                    for (auto blockStart: blockStarts) {
                        NStr::StringToULong(blockStart);
                    }
                }
                catch(CException&) {
                    validColumnCount = 9;
                }
            }
        }   
    } 
    mRealColumnCount = realColumnCount;
    mValidColumnCount = validColumnCount;
    mAssumeErrorsAreRecordLevel = (
        validColumnCount == realColumnCount  &&
        validColumnCount != 7  &&
        validColumnCount != 10  &&
        validColumnCount != 11);
        
    return true;
}

//  ----------------------------------------------------------------------------
void CBedReader::xPostProcessAnnot(
    CRef<CSeq_annot>& annot,
    ILineErrorListener *pEC)
//  ----------------------------------------------------------------------------
{
    xAddConversionInfo(annot, pEC);
    xAssignTrackData(annot);
    xAssignBedColumnCount(*annot);
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CBedReader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );    
    return object;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xParseTrackLine(
    const string& strLine,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "track" ) ) {
        return false;
    }
    vector<string> parts;
    CReadUtil::Tokenize( strLine, " \t", parts );
    if (parts.size() >= 3) {
        const string digits("0123456789");
        bool col2_is_numeric = 
            (string::npos == parts[1].find_first_not_of(digits));
        bool col3_is_numeric = 
            (string::npos == parts[2].find_first_not_of(digits));
        if (col2_is_numeric  &&  col3_is_numeric) {
            return false;
        }
    }
    m_currentId.clear();
    if (!CReaderBase::xParseTrackLine(strLine, pEC)) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Bad track line: Expected \"track key1=value1 key2=value2 ...\". Ignored.",
            ILineError::eProblem_BadTrackLine) );
        ProcessWarning(*pErr , pEC);    
    }
    return true;
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CBedReader::x_AppendAnnot(
    vector< CRef< CSeq_annot > >& annots )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_annot > annot( new CSeq_annot );
    CRef< CAnnot_descr > desc( new CAnnot_descr );
    annot->SetDesc( *desc );
    annots.push_back( annot );
    return annot;
}    
    
//  ----------------------------------------------------------------------------
bool
CBedReader::xParseFeature(
    const string& line,
    CRef<CSeq_annot>& pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
	CTempString record_copy = NStr::TruncateSpaces_Unsafe(line);

    //  parse
    vector<string> fields;
    NStr::Split(record_copy, " \t", fields, NStr::fSplit_MergeDelimiters);
    try {
        xCleanColumnValues(fields);
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }
    if (xParseFeature(fields, pAnnot, pEC)) {
        ++m_CurrentFeatureCount;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeature(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    static int count = 0;
    count++;

    if (fields.size()!= mRealColumnCount) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Inconsistent column count." ) );
        pErr->Throw();
    }

    if (m_iFlags & CBedReader::fThreeFeatFormat) {
        return xParseFeatureThreeFeatFormat(fields, annot, pEC);
    }
    else if (m_iFlags & CBedReader::fDirectedFeatureModel) {
        return xParseFeatureGeneModelFormat(fields, annot, pEC);
    }
    else {
        return xParseFeatureUserFormat(fields, annot, pEC);
    }
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureThreeFeatFormat(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
     unsigned int baseId = 3*m_CurrentFeatureCount;

    if (!xAppendFeatureChrom(fields, annot, baseId, pEC)) {
        return false;
    }
    if (xContainsThickFeature(fields)  &&  
            !xAppendFeatureThick(fields, annot, baseId, pEC)) {
        return false;
    }
    if (xContainsBlockFeature(fields)  &&
            !xAppendFeatureBlock(fields, annot, baseId, pEC)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureGeneModelFormat(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    unsigned int baseId = 3*m_CurrentFeatureCount;

    CRef<CSeq_feat> pGene = xAppendFeatureGene(fields, annot, baseId, pEC);
    if (!pGene) {
        return false;
    }

    CRef<CSeq_feat> pRna;
    if (xContainsRnaFeature(fields)) {
        pRna = xAppendFeatureRna(fields, annot, baseId, pEC);
        if (!pRna) {
            return false;
        }
    }

    CRef<CSeq_feat> pCds;
    if (xContainsCdsFeature(fields)) {
        pCds = xAppendFeatureCds(fields, annot, baseId, pEC);
        if (!pCds) {
            return false;
        }
    }

    if (pRna  &&  pCds) {
        CRef<CSeq_loc> pCdsLoc(new CSeq_loc);
        pCdsLoc->Assign(pCds->GetLocation());
        pRna->SetLocation(*pCdsLoc);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureChrom(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        //xSetFeatureTitle(feature, fields);
        xSetFeatureLocationChrom(feature, fields);
        xSetFeatureIdsChrom(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back(feature);
    m_currentId = fields[0];
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureGene(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        ////xSetFeatureTitle(feature, fields);
        xSetFeatureLocationGene(feature, fields);
        xSetFeatureIdsGene(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return CRef<CSeq_feat>();
    }
    ftable.push_back(feature);
    m_currentId = fields[0];
    return feature;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureThick(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        //xSetFeatureTitle(feature, fields);
        xSetFeatureLocationThick(feature, fields);
        xSetFeatureIdsThick(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back(feature);
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureCds(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        ////xSetFeatureTitle(feature, fields);
        xSetFeatureLocationCds(feature, fields);
        xSetFeatureIdsCds(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return CRef<CSeq_feat>();
    }
    ftable.push_back(feature);
    return feature;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureBlock(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        ////xSetFeatureTitle(feature, fields);
        xSetFeatureLocationBlock(feature, fields);
        xSetFeatureIdsBlock(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back(feature);
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureRna(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        //xSetFeatureTitle(feature, fields);
        xSetFeatureLocationRna(feature, fields);
        xSetFeatureIdsRna(feature, fields, baseId);
        xSetFeatureBedData(feature, fields, pEC);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return CRef<CSeq_feat>();
    }
    ftable.push_back(feature);
    return feature;
}


//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureUserFormat(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    //  assign
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset( new CSeq_feat );
    try {
        xSetFeatureTitle(feature, fields);
        x_SetFeatureLocation(feature, fields);
        x_SetFeatureDisplayData(feature, fields);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back( feature );
    m_currentId = fields[0];
    return true;
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    if (mValidColumnCount >= 4) {
        display_data->AddField( "name", fields[3] );
    }
    else {
        display_data->AddField( "name", string("") );
        feature->SetData().SetUser( *display_data );
        return;
    }
    if (mValidColumnCount >= 5) {
        if ( !m_usescore ) {
            display_data->AddField( 
                "score",
                NStr::StringToInt(fields[4],
				NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols) );
        }
        else {
            display_data->AddField( 
                "greylevel",
               	NStr::StringToInt(fields[4], 
				NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols) );
        }
    }
    if (mValidColumnCount >= 7) {
        display_data->AddField( 
            "thickStart",
            NStr::StringToInt(fields[6], NStr::fDS_ProhibitFractions) );
    }
    if (mValidColumnCount >= 8) {
        display_data->AddField( 
            "thickEnd",
            NStr::StringToInt(fields[7], NStr::fDS_ProhibitFractions) - 1 );
    }
    if (mValidColumnCount >= 9) {
        display_data->AddField( 
            "itemRGB",
            fields[8]);
    }
    if (mValidColumnCount >= 10) {
        display_data->AddField( 
            "blockCount",
            NStr::StringToInt(fields[9], NStr::fDS_ProhibitFractions) );
    }
    if (mValidColumnCount >= 11) {
        display_data->AddField( "blockSizes", fields[10] );
    }
    if (mValidColumnCount >= 12) {
        display_data->AddField( "blockStarts", fields[11] );
    }
    feature->SetData().SetUser( *display_data );
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationChrom(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    x_SetFeatureLocation(feature, fields);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "chrom");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationGene(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    x_SetFeatureLocation(feature, fields);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "chrom");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationThick(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(fields[6]);
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"ThickStart\" value." ) );
        pErr->Throw();
    }
    try {
        to = NStr::StringToInt(fields[7]) - 1;
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"ThickStop\" value.") );
        pErr->Throw();
    }
    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else if (from > to) {
        //below: flip commenting to switch from null locations to impossible 
        // intervals
        //location->SetInt().SetFrom(from);
        //location->SetInt().SetTo(to);
        location->SetNull();
    }

    if (!location->IsNull()) {
        location->SetStrand(xGetStrand(fields));
    }
    CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0], m_iFlags, false);
    location->SetId(*id);
    feature->SetLocation(*location);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "thick");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationCds(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(fields[6]);
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"ThickStart\" value." ) );
        pErr->Throw();
    }
    try {
        to = NStr::StringToInt(fields[7]) - 1;
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"ThickStop\" value.") );
        pErr->Throw();
    }
    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else if (from > to) {
        //below: flip commenting to switch from null locations to impossible 
        // intervals
        //location->SetInt().SetFrom(from);
        //location->SetInt().SetTo(to);
        location->SetNull();
    }

    if (!location->IsNull()) {
        location->SetStrand(xGetStrand(fields));
    }
    CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0], m_iFlags, false);
    location->SetId(*id);
    feature->SetLocation(*location);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "thick");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
ENa_strand CBedReader::xGetStrand(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    size_t strand_field = 5;
    if (fields.size() == 5  &&  (fields[4] == "-"  ||  fields[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < fields.size()) {
        string strand = fields[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Invalid strand character." ) );
            pErr->Throw();
        }
    }
    return (fields[strand_field] == "-" ? eNa_strand_minus : eNa_strand_plus);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationBlock(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    //already established: there are sufficient columns to do this
    size_t blockCount = NStr::StringToUInt(fields[9]);
    vector<size_t> blockSizes;
    vector<size_t> blockStarts;
    {{
        blockSizes.reserve(blockCount);
        vector<string> vals; 
        NStr::Split(fields[10], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Bad value count in \"blockSizes\"." ) );
            pErr->Throw();
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockSizes.push_back(NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Malformed \"blockSizes\" column." ) );
            pErr->Throw();
        }
    }}
    {{
        blockStarts.reserve(blockCount);
        vector<string> vals; 
        size_t baseStart = NStr::StringToUInt(fields[1]);
        NStr::Split(fields[11], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Bad value count in \"blockStarts\"." ) );
            pErr->Throw();
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockStarts.push_back(baseStart + NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Malformed \"blockStarts\" column." ) );
            pErr->Throw();
        }
    }}

    CPacked_seqint& location = feature->SetLocation().SetPacked_int();
    
    ENa_strand strand = xGetStrand(fields);
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(fields[0], m_iFlags, false);

    bool negative = fields[5] == "-";

    CPacked_seqint::Tdata& blocks = location.Set();

    for (size_t i=0; i < blockCount; ++i) {
        CRef<CSeq_interval> pInterval(new CSeq_interval);
        pInterval->SetId(*pId);
        pInterval->SetFrom(static_cast<CSeq_interval::TFrom>(blockStarts[i]));
        pInterval->SetTo(static_cast<CSeq_interval::TTo>(
            blockStarts[i] + blockSizes[i] - 1));
        pInterval->SetStrand(strand);
        if (negative)
            blocks.insert(blocks.begin(), pInterval);
        else
            blocks.push_back(pInterval);
    }

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "block");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationRna(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields)
//  ----------------------------------------------------------------------------
{
    //already established: there are sufficient columns to do this
    size_t blockCount = NStr::StringToUInt(fields[9]);
    vector<size_t> blockSizes;
    vector<size_t> blockStarts;
    {{
        blockSizes.reserve(blockCount);
        vector<string> vals; 
        NStr::Split(fields[10], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Bad value count in \"blockSizes\"." ) );
            pErr->Throw();
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockSizes.push_back(NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Malformed \"blockSizes\" column." ) );
            pErr->Throw();
        }
    }}
    {{
        blockStarts.reserve(blockCount);
        vector<string> vals; 
        size_t baseStart = NStr::StringToUInt(fields[1]);
        NStr::Split(fields[11], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Bad value count in \"blockStarts\"." ) );
            pErr->Throw();
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockStarts.push_back(baseStart + NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Malformed \"blockStarts\" column." ) );
            pErr->Throw();
        }
    }}

    CPacked_seqint& location = feature->SetLocation().SetPacked_int();
    
    ENa_strand strand = xGetStrand(fields);
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(fields[0], m_iFlags, false);

    bool negative = fields[5] == "-";

    CPacked_seqint::Tdata& blocks = location.Set();

    for (size_t i=0; i < blockCount; ++i) {
        CRef<CSeq_interval> pInterval(new CSeq_interval);
        pInterval->SetId(*pId);
        pInterval->SetFrom(static_cast<CSeq_interval::TFrom>(blockStarts[i]));
        pInterval->SetTo(static_cast<CSeq_interval::TTo>(
            blockStarts[i] + blockSizes[i] -1));
        pInterval->SetStrand(strand);
        if (negative)
            blocks.insert(blocks.begin(), pInterval);
        else
            blocks.push_back(pInterval);
    }

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "block");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsChrom(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId);

    if (xContainsThickFeature(fields)) {
        CRef<CFeat_id> pIdThick(new CFeat_id);
        pIdThick->SetLocal().SetId(baseId+1);
        CRef<CSeqFeatXref> pXrefThick(new CSeqFeatXref);
        pXrefThick->SetId(*pIdThick);  
        feature->SetXref().push_back(pXrefThick);
    }
    
    if (xContainsBlockFeature(fields)) {
        CRef<CFeat_id> pIdBlock(new CFeat_id);
        pIdBlock->SetLocal().SetId(baseId+2);
        CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
        pXrefBlock->SetId(*pIdBlock);  
        feature->SetXref().push_back(pXrefBlock);     
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsGene(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsThick(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId+1);

    CRef<CFeat_id> pIdChrom(new CFeat_id);
    pIdChrom->SetLocal().SetId(baseId);
    CRef<CSeqFeatXref> pXrefChrom(new CSeqFeatXref);
    pXrefChrom->SetId(*pIdChrom);  
    feature->SetXref().push_back(pXrefChrom);

    if (xContainsBlockFeature(fields)) {
        CRef<CFeat_id> pIdBlock(new CFeat_id);
        pIdBlock->SetLocal().SetId(baseId+2);
        CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
        pXrefBlock->SetId(*pIdBlock);  
        feature->SetXref().push_back(pXrefBlock);   
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsCds(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId+1);

    //CRef<CFeat_id> pIdChrom(new CFeat_id);
    //pIdChrom->SetLocal().SetId(baseId);
    //CRef<CSeqFeatXref> pXrefChrom(new CSeqFeatXref);
    //pXrefChrom->SetId(*pIdChrom);  
    //feature->SetXref().push_back(pXrefChrom);

    if (xContainsBlockFeature(fields)) {
        CRef<CFeat_id> pIdBlock(new CFeat_id);
        pIdBlock->SetLocal().SetId(baseId+2);
        CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
        pXrefBlock->SetId(*pIdBlock);  
        feature->SetXref().push_back(pXrefBlock);   
    }
    else {
        CRef<CFeat_id> pIdChrom(new CFeat_id);
        pIdChrom->SetLocal().SetId(baseId);
        CRef<CSeqFeatXref> pXrefChrom(new CSeqFeatXref);
        pXrefChrom->SetId(*pIdChrom);  
        feature->SetXref().push_back(pXrefChrom);   
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsBlock(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId+2);

    CRef<CFeat_id> pIdChrom(new CFeat_id);
    pIdChrom->SetLocal().SetId(baseId);
    CRef<CSeqFeatXref> pXrefChrom(new CSeqFeatXref);
    pXrefChrom->SetId(*pIdChrom);  
    feature->SetXref().push_back(pXrefChrom);

    if (xContainsThickFeature(fields)) {
        CRef<CFeat_id> pIdThick(new CFeat_id);
        pIdThick->SetLocal().SetId(baseId+1);
        CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
        pXrefBlock->SetId(*pIdThick);  
        feature->SetXref().push_back(pXrefBlock);   
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsRna(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId+2);

    CRef<CFeat_id> pIdChrom(new CFeat_id);
    pIdChrom->SetLocal().SetId(baseId);
    CRef<CSeqFeatXref> pXrefChrom(new CSeqFeatXref);
    pXrefChrom->SetId(*pIdChrom);  
    feature->SetXref().push_back(pXrefChrom);

    //if (xContainsThickFeature(fields)) {
    //    CRef<CFeat_id> pIdThick(new CFeat_id);
    //    pIdThick->SetLocal().SetId(baseId+1);
    //    CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
    //    pXrefBlock->SetId(*pIdThick);  
    //    feature->SetXref().push_back(pXrefBlock);   
    //}
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureTitle(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    if (fields.size() >= 4  &&  !fields[3].empty()  &&  fields[3] != ".") {
        feature->SetTitle(fields[0]);
    }
    else {
        feature->SetTitle(string("line_") + NStr::IntToString(m_uLineNumber));
    }
}


//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureScore(
    CRef<CUser_object> pDisplayData,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    string trackUseScore = m_pTrackDefaults->ValueOf("useScore");
    if (fields.size() < 5  || trackUseScore == "1") {
        //record does not carry score information
        return;
    }

    int int_score = NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow );
    double d_score = 0;

    if (int_score == 0 && fields[4].compare("0") != 0) {
        try {
            d_score = NStr::StringToDouble(fields[4]);
        }
        catch(std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Bad \"score\" value.") );
            pErr->Throw();
        }
    }

    if (d_score < 0 || int_score < 0) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"score\" value.") );
        pErr->Throw();
    }
    else if (d_score > 0) {
        pDisplayData->AddField("score", d_score);
    }
    else {
        pDisplayData->AddField("score", int_score);
    }
}


//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColor(
    CRef<CUser_object> pDisplayData,
    const vector<string>& fields,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    //1: if track line itemRgb is set, try that first:
    string trackItemRgb = m_pTrackDefaults->ValueOf("itemRgb");
    if (trackItemRgb == "On"  &&  fields.size() >= 9) {
        string featItemRgb = fields[8];
        if (featItemRgb != ".") {
            xSetFeatureColorFromItemRgb(pDisplayData, featItemRgb, pEC);
            return;
        }
    }

    //2: if track useScore is set, try that next:
    string trackUseScore = m_pTrackDefaults->ValueOf("useScore");
    if (trackUseScore == "1"  && fields.size() >= 5) {
        string featScore = fields[4];
        if (featScore != ".") {    
            xSetFeatureColorFromScore(pDisplayData, featScore);
            return; 
        }
    }

    //3: if track colorByStrand is set, try that next:
    string trackColorByStrand = m_pTrackDefaults->ValueOf("colorByStrand");
    if (!trackColorByStrand.empty()  && fields.size() >= 6) {
        ENa_strand strand = 
            (fields[5] == "-") ? eNa_strand_minus : eNa_strand_plus;
        xSetFeatureColorByStrand(pDisplayData, trackColorByStrand, strand, pEC);
        return;
    }
    //4: if none of the track color attributes are set, attempt feature itemRgb:
    if (fields.size() >= 9) {
        string featItemRgb = fields[8];
        if (featItemRgb != ".") {
            xSetFeatureColorFromItemRgb(pDisplayData, featItemRgb, pEC);
            return;
        }
    }
    
    //5: if still here, assign default color:
    xSetFeatureColorDefault(pDisplayData);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColorDefault(
    CRef<CUser_object> pDisplayData)
//  ----------------------------------------------------------------------------
{
    const string colorDefault("0 0 0");
    pDisplayData->AddField("color", colorDefault);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColorByStrand(
    CRef<CUser_object> pDisplayData,
    const string& trackColorByStrand,
    ENa_strand strand,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    try {
        string colorPlus, colorMinus;
        NStr::SplitInTwo(trackColorByStrand, " ", colorPlus, colorMinus);
        string useColor = (strand == eNa_strand_minus) ? colorMinus : colorPlus;
        xSetFeatureColorFromItemRgb(pDisplayData, useColor, pEC);
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid track line: Bad colorByStrand value.") );
        pErr->Throw();
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColorFromScore(
    CRef<CUser_object> pDisplayData,
    const string& featScore )
//  ----------------------------------------------------------------------------
{
    int score = 0;
    try {
        score = NStr::StringToInt(featScore);
    }
    catch (const std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad score value to be used for color.") );
        pErr->Throw();
    }
    if (score < 0  ||  1000 < score) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad score value to be used for color.") );
        pErr->Throw();
    }
    string greyValue  = NStr::IntToString(255 - (score/4));
    vector<string> srgb{ greyValue, greyValue, greyValue};
    string rgbValue = NStr::Join(srgb, " ");
    pDisplayData->AddField("color", rgbValue);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColorFromItemRgb(
    CRef<CUser_object> pDisplayData,
    const string& itemRgb,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
            eDiag_Warning,
            this->m_uLineNumber,
            "Bad color value - converted to BLACK.") );
    const string rgbDefault = "0 0 0";

    //optimization for common case:
    if (itemRgb == "0") {
        pDisplayData->AddField("color", rgbDefault);
        return;
    }

    vector<string> srgb;
    NStr::Split(itemRgb, ",", srgb);

    if (srgb.size() == 3) {
        auto valuesOk = true;
        for (auto i=0; i<3; ++i) {
            int test;
            try {
                test = NStr::StringToInt(srgb[i], NStr::fDS_ProhibitFractions);
            }
            catch(CException&) {
                valuesOk = false;
                break;
            }
            if ((test < 0)  ||  (256 <= test)) {
                valuesOk = false;
                break;
            }
        }
        if (!valuesOk) {
            ProcessWarning(*pErr, pEC);
            pDisplayData->AddField("color", rgbDefault);
            return;
        }
        auto outValue = srgb[0] + " " + srgb[1] + " " + srgb[2];
        pDisplayData->AddField("color", outValue);
        return;
    } 

    if (srgb.size() == 1) {
        auto assumeHex = false;
        string itemRgbCopy(itemRgb);
        if (NStr::StartsWith(itemRgbCopy, "0x")) {
            assumeHex = true;
            itemRgbCopy = itemRgb.substr(2);
        }
        else if (NStr::StartsWith(itemRgbCopy, "#")) {
            assumeHex = true;
            itemRgbCopy = itemRgbCopy.substr(1);
        }
        unsigned long colorValue;
        int radix = (assumeHex ? 16 : 10);
        try {
            colorValue = NStr::StringToULong(
                itemRgbCopy, NStr::fDS_ProhibitFractions, radix);
        }
        catch (CStringException&) {
            ProcessWarning(*pErr, pEC);
            pDisplayData->AddField("color", rgbDefault);
            return;
        }
        int blue = colorValue & 0xFF;
        colorValue >>= 8;
        int green = colorValue & 0xFF;
        colorValue >>= 8;
        int red = colorValue & 0xFF;
        auto outValue = NStr::IntToString(red) + " " + NStr::IntToString(green) + 
            " " + NStr::IntToString(blue);
        pDisplayData->AddField("color", outValue);
        return;
    }

    ProcessWarning(*pErr, pEC);
    pDisplayData->AddField("color", rgbDefault);
    return;
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureBedData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields,
    ILineErrorListener* pEc )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData& data = feature->SetData();
	if (fields.size() >= 4  &&  fields[3] != ".") {
		data.SetRegion() = fields[3];
	}
	else {
		data.SetRegion() = fields[0];
	}
    
    CRef<CUser_object> pDisplayData(new CUser_object());

    CSeq_feat::TExts& exts = feature->SetExts();
    pDisplayData->SetType().SetStr("DisplaySettings");
    exts.push_front(pDisplayData);

    xSetFeatureScore(pDisplayData, fields);
    xSetFeatureColor(pDisplayData, fields, pEc);
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    //
    //  Note:
    //  BED convention for specifying intervals is 0-based, first in, first out.
    //  ASN convention for specifying intervals is 0-based, first in, last in.
    //  Hence, conversion BED->ASN  leaves the first leaves the "from" coordinate
    //  unchanged, and decrements the "to" coordinate by one.
    //

    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(fields[1]);
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"SeqStart\" value." ) );
        pErr->Throw();
    }
    try {
        to = NStr::StringToInt(fields[2]) - 1;
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"SeqStop\" value.") );
        pErr->Throw();
    }
    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: \"SeqStop\" less than \"SeqStart\"." ) );
        pErr->Throw();
    }

    size_t strand_field = 5;
    if (fields.size() == 5  &&  (fields[4] == "-"  ||  fields[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < fields.size()) {
        string strand = fields[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Invalid data line: Invalid strand character." ) );
            pErr->Throw();
        }
        location->SetStrand(( fields[strand_field] == "+" ) ?
                           eNa_strand_plus : eNa_strand_minus );
    }
    
    CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0], m_iFlags, false);
    location->SetId(*id);
    feature->SetLocation(*location);
}

//  ----------------------------------------------------------------------------
bool 
CBedReader::ReadTrackData(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (m_CurBatchSize == m_MaxBatchSize) {
        m_CurBatchSize = 0;
        return xReadBedDataRaw(lr, rawdata, pMessageListener);
    }

    string line;
    while (xGetLine(lr, line)) {
        m_CurBatchSize = 0;
        if (line == "browser"  ||  NStr::StartsWith(line, "browser ")) {
            continue;
        }
        if (line == "track"  ||  NStr::StartsWith(line, "track ")) {
            continue;
        }
        //data line
        lr.UngetLine();
        return xReadBedDataRaw(lr, rawdata, pMessageListener);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xReadBedRecordRaw(
    const string& line,
    CRawBedRecord& record,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (line == "browser"  || NStr::StartsWith(line, "browser ")
            || NStr::StartsWith(line, "browser\t")) {
        return false;
    }
    if (line == "track"  || NStr::StartsWith(line, "track ")
            || NStr::StartsWith(line, "track\t")) {
        return false;
    }

    vector<string> columns;
	string linecopy = line;
	NStr::TruncateSpacesInPlace(linecopy);

    //  parse
    NStr::Split(linecopy, " \t", columns, NStr::fSplit_MergeDelimiters);
    try {
        xCleanColumnValues(columns);
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pMessageListener);
        return false;
    }

    if (columns.size() != mRealColumnCount) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Inconsistent column count." ) );
        ProcessError(*pErr, pMessageListener);
        return false;
    }

    //assign columns to record:
    CRef<CSeq_id> id = CReadUtil::AsSeqId(columns[0], m_iFlags, false);

    unsigned int start;
    try {
        start = NStr::StringToInt(columns[1]);
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStart\" (column 2) value." ) );
        ProcessError(*pErr, pMessageListener);
        return false;
    }

    unsigned int stop;
    try {
        stop = NStr::StringToInt(columns[2]);
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStop\" (column 3) value." ) );
        ProcessError(*pErr, pMessageListener);
        return false;
    }

    int score(-1);
    if (mValidColumnCount >= 5  &&  columns[4] != ".") {
        try {
            score = NStr::StringToInt(columns[4], 
			NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols);
        }
        catch(std::exception&) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad data line: Invalid \"Score\" (column 5) value." ) );
            ProcessError(*pErr, pMessageListener);
            return false;
        }
    }
    ENa_strand strand = eNa_strand_plus;
    if (mValidColumnCount >= 6) {
        if (columns[5] == "-") {
            strand = eNa_strand_minus;
        }
    }
    record.SetInterval(*id, start, stop, strand);
    if (score >= 0) {
        record.SetScore(score);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsThickFeature(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    if (fields.size() < 8  ||  mValidColumnCount < 8) {
        return false;
    }

    int start = -1, from = -1, to = -1;
    try {
        start = NStr::StringToInt(fields[1]);
        from = NStr::StringToInt(fields[6]);
        to = NStr::StringToInt(fields[7]);
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"Start/ThickStart/ThickStop\" values." ) );
        pErr->Throw();
    }
    if (start == from  &&  from == to) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsRnaFeature(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    if (fields.size() < 12  ||  mValidColumnCount < 12) {
        return false;
    }

    int start = -1, from = -1, to = -1;
    try {
        start = NStr::StringToInt(fields[1]);
        from = NStr::StringToInt(fields[6]);
        to = NStr::StringToInt(fields[7]);
    }
    catch (std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"Start/ThickStart/ThickStop\" values." ) );
        pErr->Throw();
    }
    if (start == from  &&  from == to) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsBlockFeature(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    return (fields.size() >= 12  &&  mValidColumnCount >= 12);
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsCdsFeature(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    return (fields.size() >= 8  &&  mValidColumnCount >= 8);
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xReadBedDataRaw(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();
    string line;
    while (xGetLine(lr, line)) {
        CRawBedRecord record;
        if (!xReadBedRecordRaw(line, record, pMessageListener)) {
            lr.UngetLine();
            break;
        }
        rawdata.AddRecord(record);
        ++m_CurBatchSize;
        if (m_CurBatchSize == m_MaxBatchSize) {
            return rawdata.HasData();
        }
    }

    return rawdata.HasData();
}

//  ----------------------------------------------------------------------------
void
CBedReader::xCleanColumnValues(
   vector<string>& columns)
//  ----------------------------------------------------------------------------
{
    string fixup;

    if (NStr::EqualNocase(columns[0], "chr")  &&  columns.size() > 1) {
        columns[1] = columns[0] + columns[1];
        columns.erase(columns.begin());
    }
    if (columns.size() < 3) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Insufficient column count." ) );
        pErr->Throw();
    }

    try {
        NStr::Replace(columns[1], ",", "", fixup);
        columns[1] = fixup;
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStart\" (column 2) value." ) );
        pErr->Throw();
    }

    try {
        NStr::Replace(columns[2], ",", "", fixup);
        columns[2] = fixup;
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStop\" (column 3) value." ) );
        pErr->Throw();
    }
}

//  ----------------------------------------------------------------------------
void 
CBedReader::xAssignBedColumnCount(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if(mValidColumnCount < 3) {
        return;
    }
    CRef<CUser_object> columnCountUser(new CUser_object());
    columnCountUser->SetType().SetStr("NCBI_BED_COLUMN_COUNT");
    columnCountUser->AddField("NCBI_BED_COLUMN_COUNT", int (mValidColumnCount));
    
    CRef<CAnnotdesc> userDesc(new CAnnotdesc());
    userDesc->SetUser().Assign(*columnCountUser);
    annot.SetDesc().Set().push_back(userDesc);
}                   

END_objects_SCOPE
END_NCBI_SCOPE
