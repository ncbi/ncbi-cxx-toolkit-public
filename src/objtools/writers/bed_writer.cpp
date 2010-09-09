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
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CBedWriter::CBedWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    m_Scope( scope ),
    m_Os( ostr ),
    m_uFlags( uFlags )
{
};

//  ----------------------------------------------------------------------------
CBedWriter::~CBedWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CBedWriter::WriteAnnot( const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( annot.IsFtable() ) {
        return x_WriteAnnotFTable( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedWriter::x_WriteAnnotFTable( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsFtable() ) {
        return false;
    }

    CBedTrackRecord track;
    if ( ! track.Assign( annot ) ) {
        return false;
    }
    x_WriteRecord( track );

    SAnnotSelector sel = x_GetAnnotSelector();
    CSeq_annot_Handle sah = m_Scope.AddSeq_annot( annot );
    CBedFeatureRecord record;
    for ( CFeat_CI pMf( sah, sel ); pMf; ++pMf ) {
        if ( ! record.AssignDisplayData( *pMf, track.UseScore() ) ) {
            continue;
        }

        CRef< CSeq_loc > pPackedInt( new CSeq_loc( CSeq_loc::e_Mix ) );
        pPackedInt->Add( pMf->GetLocation() );
        pPackedInt->ChangeToPackedInt();

        if ( pPackedInt->IsPacked_int() && pPackedInt->GetPacked_int().CanGet() ) {
            const list< CRef< CSeq_interval > >& sublocs 
                = pPackedInt->GetPacked_int().Get();
            list< CRef< CSeq_interval > >::const_iterator it;
            for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
                if ( record.AssignLocation( **it ) ) {
                    x_WriteRecord( record );
                }
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
SAnnotSelector CBedWriter::x_GetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.SetSortOrder( SAnnotSelector::eSortOrder_None );
    return sel;
}

//  ----------------------------------------------------------------------------
bool CBedWriter::x_WriteRecord(
    const CBedTrackRecord& track )
//  ----------------------------------------------------------------------------
{
    m_Os << "track";
    if ( ! track.Name().empty() ) {
        m_Os << " name=\"" << track.Name() << "\"";
    }
    if ( ! track.Title().empty() ) {
        m_Os << " title=\"" << track.Title() << "\"";
    }
    if ( track.UseScore() ) {
        m_Os << " useScore=1";
    }
    if ( track.ItemRgb() ) {
        m_Os << " itemRgb=\"on\"";
    }
    if ( ! track.Color().empty() ) {
        m_Os << " color=\"" << track.Color() << "\"";
    }
    if ( ! track.Visibility().empty() ) {
        m_Os << " visibility=" << track.Visibility();
    }
    m_Os << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedWriter::x_WriteRecord( 
    const CBedFeatureRecord& record )
//  ----------------------------------------------------------------------------
{
    m_Os << record.Chrom() << "\t";
    m_Os << record.ChromStart() << "\t";
    m_Os << record.ChromEnd() << "\t";
    m_Os << record.Name() << "\t";
    m_Os << record.Score() << "\t";
    m_Os << record.Strand() << "\t";
    m_Os << record.ThickStart() << "\t";
    m_Os << record.ThickEnd() << "\t";
    m_Os << record.ItemRgb() << "\t";
    m_Os << record.BlockCount() << "\t";
    m_Os << record.BlockSizes() << "\t";
    m_Os << record.BlockStarts() << endl;
    return true;
}

END_NCBI_SCOPE
