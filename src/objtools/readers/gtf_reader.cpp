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
bool CGtfReadRecord::x_AssignAttributesFromGff(
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    vector< string > attributes;
    x_SplitGffAttributes(strRawAttributes, attributes);

	for ( size_t u=0; u < attributes.size(); ++u ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( attributes[u], "=", strKey, strValue ) ) {
            if ( ! NStr::SplitInTwo( attributes[u], " ", strKey, strValue ) ) {
                return false;
            }
        }
        strKey = xNormalizedAttributeKey( strKey );
        strValue = xNormalizedAttributeValue( strValue );
		if ( strKey.empty() && strValue.empty() ) {
            // Probably due to trailing "; ". Sequence Ontology generates such
            // things. 
            continue;
        }
        if ( NStr::StartsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 1, string::npos );
        }
        if ( NStr::EndsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 0, strValue.length() - 1 );
        }
        m_Attributes[ strKey ] = strValue;        
    }
    return true;
}

//  ----------------------------------------------------------------------------
string s_GeneKey(
    const CGff2Record& gff )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! gff.GetAttribute( "gene_id", strGeneId ) ) {
        cerr << "Unexpected: GTF feature without a gene_id." << endl;
        return "gene_id";
    }
    return strGeneId;
}

//  ----------------------------------------------------------------------------
string s_FeatureKey(
    const CGff2Record& gff )
//  ----------------------------------------------------------------------------
{
    string strGeneId = s_GeneKey( gff );
    if ( gff.Type() == "gene" ) {
        return strGeneId;
    }

    string strTranscriptId;
    if ( ! gff.GetAttribute( "transcript_id", strTranscriptId ) ) {
        cerr << "Unexpected: GTF feature without a transcript_id." << endl;
        strTranscriptId = "transcript_id";
    }

    return strGeneId + "|" + strTranscriptId;
}

//  ----------------------------------------------------------------------------
CGtfReader::CGtfReader( 
    unsigned int uFlags,
    const string& strAnnotName,
    const string& strAnnotTitle ):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, strAnnotName, strAnnotTitle )
{
}

//  ----------------------------------------------------------------------------
CGtfReader::~CGtfReader()
//  ----------------------------------------------------------------------------
{
}

//  ---------------------------------------------------------------------------                       
void
CGtfReader::ReadSeqAnnots(
    TAnnots& annots,
    ILineReader& lr,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);

    string line;
    while (xGetLine(lr, line)) {
        if (IsCanceled()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Info,
                0,
                "Reader stopped by user.",
                ILineError::eProblem_ProgressInfo));
            ProcessError(*pErr, pEC);
            annots.clear();
            return;
        }
        xReportProgress(pEC);
        try {
            if (x_ParseBrowserLineGff(line, m_CurrentBrowserInfo)) {
                continue;
            }
            if (x_ParseTrackLineGff(line, m_CurrentTrackInfo)) {
                continue;
            }
            if (x_ParseFeatureGff(line, annots, pEC)) {
                continue;
            }
        }
        catch(CObjReaderLineException& err) {
            err.SetLineNumber(m_uLineNumber);
        }
    }
}

//  --------------------------------------------------------------------------- 
void
CGtfReader::ReadSeqAnnots(
    TAnnots& annots,
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotFeature(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    //
    // Handle officially recognized GTF types:
    //
    string strType = gff.Type();
    if ( strType == "CDS" ) {
        //
        // Observations:
        // Location does not include the stop codon hence must be fixed up once
        //  the stop codon is seen.
        //
        return x_UpdateAnnotCds( gff, pAnnot );
    }
    if ( strType == "start_codon" ) {
        //
        // Observation:
        // Comes in up to three pieces (depending on splicing).
        // Location _is_ included in CDS.
        //
        return x_UpdateAnnotStartCodon( gff, pAnnot );
    }
    if ( strType == "stop_codon" ) {
        //
        // Observation:
        // Comes in up to three pieces (depending on splicing).
        // Location not included in CDS hence must be used to fix up location of
        //  the coding region.
        //
        return x_UpdateAnnotStopCodon( gff, pAnnot );
    }
    if ( strType == "5UTR" ) {
        return x_UpdateAnnot5utr( gff, pAnnot );
    }
    if ( strType == "3UTR" ) {
        return x_UpdateAnnot3utr( gff, pAnnot );
    }
    if ( strType == "inter" ) {
        return x_UpdateAnnotInter( gff, pAnnot );
    }
    if ( strType == "inter_CNS" ) {
        return x_UpdateAnnotInterCns( gff, pAnnot );
    }
    if ( strType == "intron_CNS" ) {
        return x_UpdateAnnotIntronCns( gff, pAnnot );
    }
    if ( strType == "exon" ) {
        return x_UpdateAnnotExon( gff, pAnnot );
    }

    //
    //  Every other type is not officially sanctioned GTF, and per spec we are
    //  supposed to ignore it. In the spirit of being lenient on input we may
    //  try to salvage some of it anyway.
    //
    if ( strType == "gene" ) {
        //
        // Not an official GTF feature type but seen frequently. Hence we give
        //  it some recognition.
        //
        if ( ! x_CreateParentGene( gff, pAnnot ) ) {
            return false;
        }
    }
    return x_UpdateAnnotMiscFeature( gff, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotCds(
    const CGff2Record& gff,
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
    }
    
    //
    // If there is no CDS feature with this gene_id|transcript_id then make one.
    //  Otherwise, fix up the location of the existing one.
    //
    CRef< CSeq_feat > pCds;
    if ( ! x_FindParentCds( gff, pCds ) ) {
        //
        // Create a brand new CDS feature:
        //
        if ( ! x_CreateParentCds( gff, pAnnot ) ) {
            return false;
        }
        x_FindParentCds( gff, pCds );
    }
    else {
        //
        // Update an already existing CDS features:
        //
        if ( ! x_MergeFeatureLocationMultiInterval( gff, pCds ) ) {
            return false;
        }
        if (!x_FeatureTrimQualifiers(gff, pCds)) {
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
bool CGtfReader::x_UpdateAnnotStartCodon(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    //  If this belongs to a partial feature then that feature has been given a
    //  5' fuzz. Now that we see an actual start codon it is time to remove that
    //  fuzz.
    //
    CRef< CSeq_feat > pCds;
    if ( ! x_FindParentCds( gff, pCds ) ) {
        if ( ! x_CreateParentCds( gff, pAnnot ) || ! x_FindParentCds( gff, pCds ) ) {
            return false;
        }
    }
    if ( ! pCds->IsSetPartial() || ! pCds->GetPartial() ) {
        return true;
    }
    CSeq_loc& loc = pCds->SetLocation();
    if ( loc.IsPartialStart( eExtreme_Positional ) ) {
        loc.SetPartialStart( false, eExtreme_Positional );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotStopCodon(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
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
        if (!x_CreateParentGene(gff, pAnnot)) {
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
        if (!x_CreateParentCds(gff, pAnnot)) {
            return false;
        }
        x_FindParentCds(gff, pCds);
    }
    else {
        //
        // Update an already existing CDS features:
        //
        if (!x_MergeFeatureLocationMultiInterval(gff, pCds)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnot5utr(
    const CGff2Record& gff,
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
        //if (!x_FeatureTrimQualifiers(gff, pMrna)) {
        //    return false;
        //}
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnot3utr(
    const CGff2Record& gff,
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
        //if (!x_FeatureTrimQualifiers(gff, pMrna)) {
        //    return false;
        //}
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotInter(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotInterCns(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotIntronCns(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotExon(
    const CGff2Record& gff,
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
        if (!x_MergeParentGene(gff, pGene)) {
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
        if (!x_MergeFeatureLocationMultiInterval(gff, pMrna)) {
            return false;
        }
        if (!x_FeatureTrimQualifiers(gff, pMrna)) {
            return false;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotMiscFeature(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateFeatureId(
    const CGff2Record& record,
    const string& prefix,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    static int seqNum(1);

    string strFeatureId = prefix;
    if (strFeatureId.empty()) {
        strFeatureId = "id";
    }
    strFeatureId += "|";
    strFeatureId += NStr::IntToString(seqNum++);
    pFeature->SetId().SetLocal().SetStr( strFeatureId );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateFeatureLocation(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(
        record.Id(), m_iFlags & fAllIdsAsLocal);

    CSeq_interval& location = pFeature->SetLocation().SetInt();
    location.SetId( *pId );
    location.SetFrom( record.SeqStart() );
    location.SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        location.SetStrand( record.Strand() );
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateGeneXrefs(
    const CGff2Record& record,
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
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pParent;
    if ( ! x_FindParentMrna( record, pParent ) ) {
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
    const CGff2Record& record,
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
    if (record.Type() == "exon"  &&  pFeature->GetData().IsGene()) {
        return x_FeatureTrimQualifiers(record, pFeature);
    }
    if (record.Type() == "CDS"  &&  pFeature->GetData().IsCdregion()) {
        return x_FeatureTrimQualifiers(record, pFeature);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeFeatureLocationMultiInterval(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(
        record.Id(), m_iFlags & fAllIdsAsLocal);

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
    const CGff2Record& gff,
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
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }
    m_GeneMap[ s_GeneKey( gff ) ] = pFeature;

    x_AddFeatureToAnnot( pFeature, pAnnot );
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeParentGene(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if (!x_MergeFeatureLocationSingleInterval( record, pFeature )) {
        return false;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentCds(
    const CGff2Record& gff,
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
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }

    m_CdsMap[ s_FeatureKey( gff ) ] = pFeature;

    return x_AddFeatureToAnnot( pFeature, pAnnot );
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentMrna(
    const CGff2Record& gff,
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
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }

    m_MrnaMap[ s_FeatureKey( gff ) ] = pFeature;

    return x_AddFeatureToAnnot( pFeature, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentGene(
    const CGff2Record& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator gene_it = m_GeneMap.find( s_GeneKey( gff ) );
    if ( gene_it == m_GeneMap.end() ) {
        return false;
    }
    pFeature = gene_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentCds(
    const CGff2Record& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator cds_it = m_CdsMap.find( s_FeatureKey( gff ) );
    if ( cds_it == m_CdsMap.end() ) {
        return false;
    }
    pFeature = cds_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentMrna(
    const CGff2Record& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    TIdToFeature::iterator rna_it = m_MrnaMap.find( s_FeatureKey( gff ) );
    if ( rna_it == m_MrnaMap.end() ) {
        return false;
    }
    pFeature = rna_it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FeatureSetDataGene(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( ! CGff2Reader::x_FeatureSetDataGene( record, pFeature ) ) {
        return false;
    }

    CGene_ref& gene = pFeature->SetData().SetGene();

    string strValue;
    if ( record.GetAttribute( "gene_synonym", strValue ) ) {
        gene.SetSyn().push_back( strValue );
    }
    if ( record.GetAttribute( "gene_id", strValue ) ) {
        gene.SetSyn().push_front( strValue );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FeatureSetDataMRNA(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if ( ! CGff2Reader::x_FeatureSetDataRna( 
            record, pFeature, CSeqFeatData::eSubtype_mRNA)) {
        return false;
    }
    
    CRNA_ref& rna = pFeature->SetData().SetRna();

    string strValue;
    if (record.GetAttribute("product", strValue)) {
        rna.SetExt().SetName(strValue);
    }
    if (record.GetAttribute("transcript_id", strValue)) {
        pFeature->SetProduct().SetWhole(
            *CReadUtil::AsSeqId(strValue, m_iFlags & fAllIdsAsLocal));
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FeatureSetDataCDS(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( ! CGff2Reader::x_FeatureSetDataCDS( record, pFeature ) ) {
        return false;
    }

    CCdregion& cdr = pFeature->SetData().SetCdregion();

    string strValue;
    if ( record.GetAttribute( "protein_id", strValue ) ) {
        pFeature->SetProduct().SetWhole(
            *CReadUtil::AsSeqId(strValue,m_iFlags));
    }
    if ( record.GetAttribute( "ribosomal_slippage", strValue ) ) {
        pFeature->SetExcept( true );
        pFeature->SetExcept_text( "ribosomal slippage" );
    }
    if ( record.GetAttribute( "product", strValue ) ) {
        CRef< CSeqFeatXref > pXref( new CSeqFeatXref );
        pXref->SetData().SetProt().SetName().push_back( strValue );
        pFeature->SetXref().push_back( pXref );
    }
    if ( record.GetAttribute( "transl_table", strValue ) ) {
        CRef< CGenetic_code::C_E > pGc( new CGenetic_code::C_E );
        pGc->SetId( NStr::StringToUInt( strValue ) );
        cdr.SetCode().Set().push_back( pGc );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_SkipAttribute(
    const CGff2Record& record,
    const string& strKey ) const
//  ----------------------------------------------------------------------------
{
    if ( strKey == "exon_number" ) {
        return true;
    }

    if ( record.Type() == "CDS" ) {
        if ( strKey == "protein_id" ) {
            return true;
        }
        if ( strKey == "ribosomal_slippage" ) {
            return true;
        }
        if ( strKey == "product" ) {
            return true;
        }
        if ( strKey == "transl_table" ) {
            return true;
        }
        if ( strKey == "gene_id" ) {
            return true;
        }
        if ( strKey == "transcript_id" ) { // ! implied by parent mRNA
            return true;
        }
    }

    if ( record.Type() == "exon" ) {
        if ( strKey == "product" ) {
            return true;
        }
        if ( strKey == "gene_id" ) {
            return true;
        }
        if ( strKey == "transcript_id" ) {
            return true;
        }
    }

    if ( record.Type() == "gene" ) {
        if ( strKey == "gene_synonym" ) {
            return true;
        }
        if ( strKey == "gene_id" ) {
            return true;
        }
    }

    return false;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CdsIsPartial(
    const CGff2Record& record )
//  ----------------------------------------------------------------------------
{
    string strPartial;
//    if ( record.Type() != "CDS" ) {
//        return false;
//    }
    if ( record.GetAttribute( "partial", strPartial ) ) {
        return true;
    }
    CRef< CSeq_feat > mRna;
    if ( ! x_FindParentMrna( record, mRna ) ) {
        return false;
    }
    return ( mRna->IsSetPartial() && mRna->GetPartial() );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_ProcessQualifierSpecialCase(
    CGff2Record::TAttrCit it,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CGb_qual> pQual(0);

    if (0 == NStr::CompareNocase(it->first, "exon_id")) {
        return true;
    }
    if (0 == NStr::CompareNocase(it->first, "exon_number")) {
        return true;
    }
    if (0 == NStr::CompareNocase(it->first, "transcript_id")  &&  
            pFeature->GetData().IsGene()) {
        return true;
    }
    if ( 0 == NStr::CompareNocase( it->first, "note" ) ) {
        pFeature->SetComment( it->second );
        return true;
    }
    if ( 0 == NStr::CompareNocase( it->first, "dbxref" ) || 
        0 == NStr::CompareNocase( it->first, "db_xref" ) ) 
    {
        vector< string > tags;
        NStr::Tokenize( it->second, ";", tags );
        for ( vector<string>::iterator it = tags.begin(); 
            it != tags.end(); ++it ) {
            pFeature->SetDbxref().push_back( x_ParseDbtag( *it ) );
        }
        return true;
    }

    if ( 0 == NStr::CompareNocase( it->first, "pseudo" ) ) {
        pFeature->SetPseudo( true );
        return true;
    }
    if ( 0 == NStr::CompareNocase( it->first, "partial" ) ) {
        pFeature->SetPartial( true );
        return true;
    }

    return false;
}  


END_objects_SCOPE
END_NCBI_SCOPE
