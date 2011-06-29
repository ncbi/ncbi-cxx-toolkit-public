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

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACaa.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/vcf_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

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
bool CVcfWriter::x_WriteMeta(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsSetDesc()  || ! annot.GetDesc().IsSet() ) {
        return x_WriteMetaCreateNew( annot );
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
        return x_WriteMetaCreateNew( annot );
    }

    unsigned int pos = 1;
    const CAnnotdesc::TUser& meta = pDescMeta->GetUser();
    const CUser_object::TData& data = meta.GetData();

    while ( true ) {
        string key = NStr::UIntToString( pos++ );
        if ( meta.HasField( key ) ) {
            string value = meta.GetField( key ).GetData().GetStr();
            m_Os << "##" << value << endl;
        }
        else {
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteMetaCreateNew(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    m_Os << "##fileformat=VCFv4.1" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteHeader(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    m_Os << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO";
    //maybe add format header
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

    feature::CFeatTree feat_tree( CFeat_CI( sah, sel) );
    for ( CFeat_CI mf( sah, sel ); mf; ++mf ) {
        if ( ! x_WriteFeature( feat_tree, *mf ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeature(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteFeatureChrom( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeaturePos( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureId( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureRef( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureAlt( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureQual( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureFilter( ftree, mf ) ) {
        return false;
    }
    m_Os << "\t";
    if ( ! x_WriteFeatureInfo( ftree, mf ) ) {
        return false;
    }
    m_Os << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureChrom(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string chrom = mf.GetLocationId().AsString();
    string db, id;
    NStr::SplitInTwo( mf.GetLocationId().AsString(), "|", db, id );
    m_Os << id;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeaturePos(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.GetLocation().IsInt() ) {
        size_t from = mf.GetLocation().GetInt().GetFrom();
        m_Os << NStr::UIntToString( from + 1 );
        return true;
    }
    m_Os << ".";    
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureId(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
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
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    typedef CVariation_ref::TData::TSet::TVariations TVARS;

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
            if ( ! inst.IsSetObservation() ) {
                continue;
            }
            if ( inst.GetObservation()  == CVariation_inst::eObservation_reference )
            {
                const CDelta_item& delta = **( inst.GetDelta().begin() );
                m_Os << delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                return true;
            }
        }
    }
    catch( ... ) {
    }
    m_Os << ".";
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureAlt(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    typedef CVariation_ref::TData::TSet::TVariations TVARS;

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
            if ( ! inst.IsSetObservation() ) {
                continue;
            }
            if ( inst.GetObservation()  == CVariation_inst::eObservation_variant )
            {
                const CDelta_item& delta = **( inst.GetDelta().begin() );
                alternatives.push_back( 
                    delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get() );
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
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string score = ".";

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
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string filter = "<FIX ME>";
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "VcfAttributes" ) 
        {
            if ( ext.HasField( "filter" ) ) {
                filter = ext.GetField( "filter" ).GetData().GetStr();
            }
        }
    }
    m_Os << filter;
    return true;
}

//  ----------------------------------------------------------------------------
bool CVcfWriter::x_WriteFeatureInfo(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{

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
        if ( db == "dbvar" ) {
            infos.push_back( "DB" );
        }
        if ( db == "hapmap2" ) {
            infos.push_back( "H2" );
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

END_NCBI_SCOPE
