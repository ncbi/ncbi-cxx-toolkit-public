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
 *   GVF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/line_reader.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objtools/readers/gvf_reader.hpp>
#include "reader_message_handler.hpp"

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

typedef map<string, CVariantProperties::EAllele_state> TAlleleStateMap;

//  ----------------------------------------------------------------------------
const TAlleleStateMap& s_AlleleStateMap()
//  ----------------------------------------------------------------------------
{
    static CSafeStatic<TAlleleStateMap> s_Map;
    TAlleleStateMap& m = *s_Map;
    if ( m.empty() ) {
        m["heterozygous"] = CVariantProperties::eAllele_state_heterozygous;
        m["homozygous"] = CVariantProperties::eAllele_state_homozygous;
        m["hemizygous"] = CVariantProperties::eAllele_state_hemizygous;
    }
    return m;
}

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::AssignFromGff(
    const string& strGff )
//  ----------------------------------------------------------------------------
{
    if ( ! CGff3ReadRecord::AssignFromGff( strGff ) ) {
        return false;
    }
    // GVF specific fixup goes here ...
    TAttrIt idIt = m_Attributes.find("ID");
    if (idIt == m_Attributes.end()) {
        CReaderMessage fatal(
            eDiag_Error,
            0,
            "Mandatory attribute ID missing.");
        throw fatal;
    }
    TAttrIt variantSeqIt = m_Attributes.find("Variant_seq");
    TAttrIt referenceSeqIt = m_Attributes.find("Reference_seq");
    if (variantSeqIt == m_Attributes.end()  ||  referenceSeqIt == m_Attributes.end()) {
        CReaderMessage fatal(
            eDiag_Error,
            0,
            "Mandatory attribute Reference_seq and/or Variant_seq missing.");
        throw fatal;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::xAssignAttributesFromGff(
    const string& strGffType,
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    vector< string > attributes;
    xSplitGffAttributes(strRawAttributes, attributes);
    for ( size_t u=0; u < attributes.size(); ++u ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( attributes[u], "=", strKey, strValue ) ) {
            if ( ! NStr::SplitInTwo( attributes[u], " ", strKey, strValue ) ) {
                return false;
            }
        }
        strKey = x_NormalizedAttributeKey( strKey );
        strValue = xNormalizedAttributeValue( strValue );

        if ( strKey.empty() && strValue.empty() ) {
            // Probably due to trailing "; ". Sequence Ontology generates such
            // things.
            continue;
        }

        if ( strKey == "Dbxref" ) {
            TAttrIt it = m_Attributes.find( strKey );
            if ( it != m_Attributes.end() ) {
                m_Attributes[ strKey ] += ";";
                m_Attributes[ strKey ] += strValue;
                continue;
            }
        }
        m_Attributes[ strKey ] = strValue;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::SanityCheck() const
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
CGvfReader::CGvfReader(
    unsigned int uFlags,
    const string& name,
    const string& title,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CGff3Reader(uFlags, name, title, CReadUtil::AsSeqId, pRL)
{
}

//  ----------------------------------------------------------------------------
CGvfReader::~CGvfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CGvfReader::ReadSeqAnnot(
    ILineReader& lineReader,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    return CReaderBase::ReadSeqAnnot(lineReader, pEC);
}

//  ----------------------------------------------------------------------------
void
CGvfReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    return CReaderBase::xGetData(lr, readerData);
}

//  ----------------------------------------------------------------------------
void
CGvfReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (const auto& lineData: readerData) {
        const auto& line = lineData.mData;
        if (xParseStructuredComment(line)) {
            continue;
        }
        if (xParseBrowserLine(line, annot)) {
            continue;
        }
        if (xParseFeature(line, annot, nullptr)) {
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
bool
CGvfReader::xParseFeature(
    const string& line,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CGvfReadRecord record(m_uLineNumber);
    if (!record.AssignFromGff(line)) {
        return false;
    }
    if (!xMergeRecord(record, annot, pEC)) {
        return false;
    }
    ++mCurrentFeatureCount;
    return true;
}

//  ----------------------------------------------------------------------------
void CGvfReader::xPostProcessAnnot(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    xAddConversionInfo(annot, nullptr);
    xAssignTrackData(annot);
    xAssignAnnotId(annot);
    if (m_Pragmas) {
        annot.SetDesc().Set().push_back(m_Pragmas);
    }
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CGvfReader::x_GetAnnotById(
    TAnnots& annots,
    const string& strId )
//  ----------------------------------------------------------------------------
{
    for ( TAnnotIt it = annots.begin(); it != annots.end(); ++it ) {
        CSeq_annot& annot = **it;
        if ( ! annot.CanGetId() || annot.GetId().size() != 1 ) {
            // internal error
            return CRef<CSeq_annot>();
        }

        CRef< CAnnot_id > pId = *( annot.GetId().begin() );
        if ( ! pId->IsLocal() ) {
            // internal error
            return CRef<CSeq_annot>();
        }
        if ( strId == pId->GetLocal().GetStr() ) {
            return *it;
        }
    }
    CRef<CSeq_annot> pNewAnnot( new CSeq_annot );
    annots.insert(annots.begin(), pNewAnnot);

    CRef< CAnnot_id > pAnnotId( new CAnnot_id );
    pAnnotId->SetLocal().SetStr( strId );
    pNewAnnot->SetId().push_back( pAnnotId );
    pNewAnnot->SetData().SetFtable();

    // if available, add current browser information
    if ( m_CurrentBrowserInfo ) {
        pNewAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
    }

    // if available, add current track information
    if (m_pTrackDefaults->ContainsData()) {
        xAssignTrackData(*pNewAnnot);
    }

    if ( !m_AnnotName.empty() ) {
        pNewAnnot->SetNameDesc(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pNewAnnot->SetTitleDesc(m_AnnotTitle);
    }

    // if available, add gvf pragma information
    if ( m_Pragmas ) {
        pNewAnnot->SetDesc().Set().push_back( m_Pragmas );
    }
    return pNewAnnot;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xMergeRecord(
    const CGvfReadRecord& record,
    CSeq_annot& annot,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( ! record.SanityCheck() ) {
        return false;
    }
    CRef< CSeq_feat > pFeature( new CSeq_feat );
    if ( ! xFeatureSetLocation( record, *pFeature ) ) {
        return false;
    }
    if ( ! xFeatureSetVariation( record, *pFeature ) ) {
        return false;
    }
    if ( ! xFeatureSetExt( record, *pFeature, pMessageListener ) ) {
        return false;
    }
    annot.SetData().SetFtable().push_back( pFeature );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetLocation(
    const CGff2Record& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if (record.SeqStart() < record.SeqStop()) {
        return xFeatureSetLocationInterval(record, feature);
    }
    else {// record.SeqStart() == record.SeqStop()
        return xFeatureSetLocationPoint(record, feature);
    }
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xSetLocation(
        const CGff2Record& record,
        CSeq_loc& location)
//  ----------------------------------------------------------------------------
{

    if (record.SeqStart() < record.SeqStop()) {
        return xSetLocationInterval(record, location);
    }
    else { // record.SeqStart() == record.SeqStop()
        return xSetLocationPoint(record, location);
    }
}



//  ----------------------------------------------------------------------------
bool CGvfReader::xSetLocationInterval(
    const CGff2Record& record,
    CSeq_loc& location)
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);

    location.SetInt().SetId(*pId);
    location.SetInt().SetFrom(record.SeqStart());
    location.SetInt().SetTo(record.SeqStop());
    if (record.IsSetStrand()) {
        location.SetInt().SetStrand( record.Strand() );
    }

    //  deal with fuzzy range indicators / lower end:
    string strRange;
    list<string> range_borders;
    TSeqPos lower, upper;
    if (record.GetAttribute( "Start_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders, 0 );
        if ( range_borders.size() != 2 ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad Start_range attribute: Start_range=" + strRange + ".");
            throw error;
        }
        try {
            if ( range_borders.back() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.front() );
                location.SetInt().SetFuzz_from().SetLim( CInt_fuzz::eLim_gt );
            }
            else if ( range_borders.front() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.back() );
                location.SetInt().SetFuzz_from().SetLim( CInt_fuzz::eLim_lt );
            }
            else {
                lower = NStr::StringToUInt( range_borders.front() );
                upper = NStr::StringToUInt( range_borders.back() );
                location.SetInt().SetFuzz_from().SetRange().SetMin( lower-1 );
                location.SetInt().SetFuzz_from().SetRange().SetMax( upper-1 );
            }
        }
        catch ( std::exception& ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad Start_range attribute: Start_range=" + strRange + ".");
            throw error;
        }
    }

    //  deal with fuzzy range indicators / upper end:
    range_borders.clear();
    if (record.GetAttribute( "End_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders, 0 );
        if ( range_borders.size() != 2 ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad End_range attribute: End_range=" + strRange + ".");
            throw error;
        }
        try {
            if ( range_borders.back() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.front() );
                location.SetInt().SetFuzz_to().SetLim( CInt_fuzz::eLim_gt );
            }
            else if ( range_borders.front() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.back() );
                location.SetInt().SetFuzz_to().SetLim( CInt_fuzz::eLim_lt );
            }
            else {
                lower = NStr::StringToUInt( range_borders.front() );
                upper = NStr::StringToUInt( range_borders.back() );
                location.SetInt().SetFuzz_to().SetRange().SetMin( lower-1 );
                location.SetInt().SetFuzz_to().SetRange().SetMax( upper-1 );
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad End_range attribute: End_range=" + strRange + ".");
            throw error;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xSetLocationPoint(
    const CGff2Record& record,
    CSeq_loc& location)
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);
    location.SetPnt().SetId(*pId);
    if (record.Type() == "insertion") {
        //for insertions, GVF uses insert-after logic while NCBI uses insert-before
        location.SetPnt().SetPoint(record.SeqStart()+1);
    }
    else {
        location.SetPnt().SetPoint(record.SeqStart());
    }
    if (record.IsSetStrand()) {
        location.SetStrand(record.Strand());
    }

    string strRangeLower, strRangeUpper;
    bool hasLower = record.GetAttribute("Start_range", strRangeLower);
    bool hasUpper = record.GetAttribute("End_range", strRangeUpper);
    if (hasLower  &&  hasUpper  &&  strRangeLower != strRangeUpper) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: Conflicting fuzz ranges for single point location.");
        throw error;
    }
    if (!hasLower  &&  !hasUpper) {
        return true;
    }
    if (!hasLower) {
        strRangeLower = strRangeUpper;
    }

    list<string> bounds;
    TSeqPos lower, upper;
    NStr::Split( strRangeLower, ",", bounds, 0 );
    if (bounds.size() != 2) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: XXX_range=" + strRangeLower + ".");
        throw error;
    }
    try {
        if (bounds.back() == ".") {
            lower = upper = NStr::StringToUInt(bounds.front());
            location.SetPnt().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
        }
        else if (bounds.front() == ".") {
            lower = upper = NStr::StringToUInt(bounds.back());
            location.SetPnt().SetFuzz().SetLim( CInt_fuzz::eLim_lt );
        }
        else {
            lower = NStr::StringToUInt(bounds.front());
            upper = NStr::StringToUInt(bounds.back());
            location.SetPnt().SetFuzz().SetRange().SetMin(lower-1);
            location.SetPnt().SetFuzz().SetRange().SetMax(upper-1);
        }
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: XXX_range=" + strRangeLower + ".");
        throw error;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetLocationInterval(
    const CGff2Record& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);
    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId(*pId);
    pLocation->SetInt().SetFrom(record.SeqStart());
    pLocation->SetInt().SetTo(record.SeqStop());
    if (record.IsSetStrand()) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }

    //  deal with fuzzy range indicators / lower end:
    string strRange;
    list<string> range_borders;
    TSeqPos lower, upper;
    if (record.GetAttribute( "Start_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders, 0 );
        if ( range_borders.size() != 2 ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad Start_range attribute: Start_range=" + strRange + ".");
            throw error;
        }
        try {
            if ( range_borders.back() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.front() );
                pLocation->SetInt().SetFuzz_from().SetLim( CInt_fuzz::eLim_gt );
            }
            else if ( range_borders.front() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.back() );
                pLocation->SetInt().SetFuzz_from().SetLim( CInt_fuzz::eLim_lt );
            }
            else {
                lower = NStr::StringToUInt( range_borders.front() );
                upper = NStr::StringToUInt( range_borders.back() );
                pLocation->SetInt().SetFuzz_from().SetRange().SetMin( lower-1 );
                pLocation->SetInt().SetFuzz_from().SetRange().SetMax( upper-1 );
            }
        }
        catch ( std::exception& ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad Start_range attribute: Start_range=" + strRange + ".");
            throw error;
        }
    }

    //  deal with fuzzy range indicators / upper end:
    range_borders.clear();
    if (record.GetAttribute( "End_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders, 0 );
        if ( range_borders.size() != 2 ) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad End_range attribute: End_range=" + strRange + ".");
            throw error;
        }
        try {
            if ( range_borders.back() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.front() );
                pLocation->SetInt().SetFuzz_to().SetLim( CInt_fuzz::eLim_gt );
            }
            else if ( range_borders.front() == "." ) {
                lower = upper = NStr::StringToUInt( range_borders.back() );
                pLocation->SetInt().SetFuzz_to().SetLim( CInt_fuzz::eLim_lt );
            }
            else {
                lower = NStr::StringToUInt( range_borders.front() );
                upper = NStr::StringToUInt( range_borders.back() );
                pLocation->SetInt().SetFuzz_to().SetRange().SetMin( lower-1 );
                pLocation->SetInt().SetFuzz_to().SetRange().SetMax( upper-1 );
            }
        }
        catch (std::exception&) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad End_range attribute: End_range=" + strRange + ".");
            throw error;
        }
    }

    feature.SetLocation( *pLocation );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetLocationPoint(
    const CGff2Record& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);
    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetPnt().SetId(*pId);
    if (record.Type() == "insertion") {
        //for insertions, GVF uses insert-after logic while NCBI uses insert-before
        pLocation->SetPnt().SetPoint(record.SeqStart()+1);
    }
    else {
        pLocation->SetPnt().SetPoint(record.SeqStart());
    }
    if (record.IsSetStrand()) {
        pLocation->SetStrand(record.Strand());
    }

    string strRangeLower, strRangeUpper;
    bool hasLower = record.GetAttribute("Start_range", strRangeLower);
    bool hasUpper = record.GetAttribute("End_range", strRangeUpper);
    if (hasLower  &&  hasUpper  &&  strRangeLower != strRangeUpper) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: Conflicting fuzz ranges for single point location.");
        throw error;
    }
    if (!hasLower  &&  !hasUpper) {
        feature.SetLocation(*pLocation);
        return true;
    }
    if (!hasLower) {
        strRangeLower = strRangeUpper;
    }

    list<string> bounds;
    TSeqPos lower, upper;
    NStr::Split( strRangeLower, ",", bounds, 0 );
    if (bounds.size() != 2) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: XXX_range=" + strRangeLower + ".");
        throw error;
    }
    try {
        if (bounds.back() == ".") {
            lower = upper = NStr::StringToUInt(bounds.front());
            pLocation->SetPnt().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
        }
        else if (bounds.front() == ".") {
            lower = upper = NStr::StringToUInt(bounds.back());
            pLocation->SetPnt().SetFuzz().SetLim( CInt_fuzz::eLim_lt );
        }
        else {
            lower = NStr::StringToUInt(bounds.front());
            upper = NStr::StringToUInt(bounds.back());
            pLocation->SetPnt().SetFuzz().SetRange().SetMin(lower-1);
            pLocation->SetPnt().SetFuzz().SetRange().SetMax(upper-1);
        }
    }
    catch (std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Bad range attribute: XXX_range=" + strRangeLower + ".");
        throw error;
    }
    feature.SetLocation( *pLocation );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xIsDbvarCall(const string& nameAttr) const
//  ----------------------------------------------------------------------------
{
    return (nameAttr.find("ssv") != string::npos);
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xGetNameAttribute(const CGvfReadRecord& record, string& name) const
//  ----------------------------------------------------------------------------
{
    if ( record.GetAttribute( "Name", name ) ) {
        return true;
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetVariation(
    const CGvfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CRef<CVariation_ref> pVariation(new CVariation_ref);
    string strType = record.Type();
    NStr::ToLower( strType );

    string nameAttr;
    xGetNameAttribute(record, nameAttr);

    if ( strType == "snv" ) {
        if (!xVariationMakeSNV( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "insertion" ||
             strType == "alu_insertion" ||
             strType == "line1_insertion" ||
             strType == "sva_insertion" ||
             strType == "mobile_element_insertion" ||
             strType == "mobile_sequence_insertion" ||
             strType == "novel_sequence_insertion") {
        if (!xVariationMakeInsertions( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "deletion" ||
             strType == "alu_deletion" ||
             strType == "line1_deletion" ||
             strType == "sva_deletion" ||
             strType == "herv_deletion" ||
             (strType == "mobile_element_deletion" &&
              xIsDbvarCall(nameAttr))) {
        if (!xVariationMakeDeletions( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "indel") {
        if (!xVariationMakeIndels( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "inversion") {
        if (!xVariationMakeInversions( record, *pVariation )) {
            return false;
        }

    }
    else if (strType == "tandem_duplication") {
        if (!xVariationMakeEversions( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "translocation" ||
             strType == "interchromosomal_translocation" ||
             strType == "intrachromosomal_translocation") {

        if (!xVariationMakeTranslocations( record, *pVariation )) {
            return false;
        }
    }
    else if (strType == "complex" ||
             strType == "complex_substitution" ||
             strType == "complex_chromosomal_rearrangement" ||
             strType == "complex_sequence_alteration") {
        if (!xVariationMakeComplex( record, *pVariation )){
            return false;
        }
    }
    else if (strType == "unknown" ||
             strType == "other" ||
             strType == "sequence_alteration") {
        if (!xVariationMakeUnknown( record, *pVariation )){
            return false;
        }
    }
    else {
        if (!xVariationMakeCNV( record, *pVariation )) {
            return false;
        }
    }
    if ( pVariation ) {
        feature.SetData().SetVariation( *pVariation );
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xParseStructuredComment(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    if ( !CGff2Reader::xParseStructuredComment( strLine) ) {
        return false;
    }
    if ( ! m_Pragmas ) {
        m_Pragmas.Reset( new CAnnotdesc );
        m_Pragmas->SetUser().SetType().SetStr( "gvf-import-pragmas" );
    }

    string key, value;
    NStr::SplitInTwo(strLine.substr(2), " ", key, value);
    m_Pragmas->SetUser().AddField(key, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationSetInsertions(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    CRef<CVariation_ref> pReference(new CVariation_ref);
    pReference->SetData().SetInstance().SetType(
        CVariation_inst::eType_identity);
    CRef<CDelta_item> pDelta(new CDelta_item);
    pDelta->SetSeq().SetThis();
    pReference->SetData().SetInstance().SetDelta().push_back(pDelta);
    pReference->SetData().SetInstance().SetObservation(
        CVariation_inst::eObservation_asserted);
    variation.SetData().SetSet().SetVariations().push_back(
        pReference );

    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles, 0 );
        alleles.sort();
        alleles.unique();
        for ( list<string>::const_iterator cit = alleles.begin();
            cit != alleles.end(); ++cit )
        {
            string allele(*cit);
            if (allele == "-") {
                pReference->SetVariant_prop().SetAllele_state(
                    (alleles.size() == 1) ?
                        CVariantProperties::eAllele_state_homozygous :
                        CVariantProperties::eAllele_state_heterozygous);
                pReference->SetData().SetInstance().SetObservation(
                    CVariation_inst::eObservation_asserted |
                    CVariation_inst::eObservation_variant);
                continue;
            }
            CRef<CVariation_ref> pAllele(new CVariation_ref);
            //if (allele == strReference) {
            //    continue;
            //}
            if (alleles.size() == 1) {
                pAllele->SetVariant_prop().SetAllele_state(
                    CVariantProperties::eAllele_state_homozygous);
            }
            else {
                pAllele->SetVariant_prop().SetAllele_state(
                    CVariantProperties::eAllele_state_heterozygous);
            }
            ///pAllele->SetInsertion(allele, CVariation_ref::eSeqType_na);
            CRef<CDelta_item> pDelta(new CDelta_item);
            pDelta->SetSeq().SetLiteral().SetLength(
                static_cast<TSeqPos>(allele.size()));
            pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
            pDelta->SetAction(CDelta_item::eAction_ins_before);
            pAllele->SetData().SetInstance().SetDelta().push_back(pDelta);
            pAllele->SetData().SetInstance().SetType(CVariation_inst::eType_ins);
            pAllele->SetData().SetInstance().SetObservation(
                CVariation_inst::eObservation_variant );

            variation.SetData().SetSet().SetVariations().push_back(
               pAllele );
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeCNV(
    const CGvfReadRecord& record,
    CVariation_ref& variation )
//  ----------------------------------------------------------------------------
{
    if (!xVariationSetId(record, variation)) {
        return false;
    }
    if (!xVariationSetParent(record, variation)) {
        return false;
    }
    if (!xVariationSetName(record, variation)) {
        return false;
    }

    string nameAttr;
    xGetNameAttribute(record, nameAttr);

    string strType = record.Type();
    NStr::ToLower( strType );
    if ( strType == "cnv" ||
        strType == "copy_number_variation" ) {
        variation.SetCNV();
        return true;
    }
    if ( strType == "gain" ||
         strType == "copy_number_gain" ||
         strType == "duplication" ) {
        variation.SetGain();
        return true;
    }
    if ( strType == "loss" ||
         strType == "copy_number_loss" ||
         (strType == "mobile_element_deletion" && !xIsDbvarCall(nameAttr)) ) {
        variation.SetLoss();
        return true;
    }
    if ( strType == "loss_of_heterozygosity" ) {
        variation.SetLoss();
        CRef<CVariation_ref::C_E_Consequence> pConsequence(
            new CVariation_ref::C_E_Consequence );
        pConsequence->SetLoss_of_heterozygosity();
        variation.SetConsequence().push_back( pConsequence );
        return true;
    }

    CReaderMessage error(
        eDiag_Error,
        m_uLineNumber,
        "Bad data line: Unknown type \"" + strType + "\".");
    throw error;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationSetCommon(
        const CGvfReadRecord& record,
        CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    variation.SetData().SetSet().SetType(
        CVariation_ref::C_Data::C_Set::eData_set_type_package );

    if ( ! xVariationSetId( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetParent( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetName( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetProperties( record, variation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeInversions(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    CRef<CSeq_loc> null = Ref(new CSeq_loc());
    null->SetNull();
    variation.SetInversion(*null);
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeEversions(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    CRef<CSeq_loc> null = Ref(new CSeq_loc());
    null->SetNull();
    variation.SetEversion(*null);
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeTranslocations(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    CRef<CSeq_loc> null = Ref(new CSeq_loc());
    null->SetNull();
    variation.SetTranslocation(*null);
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeComplex(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    variation.SetComplex();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeUnknown(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    variation.SetUnknown();
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeSNV(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetSnvs( record, variation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeInsertions(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{

    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetInsertions( record, variation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeDeletions(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    if ( ! xVariationSetDeletions( record, variation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeIndels(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ----------------------------------------------------------------------------
{
    if ( ! xVariationSetCommon( record, variation ) ) {
        return false;
    }
    variation.SetDeletionInsertion("", CVariation_ref::eSeqType_na);
    variation.SetData().SetInstance().SetType(CVariation_inst::eType_delins);
    return true;
}


//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetId(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ---------------------------------------------------------------------------
{
    string id;
    if ( record.GetAttribute( "ID", id ) ) {
        variation.SetId().SetDb( record.Source() );
        variation.SetId().SetTag().SetStr( id );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetParent(
    const CGvfReadRecord& record,
    CVariation_ref& variation)
//  ---------------------------------------------------------------------------
{
    string id;
    if ( record.GetAttribute( "Parent", id ) ) {
        variation.SetParent_id().SetDb( record.Source() );
        variation.SetParent_id().SetTag().SetStr( id );
    }
    return true;
}


//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetName(
    const CGvfReadRecord& record,
    CVariation_ref& variation )
//  ---------------------------------------------------------------------------
{
    string name;
    if ( record.GetAttribute( "Name", name ) ) {
        variation.SetName( name );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetProperties(
    const CGvfReadRecord& record,
    CVariation_ref& variation )
//  ---------------------------------------------------------------------------
{
    typedef map<string, CVariantProperties::EAllele_state>::const_iterator ALLIT;

    string strGenotype;
    if ( record.GetAttribute( "Genotype", strGenotype ) ) {
        ALLIT it = s_AlleleStateMap().find( strGenotype );
        if ( it != s_AlleleStateMap().end() ) {
            variation.SetVariant_prop().SetAllele_state( it->second );
        }
        else {
            variation.SetVariant_prop().SetAllele_state(
                CVariantProperties::eAllele_state_other );
        }
    }
    string strValidated;
    if ( record.GetAttribute( "validated", strValidated ) ) {
        if ( strValidated == "1" ) {
            variation.SetVariant_prop().SetOther_validation( true );
        }
        if ( strValidated == "0" ) {
            variation.SetVariant_prop().SetOther_validation( false );
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetDeletions(
    const CGvfReadRecord& record,
    CVariation_ref& variation )
//  ---------------------------------------------------------------------------
{
    string strReference;
    CRef<CVariation_ref> pReference(new CVariation_ref);
    if (!record.GetAttribute("Reference_seq", strReference)) {
        return false;
    }
    pReference->SetData().SetInstance().SetType(
        CVariation_inst::eType_identity);
    CRef<CDelta_item> pDelta(new CDelta_item);
    pDelta->SetSeq().SetLiteral().SetLength(
        static_cast<TSeqPos>(strReference.size()));
    pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(
        strReference);
    pReference->SetData().SetInstance().SetDelta().push_back(pDelta);
    pReference->SetData().SetInstance().SetObservation(
        CVariation_inst::eObservation_asserted);
    variation.SetData().SetSet().SetVariations().push_back(
        pReference );

    string strAlleles;
    if (!record.GetAttribute( "Variant_seq", strAlleles)) {
        return false;
    }
    list<string> alleles;
    NStr::Split( strAlleles, ",", alleles, 0 );
    alleles.sort();
    alleles.unique();
    for ( list<string>::const_iterator cit = alleles.begin();
        cit != alleles.end(); ++cit )
    {
        string allele(*cit);
        if (allele == strReference) {
            pReference->SetVariant_prop().SetAllele_state(
                (alleles.size() == 1) ?
                    CVariantProperties::eAllele_state_homozygous :
                    CVariantProperties::eAllele_state_heterozygous);
            pReference->SetData().SetInstance().SetObservation(
                CVariation_inst::eObservation_asserted |
                CVariation_inst::eObservation_variant);
            continue;
        }
        CRef<CVariation_ref> pAllele(new CVariation_ref);
        pAllele->SetVariant_prop().SetAllele_state(
            (alleles.size() == 1) ?
            CVariantProperties::eAllele_state_homozygous :
            CVariantProperties::eAllele_state_heterozygous);
        CRef<CDelta_item> pDelta(new CDelta_item);
        pDelta->SetSeq().SetThis();
        pDelta->SetAction(CDelta_item::eAction_del_at);
        pAllele->SetData().SetInstance().SetDelta().push_back(pDelta);

        pAllele->SetData().SetInstance().SetType(CVariation_inst::eType_del);
        pAllele->SetData().SetInstance().SetObservation(
            CVariation_inst::eObservation_variant );

        variation.SetData().SetSet().SetVariations().push_back(
            pAllele );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetSnvs(
    const CGvfReadRecord& record,
    CVariation_ref& variation )
//  ---------------------------------------------------------------------------
{
    string strReference;
    CRef<CVariation_ref> pReference(new CVariation_ref);
    if (record.GetAttribute("Reference_seq", strReference)) {
        pReference->SetData().SetInstance().SetType(
            CVariation_inst::eType_identity);
        CRef<CDelta_item> pDelta(new CDelta_item);
        pDelta->SetSeq().SetLiteral().SetLength(
            static_cast<TSeqPos>(strReference.size()));
        pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(
            strReference);
        pReference->SetData().SetInstance().SetDelta().push_back(pDelta);
        pReference->SetData().SetInstance().SetObservation(
            CVariation_inst::eObservation_asserted);
        variation.SetData().SetSet().SetVariations().push_back(
            pReference );
    }

    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles, 0 );
        alleles.sort();
        alleles.unique();
        for ( list<string>::const_iterator cit = alleles.begin();
            cit != alleles.end(); ++cit )
        {
            string allele(*cit);
            CRef<CVariation_ref> pAllele(new CVariation_ref);
            if (allele == strReference) {
                pReference->SetVariant_prop().SetAllele_state(
                    (alleles.size() == 1) ?
                        CVariantProperties::eAllele_state_homozygous :
                        CVariantProperties::eAllele_state_heterozygous);
                pReference->SetData().SetInstance().SetObservation(
                    CVariation_inst::eObservation_asserted |
                    CVariation_inst::eObservation_variant);
                continue;
            }
            if (alleles.size() == 1) {
                pAllele->SetVariant_prop().SetAllele_state(
                    CVariantProperties::eAllele_state_homozygous);
            }
            else {
                pAllele->SetVariant_prop().SetAllele_state(
                    CVariantProperties::eAllele_state_heterozygous);
            }
            vector<string> replaces;
            replaces.push_back(*cit);
            pAllele->SetSNV(replaces, CVariation_ref::eSeqType_na);
            pAllele->SetData().SetInstance().SetObservation(
                CVariation_inst::eObservation_variant);
            pAllele->SetData().SetInstance().SetType(
                CVariation_inst::eType_snv );
            variation.SetData().SetSet().SetVariations().push_back(
               pAllele );
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xFeatureSetExt(
    const CGvfReadRecord& record,
    CSeq_feat& feature,
    ILineErrorListener* pMessageListener)
//  ---------------------------------------------------------------------------
{
    string strAttribute;

    CSeq_feat::TExt& ext = feature.SetExt();
    ext.SetType().SetStr( "GvfAttributes" );
    ext.AddField( "orig-var-type", record.Type() );

    if ( record.Source() != "." ) {
        ext.AddField( "source", record.Source() );
    }
    if ( record.IsSetScore() ) {
        ext.AddField( "score", record.Score() );
    }
    for ( CGff2Record::TAttrCit cit = record.Attributes().begin();
        cit != record.Attributes().end(); ++cit )
    {

        if ( cit->first == "Start_range" ) {
            continue;
        }
        if ( cit->first == "End_range" ) {
            continue;
        }
        if ( cit->first == "validated" ) {
            continue;
        }

        string strAttribute;
        if ( ! record.GetAttribute( cit->first, strAttribute ) ) {
            CReaderMessage warning(
                eDiag_Warning,
                m_uLineNumber,
                "Suspicious data line: Funny attribute \"" + cit->first + "\".");
            m_pMessageHandler->Report(warning);
            continue;
        }
        if ( cit->first == "ID" ) {
            ext.AddField( "id", strAttribute );
            continue;
        }
        if ( cit->first == "Parent" ) {
            ext.AddField( "parent", strAttribute );
            continue;
        }
        if ( cit->first == "Variant_reads" ) {
            ext.AddField( "variant-reads", strAttribute ); // for lack of better idea
            continue;
        }
        if ( cit->first == "Variant_effect" ) {
            ext.AddField( "variant-effect", strAttribute ); // for lack of better idea
            continue;
        }
        if ( cit->first == "Total_reads" ) {
            ext.AddField( "total-reads", strAttribute );
            continue;
        }
        if ( cit->first == "Variant_copy_number" ) {
            ext.AddField( "variant-copy-number", strAttribute );
            continue;
        }
        if ( cit->first == "Reference_copy_number" ) {
            ext.AddField( "reference-copy-number", strAttribute );
            continue;
        }
        if ( cit->first == "Phased" ) {
            ext.AddField( "phased", strAttribute );
            continue;
        }
        if ( cit->first == "Name" ) {
            ext.AddField( "name", strAttribute );
            continue;
        }
        ext.AddField( string("custom-") + cit->first, strAttribute );
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
