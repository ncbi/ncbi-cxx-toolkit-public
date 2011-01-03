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
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CScope& scope,
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
    m_uPendingExonId = 0;
};

//  ----------------------------------------------------------------------------
CGff3Writer::~CGff3Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "##gff-version 3" << endl;
    return true;
}

//  ============================================================================
CGff2WriteRecord* CGff3Writer::x_CreateRecord(
    feature::CFeatTree& feat_tree )
//  ============================================================================
{
    return new CGff3WriteRecord( feat_tree );
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_AssignObject(
   feature::CFeatTree& feat_tree,
   CMappedFeat mapped_feature,        
   CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    switch( mapped_feature.GetFeatSubtype() ) {

    default:
        return CGff2Writer::x_AssignObject( feat_tree, mapped_feature, set ); 

    case CSeqFeatData::eSubtype_gene:
        return x_AssignObjectGene( feat_tree, mapped_feature, set );

    case CSeqFeatData::eSubtype_mRNA:
        return x_AssignObjectMrna( feat_tree, mapped_feature, set );

    case CSeqFeatData::eSubtype_cdregion:
        return x_AssignObjectCds( feat_tree, mapped_feature, set );
    }   
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_AssignObjectGene(
   feature::CFeatTree& feat_tree,
   CMappedFeat mapped_feature,        
   CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feat = mapped_feature.GetOriginalFeature();        

    CGff3WriteRecord* pGene = dynamic_cast< CGff3WriteRecord* >(
        x_CreateRecord( feat_tree ) );
    if ( ! pGene->AssignFromAsn( mapped_feature ) ) {
        return false;
    }

    string strId;
    if ( ! pGene->GetAttribute( "ID", strId ) ) {
        pGene->ForceAttributeID( 
            string( "gene" ) + NStr::UIntToString( m_uPendingGeneId++ ) );
    }

    set.AddRecord( pGene );
    m_GeneMap[ mapped_feature ] = pGene;
    return true;

}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_AssignObjectMrna(
   feature::CFeatTree& feat_tree,
   CMappedFeat mapped_feature,        
   CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feat = mapped_feature.GetOriginalFeature();        

    CGff3WriteRecord* pMrna = dynamic_cast< CGff3WriteRecord* >(
        x_CreateRecord( feat_tree ) );
    if ( ! pMrna->AssignFromAsn( mapped_feature ) ) {
        return false;
    }
    CMappedFeat gene = feature::GetBestGeneForMrna( mapped_feature, &feat_tree );
    TGeneMap::iterator it = m_GeneMap.find( gene );
    if ( it != m_GeneMap.end() ) {
        pMrna->AssignParent( *(it->second) );
    }

    string strId;
    if ( ! pMrna->GetAttribute( "ID", strId ) ) {
        pMrna->ForceAttributeID( 
            string( "mrna" ) + NStr::UIntToString( m_uPendingMrnaId++ ) );
    }
    set.AddRecord( pMrna );
    m_MrnaMap[ mapped_feature ] = pMrna;

    CRef< CSeq_loc > pPackedInt( new CSeq_loc( CSeq_loc::e_Mix ) );
    pPackedInt->Add( mapped_feature.GetLocation() );
    pPackedInt->ChangeToPackedInt();

    if ( pPackedInt->IsPacked_int() && pPackedInt->GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = pPackedInt->GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        unsigned int uSequenceNumber = 1;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CGff3WriteRecord* pExon = dynamic_cast< CGff3WriteRecord* >(
                x_CloneRecord( *pMrna ) );
            pExon->AssignType( "exon" );
            pExon->AssignParent( *pMrna );
            pExon->AssignLocation( subint );
            pExon->AssignSequenceNumber( uSequenceNumber++, "E" );
            string strId;
            if ( ! pExon->GetAttribute( "ID", strId ) ) {
                pExon->ForceAttributeID( 
                    string( "exon" ) + NStr::UIntToString( m_uPendingExonId++ ) );
            }
            set.AddRecord( pExon );
        }
        return true;
    }

    return true;    
}
    
//  ----------------------------------------------------------------------------
bool CGff3Writer::x_AssignObjectCds(
   feature::CFeatTree& feat_tree,
   CMappedFeat mapped_feature,        
   CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feat = mapped_feature.GetOriginalFeature();        

    CGff3WriteRecord* pCds = dynamic_cast< CGff3WriteRecord* >(
        x_CreateRecord( feat_tree ) );
    if ( ! pCds->AssignFromAsn( mapped_feature ) ) {
        return false;
    }
    CMappedFeat mrna = feature::GetBestMrnaForCds( mapped_feature, &feat_tree );
    TMrnaMap::iterator it = m_MrnaMap.find( mrna );
    if ( it != m_MrnaMap.end() ) {
        pCds->AssignParent( *(it->second) );
    }

    CRef< CSeq_loc > pPackedInt( new CSeq_loc( CSeq_loc::e_Mix ) );
    pPackedInt->Add( mapped_feature.GetLocation() );
    pPackedInt->ChangeToPackedInt();

    if ( pPackedInt->IsPacked_int() && pPackedInt->GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = pPackedInt->GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        unsigned int uSequenceNumber = 1;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CGff3WriteRecord* pExon = dynamic_cast< CGff3WriteRecord* >(
                x_CloneRecord( *pCds ) );
            pExon->AssignType( "CDS" );
            pExon->AssignLocation( subint );
//            pExon->AssignSequenceNumber( uSequenceNumber++, "C" );
            set.AddRecord( pExon );
        }
    }

    delete pCds;
    return true;    
}

//  ============================================================================
CGff2WriteRecord* CGff3Writer::x_CloneRecord(
    const CGff2WriteRecord& other )
//  ============================================================================
{
    return new CGff3WriteRecord( other );
}

END_NCBI_SCOPE
