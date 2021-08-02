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

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/bed_reader.hpp>
#include "bed_autosql.hpp"
#include "reader_message_handler.hpp"
#include "bed_column_data.hpp"

#include <algorithm>
#include <deque>

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
        string line;
        while (numLines  &&  !mLineReader.AtEOF()) {
            line = *++mLineReader;
            CLinePreBuffer::StripSpaceCharsInPlace(line);
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
                CLinePreBuffer::StripSpaceCharsInPlace(temp);
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

    int LineNumber() const
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

    void
    AssignReader(
        ILineReader& lineReader) {
        if (&mLineReader != &lineReader) {
            mLineReader = lineReader;
            mBuffer.clear();
            mLineNumber = 0;
        }
    };

    static void
    StripSpaceCharsInPlace(
        string& str)
    {
        if (str.empty()) {
            return;
        }
        auto newFirst = 0;
        while (str[newFirst] == ' ') {
            ++newFirst;
        }
        auto newLast = str.length() - 1;
        while (str[newLast] == ' ') {
            --newLast;
        }
        str = str.substr(newFirst, newLast - newFirst + 1);
    };

protected:
    ILineReader& mLineReader;
    deque<string> mBuffer;
    int mLineNumber;
};


//  ----------------------------------------------------------------------------
void
CRawBedRecord::SetInterval(
//  ----------------------------------------------------------------------------
    CSeq_id& id,
    unsigned int start,
    unsigned int stop,
    ENa_strand strand)
{
    m_pInterval.Reset(new CSeq_interval());
    m_pInterval->SetId(id);
    m_pInterval->SetFrom(start);
    m_pInterval->SetTo(stop-1);
    m_pInterval->SetStrand(strand);
};

//  ----------------------------------------------------------------------------
void
CRawBedRecord::SetScore(
    unsigned int score)
//  ----------------------------------------------------------------------------
{
    m_score = score;
};

//  ----------------------------------------------------------------------------
void
CRawBedRecord::Dump(
    CNcbiOstream& ostr) const
//  ----------------------------------------------------------------------------
{
    ostr << "  [CRawBedRecord" << endl;
    ostr << "id=\"" << m_pInterval->GetId().AsFastaString() << "\" ";
    ostr << "start=" << m_pInterval->GetFrom() << " ";
    ostr << "stop=" << m_pInterval->GetTo() << " ";
    ostr << "strand=" <<
        (m_pInterval->GetStrand() == eNa_strand_minus ? "-" : "+") << " ";
    if (m_score >= 0) {
        ostr << "score=" << m_score << " ";
    }
    ostr << "]" << endl;
};

//  ----------------------------------------------------------------------------
void
CRawBedTrack::Dump(
    CNcbiOstream& ostr) const
//  ----------------------------------------------------------------------------
{
    ostr << "[CRawBedTrack" << endl;
    for (vector<CRawBedRecord>::const_iterator it = m_Records.begin();
            it != m_Records.end(); ++it) {
        it->Dump(ostr);
    }
    ostr << "]" << std::endl;
}

//  ----------------------------------------------------------------------------
CBedReader::CBedReader(
    int flags,
    const string& annotName,
    const string& annotTitle,
    CReaderListener* pRL ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags, annotName, annotTitle, CReadUtil::AsSeqId, pRL),
    m_currentId(""),
    mColumnSeparator(""),
    mColumnSplitFlags(0),
    mRealColumnCount(0),
    mValidColumnCount(0),
    mAssumeErrorsAreRecordLevel(true),
    m_CurrentFeatureCount(0),
    m_usescore(false),
    m_CurBatchSize(0),
    m_MaxBatchSize(10000),
    mLinePreBuffer(nullptr),
    mpAutoSql(new CBedAutoSql(flags))
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
    ILineReader& lineReader,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    m_CurrentFeatureCount = 0;
    return CReaderBase::ReadSeqAnnot(lineReader, pEC);
}

//  ----------------------------------------------------------------------------
bool
CBedReader::SetAutoSql(
    const string& fileName)
//  ----------------------------------------------------------------------------
{
    CNcbiIfstream istr;
    try {
        auto origExceptions = istr.exceptions();
        istr.exceptions(std::istream::failbit);
        istr.open(fileName);
        istr.exceptions(origExceptions);
    }
    catch (CException& e) {
        cerr << e.GetMsg() << endl;
        return false;
    }
    m_iFlags |= CBedReader::fAutoSql;
    return SetAutoSql(istr);
}

//  ----------------------------------------------------------------------------
bool
CBedReader::SetAutoSql(
    CNcbiIstream& istr)
//  ----------------------------------------------------------------------------
{
   return  mpAutoSql->Load(istr, *m_pMessageHandler);
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CBedReader::xCreateSeqAnnot()
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pAnnot(new CSeq_annot);
    if (!m_AnnotName.empty()) {
        pAnnot->SetNameDesc(m_AnnotName);
    }
    if (!m_AnnotTitle.empty()) {
        pAnnot->SetTitleDesc(m_AnnotTitle);
    }
    CRef<CAnnot_descr> pDescr(new CAnnot_descr);
    pAnnot->SetDesc(*pDescr);
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CBedReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    if (!mLinePreBuffer) {
        mLinePreBuffer.reset(new CLinePreBuffer(lr));
    }
    if (mRealColumnCount == 0) {
        xDetermineLikelyColumnCount(*mLinePreBuffer, nullptr);
    }

    readerData.clear();
    string line;
    if (!mLinePreBuffer->GetLine(line)) {
        return;
    }
    bool isBrowserLine = NStr::StartsWith(line, "browser ");
    bool isTrackLine = NStr::StartsWith(line, "track ");
    if (xIsTrackLine(line)  && m_uDataCount != 0) {
        mLinePreBuffer->UngetLine(line);
        return;
    }
    m_uLineNumber = mLinePreBuffer->LineNumber();
    readerData.push_back(TReaderLine{m_uLineNumber, line});
    if (!isBrowserLine  &&  !isTrackLine) {
        ++m_uDataCount;
    }
}

//  ----------------------------------------------------------------------------
void
CBedReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (const auto& lineData: readerData) {
        string line = lineData.mData;
        if (xParseTrackLine(line)) {
            return;
        }
        if (xParseBrowserLine(line, annot)) {
            return;
        }
        xParseFeature(lineData, annot, nullptr);
        ++m_CurrentFeatureCount;
    }
}

//  ----------------------------------------------------------------------------
bool CBedReader::xDetermineLikelyColumnCount(
    CLinePreBuffer& preBuffer,
    ILineErrorListener* pEc)
//  ----------------------------------------------------------------------------
{
    if (this->m_iFlags & fAutoSql) {
        mValidColumnCount = mRealColumnCount = mpAutoSql->ColumnCount();;
        return true;
    }

    using LineIt = CLinePreBuffer::LinePreIt;
    int bufferLineNumber = 0;
    CReaderMessage fatalColumns(
        eDiag_Fatal,
        0,
        "Bad data line: Inconsistent column count.");

    CReaderMessage fatalChroms(
        eDiag_Fatal,
        0,
        "Bad data line: Invalid chrom boundaries.");

    const size_t MIN_SAMPLE_SIZE = 50;
    preBuffer.FillBuffer(MIN_SAMPLE_SIZE);

    mRealColumnCount = mValidColumnCount = 0;
    vector<string>::size_type realColumnCount = 0;
    vector<string>::size_type validColumnCount = 0;
    for (LineIt lineIt = preBuffer.begin(); lineIt != preBuffer.end(); ++lineIt) {
        bufferLineNumber++;
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

        CBedColumnData columnData(SReaderLine(bufferLineNumber, line));
        if (realColumnCount == 0 ) {
            realColumnCount = columnData.ColumnCount();
        }
        if (realColumnCount !=  columnData.ColumnCount()) {
            fatalColumns.SetLineNumber(bufferLineNumber);
            throw(fatalColumns);
        }

        if (validColumnCount == 0) {
            validColumnCount = realColumnCount;
            if (validColumnCount > 12) {
                validColumnCount = 12;
            }
        }
        unsigned long chromStart = 0, chromEnd = 0;
        try {
            chromStart = NStr::StringToULong(columnData[1]);
            chromEnd = NStr::StringToULong(columnData[2]);
        }
        catch (CException&) {
            fatalChroms.SetLineNumber(bufferLineNumber);
            throw(fatalChroms);
        }
        if (validColumnCount >= 7) {
            try {
                auto thickStart = NStr::StringToULong(columnData[6]);
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
                auto thickEnd = NStr::StringToULong(columnData[7]);
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
                    columnData[9], NStr::fDS_ProhibitFractions);
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
            auto col10 = columnData[10];
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
            auto col11 = columnData[11];
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
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    xAddConversionInfo(annot, nullptr);
    xAssignTrackData(annot);
    xAssignBedColumnCount(annot);
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xParseTrackLine(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    CReaderMessage warning(
        eDiag_Warning,
        m_uLineNumber,
        "Bad track line: Expected \"track key1=value1 key2=value2 ...\". Ignored.");

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
    if (!CReaderBase::xParseTrackLine(strLine)) {
        m_pMessageHandler->Report(warning);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xParseFeature(
    const SReaderLine& lineData,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CBedColumnData columnData(lineData);
    if (columnData.ColumnCount()!= mRealColumnCount) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad data line: Inconsistent column count.");
        throw error;
    }

    if (m_iFlags & CBedReader::fThreeFeatFormat) {
        return xParseFeatureThreeFeatFormat(columnData, annot, pEC);
    }
    else if (m_iFlags & CBedReader::fDirectedFeatureModel) {
        return xParseFeatureGeneModelFormat(columnData, annot, pEC);
    }
    else if (m_iFlags & CBedReader::fAutoSql) {
        return xParseFeatureAutoSql(columnData, annot, pEC);
    }
    else {
        return xParseFeatureUserFormat(columnData, annot, pEC);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureThreeFeatFormat(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
     unsigned int baseId = 3*m_CurrentFeatureCount;

    if (!xAppendFeatureChrom(columnData, annot, baseId, pEC)) {
        return false;
    }
    if (xContainsThickFeature(columnData)  &&
            !xAppendFeatureThick(columnData, annot, baseId, pEC)) {
        return false;
    }
    if (xContainsBlockFeature(columnData)  &&
            !xAppendFeatureBlock(columnData, annot, baseId, pEC)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureGeneModelFormat(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    unsigned int baseId = 3*m_CurrentFeatureCount;

    CRef<CSeq_feat> pGene = xAppendFeatureGene(columnData, annot, baseId, pEC);
    if (!pGene) {
        return false;
    }

    CRef<CSeq_feat> pRna;
    if (xContainsRnaFeature(columnData)) {//blocks
        pRna = xAppendFeatureRna(columnData, annot, baseId, pEC);
        if (!pRna) {
            return false;
        }
    }

    CRef<CSeq_feat> pCds;
    if (xContainsCdsFeature(columnData)) {//thick
        pCds = xAppendFeatureCds(columnData, annot, baseId, pEC);
        if (!pCds) {
            return false;
        }
    }

    if (pRna  &&  pCds) {
        CRef<CSeq_loc> pRnaLoc(new CSeq_loc);
        CRef<CSeq_loc> pClippedLoc = pRna->GetLocation().Intersect(pCds->GetLocation(), 0, 0);
        pCds->SetLocation(*pClippedLoc);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureChrom(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationChrom(feature, columnData);
    xSetFeatureIdsChrom(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    m_currentId = columnData[0];
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureGene(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationGene(feature, columnData);
    xSetFeatureIdsGene(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    m_currentId = columnData[0];
    return feature;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureThick(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationThick(feature, columnData);
    xSetFeatureIdsThick(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureCds(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationCds(feature, columnData);
    xSetFeatureIdsCds(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    return feature;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xAppendFeatureBlock(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationBlock(feature, columnData);
    xSetFeatureIdsBlock(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    return true;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CBedReader::xAppendFeatureRna(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    unsigned int baseId,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);

    xSetFeatureLocationRna(feature, columnData);
    xSetFeatureIdsRna(feature, columnData, baseId);
    xSetFeatureBedData(feature, columnData, pEC);

    ftable.push_back(feature);
    return feature;
}


//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureUserFormat(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    //  assign
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset( new CSeq_feat );

    xSetFeatureTitle(feature, columnData);
    xSetFeatureLocation(feature, columnData);
    xSetFeatureDisplayData(feature, columnData);

    ftable.push_back( feature );
    m_currentId = columnData[0];
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureAutoSql(
    const CBedColumnData& columnData,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pFeat(new CSeq_feat);;
    if (!mpAutoSql->ReadSeqFeat(columnData, *pFeat, *m_pMessageHandler)) {
        return false;
    }
    CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
    ftable.push_back(pFeat);
    m_currentId = columnData[0];
    return true;
}


//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    if (mValidColumnCount >= 4) {
        display_data->AddField( "name", columnData[3] );
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
                NStr::StringToInt(columnData[4],
                    NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols) );
            feature->AddOrReplaceQualifier("score", columnData[4]);
        }
        else {
            display_data->AddField(
                "greylevel",
                NStr::StringToInt(columnData[4],
                    NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols) );
        }
    }
    if (mValidColumnCount >= 7) {
        display_data->AddField(
            "thickStart",
            NStr::StringToInt(columnData[6], NStr::fDS_ProhibitFractions) );
    }
    if (mValidColumnCount >= 8) {
        display_data->AddField(
            "thickEnd",
            NStr::StringToInt(columnData[7], NStr::fDS_ProhibitFractions) - 1 );
    }
    if (mValidColumnCount >= 9) {
        display_data->AddField(
            "itemRGB",
            columnData[8]);
    }
    if (mValidColumnCount >= 10) {
        display_data->AddField(
            "blockCount",
            NStr::StringToInt(columnData[9], NStr::fDS_ProhibitFractions) );
    }
    if (mValidColumnCount >= 11) {
        display_data->AddField( "blockSizes", columnData[10] );
    }
    if (mValidColumnCount >= 12) {
        display_data->AddField( "blockStarts", columnData[11] );
    }
    feature->SetData().SetUser( *display_data );
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationChrom(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    xSetFeatureLocation(feature, columnData);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "chrom");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationGene(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    xSetFeatureLocation(feature, columnData);

    CRef<CUser_object> pBed(new CUser_object());
    pBed->SetType().SetStr("BED");
    pBed->AddField("location", "chrom");
    CSeq_feat::TExts& exts = feature->SetExts();
    exts.push_back(pBed);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationThick(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(columnData[6]);
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"ThickStart\" value.");
        throw error;
    }
    try {
        to = NStr::StringToInt(columnData[7]) - 1;
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"ThickStop\" value.");
        throw error;
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
        location->SetStrand(xGetStrand(columnData));
    }
    CRef<CSeq_id> id = CReadUtil::AsSeqId(columnData[0], m_iFlags, false);
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
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(columnData[6]);
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"ThickStart\" value.");
        throw error;
    }
    try {
        to = NStr::StringToInt(columnData[7]) - 1;
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"ThickStop\" value.");
        throw error;
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
        location->SetStrand(xGetStrand(columnData));
    }
    CRef<CSeq_id> id = CReadUtil::AsSeqId(columnData[0], m_iFlags, false);
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
    const CBedColumnData& columnData) const
//  ----------------------------------------------------------------------------
{
    size_t strand_field = 5;
    if (columnData.ColumnCount() == 5  &&
            (columnData[4] == "-"  ||  columnData[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < columnData.ColumnCount()) {
        string strand = columnData[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Invalid data line: Invalid strand character.");
            throw error;
        }
    }
    return (columnData[strand_field] == "-" ? eNa_strand_minus : eNa_strand_plus);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocationBlock(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    //already established: there are sufficient columns to do this
    size_t blockCount = NStr::StringToUInt(columnData[9]);
    vector<size_t> blockSizes;
    vector<size_t> blockStarts;
    {{
        blockSizes.reserve(blockCount);
        vector<string> vals;
        NStr::Split(columnData[10], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Bad value count in \"blockSizes\".");
            throw error;
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockSizes.push_back(NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Malformed \"blockSizes\" column.");
            throw error;
        }
    }}
    {{
        blockStarts.reserve(blockCount);
        vector<string> vals;
        size_t baseStart = NStr::StringToUInt(columnData[1]);
        NStr::Split(columnData[11], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Bad value count in \"blockStarts\".");
            throw error;
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockStarts.push_back(baseStart + NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Malformed \"blockStarts\" column.");
            throw error;
        }
    }}

    CPacked_seqint& location = feature->SetLocation().SetPacked_int();
    ENa_strand strand = xGetStrand(columnData);
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(columnData[0], m_iFlags, false);

    bool negative = columnData[5] == "-";

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
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    //already established: there are sufficient columns to do this
    size_t blockCount = NStr::StringToUInt(columnData[9]);
    vector<size_t> blockSizes;
    vector<size_t> blockStarts;
    {{
        blockSizes.reserve(blockCount);
        vector<string> vals;
        NStr::Split(columnData[10], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Bad value count in \"blockSizes\".");
            throw error;
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockSizes.push_back(NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Malformed \"blockSizes\" column.");
            throw error;
        }
    }}
    {{
        blockStarts.reserve(blockCount);
        vector<string> vals;
        size_t baseStart = NStr::StringToUInt(columnData[1]);
        NStr::Split(columnData[11], ",", vals);
        if (vals.back() == "") {
            vals.erase(vals.end()-1);
        }
        if (vals.size() != blockCount) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Bad value count in \"blockStarts\".");
            throw error;
        }
        try {
            for (size_t i=0; i < blockCount; ++i) {
                blockStarts.push_back(baseStart + NStr::StringToUInt(vals[i]));
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Malformed \"blockStarts\" column.");
            throw error;
        }
    }}

    CPacked_seqint& location = feature->SetLocation().SetPacked_int();
    ENa_strand strand = xGetStrand(columnData);
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(columnData[0], m_iFlags, false);

    bool negative = columnData[5] == "-";

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
    const CBedColumnData& columnData,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId);

    if (xContainsThickFeature(columnData)) {
        CRef<CFeat_id> pIdThick(new CFeat_id);
        pIdThick->SetLocal().SetId(baseId+1);
        CRef<CSeqFeatXref> pXrefThick(new CSeqFeatXref);
        pXrefThick->SetId(*pIdThick);
        feature->SetXref().push_back(pXrefThick);
    }

    if (xContainsBlockFeature(columnData)) {
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
    const CBedColumnData& columnData,
    unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureIdsThick(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData,
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

    if (xContainsBlockFeature(columnData)) {
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
    const CBedColumnData& columnData,
   unsigned int baseId)
//  ----------------------------------------------------------------------------
{
    baseId++; //0-based to 1-based
    feature->SetId().SetLocal().SetId(baseId+1);

    if (xContainsBlockFeature(columnData)) {
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
    const CBedColumnData& columnData,
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

    if (xContainsThickFeature(columnData)) {
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
    const CBedColumnData& columnData,
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
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureTitle(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    if (columnData.ColumnCount() >= 4  &&
            !columnData[3].empty()  &&  columnData[3] != ".") {
        feature->SetTitle(columnData[0]);
    }
    else {
        feature->SetTitle(string("line_") + NStr::IntToString(m_uLineNumber));
    }
}


//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureScore(
    CRef<CUser_object> pDisplayData,
    const CBedColumnData& columnData)
//  ----------------------------------------------------------------------------
{
    CReaderMessage error(
        eDiag_Error,
        columnData.LineNo(),
        "Invalid data line: Bad \"score\" value.");

    string trackUseScore = m_pTrackDefaults->ValueOf("useScore");
    if (columnData.ColumnCount() < 5  || trackUseScore == "1") {
        //record does not carry score information
        return;
    }

    int int_score = NStr::StringToInt(columnData[4], NStr::fConvErr_NoThrow );
    double d_score = 0;

    if (int_score == 0 && columnData[4].compare("0") != 0) {
        try {
            d_score = NStr::StringToDouble(columnData[4]);
        }
        catch(std::exception&) {
            throw error;
        }
    }

    if (d_score < 0 || int_score < 0) {
            throw error;
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
    const CBedColumnData& columnData,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    //1: if track line itemRgb is set, try that first:
    string trackItemRgb = m_pTrackDefaults->ValueOf("itemRgb");
    if (trackItemRgb == "On"  &&  columnData.ColumnCount() >= 9) {
        string featItemRgb = columnData[8];
        if (featItemRgb != ".") {
            xSetFeatureColorFromItemRgb(pDisplayData, featItemRgb, pEC);
            return;
        }
    }

    //2: if track useScore is set, try that next:
    string trackUseScore = m_pTrackDefaults->ValueOf("useScore");
    if (trackUseScore == "1"  && columnData.ColumnCount() >= 5) {
        string featScore = columnData[4];
        if (featScore != ".") {
            xSetFeatureColorFromScore(pDisplayData, featScore);
            return;
        }
    }

    //3: if track colorByStrand is set, try that next:
    string trackColorByStrand = m_pTrackDefaults->ValueOf("colorByStrand");
    if (!trackColorByStrand.empty()  && columnData.ColumnCount() >= 6) {
        ENa_strand strand =
            (columnData[5] == "-") ? eNa_strand_minus : eNa_strand_plus;
        xSetFeatureColorByStrand(pDisplayData, trackColorByStrand, strand, pEC);
        return;
    }
    //4: if none of the track color attributes are set, attempt feature itemRgb:
    if (columnData.ColumnCount() >= 9) {
        string featItemRgb = columnData[8];
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
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid track line: Bad colorByStrand value.");
        throw error;
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureColorFromScore(
    CRef<CUser_object> pDisplayData,
    const string& featScore )
//  ----------------------------------------------------------------------------
{
    CReaderMessage error(
        eDiag_Error,
        m_uLineNumber,
        "Invalid data line: Bad score value to be used for color.");

    int score = 0;
    try {
        score = static_cast<int>(NStr::StringToDouble(featScore));
    }
    catch (const std::exception&) {
        throw error;
    }
    if (score < 0  ||  1000 < score) {
        throw error;
    }
    string greyValue  = NStr::DoubleToString(255 - (score/4));
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
    CReaderMessage warning(
        eDiag_Warning,
        m_uLineNumber,
        "Bad color value - converted to BLACK.");
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
            m_pMessageHandler->Report(warning);
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
            m_pMessageHandler->Report(warning);
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

    m_pMessageHandler->Report(warning);
    pDisplayData->AddField("color", rgbDefault);
    return;
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureBedData(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData,
    ILineErrorListener* pEc )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData& data = feature->SetData();
    if (columnData.ColumnCount() >= 4  &&  columnData[3] != ".") {
        data.SetRegion() = columnData[3];
    }
    else {
        data.SetRegion() = columnData[0];
    }

    CRef<CUser_object> pDisplayData(new CUser_object());

    CSeq_feat::TExts& exts = feature->SetExts();
    pDisplayData->SetType().SetStr("DisplaySettings");
    exts.push_front(pDisplayData);

    xSetFeatureScore(pDisplayData, columnData);
    xSetFeatureColor(pDisplayData, columnData, pEc);
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const CBedColumnData& columnData )
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
        from = NStr::StringToInt(columnData[1]);
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "Invalid data line: Bad \"SeqStart\" value.");
        throw error;
    }
    try {
        to = NStr::StringToInt(columnData[2]) - 1;
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "Invalid data line: Bad \"SeqStop\" value.");
        throw error;
    }
    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "Invalid data line: \"SeqStop\" less than \"SeqStart\".");
        throw error;
    }

    size_t strand_field = 5;
    if (columnData.ColumnCount() == 5  &&
            (columnData[4] == "-"  ||  columnData[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < columnData.ColumnCount()) {
        string strand = columnData[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            CReaderMessage error(
                eDiag_Error,
                columnData.LineNo(),
                "Invalid data line: Invalid strand character.");
            throw error;
        }
        location->SetStrand(( columnData[strand_field] == "+" ) ?
                           eNa_strand_plus : eNa_strand_minus );
    }

    CRef<CSeq_id> id = CReadUtil::AsSeqId(columnData[0], m_iFlags, false);
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
    xCleanColumnValues(columns);
    if (mRealColumnCount == 0) {
        mRealColumnCount = columns.size();
    }
    if (columns.size() != mRealColumnCount) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Inconsistent column count.");
        m_pMessageHandler->Report(error);
        return false;
    }

    //assign columns to record:
    CRef<CSeq_id> id = CReadUtil::AsSeqId(columns[0], m_iFlags, false);

    unsigned int start;
    try {
        start = NStr::StringToInt(columns[1]);
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Invalid \"SeqStart\" (column 2) value.");
        m_pMessageHandler->Report(error);
        return false;
    }

    unsigned int stop;
    try {
        stop = NStr::StringToInt(columns[2]);
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Invalid \"SeqStop\" (column 3) value.");
        m_pMessageHandler->Report(error);
        return false;
    }

    int score(-1);
    if (mValidColumnCount >= 5  &&  columns[4] != ".") {
        try {
            score = NStr::StringToInt(columns[4],
                NStr::fConvErr_NoThrow|NStr::fAllowTrailingSymbols);
        }
        catch(std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Invalid data line: Invalid \"Score\" (column 5) value.");
            m_pMessageHandler->Report(error);
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
    const CBedColumnData& columnData) const
//  ----------------------------------------------------------------------------
{
    if (columnData.ColumnCount() < 8  ||  mValidColumnCount < 8) {
        return false;
    }

    int start = -1, from = -1, to = -1;
    try {
        start = NStr::StringToInt(columnData[1]);
        from = NStr::StringToInt(columnData[6]);
        to = NStr::StringToInt(columnData[7]);
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "Invalid data line: Bad \"Start/ThickStart/ThickStop\" values.");
        throw error;
    }
    if (start == from  &&  from == to) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsRnaFeature(
    const CBedColumnData& columnData) const
//  ----------------------------------------------------------------------------
{
    if (columnData.ColumnCount() < 12  ||  mValidColumnCount < 12) {
        return false;
    }

    int start = -1, from = -1, to = -1;
    try {
        start = NStr::StringToInt(columnData[1]);
        from = NStr::StringToInt(columnData[6]);
        to = NStr::StringToInt(columnData[7]);
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "Invalid data line: Bad \"Start/ThickStart/ThickStop\" values.");
        throw error;
    }
    if (start == from  &&  from == to) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsBlockFeature(
    const CBedColumnData& columnData) const
//  ----------------------------------------------------------------------------
{
    return (columnData.ColumnCount() >= 12  &&  mValidColumnCount >= 12);
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsCdsFeature(
    const CBedColumnData& columnData) const
//  ----------------------------------------------------------------------------
{
    return (columnData.ColumnCount() >= 8  &&  mValidColumnCount >= 8);
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
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Insufficient column count.");
        throw error;
    }

    try {
        NStr::Replace(columns[1], ",", "", fixup);
        columns[1] = fixup;
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Invalid \"SeqStart\" (column 2) value.");
        throw error;
    }

    try {
        NStr::Replace(columns[2], ",", "", fixup);
        columns[2] = fixup;
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Invalid \"SeqStop\" (column 3) value.");
        throw error;
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
