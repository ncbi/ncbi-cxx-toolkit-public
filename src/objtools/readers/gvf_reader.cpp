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
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
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
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/Delta_item.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gvf_reader.hpp>
#include <objtools/error_codes.hpp>

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
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::x_AssignAttributesFromGff(
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
void CGvfReadRecord::xTraceError(
    EDiagSev severity,
    const string& msg)
//  ----------------------------------------------------------------------------
{
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        severity,
        mLineNumber,
        msg) );
    if (!mpMessageListener->PutError(*pErr)) {
        pErr->Throw();
    }
}

//  ----------------------------------------------------------------------------
CGvfReader::CGvfReader(
    unsigned int uFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CGff3Reader( uFlags, name, title )
{
}

//  ----------------------------------------------------------------------------
CGvfReader::~CGvfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGvfReader::x_ParseFeatureGff(
    const string& strLine,
    TAnnots& annots,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    CGvfReadRecord record(m_uLineNumber);
    if ( ! record.AssignFromGff( strLine ) ) {
        return false;
    }

    CRef<CSeq_annot> pAnnot = x_GetAnnotById( annots, record.Id() );
    return x_MergeRecord( record, pAnnot, pMessageListener );
//    return x_UpdateAnnot( record, pAnnot ); 
};

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
    if ( m_CurrentTrackInfo ) {
        pNewAnnot->SetDesc().Set().push_back( m_CurrentTrackInfo );
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
bool CGvfReader::x_MergeRecord(
    const CGvfReadRecord& record,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( ! record.SanityCheck() ) {
        return false;
    }
    CRef< CSeq_feat > pFeature( new CSeq_feat );
    if ( ! x_FeatureSetLocation( record, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetVariation( record, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetExt( record, pFeature, pMessageListener ) ) {
        return false;
    }
    pAnnot->SetData().SetFtable().push_back( pFeature );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::x_FeatureSetLocation(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if (record.SeqStart() < record.SeqStop()) {
        return xFeatureSetLocationInterval(record, pFeature);
    }
    else {// record.SeqStart() == record.SeqStop()
        return xFeatureSetLocationPoint(record, pFeature);
    }
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetLocationInterval(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = CReadUtil::AsSeqId(record.Id(), m_iFlags);
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
    size_t lower, upper;
    if (record.GetAttribute( "Start_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders );
        if ( range_borders.size() != 2 ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                string("CGvfReader::x_FeatureSetLocation: Bad \"Start_range\" attribute") +
                    " (Start_range=" + strRange + ").",
                ILineError::eProblem_QualifierBadValue) );
        pErr->Throw();
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
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                string("CGvfReader::x_FeatureSetLocation: Bad \"Start_range\" attribute") +
                    " (Start_range=" + strRange + ").",
                ILineError::eProblem_QualifierBadValue) );
        pErr->Throw();
        }
    }

    //  deal with fuzzy range indicators / upper end:
    range_borders.clear();
    if (record.GetAttribute( "End_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders );
        if ( range_borders.size() != 2 ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                string("CGvfReader::x_FeatureSetLocation: Bad \"End_range\" attribute") +
                    " (End_range=" + strRange + ").",
                ILineError::eProblem_QualifierBadValue) );
        pErr->Throw();
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
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                string("CGvfReader::x_FeatureSetLocation: Bad \"End_range\" attribute") +
                    " (End_range=" + strRange + ").",
                ILineError::eProblem_QualifierBadValue) );
pErr->Throw();
        }
    }

    pFeature->SetLocation( *pLocation );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xFeatureSetLocationPoint(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = CReadUtil::AsSeqId(record.Id(), m_iFlags);
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
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            string("CGvfReader::x_FeatureSetLocation: Bad range attribute:") +
                " Conflicting fuzz ranges for single point location.",
            ILineError::eProblem_QualifierBadValue) );
    pErr->Throw();
    }
    if (!hasLower  &&  !hasUpper) {
        pFeature->SetLocation(*pLocation);
        return true;
    }
    if (!hasLower) {
        strRangeLower = strRangeUpper;
    }

    list<string> bounds;
    size_t lower, upper;
    NStr::Split( strRangeLower, ",", bounds );
    if (bounds.size() != 2) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            string("CGvfReader::x_FeatureSetLocation: Bad \"XXX_range\" attribute") +
                " (XXX_range=" + strRangeLower + ").",
            ILineError::eProblem_QualifierBadValue) );
pErr->Throw();
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
    catch ( ... ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            string("CGvfReader::x_FeatureSetLocation: Bad \"XXX_range\" attribute") +
                " (XXX_range=" + strRangeLower + ").",
            ILineError::eProblem_QualifierBadValue) );
pErr->Throw();
    }
    pFeature->SetLocation( *pLocation );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::x_FeatureSetVariation(
    const CGvfReadRecord& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef<CVariation_ref> pVariation(new CVariation_ref);
    string strType = record.Type();
    NStr::ToLower( strType );

    if ( strType == "snv" ) {
        if (!xVariationMakeSNV( record, pVariation )) {
            return false;
        }
    }
    else if (strType == "insertion") {
        if (!xVariationMakeInsertions( record, pVariation )) {
            return false;
        }
    }
    else if (strType == "deletion") {
        if (!xVariationMakeDeletions( record, pVariation )) {
            return false;
        }
    }
    else {
        if (!xVariationMakeCNV( record, pVariation )) {
            return false;
        }
    }
    if ( pVariation ) {
        pFeature->SetData().SetVariation( *pVariation );
        return true;
    }
    return false;
}
  
//  ----------------------------------------------------------------------------
bool CGvfReader::x_ParseStructuredCommentGff(
    const string& strLine,
    CRef< CAnnotdesc >& pAnnotDesc )
//  ----------------------------------------------------------------------------
{
    if ( !CGff2Reader::x_ParseStructuredCommentGff( strLine, pAnnotDesc ) ) {
        return false;
    }
    if ( ! m_Pragmas ) {
        m_Pragmas.Reset( new CAnnotdesc );
        m_Pragmas->SetUser().SetType().SetStr( "gvf-import-pragmas" );
    }
    string key, value;
    NStr::SplitInTwo( strLine.substr(2), " ", key, value );
    m_Pragmas->SetUser().AddField( key, value );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationSetInsertions(
    const CGvfReadRecord& record,
    CRef<CVariation_ref> pVariation)
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
    pVariation->SetData().SetSet().SetVariations().push_back(
        pReference );

    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles );
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
            pDelta->SetSeq().SetLiteral().SetLength(allele.size());
            pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
            pDelta->SetAction(CDelta_item::eAction_ins_before);
            pAllele->SetData().SetInstance().SetDelta().push_back(pDelta);
            pAllele->SetData().SetInstance().SetType(CVariation_inst::eType_ins);
            pAllele->SetData().SetInstance().SetObservation( 
                CVariation_inst::eObservation_variant );
            
            pVariation->SetData().SetSet().SetVariations().push_back(
               pAllele );
        }
    }
//    pVariation->SetInsertion();
    return true;
}


//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeCNV(
    const CGvfReadRecord& record,
    CRef<CVariation_ref> pVariation )
//  ----------------------------------------------------------------------------
{
    if (!xVariationSetId(record, pVariation)) {
        return false;
    }
    if (!xVariationSetParent(record, pVariation)) {
        return false;
    }
    if (!xVariationSetName(record, pVariation)) {
        return false;
    }

    string strType = record.Type();
    NStr::ToLower( strType );
    if ( strType == "cnv" || strType == "copy_number_variation" ) {
        pVariation->SetCNV();
        return true;
    }
    if ( strType == "gain" || strType == "copy_number_gain" ) {
        pVariation->SetGain();
        return true;
    }
    if ( strType == "loss" || strType == "copy_number_loss" ) {
        pVariation->SetLoss();
        return pVariation;
    }
    if ( strType == "loss_of_heterozygosity" ) {
        pVariation->SetLoss();
        CRef<CVariation_ref::C_E_Consequence> pConsequence( 
            new CVariation_ref::C_E_Consequence );
        pConsequence->SetLoss_of_heterozygosity();
        pVariation->SetConsequence().push_back( pConsequence );
        return true;
    }
    if ( strType == "complex"  || strType == "complex_substitution"  ||
        strType == "complex_sequence_alteration" ) {
        pVariation->SetComplex();
        return true;
    }
    if ( strType == "inversion" ) {
        //pVariation->SetInversion( feature.GetLocation() );
        return false;
    }
    if ( strType == "unknown" || strType == "other" || 
        strType == "sequence_alteration" ) {
        pVariation->SetUnknown();
        return true;
    }
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Error,
        0,
        string("GVF record error: Unknown type \"") + strType + "\"",
        ILineError::eProblem_QualifierBadValue) );
pErr->Throw();
    return false;
}
  
//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeSNV(
    const CGvfReadRecord& record,
    CRef<CVariation_ref> pVariation)
//  ----------------------------------------------------------------------------
{
    pVariation->SetData().SetSet().SetType( 
        CVariation_ref::C_Data::C_Set::eData_set_type_package );

    if ( ! xVariationSetId( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetParent( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetName( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetProperties( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetSnvs( record, pVariation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeInsertions(
    const CGvfReadRecord& record,
    CRef<CVariation_ref> pVariation )
//  ----------------------------------------------------------------------------
{
    pVariation->SetData().SetSet().SetType( 
        CVariation_ref::C_Data::C_Set::eData_set_type_package );

    if ( ! xVariationSetId( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetParent( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetName( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetProperties( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetInsertions( record, pVariation ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::xVariationMakeDeletions(
    const CGvfReadRecord& record,
    CRef<CVariation_ref> pVariation )
//  ----------------------------------------------------------------------------
{
    pVariation->SetData().SetSet().SetType( 
        CVariation_ref::C_Data::C_Set::eData_set_type_package );

    if ( ! xVariationSetId( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetParent( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetName( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetProperties( record, pVariation ) ) {
        return false;
    }
    if ( ! xVariationSetDeletions( record, pVariation ) ) {
        return false;
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetId(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string id;
    if ( record.GetAttribute( "ID", id ) ) {
        pVariation->SetId().SetDb( record.Source() );
        pVariation->SetId().SetTag().SetStr( id );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetParent(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string id;
    if ( record.GetAttribute( "Parent", id ) ) {
        pVariation->SetParent_id().SetDb( record.Source() );
        pVariation->SetParent_id().SetTag().SetStr( id );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetName(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string name;
    if ( record.GetAttribute( "Name", name ) ) {
        pVariation->SetName( name );
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetProperties(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    typedef map<string, CVariantProperties::EAllele_state>::const_iterator ALLIT;
    
    string strGenotype;
    if ( record.GetAttribute( "Genotype", strGenotype ) ) {
        ALLIT it = s_AlleleStateMap().find( strGenotype );
        if ( it != s_AlleleStateMap().end() ) {
            pVariation->SetVariant_prop().SetAllele_state( it->second ); 
        }
        else {
            pVariation->SetVariant_prop().SetAllele_state(
                CVariantProperties::eAllele_state_other );
        }
    }
    string strValidated;
    if ( record.GetAttribute( "validated", strValidated ) ) {
        if ( strValidated == "1" ) {
            pVariation->SetVariant_prop().SetOther_validation( true );
        }
        if ( strValidated == "0" ) {
            pVariation->SetVariant_prop().SetOther_validation( false );
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetDeletions(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string strReference;
    CRef<CVariation_ref> pReference(new CVariation_ref);
    if (record.GetAttribute("Reference_seq", strReference)) {
        pReference->SetData().SetInstance().SetType(
            CVariation_inst::eType_identity);
        CRef<CDelta_item> pDelta(new CDelta_item);
        pDelta->SetSeq().SetLiteral().SetLength(strReference.size());
        pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(
            strReference);
        pReference->SetData().SetInstance().SetDelta().push_back(pDelta);
        pReference->SetData().SetInstance().SetObservation(
            CVariation_inst::eObservation_asserted);
        pVariation->SetData().SetSet().SetVariations().push_back(
            pReference );
    }
    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles );
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
            
            pVariation->SetData().SetSet().SetVariations().push_back(
               pAllele );
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::xVariationSetSnvs(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string strReference;
    CRef<CVariation_ref> pReference(new CVariation_ref);
    if (record.GetAttribute("Reference_seq", strReference)) {
        pReference->SetData().SetInstance().SetType(
            CVariation_inst::eType_identity);
        CRef<CDelta_item> pDelta(new CDelta_item);
        pDelta->SetSeq().SetLiteral().SetLength(strReference.size());
        pDelta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(
            strReference);
        pReference->SetData().SetInstance().SetDelta().push_back(pDelta);
        pReference->SetData().SetInstance().SetObservation(
            CVariation_inst::eObservation_asserted);
        pVariation->SetData().SetSet().SetVariations().push_back(
            pReference );
    }

    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles );
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
            pVariation->SetData().SetSet().SetVariations().push_back(
               pAllele );
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::x_FeatureSetExt(
    const CGvfReadRecord& record,
    CRef< CSeq_feat > pFeature,
    ILineErrorListener* pMessageListener)
//  ---------------------------------------------------------------------------
{
    string strAttribute;

    CSeq_feat::TExt& ext = pFeature->SetExt();
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
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Warning,
                m_uLineNumber,
                "CGvfReader::x_FeatureSetExt: Funny attribute \"" + cit->first + "\"") );
            if (!pMessageListener->PutError(*pErr)) {
                pErr->Throw();
            }
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
