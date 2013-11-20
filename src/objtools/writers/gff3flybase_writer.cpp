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
 * Authors:
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/util/sequence.hpp> 
#include <objtools/alnmgr/alnmap.hpp>
 
#include <objtools/writers/gff3flybase_record.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ----------------------------------------------------------------------------
static CConstRef<CSeq_id> s_GetSourceId(
    const CSeq_id& id, CScope& scope )
//  ----------------------------------------------------------------------------
{
    try {
        return sequence::GetId(id, scope, sequence::eGetId_Best).GetSeqId();
    }
    catch (CException&) {
    }
    return CConstRef<CSeq_id>(&id);
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << endl;
        m_Os << "#!gff-spec-version 1.20" << endl;
        m_Os << "##!gff-variant flybase" << endl;
        m_Os << "# This variant of GFF3 interprets ambiguities in the" << endl;
        m_Os << "# GFF3 specifications in accordance with the views of Flybase." << endl;
        m_Os << "# This impacts the feature tag set, and meaning of the phase." << endl;
        m_Os << "#!processor NCBI annotwriter" << endl;
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::x_WriteAlignDisc(
    const CSeq_align& align,
    bool bInvertWidth )
//  ----------------------------------------------------------------------------
{
    CGff3Writer::x_WriteAlignDisc(align, bInvertWidth);
    m_Os << "###" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::x_WriteAlignDenseg(
    const CSeq_align& align,
    bool bInvertWidth )
//  ----------------------------------------------------------------------------
{
    const CSeq_id& targetId = align.GetSeq_id( 1 );
    CBioseq_Handle bsh = m_pScope->GetBioseqHandle( targetId );
    CSeq_id_Handle pTargetId = bsh.GetSeq_id_Handle();
    CSeq_id_Handle best = sequence::GetId(bsh, sequence::eGetId_Best);
    if (best) {
        pTargetId = best;
    }

    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CRef<CDense_seg> ds_filled = ds.FillUnaligned();
    if ( bInvertWidth ) {
//        ds_filled->ResetWidths();
    }

    CAlnMap align_map( *ds_filled );

    int iTargetRow = -1;
    for ( int row = 0;  row < align_map.GetNumRows();  ++row ) {
        if ( sequence::IsSameBioseq(
            align_map.GetSeqId( row ), *pTargetId.GetSeqId(), m_pScope ) ) {
            iTargetRow = row;
            break;
        }
    }
    if ( iTargetRow == -1 ) {
        cerr << "No alignment row containing primary ID." << endl;
        return false;
    }
    
    TSeqPos iTargetWidth = 1;
    if ( static_cast<size_t>( iTargetRow ) < ds.GetWidths().size() ) {
        iTargetWidth = ds.GetWidths()[ iTargetRow ];
    }
    
    ENa_strand targetStrand = eNa_strand_plus;
    if ( align_map.StrandSign( iTargetRow ) != 1 ) {
        targetStrand = eNa_strand_minus;
    }
    
    CGffFeatureContext fc;
    
    for ( int iSourceRow = 0;  iSourceRow < align_map.GetNumRows();  ++iSourceRow ) {
        if ( iSourceRow == iTargetRow ) {
            continue;
        }
        CGff3FlybaseRecord record(fc, m_uRecordId );
    
        // Obtain and report basic source information:
        CConstRef<CSeq_id> pSourceId =
            s_GetSourceId( align_map.GetSeqId(iSourceRow), *m_pScope );
        TSeqPos iSourceWidth = 1;
        if ( static_cast<size_t>( iSourceRow ) < ds.GetWidths().size() ) {
            iSourceWidth = ds.GetWidths()[ iSourceRow ];
        }
        ENa_strand sourceStrand = eNa_strand_plus;
        if ( align_map.StrandSign( iSourceRow ) != 1 ) {
            sourceStrand = eNa_strand_minus;
        }
        record.SetSourceLocation( *pSourceId, sourceStrand );
    
        // Place insertions, deletion, matches, compute resulting source and target
        //  ranges:
        for ( int i0 = 0;  i0 < align_map.GetNumSegs();  ++i0 ) {
            CAlnMap::TSignedRange targetPiece = align_map.GetRange( iTargetRow, i0);
            CAlnMap::TSignedRange sourcePiece = align_map.GetRange( iSourceRow, i0 );
            CAlnMap::TSegTypeFlags targetFlags = align_map.GetSegType( iTargetRow, i0 );
            CAlnMap::TSegTypeFlags sourceFlags = align_map.GetSegType( iSourceRow, i0 );
    
            if ( INSERTION( targetFlags, sourceFlags ) ) {
                record.AddInsertion( CAlnMap::TSignedRange(
                    targetPiece.GetFrom() / iTargetWidth,
                    targetPiece.GetTo() / iTargetWidth ) );
            }
            if ( DELETION( targetFlags, sourceFlags ) ) {
                record.AddDeletion( CAlnMap::TSignedRange(
                    sourcePiece.GetFrom() / iSourceWidth,
                    sourcePiece.GetTo() / iSourceWidth ) );
            }
            if ( MATCH( targetFlags, sourceFlags ) ) {
                record.AddMatch(
                    CAlnMap::TSignedRange(
                        sourcePiece.GetFrom() / iSourceWidth,
                        sourcePiece.GetTo() / iSourceWidth ),
                    CAlnMap::TSignedRange(
                        targetPiece.GetFrom() / iTargetWidth,
                        targetPiece.GetTo() / iTargetWidth ) );
            }
            if (i0 == 0 && iSourceWidth == 3) {
                //record.SetPhase(sourcePiece.GetFrom() % 3);
            }
        }
    
        // Record basic target information:
        record.SetTargetLocation( *pTargetId.GetSeqId(), targetStrand );
    
        // Add scores, if available:
        if (align.IsSetScore()) {
            const vector<CRef<CScore> >& scores = align.GetScore();
            for (vector<CRef<CScore> >::const_iterator cit = scores.begin();
                    cit != scores.end(); ++cit) {
                 record.SetScore(**cit);
            }
        }
        if ( ds.IsSetScores() ) {
            ITERATE ( CDense_seg::TScores, score_it, ds.GetScores() ) {
                record.SetScore( **score_it );
            }
        }
        bsh = m_pScope->GetBioseqHandle(align_map.GetSeqId(iSourceRow) );
        string defline = sequence::GetTitle(bsh);
        unsigned taxid = sequence::GetTaxId(bsh);
        if (!defline.empty()) {
            record.SetDefline(defline);
        }
        if (taxid) {
            record.SetTaxid(taxid);
        }

        if ( bInvertWidth ) {
//            record.InvertWidth( 0 );
        }
        x_WriteAlignment( record );
    }
    return true;
}

END_NCBI_SCOPE
