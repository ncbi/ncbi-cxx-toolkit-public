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
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gvf_reader.hpp>
//#include <objtools/readers/gvf_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const map<string, CVariantProperties::EAllele_state>&
s_AlleleStateMap()
//  ----------------------------------------------------------------------------
{
    static map<string, CVariantProperties::EAllele_state> m;
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
bool CGvfReadRecord::SanityCheck() const
//  ----------------------------------------------------------------------------
{
    return true;
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
    TAnnots& annots )
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    CGvfReadRecord record;
    if ( ! record.AssignFromGff( strLine ) ) {
        return false;
    }

    CRef<CSeq_annot> pAnnot = x_GetAnnotById( annots, record.Id() );
    return x_MergeRecord( record, pAnnot );
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
    annots.push_back( pNewAnnot );

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
        pNewAnnot->AddName(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pNewAnnot->SetTitle(m_AnnotTitle);
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
    CRef< CSeq_annot > pAnnot )
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
    if ( ! x_FeatureSetExt( record, pFeature ) ) {
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
    CRef< CSeq_id > pId;

    const string& id_str = record.Id();
    if (m_uFlags & fAllIdsAsLocal) {
        pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
    } else {
        bool is_numeric =
            id_str.find_first_not_of("0123456789") == string::npos;

        if (is_numeric  &&  !!(m_uFlags & fNumericIdsAsLocal)) {
            pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
        }
        else {
            try {
                pId.Reset( new CSeq_id(id_str));
                if (!pId || (pId->IsGi() && pId->GetGi() < 500) ) {
                    pId = new CSeq_id(CSeq_id::e_Local, id_str);
                }
            }
            catch (CException&) {
                pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
            }
        }
    }

    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId( *pId );
    pLocation->SetInt().SetFrom( record.SeqStart() );
    pLocation->SetInt().SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }

    //  deal with fuzzy range indicators / lower end:
    string strRange;
    list<string> range_borders;
    size_t lower, upper;
    if ( record.GetAttribute( "Start_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders );
        if ( range_borders.size() != 2 ) {
            cerr << "CGvfReader::x_FeatureSetLocation: "
                 << "Bad \"Start_range\" attribute"
                 << endl;
            return false; //overly harsh?
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
        catch ( ... ) {
            cerr << "CGvfReader::x_FeatureSetLocation: "
                 << "Bad \"Start_range\" attribute"
                 << endl;
            return false; //overly harsh?
        }
    }

    //  deal with fuzzy range indicators / upper end:
    range_borders.clear();
    if ( record.GetAttribute( "End_range", strRange ) )
    {
        NStr::Split( strRange, ",", range_borders );
        if ( range_borders.size() != 2 ) {
            cerr << "CGvfReader::x_FeatureSetLocation: "
                 << "Bad \"End_range\" attribute"
                 << endl;
            return false; //overly harsh?
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
        catch ( ... ) {
            cerr << "CGvfReader::x_FeatureSetLocation: "
                 << "Bad \"End_range\" attribute"
                 << endl;
            return false; //overly harsh?
        }
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
    CRef<CVariation_ref> pVariation;
    string strType = record.Type();
    NStr::ToLower( strType );

    if ( strType == "snv" ) {
        pVariation = x_VariationSNV( record, *pFeature );
    }
    else {
        pVariation = x_VariationCNV( record, *pFeature );
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
CRef<CVariation_ref> CGvfReader::x_VariationCNV(
    const CGvfReadRecord& record,
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    CRef<CVariation_ref> pVariation( new CVariation_ref );
    string id;
    if ( ! x_VariationSetId( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetParent( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetName( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }

    string strType = record.Type();
    NStr::ToLower( strType );
    if ( strType == "cnv" || strType == "copy_number_variation" ) {
        pVariation->SetCNV();
        return pVariation;
    }
    if ( strType == "gain" || strType == "copy_number_gain" ) {
        pVariation->SetGain();
        return pVariation;
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
        return pVariation;
    }
    if ( strType == "insertion" ) {
        pVariation->SetInsertion();
        return pVariation;
    }
    if ( strType == "complex"  || strType == "complex_substitution"  ||
        strType == "complex_sequence_alteration" ) {
        pVariation->SetComplex();
        return pVariation;
    }
    if ( strType == "inversion" ) {
        pVariation->SetInversion( feature.GetLocation() );
        return pVariation;
    }
//    if ( strType == "unknown" || strType == "other" || 
//        strType == "sequence_alteration" ) {
//        pVariation->SetUnknown();
//        return pVariation;
//    }
    pVariation->SetUnknown();
    
    return pVariation;
}
  
//  ----------------------------------------------------------------------------
CRef<CVariation_ref> CGvfReader::x_VariationSNV(
    const CGvfReadRecord& record,
    const CSeq_feat& )
//  ----------------------------------------------------------------------------
{
    CRef<CVariation_ref> pVariation( new CVariation_ref );
    pVariation->SetData().SetSet().SetType( 
        CVariation_ref::C_Data::C_Set::eData_set_type_package );

    if ( ! x_VariationSetId( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetParent( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetName( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetProperties( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    if ( ! x_VariationSetAlleleInstances( record, pVariation ) ) {
        return CRef<CVariation_ref>();
    }
    return pVariation;
}

//  ---------------------------------------------------------------------------
bool CGvfReader::x_VariationSetId(
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
bool CGvfReader::x_VariationSetParent(
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
bool CGvfReader::x_VariationSetName(
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
bool CGvfReader::x_VariationSetProperties(
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
bool CGvfReader::x_VariationSetAlleleInstances(
    const CGvfReadRecord& record,
    CRef< CVariation_ref > pVariation )
//  ---------------------------------------------------------------------------
{
    string strAlleles;
    if ( record.GetAttribute( "Variant_seq", strAlleles ) ) {
        list<string> alleles;
        NStr::Split( strAlleles, ",", alleles );
        for ( list<string>::const_iterator cit = alleles.begin(); 
            cit != alleles.end(); ++cit )
        {
            vector<string> replaces;
            replaces.push_back( *cit );
            CRef<CVariation_ref> pAllele( new CVariation_ref );
            pAllele->SetSNV( replaces, CVariation_ref::eSeqType_na );
            string strReference;
            if ( record.GetAttribute( "Reference_seq", strReference ) 
                && *cit == strReference ) 
            {
                pAllele->SetData().SetInstance().SetObservation( 
                    CVariation_inst::eObservation_reference );
            }
            else {
                pAllele->SetData().SetInstance().SetObservation( 
                    CVariation_inst::eObservation_variant );
            }
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
    CRef< CSeq_feat > pFeature )
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
            cerr << "CGvfReader::x_FeatureSetExt: Funny attribute \""
                 << cit->first
                 << "\"" << endl;
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
