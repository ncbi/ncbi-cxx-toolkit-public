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
#include <objects/seq/so_map.hpp>

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
        CTempString line = lineData.mData;
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


static bool s_IsExonOrUTR(const string& recType)
{
    return (recType == "exon" || recType == "5utr" || recType == "3utr");
}

static bool s_IsCDSType(const string& recType)
{
    return (recType == "cds" || recType == "start_codon" || recType == "stop_codon");
}


//  ----------------------------------------------------------------------------
bool CGtfReader::xUpdateAnnotFeature(
    const CGff2Record& record,
    CSeq_annot& annot,
    ILineErrorListener* /*pEC*/)
//  ----------------------------------------------------------------------------
{
    const CGtfReadRecord& gff = dynamic_cast<const CGtfReadRecord&>(record);
    auto recType = gff.NormalizedType();


    if (s_IsCDSType(recType)) { // RW-2062
        xUpdateAnnotCds(gff, annot);
        xUpdateGeneAndMrna(gff, annot);
        return true;
    }

    if (s_IsExonOrUTR(recType)) { // RW-2062
        xUpdateGeneAndMrna(gff, annot);
        return true;
    }

    if (recType == "gene") {
        xCreateGene(gff, annot);
    } else if (recType == "mrna" || recType == "transcript") {
        xCreateRna(gff, annot);
    }
    return true;
}


static CGtfAttributes s_GetIntersection(const CGtfAttributes& x, const CGtfAttributes& y)
{
    CGtfAttributes result;
    const auto& xAttributes = x.Get(); // xAttributes has type map<string, set<string>>
    const auto& yAttributes = y.Get();

    auto xit = xAttributes.begin();
    auto yit = yAttributes.begin();
    while (xit != xAttributes.end() && yit != yAttributes.end()) {
        if (xit->first < yit->first) {
            ++xit;
        } else if (yit->first < xit->first) {
            ++yit;
        }
        else { // xit->first == yit->first
            const set<string>& xVals = xit->second;
            if (xVals.empty()) {
                result.AddValue(xit->first, "");
            }
            else {
                const set<string>& yVals = yit->second;
                set<string> commonVals;
                set_intersection(begin(xVals), end(xVals),
                    begin(yVals), end(yVals),
                    inserter(commonVals, commonVals.begin()));
                if (!commonVals.empty()) {
                    for (const auto& val : commonVals) {
                        result.AddValue(xit->first, val);
                    }
                }
            }
            ++xit;
            ++yit;
        }

    }

    return result;
}


//  ----------------------------------------------------------------------------
void CGtfReader::xUpdateAnnotCds(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(gff, "cds");
    mpLocations->AddRecordForId(featId, gff) ;
    if (! xFindFeatById(featId)) {
        xCreateCds(gff, annot);
    }
}


bool CGtfReader::xIsCommentLine(
        const CTempString& line)
{
    return (line.empty() || line[0] == '#');
}


//  ----------------------------------------------------------------------------
void CGtfReader::xAddQualToFeat(
        const CGtfAttributes& attribs,
        const string& qualName,
        CSeq_feat& feat)
//  ----------------------------------------------------------------------------
{
    CGtfAttributes::MultiValue values;
    attribs.GetValues(qualName, values);
    if (!values.empty()) {
        xFeatureAddQualifiers(qualName, values, feat);
    }
}


//  ----------------------------------------------------------------------------
void CGtfReader::xCreateParent(const CGtfReadRecord& record,
        const string& parentType,
        CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (parentType == "gene") {
        xCreateGene(record, annot);
    } else {
        xCreateRna(record, annot);
    }
}



static void s_TrimFeatQuals(
    const CGtfAttributes& prevAttributes,
    const CGtfAttributes& attributes,
    CSeq_feat& feature)
    //  ----------------------------------------------------------------------------
{
    //task:
    // for each attribute of the new piece check if we already got a feature
    //  qualifier
    // if so, and with the same value, then the qualifier is allowed to live
    // otherwise it is subfeature specific and hence removed from the feature
    auto& quals = feature.SetQual();
    for (auto it = quals.begin(); it != quals.end(); /**/) {
        const string& qualKey = (*it)->GetQual();

        if (NStr::StartsWith(qualKey, "gff_") ||
            qualKey == "locus_tag" ||
            qualKey == "old_locus_tag" ||
            qualKey == "product" ||
            qualKey == "protein_id") {
            ++it;
            continue;
        }
        
        const string& qualVal = (*it)->GetVal();

        // erase attributes encountered previously, but not in current list of attributes  
        if (prevAttributes.HasValue(qualKey, qualVal) && (!attributes.HasValue(qualKey, qualVal))) {
            //superfluous qualifier - squish
            it = quals.erase(it);
            continue;
        }
        ++it;
    }
}

//  ----------------------------------------------------------------------------
void CGtfReader::xAddQualsToParent(
        const string& recType, 
        const CGtfAttributes& attribs,
        const string& parentType,
        CSeq_feat& parent)
//  ----------------------------------------------------------------------------
{
    if (parentType == "gene") {
        if (s_IsCDSType(recType)) {
            xAddQualToFeat(attribs, "gene_id", parent);
        } else {
            xFeatureSetQualifiersGene(attribs, parent);
        }
        return;
    } 

    if (s_IsCDSType(recType)) {
        xAddQualToFeat(attribs, "gene_id", parent);
        xAddQualToFeat(attribs, "transcript_id", parent);
    } else {
        xFeatureSetQualifiersRna(attribs, parent);
    }
}

//  ----------------------------------------------------------------------------
void CGtfReader::xUpdateAnnotParent(
        const CGtfReadRecord& record,
        const string& parentType,
        CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{

    auto recType = record.NormalizedType();
    //
    // If there is no gene feature to go with this CDS then make one. Otherwise,
    //  make sure the existing gene feature includes the location of the CDS.
    //
    auto parentFeatId = mpLocations->GetFeatureIdFor(record, parentType);
    auto pParent = xFindFeatById(parentFeatId);
    if (!pParent) {
        xCreateParent(record, parentType, annot);
        m_ParentChildQualMap[parentFeatId].emplace(recType, record.GtfAttributes());
        mpLocations->AddRecordForId(parentFeatId, record);
        return;
    }

    // else parent already exits
    mpLocations->AddRecordForId(parentFeatId, record);

    // RW-2484
    if (s_IsCDSType(recType) && 
            pParent->GetData().IsRna() && (pParent->GetData().GetRna().GetType() == CRNA_ref::eType_miscRNA)) {
        pParent->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    }

    if (auto parentIt = m_ParentChildQualMap.find(parentFeatId);
        parentIt != m_ParentChildQualMap.end()) {

        auto& childTypeToAttribs = parentIt->second;
        if (auto childIt = childTypeToAttribs.find(recType);
            childIt != childTypeToAttribs.end()) { // Not the first child feature for this parent

            auto& childAttributes = childIt->second;
            // Remove qualifiers assigned by other child features if they don't also appear in the current record
            s_TrimFeatQuals(childAttributes, record.GtfAttributes(), *pParent);
            auto accumulatedAttributes = s_GetIntersection(childAttributes, record.GtfAttributes());
            childAttributes            = accumulatedAttributes;
        } else { // First child feature
            const auto& attribs = record.GtfAttributes();
            childTypeToAttribs.emplace(recType, attribs);
            xAddQualsToParent(recType, attribs, parentType, *pParent);
        }
    }
}

//  ----------------------------------------------------------------------------
void CGtfReader::xUpdateGeneAndMrna(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    xUpdateAnnotParent(gff, "gene", annot);
    xUpdateAnnotParent(gff, "transcript", annot);
}

//  ----------------------------------------------------------------------------
void CGtfReader::xAssignFeatureId(
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
}

//  -----------------------------------------------------------------------------
void CGtfReader::xCreateGene(
    const CGtfReadRecord& record,
    CSeq_annot& annot )
//  -----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(record, "gene");
    if (m_MapIdToFeature.find(featId) != m_MapIdToFeature.end()) {
        return;
    }

    auto pFeature = Ref(new CSeq_feat());
    xFeatureSetDataGene(record, *pFeature);
    xAssignFeatureId("gene", *pFeature);

    const auto& attribs = record.GtfAttributes();
    if (record.NormalizedType() == "cds") {
        xAddQualToFeat(attribs, "gene_id", *pFeature);
    } else {
        xFeatureSetQualifiersGene(attribs, *pFeature);
    }

    (record.Type() == "gene") ?
        mpLocations->AddRecordForId(featId, record) :
        mpLocations->AddStubForId(featId);
    m_MapIdToFeature[featId] = pFeature;
    xAddFeatureToAnnot(pFeature, annot);
}


//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureSetQualifiersGene(
    const CGtfAttributes& attribs,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    set<string> ignoredAttrs = {
        "locus_tag", "transcript_id", "gene"
    };
    xFeatureSetQualifiers(attribs, ignoredAttrs, feature);
}


//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureSetQualifiersRna(
    const CGtfAttributes& attribs,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    set<string> ignoredAttrs = {
        "locus_tag"
    };
    xFeatureSetQualifiers(attribs, ignoredAttrs, feature);
}


//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureSetQualifiersCds(
    const CGtfAttributes& attribs,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    set<string> ignoredAttrs = {
        "locus_tag"
    };
    xFeatureSetQualifiers(attribs, ignoredAttrs, feature);
}


//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureSetQualifiers(
    const CGtfAttributes& attribs,
    const set<string>& ignoredAttrs,
    CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    //
    //  Create GB qualifiers for the record attributes:
    //
    for (const auto& attribute : attribs.Get()) {
        const auto& name = attribute.first;
        if (ignoredAttrs.contains(name)) {
            continue;
        }
        const auto& vals = attribute.second;
        // special case some well-known attributes
        if (xProcessQualifierSpecialCase(name, vals, feature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(name, vals, feature);
    }
}


//  -----------------------------------------------------------------------------
void CGtfReader::xCreateCds(
    const CGtfReadRecord& gff,
    CSeq_annot& annot )
//  -----------------------------------------------------------------------------
{
    auto pFeature = Ref(new CSeq_feat());

    xFeatureSetDataCds(gff, *pFeature);
    xAssignFeatureId("cds", *pFeature);
    xFeatureSetQualifiersCds(gff.GtfAttributes(), *pFeature);

    auto featId = mpLocations->GetFeatureIdFor(gff, "cds");

    xCheckForGeneIdConflict(gff);

    m_MapIdToFeature[featId] = pFeature;
    xAddFeatureToAnnot(pFeature, annot);
}


void CGtfReader::xCheckForGeneIdConflict(
    const CGtfReadRecord& gff)
{
    auto transcriptId = gff.TranscriptId();
    if (!transcriptId.empty()) {
        if (auto geneId = gff.GeneKey(); !geneId.empty()) {
            if (auto it = m_TranscriptToGeneMap.find(transcriptId); it != m_TranscriptToGeneMap.end()) {
                if (it->second != geneId) {
                    string msg = "Gene id '" + geneId + "' for transcript '" + transcriptId +
                    "' conflicts with previously-assigned '" + it->second + "'";
                    CReaderMessage error(
                        eDiag_Error,
                        m_uLineNumber,
                        msg);
                    m_pMessageHandler->Report(error);
                }
            }
            else {
                m_TranscriptToGeneMap.emplace(transcriptId, geneId);
            }
        }
    }
}


static CRNA_ref::EType s_RnaTypeFromRecType(const string& rec_type) 
{
    if (rec_type == "mrna" || s_IsCDSType(rec_type)) {
        return CRNA_ref::eType_mRNA;
    }
    return CRNA_ref::eType_miscRNA;
}


static string s_GetTranscriptBiotype(const CGtfAttributes& attrs)
{
    if (auto biotype = attrs.ValueOf("transcript_biotype");
        ! biotype.empty()) {
        return biotype;
    }

    return attrs.ValueOf("transcript_type");
}


//  -----------------------------------------------------------------------------
void CGtfReader::xCreateRna(
    const CGtfReadRecord& record,
    CSeq_annot&           annot)
//  -----------------------------------------------------------------------------
{
    auto featId = mpLocations->GetFeatureIdFor(record, "transcript");
    if (m_MapIdToFeature.contains(featId)) { // A Seq-feat for this transcript already exists
        return;
    }

    const auto rec_type = record.NormalizedType();
    auto       pFeature = Ref(new CSeq_feat());

    // RW-2484 - first attempt use transcript_biotype or transcript_type
    if (auto biotype = s_GetTranscriptBiotype(record.GtfAttributes());
        biotype.empty() || (! CSoMap::SoTypeToFeature(biotype, *pFeature))) { 
        auto rna_type = s_RnaTypeFromRecType(rec_type);
        pFeature->SetData().SetRna().SetType(rna_type);
    }

    if (pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        if (auto product = record.GtfAttributes().ValueOf("product");
            ! product.empty()) {
            pFeature->SetData().SetRna().SetExt().SetName(product);
        }
    }

    xAssignFeatureId("rna", *pFeature);

    const auto& attribs = record.GtfAttributes();
    if (rec_type == "cds") {
        xAddQualToFeat(attribs, "gene_id", *pFeature);
        xAddQualToFeat(attribs, "transcript_id", *pFeature);
    } else {
        xFeatureSetQualifiersRna(attribs, *pFeature);
    }

    mpLocations->AddStubForId(featId);
    m_MapIdToFeature[featId] = pFeature;

    xAddFeatureToAnnot(pFeature, annot);
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
void CGtfReader::xFeatureSetDataGene(
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
    string locus = attributes.ValueOf("gene");

    if (!locus.empty()) {
        gene.SetLocus(locus);
    }
}


//  ----------------------------------------------------------------------------
void CGtfReader::xFeatureSetDataCds(
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
    set<string> existingVals;
    for (const auto& pQual : feature.GetQual()) {
        if (pQual->GetQual() == key) {
            existingVals.insert(pQual->GetVal());
        }
    }

    for (auto value: values) {
        if (existingVals.find(value) == existingVals.end()) {
            feature.AddQualifier(key, value);
        }
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
        if (pFeature->GetData().IsRna()) {
            auto            parentGeneFeatId = string("gene:") + pFeature->GetNamedQual("gene_id");
            CRef<CSeq_feat> pParentGene;
            if (x_GetFeatureById(parentGeneFeatId, pParentGene)) {
                xSetAncestorXrefs(*pFeature, *pParentGene);
            }
        } else if (pFeature->GetData().IsCdregion()) {
            auto parentRnaFeatId = string("transcript:") + pFeature->GetNamedQual("gene_id") +
                                   "_" + pFeature->GetNamedQual("transcript_id");
            CRef<CSeq_feat> pParentRna;
            if (x_GetFeatureById(parentRnaFeatId, pParentRna)) {
                xSetAncestorXrefs(*pFeature, *pParentRna);
            }
            auto            parentGeneFeatId = string("gene:") + pFeature->GetNamedQual("gene_id");
            CRef<CSeq_feat> pParentGene;
            if (x_GetFeatureById(parentGeneFeatId, pParentGene)) {
                xSetAncestorXrefs(*pFeature, *pParentGene);
            }
        }
    }
    return CGff2Reader::xPostProcessAnnot(annot);
}

END_objects_SCOPE
END_NCBI_SCOPE
