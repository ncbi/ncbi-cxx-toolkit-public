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

#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CScope&scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
};

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags )
{
};

//  ----------------------------------------------------------------------------
CGtfWriter::~CGtfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "#gtf-version 2.2" << endl;
        m_bHeaderWritten = true;
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteRecord( 
    const CGffWriteRecord* pRecord )
//  ----------------------------------------------------------------------------
{
    m_Os << pRecord->StrId() << '\t';
    m_Os << pRecord->StrSource() << '\t';
    m_Os << pRecord->StrType() << '\t';
    m_Os << pRecord->StrSeqStart() << '\t';
    m_Os << pRecord->StrSeqStop() << '\t';
    m_Os << pRecord->StrScore() << '\t';
    m_Os << pRecord->StrStrand() << '\t';
    m_Os << pRecord->StrPhase() << '\t';

    if ( m_uFlags & fStructibutes ) {
        m_Os << pRecord->StrStructibutes() << endl;
    }
    else {
        m_Os << pRecord->StrAttributes() << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteFeature(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    switch( mf.GetFeatSubtype() ) {
        default:
            // GTF is not interested --- ignore
            return true;
        case CSeqFeatData::eSubtype_gene: 
            return x_WriteFeatureGene( context, mf );
        case CSeqFeatData::eSubtype_mRNA:
            return x_WriteFeatureMrna( context, mf );
        case CSeqFeatData::eSubtype_cdregion:
            return x_WriteFeatureCds( context, mf );
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteFeatureGene(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (m_uFlags & fNoGeneFeatures) {
        return true;
    }

    CRef<CGtfRecord> pRecord( 
        new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
    if (!pRecord->AssignFromAsn(mf)) {
        return false;
    }
    return x_WriteRecord(pRecord);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteFeatureMrna(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_exonMap.clear();

    CRef<CGtfRecord> pMrna( new CGtfRecord( context ) );
    if ( ! pMrna->AssignFromAsn( mf ) ) {
        return false;
    }
    pMrna->CorrectType( "exon" );

    const CSeq_loc& loc = mf.GetLocation();
    unsigned int uExonNumber = 1;

    CRef< CSeq_loc > pLocMrna( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocMrna->Add( loc );
    pLocMrna->ChangeToPackedInt();

    if ( pLocMrna->IsPacked_int() && pLocMrna->GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > >& sublocs = pLocMrna->SetPacked_int().Set();
        list< CRef< CSeq_interval > >::iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            CSeq_interval& subint = **it;
            CRef<CGtfRecord> pExon( 
                new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
            pExon->MakeChildRecord( *pMrna, subint, uExonNumber++ );
            x_WriteRecord( pExon );
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteFeatureCds(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGtfRecord> pParent( 
        new CGtfRecord( context, (m_uFlags & fNoExonNumbers) ) );
    if ( ! pParent->AssignFromAsn( mf ) ) {
        return false;
    }

    CRef< CSeq_loc > pLocStartCodon;
    CRef< CSeq_loc > pLocCode;
    CRef< CSeq_loc > pLocStopCodon;
    if ( ! x_SplitCdsLocation( mf, pLocStartCodon, pLocCode, pLocStopCodon ) ) {
        return false;
    }

    if ( pLocCode ) {
        pParent->CorrectType( "CDS" );
        if ( ! x_WriteFeatureCdsFragments( *pParent, *pLocCode ) ) {
            return false;
        }
    }
    if ( pLocStartCodon ) {
        pParent->CorrectType( "start_codon" );
        if ( ! x_WriteFeatureCdsFragments( *pParent, *pLocStartCodon ) ) {
            return false;
        }
    }
    if ( pLocStopCodon ) {
        pParent->CorrectType( "stop_codon" );
        if ( ! x_WriteFeatureCdsFragments( *pParent, *pLocStopCodon ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteFeatureCdsFragments(
    CGtfRecord& record,
    const CSeq_loc& location )
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> > SUBINTS;
    const SUBINTS& subints = location.GetPacked_int().Get();
    for ( SUBINTS::const_iterator it = subints.begin(); it != subints.end(); ++it ) {
        const CSeq_interval& subint = **it;
        unsigned int uExonNumber = 0;
        for ( TExonCit xit = m_exonMap.begin(); xit != m_exonMap.end(); ++xit ) {
            const CSeq_interval& compint = *(xit->second);
            if ( compint.GetFrom() <= subint.GetFrom()  &&  compint.GetTo() >= subint.GetTo() ) {
                uExonNumber = xit->first;
                break;
            }
        }
        CGtfRecord* pRecord = new CGtfRecord(record);
        pRecord->MakeChildRecord( record, subint, uExonNumber );
        pRecord->SetCdsPhase( subints, location.GetStrand() );
        x_WriteRecord( pRecord );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_SplitCdsLocation(
    CMappedFeat cds,
    CRef< CSeq_loc >& pLocStartCodon,
    CRef< CSeq_loc >& pLocCode,
    CRef< CSeq_loc >& pLocStopCodon ) const
//  ----------------------------------------------------------------------------
{
    //  Note: pLocCode will contain the location of the start codon but not the
    //  stop codon.

    const CSeq_loc& cdsLocation = cds.GetLocation();
    if ( cdsLocation.GetTotalRange().GetLength() < 6 ) {
        return false;
    }
    CSeq_id cdsLocId( cdsLocation.GetId()->AsFastaString() );

    pLocStartCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocCode.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocStopCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 

    pLocCode->Add( cdsLocation );
    CRef< CSeq_loc > pLocCode2( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocCode2->Add( cdsLocation );

    CSeq_loc interval;
    interval.SetInt().SetId( cdsLocId );
    interval.SetStrand( cdsLocation.GetStrand() );

    for ( size_t u = 0; u < 3; ++u ) {

        size_t uLowest = pLocCode2->GetTotalRange().GetFrom();
        interval.SetInt().SetFrom( uLowest );
        interval.SetInt().SetTo( uLowest );
        pLocStartCodon = pLocStartCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode2 = pLocCode2->Subtract( 
            interval, CSeq_loc::fSortAndMerge_All, 0, 0 );    

        size_t uHighest = pLocCode->GetTotalRange().GetTo();
        interval.SetInt().SetFrom( uHighest );
        interval.SetInt().SetTo( uHighest );
        pLocStopCodon = pLocStopCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode = pLocCode->Subtract( 
            interval, CSeq_loc::fSortAndMerge_All, 0, 0 );    
    }

    if ( cdsLocation.GetStrand() == eNa_strand_minus ) {
        pLocCode = pLocCode2;
        pLocCode2 = pLocStartCodon;
        pLocStartCodon = pLocStopCodon;
        pLocStopCodon = pLocCode2;
    }

    pLocStartCodon->ChangeToPackedInt();
    pLocCode->ChangeToPackedInt();
    pLocStopCodon->ChangeToPackedInt();

    return true;
}

//  ----------------------------------------------------------------------------
SAnnotSelector CGtfWriter::x_GetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    sel.SetSortOrder( SAnnotSelector::eSortOrder_Normal );
    return sel;
}

END_NCBI_SCOPE
