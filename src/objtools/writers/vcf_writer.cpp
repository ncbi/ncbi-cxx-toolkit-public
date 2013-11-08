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
    m_GenotypeHeaders.clear();
    const CUser_field::C_Data::TStrs& strs =
        pVcfMetaInfo->GetField("genotype-headers").GetData().GetStrs();
    copy(strs.begin(), strs.end(), back_inserter(m_GenotypeHeaders));
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
    const CUser_field::C_Data::TStrs& directives = 
        meta.GetFieldRef("meta-information")->GetData().GetStrs();
    for (CUser_field::C_Data::TStrs::const_iterator cit = directives.begin();
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
    if (NStr::EndsWith(id, "|")) {
        id = id.substr(0, id.size()-1);
    }
    m_Os << id;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeaturePos(
    CGffFeatureContext& context,
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    m_Os << "\t";

    bool isInsertions = false;
    try {
        typedef CVariation_ref::TData::TSet::TVariations TVARS;
        const TVARS& variations =
            mf.GetData().GetVariation().GetData().GetSet().GetVariations();
        for (TVARS::const_iterator cit = variations.begin(); 
                cit != variations.end();
                ++cit) {
            if ( !(**cit).GetData().IsInstance() ) {
                continue;
            }
            const CVariation_inst& instance = (**cit).GetData().GetInstance();
            if (!instance.IsSetType()) {
                continue;
            }
            switch(instance.GetType()) {
            default:
                continue;
            case CVariation_inst::eType_ins:
                isInsertions = true;
                break;
            case CVariation_inst::eType_del:
            case CVariation_inst::eType_delins:
            case CVariation_inst::eType_snv:
                break;
            }
        }
    }
    catch(...) {
        //empty set???
    }
    const CSeq_loc& loc = mf.GetLocation();
    unsigned int start = loc.GetStart(eExtreme_Positional);
    if (!isInsertions) {
        start += 1;
    }
    m_Os << NStr::UIntToString(start);
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
    catch(const std::exception&) {
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
            if (instance.IsSetType()  &&
                    instance.GetType() == CVariation_inst::eType_identity) {
                const CDelta_item& delta = **( instance.GetDelta().begin() );   
                if ( delta.GetSeq().IsLiteral() ) {
                    m_Os << delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    return true;
                }
            }
            if (instance.IsSetObservation()  &&
                    instance.GetObservation() == CVariation_inst::eObservation_reference ) {
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
                case CVariation_inst::eType_identity: {
                    //that would be the reference- already taken care of
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
                        alternatives.push_back(seqstr + 
                            (*cit)->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get());
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
    typedef CVariantProperties VP;

    m_Os << "\t";

    vector<string> infos;
    const CVariation_ref& var = mf.GetData().GetVariation();

    if (var.IsSetId()) {
        string db = var.GetId().GetDb();
        NStr::ToLower(db);
        if (db == "dbsnp") {
            infos.push_back("DB");
        }
        if (db == "hapmap2") {
            infos.push_back("H2");
        }
        if (db == "hapmap3") {
            infos.push_back("H3");
        }
    }

    if (mf.IsSetDbxref()) {
        const vector<CRef<CDbtag> >& refs = mf.GetDbxref();
        string pmids;
        for ( vector<CRef<CDbtag> >::const_iterator cit = refs.begin();
            cit != refs.end(); ++cit) 
        {
            const CDbtag& ref = **cit;
            if (ref.IsSetDb()  &&  ref.IsSetTag()  &&  ref.GetDb() == "PM") {
                if (!pmids.empty()) {
                    pmids += ",";
                }
                pmids += "PM:";
                pmids += NStr::IntToString(ref.GetTag().GetId());
            }
        }
        if (!pmids.empty()) {
            //infos.push_back("PMC");
            infos.push_back(string("PMID=")+pmids);
        }
    }

    if (var.IsSetVariant_prop()) {
        const CVariantProperties& props = var.GetVariant_prop();
        if ( props.IsSetAllele_frequency()) {
            infos.push_back( string("AF=") + 
                NStr::DoubleToString( props.GetAllele_frequency(), 4));
        }
        if (props.IsSetResource_link()) {
            int rl = props.GetResource_link();
            if (rl & VP::eResource_link_preserved) {
                infos.push_back("PM");
            }
            if (rl & VP::eResource_link_provisional) {
                infos.push_back("TPA");
            }
            if (rl & VP::eResource_link_has3D) {
                infos.push_back("S3D");
            }
            if (rl & VP::eResource_link_submitterLinkout) {
                infos.push_back("SLO");
            }
            if (rl & VP::eResource_link_clinical) {
                infos.push_back("CLN");
            }
            if (rl & VP::eResource_link_genotypeKit) {
                 infos.push_back("HD");
            }
        }
        if (props.IsSetGene_location()) {
            int gl = props.GetGene_location();
            if (gl & VP::eGene_location_near_gene_5) {
                infos.push_back("R5");
            }
            if (gl & VP::eGene_location_near_gene_3) {
                infos.push_back("R3");
            }
            if (gl & VP::eGene_location_intron) {
                infos.push_back("INT");
            }
            if (gl & VP::eGene_location_donor) {
                infos.push_back("DSS");
            }
            if (gl & VP::eGene_location_acceptor) {
                infos.push_back("ASS");
            }
            if (gl & VP::eGene_location_utr_5) {
                infos.push_back("U5");
            }
            if (gl & VP::eGene_location_utr_3) {
                infos.push_back("U3");
            }
        }

        if (props.IsSetEffect()) {
            int effect = props.GetEffect();
            if (effect & VP::eEffect_synonymous) {
                infos.push_back("SYN");
            }
            if (effect & VP::eEffect_stop_gain) {
                infos.push_back("NSN");
            }
            if (effect & VP::eEffect_missense) {
                infos.push_back("NSM");
            }
            if (effect & VP::eEffect_frameshift) {
                infos.push_back("NSF");
            }
        }

        if (props.IsSetFrequency_based_validation()) {
            int fbv = props.GetFrequency_based_validation();
            if (fbv & VP::eFrequency_based_validation_is_mutation) {
                infos.push_back("MUT");
            }
            if (fbv & VP::eFrequency_based_validation_above_5pct_all) {
                infos.push_back("G3");
            }
            if (fbv & VP::eFrequency_based_validation_above_5pct_1plus) {
                infos.push_back("G5");
            }
            if (fbv & VP::eFrequency_based_validation_validated) {
                infos.push_back("VLD");
            }
        }

        if (props.IsSetAllele_frequency()) {
            double alfrq = props.GetAllele_frequency();
            infos.push_back(string("GMAF=") + NStr::DoubleToString(alfrq));
        }

        if (props.IsSetGenotype()) {
            int gt = props.GetGenotype();
            if (gt & VP::eGenotype_has_genotypes) {
                infos.push_back("GNO");
            }
        }

        if (props.IsSetQuality_check()) {
            int qc = props.GetQuality_check();
            if (qc & VP::eQuality_check_contig_allele_missing) {
                infos.push_back("NOC");
            }
            if (qc & VP::eQuality_check_withdrawn_by_submitter) {
                infos.push_back("WTD");
            }
            if (qc & VP::eQuality_check_non_overlapping_alleles) {
                infos.push_back("NOV");
            }
            if (qc & VP::eQuality_check_genotype_conflict) {
                infos.push_back("GCF");
            }
        }

        if (var.IsSetOther_ids()) {
            const list<CRef<CDbtag> >&  oids = var.GetOther_ids();
            list<CRef<CDbtag> >::const_iterator cit; 
            for (cit = oids.begin(); cit != oids.end(); ++cit) {
                const CDbtag& dbtag = **cit;
                if (dbtag.GetType() != CDbtag::eDbtagType_BioProject) {
                    continue;
                }
                if (!dbtag.CanGetTag()) {
                    continue;
                }
                if (!dbtag.GetTag().IsId()) {
                    continue;
                }
                int id = dbtag.GetTag().GetId();
                if (id == 60835) {
                    infos.push_back("PH3");
                }
                else if (id == 28889) {
                    infos.push_back("KGPhase1");
                }
            }
        }
    }

    if ( mf.IsSetExt() ) {
        string info = ".";
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "VcfAttributes" ) 
        {
            if ( ext.HasField( "info" ) ) {
                vector<string> extraInfos;
                info = ext.GetField( "info" ).GetData().GetStr();
                NStr::Tokenize(info, ";", extraInfos, NStr::eMergeDelims);
                for (vector<string>::const_iterator cit = extraInfos.begin();
                        cit != extraInfos.end();
                        ++cit) {
                    string value = *cit;
                    vector<string>::iterator fit = 
                        std::find(infos.begin(), infos.end(), value);
                    if (fit == infos.end()) {
                        infos.push_back(value);
                    }
                }
            }
        }
    }
   
    if (infos.empty()) {
        m_Os << ".";
    }
    else {
        m_Os << NStr::Join(infos, ";");
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
    const CUser_field_Base::C_Data::TStrs& labels = pFormat->GetData().GetStrs();
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
