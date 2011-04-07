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
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/writers/gff2_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string CGffWriteRecord::x_MakeGffDbtag( 
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
string CGffWriteRecord::StrId() const
//  ----------------------------------------------------------------------------
{
    return Id();
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSource() const
//  ----------------------------------------------------------------------------
{
    return Source();
}

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord():
    m_strId( "" ),
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_strSource( "" ),
    m_strType( "" ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    const CGffWriteRecord& other ):
    m_strId( other.m_strId ),
    m_uSeqStart( other.m_uSeqStart ),
    m_uSeqStop( other.m_uSeqStop ),
    m_strSource( other.m_strSource ),
    m_strType( other.m_strType ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    if ( other.IsSetScore() ) {
        m_pdScore = new double( other.Score() );
    }
    if ( other.IsSetStrand() ) {
        m_peStrand = new ENa_strand( other.Strand() );
    }
    if ( other.IsSetPhase() ) {
        m_puPhase = new unsigned int( other.Phase() );
    }

    this->m_Attributes.insert( 
        other.m_Attributes.begin(), other.m_Attributes.end() );
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::~CGffWriteRecord()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_puPhase; 
};

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::GetAttribute(
    const string& strKey,
    string& strValue ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = m_Attributes.find( strKey );
    if ( it == m_Attributes.end() ) {
        return false;
    }
    strValue = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::MergeRecord(
    const CGffWriteRecord& other )
//  ----------------------------------------------------------------------------
{
    const TAttributes& newAttrs = other.Attributes(); 
    for ( TAttrCit cit  = newAttrs.begin(); cit != newAttrs.end(); ++cit ) {
        if ( cit->first == "gff_score" ) {
            delete m_pdScore;
            m_pdScore = new double( NStr::StringToDouble( cit->second ) );
            continue;
        }
        m_Attributes[ cit->first ] = cit->second;
    }

    return true;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    string strGffType;
    if ( GetAttribute( "gff_type", strGffType ) ) {
        return strGffType;
    }
    return Type();
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( SeqStart() + 1 );;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( SeqStop() + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetScore() ) {
        return ".";
    }

    //  can NStr format floating point numbers? Didn't see ...
    char pcBuffer[ 16 ];
    ::sprintf( pcBuffer, "%6.6f", Score() );
    return string( pcBuffer );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrStrand() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetStrand() ) {
        return ".";
    }
    switch ( Strand() ) {
    default:
        return ".";
    case eNa_strand_plus:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetPhase() ) {
        return ".";
    }
    return NStr::UIntToString( Phase() );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffWriteRecord::TAttrIt it;

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
//        strAttributes += " ";
		
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
bool CGffWriteRecord::x_NeedsQuoting(
    const string& str )
//  ----------------------------------------------------------------------------
{
    if( str.empty() )
		return true;

	for ( size_t u=0; u < str.length(); ++u ) {
        if ( str[u] == '\"' )
			return false;
		if ( str[u] == ' ' || str[u] == ';' ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignSeqIdFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    m_strId = "<unknown>";

    if ( feature.CanGetLocation() ) {
    	const CSeq_loc& loc = feature.GetLocation();
	    CConstRef<CSeq_id> id(loc.GetId());
		if (id) {
			m_strId.clear();
			id->GetLabel(&m_strId, CSeq_id::eContent);
		}
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignTypeFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    m_strType = "region";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "standard_name" ) {
                    m_strType = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }

    if ( ! feature.CanGetData() ) {
        return true;
    }

    switch ( feature.GetData().GetSubtype() ) {
    default:
        m_strType = feature.GetData().GetKey();
        break;

    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        break;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;

    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;

    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignStartFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uStart = location.GetStart( eExtreme_Positional );
        m_uSeqStart = uStart;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignStopFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uEnd = location.GetStop( eExtreme_Positional );
        m_uSeqStop = uEnd;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignSourceFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    m_strSource = ".";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_source" ) {
                    m_strSource = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignScoreFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_score" ) {
                    m_pdScore = new double( NStr::StringToDouble( 
                        (*it)->GetVal() ) );
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignStrandFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        m_peStrand = new ENa_strand( feature.GetLocation().GetStrand() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignPhaseFromAsn(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( ! feature.CanGetData() ) {
        return true;
    }
    const CSeq_feat::TData& data = feature.GetData();
    if ( data.GetSubtype() == CSeq_feat::TData::eSubtype_cdregion ) {
        m_puPhase = new unsigned int( 0 );
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_IsParentOf(
    CSeq_feat::TData::ESubtype maybe_parent,
    CSeq_feat::TData::ESubtype maybe_child )
//  ----------------------------------------------------------------------------
{
    switch ( maybe_parent ) {
    default:
        return false;

    case CSeq_feat::TData::eSubtype_10_signal:
    case CSeq_feat::TData::eSubtype_35_signal:
    case CSeq_feat::TData::eSubtype_3UTR:
    case CSeq_feat::TData::eSubtype_5UTR:
        return false;

    case CSeq_feat::TData::eSubtype_mRNA:
        switch ( maybe_child ) {
        default:
            return false;
        case CSeq_feat::TData::eSubtype_cdregion:
            return true;
        }

    case CSeq_feat::TData::eSubtype_operon:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_gene:
        case CSeq_feat::TData::eSubtype_promoter:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_gene, maybe_child ) ||
                x_IsParentOf( CSeq_feat::TData::eSubtype_promoter, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_gene:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_intron:
        case CSeq_feat::TData::eSubtype_mRNA:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_intron, maybe_child ) ||
                x_IsParentOf( CSeq_feat::TData::eSubtype_mRNA, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_cdregion:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_exon:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_exon, maybe_child );
        }
    }

    return false;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::x_FeatIdString(
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    switch ( id.Which() ) {
    default:
        break;

    case CFeat_id::e_Local: {
        const CFeat_id::TLocal& local = id.GetLocal();
        if ( local.IsId() ) {
            return NStr::IntToString( local.GetId() );
        }
        if ( local.IsStr() ) {
            return local.GetStr();
        }
        break;
        }
    }
    return "FEATID";
}

//  ----------------------------------------------------------------------------
void CGffWriteRecord::x_PriorityProcess(
    const string& strKey,
    map<string, string >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string strKeyMod( strKey );

    map< string, string >::iterator it = attrs.find( strKeyMod );
    if ( it == attrs.end() ) {
        return;
    }

    // Some of the attributes are multivalue translating into multiple gff attributes
    //  all carrying the same key. These are special cased here:
    //
    if ( strKey == "Dbxref" ) {
        vector<string> tags;
        NStr::Tokenize( it->second, ";", tags );
        for ( vector<string>::iterator pTag = tags.begin(); 
            pTag != tags.end(); pTag++ ) {
            if ( ! strAttributes.empty() ) {
                strAttributes += "; ";
            }
            strAttributes += strKeyMod;
            strAttributes += "=\""; // quoted in all samples I have seen
            strAttributes += *pTag;
            strAttributes += "\"";
        }
		attrs.erase(it);
        return;
    }

    // General case: Single value, make straight forward gff attribute:
    //
    if ( ! strAttributes.empty() ) {
        strAttributes += "; ";
    }

    strAttributes += strKeyMod;
    strAttributes += "=";
   	bool quote = x_NeedsQuoting(it->second);
	if ( quote )
		strAttributes += '\"';		
	strAttributes += it->second;
	attrs.erase(it);
	if ( quote )
		strAttributes += '\"';
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignFromAsn(
    CMappedFeat mapped_feature )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignType( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignSeqIdFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignSourceFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStartFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStopFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignScoreFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStrandFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignPhaseFromAsn( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributes( mapped_feature ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignType(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();
    switch ( feature.GetData().GetSubtype() ) {
    default:
        m_strType = "misc_feature";
        return true;

    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        return true;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        return true;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignLocation(
    const CSeq_interval& interval ) 
//  ----------------------------------------------------------------------------
{
    if ( interval.CanGetFrom() ) {
        m_uSeqStart = interval.GetFrom();
    }
    if ( interval.CanGetTo() ) {
        m_uSeqStop = interval.GetTo();
    }
    if ( interval.IsSetStrand() ) {
        if ( 0 == m_peStrand ) {
            m_peStrand = new ENa_strand( interval.GetStrand() );
        }
        else {
            *m_peStrand = interval.GetStrand();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignSequenceNumber(
    unsigned int uSequenceNumber,
    const string& strPrefix ) 
//  ----------------------------------------------------------------------------
{
    TAttrIt it = m_Attributes.find( "ID" );
    if ( it != m_Attributes.end() ) {
        it->second += string( "|" ) + strPrefix + NStr::UIntToString( uSequenceNumber );
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributes(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    cerr << "FIXME: CGffWriteRecord::x_AssignAttributes" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeNote(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetComment() ) {
        m_Attributes[ "note" ] = mapped_feat.GetComment();
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributePseudo(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetPseudo() && mapped_feat.GetPseudo() == true ) {
        m_Attributes[ "pseudo" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributePartial(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetPartial() && mapped_feat.GetPartial() == true ) {
        m_Attributes[ "partial" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeDbXref(
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
    string value = x_MakeGffDbtag( *dbxrefs[ 0 ] );
    for ( size_t i=1; i < dbxrefs.size(); ++i ) {
        value += ";";
        value += x_MakeGffDbtag( *dbxrefs[ i ] );
    }
    m_Attributes[ "db_xref" ] = value;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeGeneSynonym(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    string strGeneSyn = x_GeneRefToGeneSyn( gene_ref );
    if ( ! strGeneSyn.empty() ) {
        m_Attributes[ "gene_synonym" ] = strGeneSyn;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeLocusTag(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    string strLocusTag = x_GeneRefToLocusTag( gene_ref );
    if ( ! strLocusTag.empty() ) {
        m_Attributes[ "locus_tag" ] = strLocusTag;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeAllele(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetAllele() ) {
        return true;
    }
    string strAllele = gene_ref.GetAllele();
    if ( ! strAllele.empty() ) {
        m_Attributes[ "allele" ] = strAllele;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeCodonStart(
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
bool CGffWriteRecord::x_AssignAttributeMap(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetMaploc() ) {
        return true;
    }
    string strMap = gene_ref.GetMaploc();
    if ( ! strMap.empty() ) {
        m_Attributes[ "map" ] = strMap;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_AssignAttributeProduct(
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
string CGffWriteRecord::x_GeneRefToGene(
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
string CGffWriteRecord::x_GeneRefToLocusTag(
    const CGene_ref& gene_ref )
//  ----------------------------------------------------------------------------
{
    if ( ! gene_ref.IsSetLocus() || ! gene_ref.IsSetLocus_tag() ) {
        return "";
    }
    return gene_ref.GetLocus_tag();
}
    
//  ----------------------------------------------------------------------------
string CGffWriteRecord::x_GeneRefToGeneSyn(
    const CGene_ref& gene_ref )
//  ----------------------------------------------------------------------------
{
    //
    //  Comma concatenate list of gene synonyms. If the first in this list has
    //  been burnt to make up for a missing locus or locus tag, omit it here.
    //
    if ( ! gene_ref.IsSetSyn() ) {
        return "";
    }
    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if ( ! gene_ref.IsSetLocus() && ! gene_ref.IsSetLocus_tag() ) {
        ++it;
    }
    if ( it == syns.end() ) {
        return "";
    }
    string strGeneSyn = *( it++ );
    while ( it != syns.end() ) {
        strGeneSyn += ",";
        strGeneSyn += * (it++ );
    }
    return strGeneSyn;
}
  
END_objects_SCOPE
END_NCBI_SCOPE
