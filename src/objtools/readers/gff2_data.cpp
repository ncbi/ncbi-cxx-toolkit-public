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

#include <objtools/readers/gff2_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGff2Record::CGff2Record():
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_pePhase( 0 )
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
CGff2Record::~CGff2Record()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_pePhase; 
};

//  ----------------------------------------------------------------------------
bool CGff2Record::AssignFromGff(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    vector< string > columns;

    string strLeftOver = strRawInput;
    for ( size_t i=0; i < 8 && ! strLeftOver.empty(); ++i ) {
        string strFront;
        NStr::SplitInTwo( strLeftOver, " \t", strFront, strLeftOver );
		columns.push_back( strFront );
        NStr::TruncateSpacesInPlace( strLeftOver, NStr::eTrunc_Begin );
    }
    columns.push_back( strLeftOver );
        
    if ( columns.size() < 9 ) {
        // not enough fields to work with
        return false;
    }
    //  to do: more sanity checks

    m_strId = columns[0];
    m_strSource = columns[1];
    m_strType = columns[2];
    m_uSeqStart = NStr::StringToUInt( columns[3] ) - 1;
    m_uSeqStop = NStr::StringToUInt( columns[4] ) - 1;
    if (m_uSeqStop < m_uSeqStart) {
        ERR_POST( 
            m_strId + ":" + m_strType + " " + columns[3] + "-" + columns[4] + ": " +
            "Negative length feature--- TOSSED !!!" );
        return false;
    }

    if ( columns[5] != "." ) {
        m_pdScore = new double( NStr::StringToDouble( columns[5] ) );
    }

    if ( columns[6] == "+" ) {
        m_peStrand = new ENa_strand( eNa_strand_plus );
    }
    if ( columns[6] == "-" ) {
        m_peStrand = new ENa_strand( eNa_strand_minus );
    }
    if ( columns[6] == "?" ) {
        m_peStrand = new ENa_strand( eNa_strand_unknown );
    }

    if ( columns[7] == "0" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_one );
    }
    if ( columns[7] == "1" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_two );
    }
    if ( columns[7] == "2" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_three );
    }

    m_strAttributes = columns[8];
    
    return x_AssignAttributesFromGff( m_strAttributes );
}

//  ----------------------------------------------------------------------------
bool CGff2Record::GetAttribute(
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
string CGff2Record::x_NormalizedAttributeKey(
    const string& strRawKey )
//  ----------------------------------------------------------------------------
{
    string strKey = NStr::TruncateSpaces( strRawKey );
    return strKey;
}

//  ----------------------------------------------------------------------------
string CGff2Record::x_NormalizedAttributeValue(
    const string& strRawValue )
//  ----------------------------------------------------------------------------
{
    string strValue = NStr::TruncateSpaces( strRawValue );
    if ( NStr::StartsWith( strValue, "\"" ) ) {
        strValue = strValue.substr( 1, string::npos );
    }
    if ( NStr::EndsWith( strValue, "\"" ) ) {
        strValue = strValue.substr( 0, strValue.length() - 1 );
    }
    return strValue;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::x_AssignAttributesFromGff(
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
        strValue = x_NormalizedAttributeValue( strValue );

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
bool CGff2Record::x_SplitGffAttributes(
    const string& strRawAttributes,
	vector< string >& attributes) const
//  ----------------------------------------------------------------------------
{
	string strCurrAttrib;
	bool inQuotes = false;

	ITERATE (string, iterChar, strRawAttributes) {
		if (inQuotes) {
			if (*iterChar == '\"') {
				inQuotes = false;
			}  
			strCurrAttrib += *iterChar;
		} else { // not in quotes
			if (*iterChar == ';') {
				NStr::TruncateSpacesInPlace( strCurrAttrib );
				if(!strCurrAttrib.empty())
					attributes.push_back(strCurrAttrib);
				strCurrAttrib.clear();
			} else {
				if(*iterChar == '\"') {
					inQuotes = true;
				}
				strCurrAttrib += *iterChar;
			}
		}
	}

	NStr::TruncateSpacesInPlace( strCurrAttrib );
	if (!strCurrAttrib.empty())
		attributes.push_back(strCurrAttrib);

	return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::x_IsParentOf(
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
string CGff2Record::x_FeatIdString(
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


END_objects_SCOPE
END_NCBI_SCOPE
