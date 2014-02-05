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
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CBedReader::CBedReader(
    unsigned int flags ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags | CReaderBase::fAllIdsAsLocal),
    m_currentId(""),
    m_columncount(0),
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
    ILineReader& lr,
    IMessageListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    const size_t MAX_RECORDS = 100000;

    CRef<CSeq_annot> annot;
    CRef<CAnnot_descr> desc;

    annot.Reset(new CSeq_annot);
    desc.Reset(new CAnnot_descr);
    annot->SetDesc(*desc);
    CSeq_annot::C_Data::TFtable& tbl = annot->SetData().SetFtable();

    int featureCount = 0;

    while (!lr.AtEOF()) {

        ++m_uLineNumber;

        CTempString line = *++lr;

        if (NStr::TruncateSpaces_Unsafe(line).empty()) {
            continue;
        }
        if (xParseComment(line, annot)) {
            continue;
        }
        if (x_ParseBrowserLine(line, annot, pEC)) {
            continue;
        }
        if (NStr::StartsWith(line, "track ")) {
            if (featureCount > 0) {
                --m_uLineNumber;
                lr.UngetLine();
                break;
            }
            else {
                if (xParseTrackLine(line, annot, pEC)) {
                    continue;
                }
            }
        }

	    CTempString record_copy = NStr::TruncateSpaces_Unsafe(line);

        //  parse
        vector<string> fields;
        NStr::Tokenize( record_copy, " \t", fields, NStr::eMergeDelims );
        try {
            xCleanColumnValues(fields);
        }
        catch(CObjReaderLineException& err) {
            ProcessError(err, pEC);
            continue;
        }
        if (fields[0] != m_currentId) {
            //record id has changed
            if (featureCount > 0) {
                --m_uLineNumber;
                lr.UngetLine();
                break;
            }
        }
        if (xParseFeature(fields, annot, featureCount, pEC)) {
            ++featureCount;
            continue;
        }
        if (tbl.size() >= MAX_RECORDS) {
            break;
        }
    }
    //  Only return a valid object if there was at least one feature
    if (0 == featureCount) {
        return CRef<CSeq_annot>();
    }
    x_AddConversionInfo(annot, pEC);
    x_AssignTrackData( annot );

    if(m_columncount >= 3) {
        CRef<CUser_object> columnCountUser( new CUser_object() );
        columnCountUser->SetType().SetStr( "NCBI_BED_COLUMN_COUNT" );
        columnCountUser->AddField("NCBI_BED_COLUMN_COUNT", int ( m_columncount ) );
    
        CRef<CAnnotdesc> userDesc( new CAnnotdesc() );
        userDesc->SetUser().Assign( *columnCountUser );
        annot->SetDesc().Set().push_back( userDesc );
    }
    return annot;
}

//  --------------------------------------------------------------------------- 
void
CBedReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IMessageListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}
 
//  ---------------------------------------------------------------------------                       
void
CBedReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pMessageListener);
    while (annot) {
        annots.push_back(annot);
        annot = ReadSeqAnnot(lr, pMessageListener);
    }
}
                        
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CBedReader::ReadObject(
    ILineReader& lr,
    IMessageListener* pMessageListener ) 
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
    CRef< CSeq_annot >& current,
    IMessageListener* pEC)
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
    if ( !m_currentId.empty() ) {
        m_columncount = 0;
    }
    m_currentId.clear();
    if (!CReaderBase::x_ParseTrackLine(strLine, current, pEC)) {
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
bool CBedReader::xParseFeature(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int featureCount,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    static int count = 0;
    count++;

    if (fields.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = fields.size();
        }
        else {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad data line: Inconsistent column count." ) );
            ProcessError(*pErr, pEC );
            return false;
        }
    }

    if (m_iFlags & CBedReader::fThreeFeatFormat) {
        return xParseFeatureThreeFeatFormat(fields, annot, 3*featureCount, pEC);
    }
    else {
        return xParseFeatureUserFormat(fields, annot, pEC);
    }
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeatureThreeFeatFormat(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
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
bool CBedReader::xAppendFeatureChrom(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        xSetFeatureTitle(feature, fields);
        xSetFeatureLocationChrom(feature, fields);
        xSetFeatureIdsChrom(feature, fields, baseId);
        xSetFeatureBedData(feature, fields);
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
bool CBedReader::xAppendFeatureThick(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        xSetFeatureTitle(feature, fields);
        xSetFeatureLocationThick(feature, fields);
        xSetFeatureIdsThick(feature, fields, baseId);
        xSetFeatureBedData(feature, fields);
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
bool CBedReader::xAppendFeatureBlock(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    unsigned int baseId,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    try {
        xSetFeatureTitle(feature, fields);
        xSetFeatureLocationBlock(feature, fields);
        xSetFeatureIdsBlock(feature, fields, baseId);
        xSetFeatureBedData(feature, fields);
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
bool CBedReader::xParseFeatureUserFormat(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    IMessageListener* pEC)
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
bool CBedReader::xParseComment(
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
void CBedReader::x_SetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    if ( m_columncount >= 4 ) {
        display_data->AddField( "name", fields[3] );
    }
    else {
        display_data->AddField( "name", string("") );
        feature->SetData().SetUser( *display_data );
        return;
    }
    if ( m_columncount >= 5 ) {
        if ( !m_usescore ) {
            display_data->AddField( 
                "score",
                NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow) );
        }
        else {
            display_data->AddField( 
                "greylevel",
               NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow) );
        }
    }
    if ( m_columncount >= 7 ) {
        display_data->AddField( 
            "thickStart",
            NStr::StringToInt(fields[6], NStr::fConvErr_NoThrow) );
    }
    if ( m_columncount >= 8 ) {
        display_data->AddField( 
            "thickEnd",
            NStr::StringToInt(fields[7], NStr::fConvErr_NoThrow) - 1 );
    }
    if ( m_columncount >= 9 ) {
        display_data->AddField( 
            "itemRGB",
            fields[8]);
    }
    if ( m_columncount >= 10 ) {
        display_data->AddField( 
            "blockCount",
            NStr::StringToInt(fields[9], NStr::fConvErr_NoThrow) );
    }
    if ( m_columncount >= 11 ) {
        display_data->AddField( "blockSizes", fields[10] );
    }
    if ( m_columncount >= 12 ) {
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
        to = NStr::StringToInt(fields[7])-1;
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
        NStr::Tokenize(fields[10], ",", vals);
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
        NStr::Tokenize(fields[11], ",", vals);
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
    for (size_t i=0; i < blockCount; ++i) {
        CRef<CSeq_interval> pInterval(new CSeq_interval);
        pInterval->SetId(*pId);
        pInterval->SetFrom(blockStarts[i]);
        pInterval->SetTo(blockStarts[i] + blockSizes[i]);
        pInterval->SetStrand(strand);
        location.Set().push_back(pInterval);
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

    CRef<CFeat_id> pIdBlock(new CFeat_id);
    pIdBlock->SetLocal().SetId(baseId+1);
    CRef<CSeqFeatXref> pXrefBlock(new CSeqFeatXref);
    pXrefBlock->SetId(*pIdBlock);  
    feature->SetXref().push_back(pXrefBlock);   
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureTitle(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    if (fields.size() >= 4  &&  !fields[3].empty()  &&  fields[3] != ".") {
        feature->SetTitle(fields[3]);
    }
    else {
        feature->SetTitle(string("line_") + NStr::IntToString(m_uLineNumber));
    }
}

//  ----------------------------------------------------------------------------
void CBedReader::xSetFeatureBedData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData& data = feature->SetData();
    data.SetRegion() = fields[0];
    
    CRef<CUser_object> pDisplayData(new CUser_object());
    if (fields.size() < 5  ||  fields[4] == ".") {
        return;
    }

    CSeq_feat::TExts& exts = feature->SetExts();
    pDisplayData->SetType().SetStr("DisplaySettings");
    exts.push_front(pDisplayData);

    try {
        int int_score = NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow );
        if (int_score == 0)
        {
            if (fields[4].compare("0") == 0)
            {
                pDisplayData->AddField("score", int_score);
            }
            else
            {
                double score = NStr::StringToDouble(fields[4]);       
                pDisplayData->AddField("score", score);
            }
        }
        else
        {
            pDisplayData->AddField("score", int_score);
        }
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad \"strand\" value.") );
        pErr->Throw();
    }

    if (fields.size() < 9) {
        return;
    }
    vector<string> srgb;
    if (fields[8] == "0"  ||  fields[8] == ".") {
        srgb.push_back("0");
        srgb.push_back("0");
        srgb.push_back("0");
    }
    else {
        NStr::Tokenize(fields[8], ",", srgb);
    }
    if (srgb.size() != 3)
    {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad color value.") );
        pErr->Throw();
    }
    try
    {
        for (int i=0; i < 3; i++)
        {
           int x = NStr::StringToInt(srgb[i]);
           if (x<0 || x>255) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Error,
                    0,
                    "Invalid data line: Bad color value.") );
                pErr->Throw();
           }

        }
    }
    catch(const std::exception&)
    {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Invalid data line: Bad color value.") );
        pErr->Throw();
    }
    string rgbValue = NStr::Join(srgb, " ");
    pDisplayData->AddField("color", rgbValue);
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
void CBedReader::x_SetTrackData(
    CRef<CSeq_annot>& annot,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CAnnot_descr& desc = annot->SetDesc();

    if ( strKey == "useScore" ) {
        m_usescore = ( 1 == NStr::StringToInt( strValue ) );
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        return;
    }
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
    if ( strKey == "visibility" ) {
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        return;
    }
    CReaderBase::x_SetTrackData( annot, trackdata, strKey, strValue );
}

//  ----------------------------------------------------------------------------
bool CBedReader::xGetLine(
    ILineReader& lr,
    string& line)
//  ----------------------------------------------------------------------------
{
    while (!lr.AtEOF()) {
        line = *++lr;
        if (!xCommentLine(line)) {
            return true;
        }
    }
	return false;
}

//  ----------------------------------------------------------------------------
inline bool CBedReader::xCommentLine(
    const string& line) const
//  ----------------------------------------------------------------------------
{
    char c = line.data()[0];
    return c == '#' || c == '\0';
}

//  ----------------------------------------------------------------------------
bool 
CBedReader::ReadTrackData(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    IMessageListener* pMessageListener)
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
    IMessageListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (line == "browser"  || NStr::StartsWith(line, "browser ")) {
        return false;
    }
    if (line == "track"  || NStr::StartsWith(line, "track ")) {
        return false;
    }

    vector<string> columns;
	string linecopy = line;
	NStr::TruncateSpacesInPlace(linecopy);

    //  parse
    NStr::Tokenize( linecopy, " \t", columns, NStr::eMergeDelims );
    try {
        xCleanColumnValues(columns);
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pMessageListener);
        return false;
    }

    if (columns.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = columns.size();
        }
        else {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad data line: Inconsistent column count." ) );
            ProcessError(*pErr, pMessageListener);
            return false;
        }
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
    if (m_columncount >= 7  &&  columns[6] != ".") {
        try {
            score = NStr::StringToInt(columns[6]);
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
    if (m_columncount >= 6) {
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
    return (fields.size() >= 8);
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xContainsBlockFeature(
    const vector<string>& fields) const
//  ----------------------------------------------------------------------------
{
    return (fields.size() >= 12);
}


//  ----------------------------------------------------------------------------
bool
CBedReader::xReadBedDataRaw(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    IMessageListener* pMessageListener)
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

END_objects_SCOPE
END_NCBI_SCOPE
