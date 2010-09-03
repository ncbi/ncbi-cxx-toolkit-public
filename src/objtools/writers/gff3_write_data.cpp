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
CGff3WriteRecord::CGff3WriteRecord(
    feature::CFeatTree& feat_tree ):
    CGff2WriteRecord( feat_tree )
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecord::CGff3WriteRecord(
    const CGff2WriteRecord& other ) :
    CGff2WriteRecord( other )
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecord::~CGff3WriteRecord()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::AssignFromAsn(
    CMappedFeat mapped_feature )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignTypeFromAsn( mapped_feature ) ) {
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
    if ( ! x_AssignAttributesFromAsnCore( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( mapped_feature ) ) {
        return false;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignAttributesFromAsnCore(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    // If feature ids are present then they are likely used to show parent/child
    // relationships, via corresponding xrefs. Thus, any feature ids override
    // gb ID tags (feature ids and ID tags should agree in the first place, but
    // if not, feature ids must trump ID tags).
    //
    bool bIdAssigned = false;

    if ( feature.CanGetId() ) {
        const CSeq_feat::TId& id = feature.GetId();
        string value = CGff2WriteRecord::x_FeatIdString( id );
        m_Attributes[ "ID" ] = value;
        bIdAssigned = true;
    }

    if ( feature.CanGetXref() ) {
        const CSeq_feat::TXref& xref = feature.GetXref();
        string value;
        for ( size_t i=0; i < xref.size(); ++i ) {
            if ( xref[i]->CanGetId() /* && xref[i]->CanGetData() */ ) {
                const CSeqFeatXref::TId& id = xref[i]->GetId();
                if ( ! value.empty() ) {
                    value += ",";
                }
                value += CGff2WriteRecord::x_FeatIdString( id );
            }
        }
        if ( ! value.empty() ) {
            m_Attributes[ "Parent" ] = value;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignAttributesFromAsnExtended(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = feature.GetDbxref();
        if ( dbxrefs.size() > 0 ) {
            string value = x_MakeGffDbtag( *dbxrefs[ 0 ] );
            for ( size_t i=1; i < dbxrefs.size(); ++i ) {
                value += ";";
                value += x_MakeGffDbtag( *dbxrefs[ i ] );
            }
            m_Attributes[ "Dbxref" ] = value;
        }
    }
    if ( feature.CanGetComment() ) {
        m_Attributes[ "Note" ] = feature.GetComment();
    }
    if ( feature.CanGetPseudo()  &&  feature.GetPseudo() ) {
        m_Attributes[ "pseudo" ] = "";
    }
    if ( feature.CanGetPartial()  &&  feature.GetPartial() ) {
        m_Attributes[ "partial" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGff2WriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGff2WriteRecord::TAttrIt it;

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
bool CGff3WriteRecord::AssignParent(
    const CGff3WriteRecord& parent )
//  ----------------------------------------------------------------------------
{
    string strParentId;
    if ( ! parent.GetAttribute( "ID", strParentId ) ) {
        cerr << "Fix me: Parent record without GFF3 ID tag!" << endl;
        return false;
    }
    this->m_Attributes[ "Parent" ] = strParentId;
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
