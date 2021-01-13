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

#include <util/line_reader.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/gtf_reader.hpp>
#include "gtf_location_merger.hpp"
#include "reader_message_handler.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CGtfReadRecord::xAssignAttributesFromGff(
    const string& strGtfType,
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    vector< string > attributes;
    xSplitGffAttributes(strRawAttributes, attributes);

	for ( size_t u=0; u < attributes.size(); ++u ) {
        string key, value;
        string attribute(attributes[u]);
        if (!NStr::SplitInTwo(attribute, "=", key, value)) {
            if (!NStr::SplitInTwo(attribute, " ", key, value)) {
                if (strGtfType == "gene") {
                    mAttributes.AddValue(
                        "gene_id", xNormalizedAttributeValue(attribute));
                    continue;
                }
                if (strGtfType == "transcript") {
                    string gid, tid;
                    if (!NStr::SplitInTwo(attribute, ".", gid, tid)) {
                        return false;
                    }
                    mAttributes.AddValue(
                        "gene_id", xNormalizedAttributeValue(gid));
                    mAttributes.AddValue(
                        "transcript_id", xNormalizedAttributeValue(attribute));
                    continue;
                }
            }
        }
        key = xNormalizedAttributeKey(key);
        value = xNormalizedAttributeValue(value);
		if ( key.empty()  &&  value.empty() ) {
            // Probably due to trailing "; ". Sequence Ontology generates such
            // things. 
            continue;
        }
        if (NStr::StartsWith(value, "\"")) {
            value = value.substr(1, string::npos);
        }
        if (NStr::EndsWith(value, "\"")) {
            value = value.substr(0, value.length() - 1);
        }
        mAttributes.AddValue(key, value);
    }
    return true;
}

//  ----------------------------------------------------------------------------
CGtfReader::CGtfReader( 
    unsigned int uFlags,
    const string& strAnnotName,
    const string& strAnnotTitle,
    SeqIdResolver resolver,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, strAnnotName, strAnnotTitle, resolver, pRL)
{
    mpLocations.reset(new CGtfLocationMerger(uFlags, resolver));
}

//  ----------------------------------------------------------------------------
CGtfReader::CGtfReader( 
    unsigned int uFlags,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CGtfReader( uFlags, "", "", CReadUtil::AsSeqId, pRL)
{
}


//  ----------------------------------------------------------------------------
CGtfReader::~CGtfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CGtfReader::ReadSeqAnnot(
    ILineReader& lineReader,
    ILineErrorListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    mCurrentFeatureCount = 0;
    return CReaderBase::ReadSeqAnnot(lineReader, pEC);
}

//  ----------------------------------------------------------------------------
void
CGtfReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot) 
//  ----------------------------------------------------------------------------
{
    for (const auto& lineData: readerData) {
        const auto& line = lineData.mData;
        if (xIsTrackTerminator(line)) {
            continue;
        }
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
bool CGtfReader::xUpdateAnnotFeature(
    const CGff2Record& record,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    const CGtfReadRecord& gff = dynamic_cast<const CGtfReadRecord&>(record);
    auto recType = gff.Type();
    NStr::ToLower(recType);

    using TYPEHANDLER = bool (CGtfReader::*)(const CGtfReadRecord&, CSeq_annot&);
    using HANDLERMAP = map<string, TYPEHANDLER>;

    HANDLERMAP typeHandlers = {
        {"cds",         &CGtfReader::xUpdateAnnotCds},
        {"start_codon", &CGtfReader::xUpdateAnnotCds},
        {"stop_codon",  &CGtfReader::xUpdateAnnotCds},
        {"5utr",        &CGtfReader::xUpdateAnnotTranscript},
        {"3utr",        &CGtfReader::xUpdateAnnotTranscript},
        {"exon",        &CGtfReader::xUpdateAnnotTranscript},
        {"initial",     &CGtfReader::xUpdateAnnotTranscript},
        {"internal",    &CGtfReader::xUpdateAnnotTranscript},
        {"terminal",    &CGtfReader::xUpdateAnnotTranscript},
        {"single",      &CGtfReader::xUpdateAnnotTranscript},
    };

    //
    // Handle officially recognized GTF types:
    //
    HANDLERMAP::iterator it = typeHandlers.find(recType);
    if (it != typeHandlers.end()) {
        TYPEHANDLER handler = it->second;
        return (this->*handler)(gff, annot);
    }
 
    //
    //  Every other type is not officially sanctioned GTF, and per spec we are
    //  supposed to ignore it. In the spirit of being lenient on input we may
    //  try to salvage some of it anyway.
    //
    if (recType == "gene") {
        return xCreateParentGene(gff, annot);
    }
    if (recType == "mrna"  ||  recType == "transcript") {
        return xCreateParentMrna(gff, annot);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xUpdateAnnotCds(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(gff, "cds");
    auto gffType = gff.Type();
    NStr::ToLower(gffType);
    mpLocations->AddRecordForId(featId, gff) ;
    return (xFindFeatById(featId)  ||  xCreateParentCds(gff, annot));
 }

//  ----------------------------------------------------------------------------
bool CGtfReader::xUpdateAnnotTranscript(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    //
    // If there is no gene feature to go with this CDS then make one. Otherwise,
    //  make sure the existing gene feature includes the location of the CDS.
    //
    auto geneFeatId = mpLocations->GetFeatureIdFor(gff, "gene");
    CRef< CSeq_feat > pGene = xFindFeatById(geneFeatId);
    if (!pGene) {
        if (!xCreateParentGene(gff, annot)) {
            return false;
        }
        mpLocations->AddRecordForId(geneFeatId, gff);
    }
    else {
        mpLocations->AddRecordForId(geneFeatId, gff);
        if (!xFeatureTrimQualifiers(gff, *pGene)) {
            return false;
        }
    }

    //
    // If there is no mRNA feature with this gene_id|transcript_id then make one.
    //  Otherwise, fix up the location of the existing one.
    //
    auto transcriptFeatId = mpLocations->GetFeatureIdFor(gff, "transcript");
    CRef<CSeq_feat> pMrna = xFindFeatById(transcriptFeatId);
    if (!pMrna) {
        //
        // Create a brand new CDS feature:
        //
        if (!xCreateParentMrna(gff, annot)) {
            return false;
        }
        mpLocations->AddRecordForId(transcriptFeatId, gff);
    }
    else {
        //
        // Update an already existing CDS features:
        //
        mpLocations->AddRecordForId(transcriptFeatId, gff);
        if (!xFeatureTrimQualifiers(gff, *pMrna)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xCreateFeatureId(
    const CGtfReadRecord& record,
    const string& prefix,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    static int seqNum(1);

    string strFeatureId = prefix;
    if (strFeatureId.empty()) {
        strFeatureId = "id";
    }
    strFeatureId += "_";
    strFeatureId += NStr::IntToString(seqNum++);
    feature.SetId().SetLocal().SetStr(strFeatureId);
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::xCreateParentGene(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  -----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(gff, "gene");
    if (m_MapIdToFeature.find(featId) != m_MapIdToFeature.end()) {
        return true;
    }

    CRef<CSeq_feat> pFeature( new CSeq_feat );

    if (!xFeatureSetDataGene(gff, *pFeature)) {
        return false;
    }
    if (!xCreateFeatureId(gff, "gene", *pFeature)) {
        return false;
    }
    if ( !xFeatureSetQualifiersGene(gff, *pFeature)) {
        return false;
    }

    (gff.Type() == "gene") ?
        mpLocations->AddRecordForId(featId, gff) :
        mpLocations->AddStubForId(featId);
    m_MapIdToFeature[featId] = pFeature;
    xAddFeatureToAnnot(pFeature, annot);
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersGene(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    list<string> ignoredAttrs = {
        "locus_tag", "transcript_id"
    };
    //
    //  Create GB qualifiers for the record attributes:
    //

    const auto& attrs = record.GtfAttributes().Get();
    auto it = attrs.begin();
    for (/*NOOP*/; it != attrs.end(); ++it) {
        auto cit = std::find(ignoredAttrs.begin(), ignoredAttrs.end(), it->first);
        if (cit != ignoredAttrs.end()) {
            continue;
        }
        // special case some well-known attributes
        if (xProcessQualifierSpecialCase(it->first, it->second, feature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, feature);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersRna(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    list<string> ignoredAttrs = {
        "locus_tag"
    };

    const auto& attrs = record.GtfAttributes().Get();
    auto it = attrs.begin();
    for (/*NOOP*/; it != attrs.end(); ++it) {
        auto cit = std::find(ignoredAttrs.begin(), ignoredAttrs.end(), it->first);
        if (cit != ignoredAttrs.end()) {
            continue;
        }
        // special case some well-known attributes
        if (xProcessQualifierSpecialCase(it->first, it->second, feature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, feature);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersCds(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    list<string> ignoredAttrs = {
        "locus_tag"
    };

    const auto& attrs = record.GtfAttributes().Get();
    auto it = attrs.begin();
    for (/*NOOP*/; it != attrs.end(); ++it) {
        auto cit = std::find(ignoredAttrs.begin(), ignoredAttrs.end(), it->first);
        if (cit != ignoredAttrs.end()) {
            continue;
        }
        // special case some well-known attributes
        if (xProcessQualifierSpecialCase(it->first, it->second, feature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, feature);
    } 
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::xCreateParentCds(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  -----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(gff, "cds");
    if (m_MapIdToFeature.find(featId) != m_MapIdToFeature.end()) {
        return true;
    }

    CRef<CSeq_feat> pFeature(new CSeq_feat);

    if (!xFeatureSetDataCds(gff, *pFeature)) {
        return false;
    }
    if (!xCreateFeatureId(gff, "cds", *pFeature)) {
        return false;
    }
    if (!xFeatureSetQualifiersCds(gff, *pFeature)) {
        return false;
    }
    m_MapIdToFeature[featId] = pFeature;
    return xAddFeatureToAnnot(pFeature, annot);
}

//  -----------------------------------------------------------------------------
bool CGtfReader::xCreateParentMrna(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  -----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(gff, "transcript");
    if (m_MapIdToFeature.find(featId) != m_MapIdToFeature.end()) {
        return true;
    }

    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if (!xFeatureSetDataMrna(gff, *pFeature)) {
        return false;
    }
    if (!xCreateFeatureId(gff, "mrna", *pFeature)) {
        return false;
    }
    if ( ! xFeatureSetQualifiersRna( gff, *pFeature ) ) {
        return false;
    }

    mpLocations->AddStubForId(featId);
    m_MapIdToFeature[featId] = pFeature;

    return xAddFeatureToAnnot( pFeature, annot );
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CGtfReader::xFindFeatById(
    const string& featId)
//  ----------------------------------------------------------------------------
{
    auto featIt = m_MapIdToFeature.find(featId);
    if (featIt == m_MapIdToFeature.end()) {
        return CRef<CSeq_feat>();
    }
    return featIt->second;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetDataGene(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CGene_ref& gene = feature.SetData().SetGene();

    const auto& attributes = record.GtfAttributes();
    string geneSynonym = attributes.ValueOf("gene_synonym");
    if (!geneSynonym.empty()) {
        gene.SetSyn().push_back(geneSynonym);
    }
    string locusTag = attributes.ValueOf("locus_tag");
    if (!locusTag.empty()) {
        gene.SetLocus_tag(locusTag);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetDataMrna(
    const CGtfReadRecord& record,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    if (!xFeatureSetDataRna(record, feature, CSeqFeatData::eSubtype_mRNA)) {
        return false;
    }    
    CRNA_ref& rna = feature.SetData().SetRna();

    string product = record.GtfAttributes().ValueOf("product");
    if (!product.empty()) {
        rna.SetExt().SetName(product);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetDataRna(
    const CGtfReadRecord& record,
    CSeq_feat& feature,
    CSeqFeatData::ESubtype subType)
//  ----------------------------------------------------------------------------
{
    CRNA_ref& rnaRef = feature.SetData().SetRna();
    switch (subType){
        default:
            rnaRef.SetType(CRNA_ref::eType_miscRNA);
            break;
        case CSeqFeatData::eSubtype_mRNA:
            rnaRef.SetType(CRNA_ref::eType_mRNA);
            break;
        case CSeqFeatData::eSubtype_rRNA:
            rnaRef.SetType(CRNA_ref::eType_rRNA);
            break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetDataCds(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CCdregion& cdr = feature.SetData().SetCdregion();
    const auto& attributes = record.GtfAttributes();

    string proteinId = attributes.ValueOf("protein_id");
    if (!proteinId.empty()) {
        CRef<CSeq_id> pId = mSeqIdResolve(proteinId, m_iFlags, true);
        if (pId->IsGenbank()) {
            feature.SetProduct().SetWhole(*pId);
        }
    }
    string ribosomalSlippage = attributes.ValueOf("ribosomal_slippage");
    if (!ribosomalSlippage.empty()) {
        feature.SetExcept( true );
        feature.SetExcept_text("ribosomal slippage");
    }
    string transTable = attributes.ValueOf("transl_table");
    if (!transTable.empty()) {
        CRef< CGenetic_code::C_E > pGc( new CGenetic_code::C_E );
        pGc->SetId(NStr::StringToUInt(transTable));
        cdr.SetCode().Set().push_back(pGc);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureTrimQualifiers(
    const CGtfReadRecord& record,
    CSeq_feat& feature )
    //  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual TQual;
    //task:
    // for each attribute of the new piece check if we already got a feature 
    //  qualifier
    // if so, and with the same value, then the qualifier is allowed to live
    // otherwise it is subfeature specific and hence removed from the feature
    TQual& quals = feature.SetQual();
    for (TQual::iterator it = quals.begin(); it != quals.end(); /**/) {
        const string& qualKey = (*it)->GetQual();
        if (NStr::StartsWith(qualKey, "gff_")) {
            it++;
            continue;
        }
        if (qualKey == "locus_tag") {
            it++;
            continue;
        }
        if (qualKey == "old_locus_tag") {
            it++;
            continue;
        }
        if (qualKey == "product") {
            it++;
            continue;
        }
        if (qualKey == "protein_id") {
            it++;
            continue;
        }
        const string& qualVal = (*it)->GetVal();
        if (!record.GtfAttributes().HasValue(qualKey, qualVal)) {
            //superfluous qualifier- squish
            it = quals.erase(it);
            continue;
        }
        it++;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xProcessQualifierSpecialCase(
    const string& key,
    const CGtfAttributes::MultiValue& values,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CRef<CGb_qual> pQual(0);

    if (0 == NStr::CompareNocase(key, "exon_id")) {
        return true;
    }
    if (0 == NStr::CompareNocase(key, "exon_number")) {
        return true;
    }
    if ( 0 == NStr::CompareNocase(key, "note") ) {
        feature.SetComment(NStr::Join(values, ";"));
        return true;
    }
    if ( 0 == NStr::CompareNocase(key, "dbxref") || 
        0 == NStr::CompareNocase(key, "db_xref")) 
    {
        for (auto value: values) {
            vector< string > tags;
            NStr::Split(value, ";", tags );
            for (auto it = tags.begin(); it != tags.end(); ++it ) {
                feature.SetDbxref().push_back(x_ParseDbtag(*it));
            }
        }
        return true;
    }

    if ( 0 == NStr::CompareNocase(key, "pseudo")) {
        feature.SetPseudo( true );
        return true;
    }
    if ( 0 == NStr::CompareNocase(key, "partial")) {
        // RW-1108 - ignore partial attribute in Genbank mode
        if (m_iFlags & CGtfReader::fGenbankMode) {
            return true;
        }
    }
    return false;
}  

//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureAddQualifiers(
    const string& key,
    const CGtfAttributes::MultiValue& values,
    CSeq_feat& feature)
    //  ----------------------------------------------------------------------------
{
    for (auto value: values) {
        feature.AddQualifier(key, value);
    }
};

//  ============================================================================
void CGtfReader::xSetAncestorXrefs(
    CSeq_feat& descendent,
    CSeq_feat& ancestor)
//  ============================================================================
{
    xSetXrefFromTo(descendent, ancestor);
    if (m_iFlags & CGtfReader::fGenerateChildXrefs) {
        xSetXrefFromTo(ancestor, descendent);
    }
}

//  ----------------------------------------------------------------------------
void CGtfReader::xPostProcessAnnot(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    //location fixup:
    for (auto itLocation: mpLocations->LocationMap()) {
        auto id = itLocation.first;
        auto itFeature = m_MapIdToFeature.find(id);
        if (itFeature == m_MapIdToFeature.end()) {
            continue;
        }
        CRef<CSeq_feat> pFeature = itFeature->second;
        auto featSubType = pFeature->GetData().GetSubtype();
        CRef<CSeq_loc> pNewLoc = mpLocations->MergeLocation(
            featSubType, itLocation.second);
        pFeature->SetLocation(*pNewLoc);
    }

    //generate xrefs:
    for (auto itLocation: mpLocations->LocationMap()) {
        auto id = itLocation.first;
        auto itFeature = m_MapIdToFeature.find(id);
        if (itFeature == m_MapIdToFeature.end()) {
            continue;
        }
        CRef<CSeq_feat> pFeature = itFeature->second;
        auto featSubType = pFeature->GetData().GetSubtype();
        switch(featSubType) {
            default: {
                break;
            }
            case CSeqFeatData::eSubtype_mRNA: {
                auto parentGeneFeatId = string("gene:") + pFeature->GetNamedQual("gene_id");
                CRef<CSeq_feat> pParentGene;
                if (x_GetFeatureById(parentGeneFeatId, pParentGene)) {
                    xSetAncestorXrefs(*pFeature, *pParentGene);
                }
                break;
            }
            case CSeqFeatData::eSubtype_cdregion: {
                auto parentRnaFeatId = string("transcript:") + pFeature->GetNamedQual("gene_id") +
                    "_" + pFeature->GetNamedQual("transcript_id");
                CRef<CSeq_feat> pParentRna;
                if (x_GetFeatureById(parentRnaFeatId, pParentRna)) {
                    xSetAncestorXrefs(*pFeature, *pParentRna);
                }
                auto parentGeneFeatId = string("gene:") + pFeature->GetNamedQual("gene_id");
                CRef<CSeq_feat> pParentGene;
                if (x_GetFeatureById(parentGeneFeatId, pParentGene)) {
                    xSetAncestorXrefs(*pFeature, *pParentGene);
                }
                break;
            }
        }
    }
    return CGff2Reader::xPostProcessAnnot(annot);
}

END_objects_SCOPE
END_NCBI_SCOPE
