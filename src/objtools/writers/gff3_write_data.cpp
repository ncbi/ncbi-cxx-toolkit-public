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
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string s_GeneRefToGene(
    const CGene_ref& gene_ref )
//  ----------------------------------------------------------------------------
{
    if ( gene_ref.IsSetLocus() ) {
        return gene_ref.GetLocus();
    }
    if ( gene_ref.IsSetLocus_tag() ) {
        return gene_ref.GetLocus_tag();
    }
    if ( gene_ref.IsSetSyn()  && gene_ref.GetSyn().size() > 0 ) {
        return *( gene_ref.GetSyn().begin() );
    }
    if ( gene_ref.IsSetDesc() ) {
        return gene_ref.GetDesc();
    }
    return "";
}

//  ----------------------------------------------------------------------------
string s_MakeGffDbtag( 
    const CDbtag& dbtag )
//
//  Currently, simply produce "DB:TAG" (which is different from 
//    dbtag.GetLabel() ).
//  In the future, may have to convert between Genbank DB abbreviations and
//    GFF DB abbreviations.
//  ----------------------------------------------------------------------------
{
    string strGffTag;
    if ( dbtag.IsSetDb() ) {
        strGffTag += dbtag.GetDb();
        strGffTag += ":";
    }
    if ( dbtag.IsSetTag() ) {
        if ( dbtag.GetTag().IsId() ) {
            strGffTag += NStr::UIntToString( dbtag.GetTag().GetId() );
        }
        if ( dbtag.GetTag().IsStr() ) {
            strGffTag += dbtag.GetTag().GetStr();
        }
    }
    return strGffTag;
}
        
//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    feature::CFeatTree& feat_tree )
//  ----------------------------------------------------------------------------
    : m_feat_tree( feat_tree )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGffWriteRecordFeature( other ),
      m_feat_tree( other.m_feat_tree )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::~CGff3WriteRecordFeature()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnCore(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    // If feature ids are present then they are likely used to show parent/child
    // relationships, via corresponding xrefs. Thus, any feature ids override
    // gb ID tags (feature ids and ID tags should agree in the first place, but
    // if not, feature ids must trump ID tags).
    //
    bool bIdAssigned = false;

    if ( mapped_feat.IsSetId() ) {
        const CSeq_feat::TId& id = mapped_feat.GetId();
        string value = CGffWriteRecordFeature::x_FeatIdString( id );
        m_Attributes[ "ID" ] = value;
        bIdAssigned = true;
    }

    if ( mapped_feat.IsSetXref() ) {
        const CSeq_feat::TXref& xref = mapped_feat.GetXref();
        string value;
        for ( size_t i=0; i < xref.size(); ++i ) {
            if ( xref[i]->CanGetId() /* && xref[i]->CanGetData() */ ) {
                const CSeqFeatXref::TId& id = xref[i]->GetId();
                if ( ! value.empty() ) {
                    value += ",";
                }
                value += CGffWriteRecordFeature::x_FeatIdString( id );
            }
        }
        if ( ! value.empty() ) {
            m_Attributes[ "Parent" ] = value;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnExtended(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    // collection of feature attribute that we want to propagate to every feature
    //  type under the sun
    //
    if ( ! x_AssignAttributeGene( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributePseudo( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributePartial( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeEvidence( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeNote( mapped_feat ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecordFeature::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffWriteRecord::TAttrIt it;

    x_PriorityProcess( "ID", attrs, strAttributes );
    x_PriorityProcess( "Name", attrs, strAttributes );
    x_PriorityProcess( "Alias", attrs, strAttributes );
    x_PriorityProcess( "Parent", attrs, strAttributes );
    x_PriorityProcess( "Target", attrs, strAttributes );
    x_PriorityProcess( "Gap", attrs, strAttributes );
    x_PriorityProcess( "Derives_from", attrs, strAttributes );
    x_PriorityProcess( "Note", attrs, strAttributes );
    x_PriorityProcess( "Dbxref", attrs, strAttributes );
    x_PriorityProcess( "Ontology_term", attrs, strAttributes );

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
        
        bool quote = x_NeedsQuoting(it->second);
        if ( quote )
            strAttributes += '\"';        
        strAttributes += it->second;
        if ( quote )
            strAttributes += '\"';
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributes(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributesFromAsnCore( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( mapped_feat ) ) {
        return false;
    }

    if ( StrType() == "gene" ) {
        return x_AssignAttributesGene( mapped_feat );
    }
    if ( StrType() == "mRNA" ) {
        return x_AssignAttributesMrna( mapped_feat );
    }
    if ( StrType() == "CDS" ) {
        return x_AssignAttributesCds( mapped_feat );
    }
    return x_AssignAttributesMiscFeature( mapped_feat );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesGene(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeGene( mapped_feat )  &&
        x_AssignAttributeGeneSynonym( mapped_feat )  &&
        x_AssignAttributeLocusTag( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeProduct( mapped_feat ) );
}
//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeProduct( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesCds(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeCodonStart( mapped_feat )  &&
        x_AssignAttributeProduct( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMiscFeature(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributePseudo( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGene(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    string strGene;
    if ( StrType() == "gene" ) {
        const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
        strGene = s_GeneRefToGene( gene_ref );
    }

    if ( strGene.empty() && mapped_feat.IsSetXref() ) {
        const vector< CRef< CSeqFeatXref > > xrefs = mapped_feat.GetXref();
        for ( vector< CRef< CSeqFeatXref > >::const_iterator it = xrefs.begin();
            it != xrefs.end();
            ++it ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.CanGetData() && xref.GetData().IsGene() ) {
                strGene = s_GeneRefToGene( xref.GetData().GetGene() );
                break;
            }
        }
    }

    if ( strGene.empty() ) {
        CMappedFeat gene = 
            mapped_feat.GetFeatSubtype() == CSeq_feat::TData::eSubtype_mRNA ?
            feature::GetBestGeneForMrna( mapped_feat, &m_feat_tree ) :
            feature::GetBestGeneForFeat( mapped_feat, &m_feat_tree );
        if ( gene.IsSetData()  &&  gene.GetData().IsGene() ) {
            strGene = s_GeneRefToGene( gene.GetData().GetGene() );
        }
    }

    if ( ! strGene.empty() ) {
        m_Attributes[ "gene" ] = strGene;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeNote(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetComment() ) {
        m_Attributes[ "note" ] = mapped_feat.GetComment();
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePartial(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetPartial() && mapped_feat.GetPartial() == true ) {
        m_Attributes[ "partial" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePseudo(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetPseudo() && mapped_feat.GetPseudo() == true ) {
        m_Attributes[ "pseudo" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeDbXref(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetDbxref() ) {
        return true;
    }
    const CSeq_feat::TDbxref& dbxrefs = mapped_feat.GetDbxref();
    if ( dbxrefs.size() == 0 ) {
        return true;
    }
    string value = s_MakeGffDbtag( *dbxrefs[ 0 ] );
    for ( size_t i=1; i < dbxrefs.size(); ++i ) {
        value += ";";
        value += s_MakeGffDbtag( *dbxrefs[ i ] );
    }
    m_Attributes[ "db_xref" ] = value;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGeneSynonym(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( !gene_ref.IsSetSyn() ) {
        return true;
    }

    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if ( ! gene_ref.IsSetLocus() && ! gene_ref.IsSetLocus_tag() ) {
        ++it;
    }
    if ( it == syns.end() ) {
        return true;
    }
    string strGeneSyn = *( it++ );
    while ( it != syns.end() ) {
        strGeneSyn += ",";
        strGeneSyn += * (it++ );
    }

    if ( ! strGeneSyn.empty() ) {
        m_Attributes[ "gene_synonym" ] = strGeneSyn;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeLocusTag(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetLocus() || ! gene_ref.IsSetLocus_tag() ) {
        return true;
    }
    m_Attributes[ "locus_tag" ] = gene_ref.GetLocus_tag();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeCodonStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CCdregion& cdr = mapped_feat.GetData().GetCdregion();
    if ( ! cdr.IsSetFrame() ) {
        return true;
    }
    string strFrame;
    switch( cdr.GetFrame() ) {
    default:
        break;

    case CCdregion::eFrame_one:
        strFrame = "1";
        break;

    case CCdregion::eFrame_two:
        strFrame = "2";
        break;

    case CCdregion::eFrame_three:
        strFrame = "3";
        break;
    }
    if ( ! strFrame.empty() ) {
        m_Attributes[ "codon_start" ] = strFrame;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeProduct(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetProduct() ) {
        return true;
    }
    const CSeq_id* pProductId = mapped_feat.GetProduct().GetId();
    if ( pProductId ) {
        string strProduct;
        pProductId->GetLabel( &strProduct );
        m_Attributes[ "product" ] = strProduct;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeEvidence(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const string strExperimentDefault(
        "experimental evidence, no additional details recorded" );
    const string strInferenceDefault(
        "non-experimental evidence, no additional details recorded" );

    bool bExperiment = false;
    bool bInference = false;
    const CSeq_feat::TQual& quals = mapped_feat.GetQual();
    for ( CSeq_feat::TQual::const_iterator it = quals.begin(); 
            ( it != quals.end() ) && ( !bExperiment && !bInference ); 
            ++it ) {
        if ( ! (*it)->CanGetQual() ) {
            continue;
        }
        string strKey = (*it)->GetQual();
        if ( strKey == "experiment" ) {
            m_Attributes[ "experiment" ] = (*it)->GetVal();
            bExperiment = true;
        }
        if ( strKey == "inference" ) {
            m_Attributes[ "inference" ] = (*it)->GetVal();
            bInference = true;
        }
    }

    // "exp_ev" only enters if neither "experiment" nor "inference" qualifiers
    //  present
    if ( !bExperiment && !bInference ) {
        if ( mapped_feat.IsSetExp_ev() ) {
            if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_not_experimental ) {
                m_Attributes[ "inference" ] = strInferenceDefault;
            }
            else if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_experimental ) {
                m_Attributes[ "experiment" ] = strExperimentDefault;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignParent(
    const CGff3WriteRecordFeature& parent )
//  ----------------------------------------------------------------------------
{
    string strParentId;
    if ( ! parent.GetAttribute( "ID", strParentId ) ) {
        cerr << "Fix me: Parent record without GFF3 ID tag!" << endl;
        return false;
    }
    m_Attributes[ "Parent" ] = strParentId;
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::ForceAttributeID(
    const string& strId )
//  ----------------------------------------------------------------------------
{
    m_Attributes[ "ID" ] = strId;
}  

END_objects_SCOPE
END_NCBI_SCOPE
