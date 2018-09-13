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
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool s_AnnotId(
    const CSeq_annot& annot,
    string& strId )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.CanGetId() || annot.GetId().size() != 1 ) {
        // internal error
        return false;
    }
    
    CRef< CAnnot_id > pId = *( annot.GetId().begin() );
    if ( ! pId->IsLocal() ) {
        // internal error
        return false;
    }
    strId = pId->GetLocal().GetStr();
    return true;
}

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
    SeqIdResolver resolver ):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, strAnnotName, strAnnotTitle, resolver )
{
}

//  ----------------------------------------------------------------------------
CGtfReader::~CGtfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xUpdateAnnotFeature(
    const CGff2Record& record,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    const CGtfReadRecord& gff = dynamic_cast<const CGtfReadRecord&>(record);
    string strType = gff.Type();

    using TYPEHANDLER = bool (CGtfReader::*)(const CGtfReadRecord&, CRef< CSeq_annot >);
    using HANDLERMAP = map<string, TYPEHANDLER>;

    HANDLERMAP typeHandlers = {
        {"CDS",         &CGtfReader::x_UpdateAnnotCds},
        {"start_codon", &CGtfReader::x_UpdateAnnotCds},
        {"stop_codon",  &CGtfReader::x_UpdateAnnotCds},
        {"5UTR",        &CGtfReader::x_UpdateAnnotTranscript},
        {"3UTR",        &CGtfReader::x_UpdateAnnotTranscript},
        {"exon",        &CGtfReader::x_UpdateAnnotTranscript},
        {"initial",     &CGtfReader::x_UpdateAnnotTranscript},
        {"internal",    &CGtfReader::x_UpdateAnnotTranscript},
        {"terminal",    &CGtfReader::x_UpdateAnnotTranscript},
        {"single",      &CGtfReader::x_UpdateAnnotTranscript},
    };

    //
    // Handle officially recognized GTF types:
    //
    HANDLERMAP::iterator it = typeHandlers.find(strType);
    if (it != typeHandlers.end()) {
        TYPEHANDLER handler = it->second;
        return (this->*handler)(gff, pAnnot);
    }
 
    //
    //  Every other type is not officially sanctioned GTF, and per spec we are
    //  supposed to ignore it. In the spirit of being lenient on input we may
    //  try to salvage some of it anyway.
    //
    if ( strType == "gene" ) {
        return x_CreateParentGene(gff, pAnnot);
    }
    if (strType == "mRNA") {
        return x_CreateParentMrna(gff, pAnnot);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotCds(
    const CGtfReadRecord& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    bool needXref = false;

    //
    // From the spec, the stop codon has _not_ been accounted for in any of the 
    //  coding regions. Hence, we will treat (pieces of) the stop codon just
    //  like (pieces of) CDS.
    //
    //
    // If there is no gene feature to go with this CDS then make one. Otherwise,
    //  make sure the existing gene feature includes the location of the CDS.
    //
    CRef<CSeq_feat> pGene;
    if (!x_FindParentGene(gff, pGene)) {
        if (!x_CreateParentGene(gff, pAnnot)  ||  !x_FindParentGene(gff, pGene)) {
            return false;
        }
    }
    else {
        if (!x_MergeParentGene(gff, pGene)) {
            return false;
        }
    }

    //
    // If there is no CDS feature with this gene_id|transcript_id then make one.
    //  Otherwise, fix up the location of the existing one.
    //
    CRef<CSeq_feat> pCds;
    if (!x_FindParentCds(gff, pCds)) {
        //
        // Create a brand new CDS feature:
        //
        if (!x_CreateParentCds(gff, pAnnot)  ||  !x_FindParentCds(gff, pCds)) {
            return false;
        }
    }
    else {
        //
        // Update an already existing CDS features:
        //
        if (!x_MergeFeatureLocationMultiInterval(gff, pCds)) {
            return false;
        }
    }
        
    if ( x_CdsIsPartial( gff ) ) {
        CRef<CSeq_feat> pParent;
        if ( x_FindParentMrna( gff, pParent ) ) {
            CSeq_loc& loc = pCds->SetLocation();
            size_t uCdsStart = gff.SeqStart();
            size_t uMrnaStart = pParent->GetLocation().GetStart( eExtreme_Positional );
            if ( uCdsStart == uMrnaStart ) {
                loc.SetPartialStart( true, eExtreme_Positional );
//                cerr << "fuzzed down: " << gff.SeqStart() << "  " << gff.SeqStop() << " vs. " << uMrnaStart << endl;
            }

            size_t uCdsStop =  gff.SeqStop();
            size_t uMrnaStop = pParent->GetLocation().GetStop( eExtreme_Positional );
            if ( uCdsStop == uMrnaStop  && gff.Type() != "stop_codon" ) {
                loc.SetPartialStop( true, eExtreme_Positional );
//                cerr << "fuzzed up  : " << gff.SeqStart() << "  " << gff.SeqStop() << " vs. " << uMrnaStop << endl;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotTranscript(
    const CGtfReadRecord& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    //
    // If there is no gene feature to go with this CDS then make one. Otherwise,
    //  make sure the existing gene feature includes the location of the CDS.
    //
    CRef< CSeq_feat > pGene;
    if ( ! x_FindParentGene( gff, pGene ) ) {
        if ( ! x_CreateParentGene( gff, pAnnot ) ) {
            return false;
        }
    }
    else {
        if ( ! x_MergeParentGene( gff, pGene ) ) {
            return false;
        }
        if (!x_FeatureTrimQualifiers(gff, pGene)) {
            return false;
        }
    }

    //
    // If there is no mRNA feature with this gene_id|transcript_id then make one.
    //  Otherwise, fix up the location of the existing one.
    //
    CRef< CSeq_feat > pMrna;
    if ( ! x_FindParentMrna( gff, pMrna ) ) {
        //
        // Create a brand new CDS feature:
        //
        if ( ! x_CreateParentMrna( gff, pAnnot ) ) {
            return false;
        }
    }
    else {
        //
        // Update an already existing CDS features:
        //
        if ( ! x_MergeFeatureLocationMultiInterval( gff, pMrna ) ) {
            return false;
        }
        if (!x_FeatureTrimQualifiers(gff, pMrna)) {
            return false;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateFeatureId(
    const CGtfReadRecord& record,
    const string& prefix,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    static int seqNum(1);

    string strFeatureId = prefix;
    if (strFeatureId.empty()) {
        strFeatureId = "id";
    }
    strFeatureId += "_";
    strFeatureId += NStr::IntToString(seqNum++);
    pFeature->SetId().SetLocal().SetStr( strFeatureId );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateFeatureLocation(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pId = mSeqIdResolve(
        record.Id(), m_iFlags & fAllIdsAsLocal, true);

    CSeq_interval& location = pFeature->SetLocation().SetInt();
    location.SetId( *pId );
    location.SetFrom( record.SeqStart() );
    if (record.Type() != "mRNA") {
        location.SetTo(record.SeqStop());
    }
    else {
        // place holder
        //  actual location will be computed from the exons and CDSs living on 
        //  this feature.
        location.SetTo(record.SeqStart());
    }
    if ( record.IsSetStrand() ) {
        location.SetStrand( record.Strand() );
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateGeneXrefs(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pParent;
    if ( ! x_FindParentGene( record, pParent ) ) {
        return true;
    }
    
    CRef< CSeqFeatXref > pXrefToParent( new CSeqFeatXref );
    pXrefToParent->SetId( pParent->SetId() );    
    pFeature->SetXref().push_back( pXrefToParent );

    if (m_iFlags & CGtfReader::fGenerateChildXrefs) {
        CRef< CSeqFeatXref > pXrefToChild( new CSeqFeatXref );
        pXrefToChild->SetId( pFeature->SetId() );
        pParent->SetXref().push_back( pXrefToChild );
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateMrnaXrefs(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pParent;
    if ( ! x_FindParentMrna( record, pParent ) ) {
        return true;
    }
    
    CRef< CSeqFeatXref > pXrefToChild( new CSeqFeatXref );
    pXrefToChild->SetId( pFeature->SetId() );
    pParent->SetXref().push_back( pXrefToChild );

    CRef< CSeqFeatXref > pXrefToParent( new CSeqFeatXref );
    pXrefToParent->SetId( pParent->SetId() );    
    pFeature->SetXref().push_back( pXrefToParent );

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateCdsXrefs(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pParent;
    if ( ! x_FindParentCds( record, pParent ) ) {
        return true;
    }
    
    CRef< CSeqFeatXref > pXrefToParent( new CSeqFeatXref );
    pXrefToParent->SetId( pParent->SetId() );    
    pFeature->SetXref().push_back( pXrefToParent );

    if (m_iFlags & CGtfReader::fGenerateChildXrefs) {
        CRef< CSeqFeatXref > pXrefToChild( new CSeqFeatXref );
        pXrefToChild->SetId( pFeature->SetId() );
        pParent->SetXref().push_back( pXrefToChild );
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeFeatureLocationSingleInterval(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    const CSeq_interval& gene_int = pFeature->GetLocation().GetInt();
    if ( gene_int.GetFrom() > record.SeqStart() -1 ) {
        pFeature->SetLocation().SetInt().SetFrom( record.SeqStart() );
    }
    if ( gene_int.GetTo() < record.SeqStop() - 1 ) {
        pFeature->SetLocation().SetInt().SetTo( record.SeqStop() );
    }
    if (record.Type() == "CDS"  &&  pFeature->GetData().IsCdregion()) {
        return x_FeatureTrimQualifiers(record, pFeature);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeFeatureLocationMultiInterval(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pId = mSeqIdResolve(
        record.Id(), m_iFlags & fAllIdsAsLocal, true);

    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId( *pId );
    pLocation->SetInt().SetFrom( record.SeqStart() );
    pLocation->SetInt().SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }
    pLocation = pLocation->Add( 
        pFeature->SetLocation(), CSeq_loc::fSortAndMerge_All, 0 );
    pFeature->SetLocation( *pLocation );
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentGene(
    const CGtfReadRecord& gff,
    CRef< CSeq_annot > pAnnot )
//  -----------------------------------------------------------------------------
{
    //
    // Create a single gene feature:
    //
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if ( ! x_FeatureSetDataGene( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureId( gff, "gene", pFeature ) ) {
        return false;
    }
    if ( ! xFeatureSetQualifiersGene( gff, pFeature ) ) {
        return false;
    }
    m_GeneMap[gff.GeneKey()] = pFeature;

    xAddFeatureToAnnot( pFeature, pAnnot );
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeParentGene(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if (!x_MergeFeatureLocationSingleInterval( record, pFeature )) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersGene(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
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
        if (xProcessQualifierSpecialCase(it->first, it->second, pFeature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, pFeature);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersRna(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
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
        if (xProcessQualifierSpecialCase(it->first, it->second, pFeature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, pFeature);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xFeatureSetQualifiersCds(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
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
        if (xProcessQualifierSpecialCase(it->first, it->second, pFeature)) {
            continue;
        }

        // turn everything else into a qualifier
        xFeatureAddQualifiers(it->first, it->second, pFeature);
    } 
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentCds(
    const CGtfReadRecord& gff,
    CRef< CSeq_annot > pAnnot )
//  -----------------------------------------------------------------------------
{
    //
    // Create a single cds feature.
	// This creation may either be triggered by an actual CDS feature found in the
	//	gtf, or by a feature that would imply a CDS feature (such as a start codon 
	//	or a stop codon). The latter is necessary because nothing the the gtf 
	//	standard stipulates that gtf features have to be arranged in any particular
	//	order.
    //
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    string strType = gff.Type();
    if ( strType != "CDS"  &&  strType != "start_codon"  &&  strType != "stop_codon" ) {
        return false;
    }

    m_CdsMap[gff.FeatureKey()] = pFeature;

    if ( ! x_FeatureSetDataCDS( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureId( gff, "cds", pFeature ) ) {
        return false;
    }
    if ( ! x_CreateGeneXrefs( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateMrnaXrefs( gff, pFeature ) ) {
        return false;
    }
    if ( ! xFeatureSetQualifiersCds( gff, pFeature ) ) {
        return false;
    }

    return xAddFeatureToAnnot( pFeature, pAnnot );
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentMrna(
    const CGtfReadRecord& gff,
    CRef< CSeq_annot > pAnnot )
//  -----------------------------------------------------------------------------
{
    //
    // Create a single cds feature:
    //
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if ( ! x_FeatureSetDataMRNA( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureId( gff, "mrna", pFeature ) ) {
        return false;
    }
    if ( ! x_CreateGeneXrefs( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateCdsXrefs( gff, pFeature ) ) {
        return false;
    }
    if ( ! xFeatureSetQualifiersRna( gff, pFeature ) ) {
        return false;
    }

    m_MrnaMap[gff.FeatureKey()] = pFeature;

    return xAddFeatureToAnnot( pFeature, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentGene(
    const CGtfReadRecord& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator gene_it = m_GeneMap.find(gff.GeneKey());
    if ( gene_it == m_GeneMap.end() ) {
        return false;
    }
    pFeature = gene_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentCds(
    const CGtfReadRecord& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator cds_it = m_CdsMap.find(gff.FeatureKey());
    if ( cds_it == m_CdsMap.end() ) {
        return false;
    }
    pFeature = cds_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentMrna(
    const CGtfReadRecord& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator rna_it = m_MrnaMap.find(gff.FeatureKey());
    if ( rna_it == m_MrnaMap.end() ) {
        return false;
    }
    pFeature = rna_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FeatureSetDataGene(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CGene_ref& gene = pFeature->SetData().SetGene();

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
bool CGtfReader::x_FeatureSetDataMRNA(
    const CGtfReadRecord& record,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if ( !x_FeatureSetDataRna( record, pFeature, CSeqFeatData::eSubtype_mRNA)) {
        return false;
    }    
    CRNA_ref& rna = pFeature->SetData().SetRna();

    string product = record.GtfAttributes().ValueOf("product");
    if (!product.empty()) {
        rna.SetExt().SetName(product);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FeatureSetDataRna(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature,
    CSeqFeatData::ESubtype subType)
//  ----------------------------------------------------------------------------
{
    CRNA_ref& rnaRef = pFeature->SetData().SetRna();
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
bool CGtfReader::x_FeatureSetDataCDS(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CCdregion& cdr = pFeature->SetData().SetCdregion();
    const auto& attributes = record.GtfAttributes();

    string proteinId = attributes.ValueOf("protein_id");
    if (!proteinId.empty()) {
        CRef<CSeq_id> pId = mSeqIdResolve(proteinId, m_iFlags, true);
        if (pId->IsGenbank()) {
            pFeature->SetProduct().SetWhole(*pId);
        }
    }
    string ribosomalSlippage = attributes.ValueOf("ribosomal_slippage");
    if (!ribosomalSlippage.empty()) {
        pFeature->SetExcept( true );
        pFeature->SetExcept_text("ribosomal slippage");
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
bool CGtfReader::x_FeatureTrimQualifiers(
    const CGtfReadRecord& record,
    CRef< CSeq_feat > pFeature )
    //  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual TQual;
    //task:
    // for each attribute of the new piece check if we already got a feature 
    //  qualifier
    // if so, and with the same value, then the qualifier is allowed to live
    // otherwise it is subfeature specific and hence removed from the feature
    TQual& quals = pFeature->SetQual();
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
bool CGtfReader::x_CdsIsPartial(
    const CGtfReadRecord& record )
//  ----------------------------------------------------------------------------
{
    if (record.GtfAttributes().HasValue("partial")) {
        return true;
    }
    CRef< CSeq_feat > mRna;
    if (!x_FindParentMrna(record, mRna)) {
        return false;
    }
    return (mRna->IsSetPartial()  &&  mRna->GetPartial());
}

//  ----------------------------------------------------------------------------
bool CGtfReader::xProcessQualifierSpecialCase(
    const string& key,
    const vector<string>& values,
    CRef< CSeq_feat > pFeature )
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
        pFeature->SetComment(NStr::Join(values, ";"));
        return true;
    }
    if ( 0 == NStr::CompareNocase(key, "dbxref") || 
        0 == NStr::CompareNocase(key, "db_xref")) 
    {
        for (auto value: values) {
            vector< string > tags;
            NStr::Split(value, ";", tags );
            for (auto it = tags.begin(); it != tags.end(); ++it ) {
                pFeature->SetDbxref().push_back(x_ParseDbtag(*it));
            }
        }
        return true;
    }

    if ( 0 == NStr::CompareNocase(key, "pseudo")) {
        pFeature->SetPseudo( true );
        return true;
    }
    if ( 0 == NStr::CompareNocase(key, "partial")) {
        pFeature->SetPartial( true );
        return true;
    }
    return false;
}  


END_objects_SCOPE
END_NCBI_SCOPE
