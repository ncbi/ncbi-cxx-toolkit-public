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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write vcf file
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACaa.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/writers/feature_context.hpp>
#include <objtools/writers/vcf_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> s_GetVcfMetaInfo(const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsSetDesc()  || ! annot.GetDesc().IsSet() ) {
        return CConstRef<CUser_object>();
    }
    const list< CRef< CAnnotdesc > > descrs = annot.GetDesc().Get();
    list< CRef< CAnnotdesc > >::const_iterator cit = descrs.begin();
    CConstRef<CAnnotdesc> pDescMeta;
    while ( cit != descrs.end() ) {
        CConstRef<CAnnotdesc> pDesc = *cit;
        cit++;
        if ( ! pDesc->IsUser() ) {
            continue;
        }
        if ( ! pDesc->GetUser().IsSetType() ) {
            continue;
        }
        if ( ! pDesc->GetUser().GetType().IsStr() ) {
            continue;
        }
        if ( pDesc->GetUser().GetType().GetStr() == "vcf-meta-info" ) {
            pDescMeta = pDesc;
            break;
        }
    }
    if ( ! pDescMeta ) {
        return CConstRef<CUser_object>();
    }
    return CConstRef<CUser_object>( &pDescMeta->GetUser() );
}

//  ----------------------------------------------------------------------------
CVcfWriter::CVcfWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags ),
    m_Scope( scope )
{
};

//  ----------------------------------------------------------------------------
CVcfWriter::~CVcfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CVcfWriter::WriteAnnot( 
    const CSeq_annot& annot,
    const string&,
    const string& )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteInit( annot ) ) {
        return false;
    }
    if ( ! x_WriteMeta( annot ) ) {
        return false;
    }
    if ( ! x_WriteHeader( annot ) ) {
        return false;
    }
    if ( ! x_WriteData( annot ) ) {
        return false;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteInit(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> pVcfMetaInfo = s_GetVcfMetaInfo( annot );
    if ( !pVcfMetaInfo  ||  !pVcfMetaInfo->HasField("genotype-headers") ) {
        return true;
    }
    m_GenotypeHeaders = 
        pVcfMetaInfo->GetField("genotype-headers").GetData().GetStrs();
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteMeta(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> pVcfMetaInfo = s_GetVcfMetaInfo( annot );
    if ( !pVcfMetaInfo ) {
        return x_WriteMetaCreateNew( annot );
    }
    const CAnnotdesc::TUser& meta = *pVcfMetaInfo;
    const vector<string>& directives = 
        meta.GetFieldRef("meta-information")->GetData().GetStrs();
    for (vector<string>::const_iterator cit = directives.begin();
            cit != directives.end(); ++cit ) {
        m_Os << "##" << *cit << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteMetaCreateNew(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    string datestr;
    if ( annot.IsSetDesc() ) {
        const CAnnot_descr& desc = annot.GetDesc();
        for ( list< CRef< CAnnotdesc > >::const_iterator cit = desc.Get().begin();
            cit != desc.Get().end(); ++cit ) 
        {
            if ( (*cit)->IsCreate_date() ) {
                const CDate& date = (*cit)->GetCreate_date();
                if ( date.IsStd() ) {
                    date.GetDate( &datestr, "%4Y%2M%2D" );
                }
            }
        }
    }

    m_Os << "##fileformat=VCFv4.1" 
         << endl;
    if ( ! datestr.empty() ) {
        m_Os << "##filedate=" << datestr << endl;
    }
    m_Os << "##INFO=<ID=DB,Number=0,Type=Flag,Description=\"dbSNP Membership\">"
         << endl;
    m_Os << "##INFO=<ID=H2,Number=0,Type=Flag,Description=\"Hapmap2 Membership\">"
         << endl;
    m_Os << "##INFO=<ID=H3,Number=0,Type=Flag,Description=\"Hapmap3 Membership\">"
         << endl;
    m_Os << "##INFO=<ID=RL,Number=1,Type=String,Description=\"Resource Link\">"
         << endl;
    m_Os << "##INFO=<ID=FBV,Number=1,Type=String,Description=\"Frequency Based Validation\">"
         << endl;
    m_Os << "##INFO=<ID=GTP,Number=1,Type=String,Description=\"Genotype\">"
         << endl;
    m_Os << "##INFO=<ID=QC,Number=1,Type=String,Description=\"Quality Check\">"
         << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteHeader(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    m_Os << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO";

    
    CConstRef<CUser_object> pVcfMetaInfo = s_GetVcfMetaInfo( annot );
    if (m_GenotypeHeaders.empty()) {
        m_Os << endl;
        return true;
    }
    m_Os << "\tFORMAT";
    for ( vector<string>::const_iterator cit = m_GenotypeHeaders.begin(); 
            cit != m_GenotypeHeaders.end(); ++cit ) {
        m_Os << '\t' << *cit;
    }
    m_Os << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteData(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    CSeq_annot_Handle sah = m_Scope.AddSeq_annot( annot );
    SAnnotSelector sel;
    sel.SetSortOrder( SAnnotSelector::eSortOrder_Normal );

    CFeat_CI mf(sah, sel);
    CGffFeatureContext fc(mf);
    for ( ; mf; ++mf ) {
        if ( ! x_WriteFeature( fc, *mf ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeature(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!x_WriteFeatureChrom(context, mf)) {
        return false;
    }
    if (!x_WriteFeaturePos(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureId(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureRef(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureAlt(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureQual(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureFilter(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureInfo(context, mf)) {
        return false;
    }
    if (!x_WriteFeatureGenotypeData(context, mf)) {
        return false;
    }
    m_Os << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureChrom(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle idh = sequence::GetIdHandle(mf.GetLocation(),
                                               &mf.GetScope());
    string chrom = idh.AsString();
    string db, id;
    NStr::SplitInTwo( idh.AsString(), "|", db, id );
    m_Os << id;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeaturePos(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    const CSeq_loc& loc = mf.GetLocation();
    unsigned int start = loc.GetStart(eExtreme_Positional);
    m_Os << NStr::UIntToString( start + 1 );
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureId(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    vector<string> ids;
    const CVariation_ref& var = mf.GetData().GetVariation();
    if ( var.IsSetId()  &&  var.GetId().GetTag().IsStr() ) {
        ids.push_back( var.GetId().GetTag().GetStr() );
    }

    if ( ids.empty() ) {
        m_Os << ".";
    }
    else {
        m_Os << NStr::Join( ids, ";" );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureRef(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    try {

        CSeqVector v(mf.GetLocation(), mf.GetScope(), CBioseq_Handle::eCoding_Iupac);
        string seqstr;
        v.GetSeqData( v.begin(), v.end(), seqstr );
        m_Os << seqstr;
        return true;
    }
    catch( ... ) {
    }
    try {
        typedef CVariation_ref::TData::TSet::TVariations TVARS;
        const TVARS& variations =
            mf.GetData().GetVariation().GetData().GetSet().GetVariations();
        for (TVARS::const_iterator cit = variations.begin(); cit != variations.end();
                ++cit) {
            if ( !(**cit).GetData().IsInstance() ) {
                continue;
            }
            const CVariation_inst& instance = (**cit).GetData().GetInstance();
            if ( !instance.IsSetObservation() ) {
                continue;
            }
            if ( instance.GetObservation() 
                    == CVariation_inst::eObservation_reference ) {
                const CDelta_item& delta = **( instance.GetDelta().begin() );   
                if ( delta.GetSeq().IsLiteral() ) {
                    m_Os << delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    return true;
                }
            }
        }
    }
    catch( ... ) {
        m_Os << "?";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureAlt(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    typedef CVariation_ref::TData::TSet::TVariations TVARS;

    m_Os << "\t";

    vector<string> alternatives;
    try {
        const TVARS& variations =
            mf.GetData().GetVariation().GetData().GetSet().GetVariations();
        for ( TVARS::const_iterator cit = variations.begin(); 
            cit != variations.end(); ++cit )
        {
            if ( ! (**cit).GetData().IsInstance() ) {
                continue;
            }
            const CVariation_inst& inst = (**cit).GetData().GetInstance();
            if (inst.IsSetObservation()  &&  
                    inst.GetObservation() == CVariation_inst::eObservation_reference) {
                continue;
            }
            switch( inst.GetType() ) {

                default: {
                    alternatives.push_back( "?" );
                    break;
                }
                case CVariation_inst::eType_snv: {
                    const CDelta_item& delta = **( inst.GetDelta().begin() );   
                    if ( delta.GetSeq().IsLiteral() ) {
                        alternatives.push_back( 
                            delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get() );
                    }
                    break;
                }
                case CVariation_inst::eType_ins: {
                    CSeqVector v(mf.GetLocation(), mf.GetScope(), CBioseq_Handle::eCoding_Iupac);
                    string seqstr;
                    v.GetSeqData( v.begin(), v.end(), seqstr );
                    CVariation_inst::TDelta::const_iterator cit = inst.GetDelta().begin();
                    while( cit != inst.GetDelta().end()  &&  (**cit).GetSeq().IsThis()  ) {
                        ++cit;
                    }
                    if ( cit == inst.GetDelta().end() ) {
                        alternatives.push_back( seqstr );
                    }
                    else {
                        alternatives.push_back( 
                            (*cit)->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get() + seqstr );
                    }
                    break;
                }
                case CVariation_inst::eType_del: {
                    CSeq_loc loc;
                    loc.Add( mf.GetLocation() );
                    CSeqVector v(loc, mf.GetScope(), CBioseq_Handle::eCoding_Iupac);
                    string seqstr;
                    v.GetSeqData( v.begin(), v.end(), seqstr );
                    CVariation_inst::TDelta::const_iterator cit = inst.GetDelta().begin();
                    if ( cit == inst.GetDelta().end() ) {
                    }
                    while( cit != inst.GetDelta().end()  &&  (**cit).GetSeq().IsThis()  ) {
                        ++cit;
                    }
                    if ( cit == inst.GetDelta().end() ) {
                        alternatives.push_back( seqstr );
                    }
                    break;
                }
            }
        }
    }
    catch( ... ) {
    }

    if ( alternatives.empty() ) {
        m_Os << ".";
    }
    else {
        m_Os << NStr::Join( alternatives, "," );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureQual(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string score = ".";

    m_Os << "\t";

    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "VcfAttributes" ) 
        {
            if ( ext.HasField( "score" ) ) {
                score = NStr::DoubleToString( 
                    ext.GetField( "score" ).GetData().GetReal() );
            }
        }
    }
    m_Os << score;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureFilter(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    feature::CFeatTree ftree = context.FeatTree();
    vector<string> filters;
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "VcfAttributes" ) 
        {
            if ( ext.HasField( "filter" ) ) {
                filters.push_back( ext.GetField( "filter" ).GetData().GetStr() );
            }
        }
    }
    if ( ! filters.empty() ) {
        m_Os << NStr::Join( filters, ":" );
    }
    else {
        m_Os << "PASS";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureInfo(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    if ( mf.IsSetExt() ) {
        string info = ".";
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "VcfAttributes" ) 
        {
            if ( ext.HasField( "info" ) ) {
                info = ext.GetField( "info" ).GetData().GetStr();
            }
        }
        m_Os << info;
        return true;
    }
   
    vector<string> infos;
    const CVariation_ref& var = mf.GetData().GetVariation();
    if ( var.IsSetId() ) {
        string db = var.GetId().GetDb();
        NStr::ToLower( db );
        if ( db == "dbsnp" ) {
            infos.push_back( "DB" );
        }
        if ( db == "hapmap2" ) {
            infos.push_back( "H2" );
        }
        if ( db == "hapmap3" ) {
            infos.push_back( "H3" );
        }
    }

    vector<string> values; 
    if ( var.IsSetVariant_prop() ) {
        const CVariantProperties& props = var.GetVariant_prop();
        if ( props.IsSetAllele_frequency() ) {
            infos.push_back( string("AF=") + 
                NStr::DoubleToString( props.GetAllele_frequency(), 4 ) );
        }
        if ( props.IsSetResource_link() ) {
            int rl = props.GetResource_link();
            values.clear();
            if ( rl & CVariantProperties::eResource_link_preserved ) {
                values.push_back( "preserved" );
            }
            if ( rl & CVariantProperties::eResource_link_provisional ) {
                values.push_back( "provisional" );
            }
            if ( rl & CVariantProperties::eResource_link_has3D ) {
                values.push_back( "has3D" );
            }
            if ( rl & CVariantProperties::eResource_link_submitterLinkout ) {
                values.push_back( "submitterLinkout" );
            }
            if ( rl & CVariantProperties::eResource_link_clinical ) {
                values.push_back( "clinical" );
            }
            if ( rl & CVariantProperties::eResource_link_genotypeKit ) {
                values.push_back( "genotypeKit" );
            }
            infos.push_back( string("RL=\"") + NStr::Join( values, "," ) + string("\"") );
        }
        if ( props.IsSetFrequency_based_validation() ) {
            int fbv = props.GetFrequency_based_validation();
            values.clear();
            if ( fbv & CVariantProperties::eFrequency_based_validation_is_mutation ) {
                values.push_back( "is-mutation" );
            }
            if ( fbv & CVariantProperties::eFrequency_based_validation_above_5pct_all ) {
                values.push_back( "above-5pct-all" );
            }
            if ( fbv & CVariantProperties::eFrequency_based_validation_above_5pct_1plus ) {
                values.push_back( "above-5pct-1plus" );
            }
            if ( fbv & CVariantProperties::eFrequency_based_validation_validated ) {
                values.push_back( "validated" );
            }
            if ( fbv & CVariantProperties::eFrequency_based_validation_above_1pct_all ) {
                values.push_back( "above-1pct-all" );
            }
            if ( fbv & CVariantProperties::eFrequency_based_validation_above_1pct_1plus ) {
                values.push_back( "above-1pct-1plus" );
            }
            infos.push_back( string("FBV=\"") + NStr::Join( values, "," ) + string("\"") );
        }
        if ( props.IsSetGenotype() ) {
            int gt = props.GetGenotype();
            values.clear();
            if ( gt & CVariantProperties::eGenotype_in_haplotype_set ) {
                values.push_back( "in_haplotype_set" );
            }
            if ( gt & CVariantProperties::eGenotype_has_genotypes ) {
                values.push_back( "has_genotypes" );
            }
            infos.push_back( string("GTP=\"") + NStr::Join( values, "," ) + string("\"") );
        }
        if ( props.IsSetQuality_check() ) {
            int qc = props.GetQuality_check();
            values.clear();
            if ( qc & CVariantProperties::eQuality_check_contig_allele_missing ) {
                values.push_back( "contig_allele_missing" );
            }
            if ( qc & CVariantProperties::eQuality_check_withdrawn_by_submitter ) {
                values.push_back( "withdrawn_by_submitter" );
            }
            if ( qc & CVariantProperties::eQuality_check_non_overlapping_alleles ) {
                values.push_back( "non_overlapping_alleles" );
            }
            if ( qc & CVariantProperties::eQuality_check_strain_specific ) {
                values.push_back( "strain_specific" );
            }
            if ( qc & CVariantProperties::eQuality_check_genotype_conflict ) {
                values.push_back( "genotype_conflict" );
            }
            infos.push_back( string("QC=\"") + NStr::Join( values, "," ) + string("\"") );
        }
    }

    if ( infos.empty() ) {
        m_Os << ".";
    }
    else {
        m_Os << NStr::Join( infos, ";" );
    }
    return true;     
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureGenotypeData(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (m_GenotypeHeaders.empty()) {
        return true;
    }

    feature::CFeatTree ftree = context.FeatTree();
    CConstRef<CUser_field> pFormat = mf.GetExt().GetFieldRef("format");
    const vector<string>& labels = pFormat->GetData().GetStrs();
    m_Os << "\t" << NStr::Join(labels, ":");

    CConstRef<CUser_field> pGenotypeData = mf.GetExt().GetFieldRef("genotype-data");
    const vector<CRef<CUser_field> > columns = pGenotypeData->GetData().GetFields(); 
    for ( size_t hpos = 0; hpos < m_GenotypeHeaders.size(); ++hpos ) {

        _ASSERT(m_GenotypeHeaders[hpos] == columns[hpos]->GetLabel().GetStr());

        string values = NStr::Join( columns[hpos]->GetData().GetStrs(), ":" );
        m_Os << "\t" << values;
    }
    return true;
}

END_NCBI_SCOPE
