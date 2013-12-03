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
#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>

#include <objtools/writers/gff3_denseg_record.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::xAddInsertion(
    const CAlnMap::TSignedRange& targetPiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = targetPiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    m_bIsTrivial = false;
    mTargetRange.CombineWith(targetPiece);

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "I";
    m_strAlignment += NStr::IntToString( uSize );
}
    
//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::xAddDeletion(
    const CAlnMap::TSignedRange& sourcePiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = sourcePiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    m_bIsTrivial = false;
    mSourceRange.CombineWith(sourcePiece);
    m_uSeqStart = mSourceRange.GetFrom();
    m_uSeqStop = mSourceRange.GetTo();

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "D";
    m_strAlignment += NStr::IntToString( uSize );
}
    
//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::xAddMatch(
    const CAlnMap::TSignedRange& sourcePiece,
    const CAlnMap::TSignedRange& targetPiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = sourcePiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    mSourceRange.CombineWith(sourcePiece);
    mTargetRange.CombineWith(targetPiece);
    m_uSeqStart = mSourceRange.GetFrom();
    m_uSeqStop = mSourceRange.GetTo();

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "M";
    m_strAlignment += NStr::IntToString( uSize );
}

//  ============================================================================
bool CGffDenseSegRecord::Initialize(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    if (!xInitAlignmentIds(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetId(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetMethod(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetType(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetAlignment(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetScores(scope, align, alnMap, row)) {
        return false;
    }
    if (!xSetAttributes(scope, align, alnMap, row)) {
        return false;
    }
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xSetId(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    _ASSERT(mpTargetId);
    m_strId.clear();
    mpTargetId->GetLabel( &m_strId, CSeq_id::eContent );
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xSetType(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    _ASSERT(mpTargetId);
    _ASSERT(mpSourceId);
    SetMatchType(*mpTargetId, *mpSourceId);
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xSetMethod(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    _ASSERT(mpSourceId);
    CWriteUtil::GetIdType(*mpSourceId, m_strSource);
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xSetAlignment(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    m_peStrand = new ENa_strand(eNa_strand_plus);
    if (alnMap.StrandSign(row) != 1) {
        *m_peStrand = eNa_strand_minus;
    }

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
    const CDense_seg& denseg = alnMap.GetDenseg();
    unsigned int sourceWidth = 1;
    if (static_cast<size_t>(row) < denseg.GetWidths().size()) {
        sourceWidth = denseg.GetWidths()[row];
    }
    unsigned int targetWidth = 1;
    if ( denseg.GetWidths().size() ) {
        targetWidth = denseg.GetWidths()[0];
    }

    // Place insertions, deletion, matches, compute resulting source and target 
    //  ranges:
    unsigned int numSegs = alnMap.GetNumSegs();
    for ( unsigned int seg = 0;  seg < numSegs;  ++seg ) {

        CAlnMap::TSignedRange targetPiece = alnMap.GetRange(0, seg);
        CAlnMap::TSignedRange sourcePiece = alnMap.GetRange(row, seg);
        CAlnMap::TSegTypeFlags targetFlags = alnMap.GetSegType(0, seg);
        CAlnMap::TSegTypeFlags sourceFlags = alnMap.GetSegType(row, seg);

        if (INSERTION(targetFlags, sourceFlags)) {
            xAddInsertion( CAlnMap::TSignedRange( 
                targetPiece.GetFrom()/targetWidth, targetPiece.GetTo()/targetWidth));
            continue;
        }
        if (DELETION(targetFlags,sourceFlags)) {
            xAddDeletion(CAlnMap::TSignedRange( 
                sourcePiece.GetFrom()/sourceWidth, sourcePiece.GetTo()/sourceWidth));
            continue;
        }
        if (MATCH(targetFlags, sourceFlags)) {
            xAddMatch(
                CAlnMap::TSignedRange( 
                    sourcePiece.GetFrom()/sourceWidth, sourcePiece.GetTo()/sourceWidth), 
                CAlnMap::TSignedRange( 
                    targetPiece.GetFrom()/targetWidth, targetPiece.GetTo()/targetWidth));
            continue;
        }
    }
    return true;
#undef INSERTION
#undef DELETION
#undef MATCH
}

//  ============================================================================
bool CGffDenseSegRecord::xSetScores(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    if (align.IsSetScore()) {
        const vector<CRef<CScore> >& scores = align.GetScore();
        for (vector<CRef<CScore> >::const_iterator cit = scores.begin(); 
                cit != scores.end(); ++cit) {
            SetScore(**cit);
        }
    }
    const CDense_seg& denseg = alnMap.GetDenseg();
    if ( denseg.IsSetScores() ) {
        ITERATE ( CDense_seg::TScores, score_it, denseg.GetScores() ) {
            SetScore( **score_it );
        }
    }
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xSetAttributes(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    _ASSERT(mpSourceId);

    //Target=
    if (!m_strAttributes.empty()) {
        m_strAttributes += ";";
    }
    ENa_strand strand = eNa_strand_plus;
    if (alnMap.StrandSign(0) == -1) {
        strand = eNa_strand_minus;
    }
    string strTarget;
    mpSourceId->GetLabel(&strTarget, CSeq_id::eContent);
    strTarget += " ";
    strTarget += NStr::UIntToString(mTargetRange.GetFrom()+1);
    strTarget += " ";
    strTarget += NStr::UIntToString(mTargetRange.GetTo()+1);
    strTarget += " ";
    strTarget += (strand == eNa_strand_plus) ? "+" : "-";
    m_strAttributes += "Target=" + strTarget;

    //Gap=
    return true;
}

//  ============================================================================
bool CGffDenseSegRecord::xInitAlignmentIds(
    CScope& scope,
    const CSeq_align& align,
    const CAlnMap& alnMap,
    unsigned int row)
//  ============================================================================
{
    const CSeq_id& productId = alnMap.GetSeqId(row);
    CBioseq_Handle bsh = scope.GetBioseqHandle(productId);
    CSeq_id_Handle pTargetId = bsh.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(bsh, sequence::eGetId_ForceAcc);
        if (best) {
            pTargetId = best;
        }
    }
    catch(std::exception&) {};
    mpTargetId = pTargetId.GetSeqId();

    const CSeq_id& sourceId = align.GetSeq_id(0);
    CBioseq_Handle bshS = scope.GetBioseqHandle(sourceId);
    CSeq_id_Handle pSourceId = bshS.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(bshS, sequence::eGetId_Best);
        if (best) {
            pSourceId = best;
        }
    }
    catch(std::exception&) {};
    mpSourceId = pSourceId.GetSeqId();

    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
