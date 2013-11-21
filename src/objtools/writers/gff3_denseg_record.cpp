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
void CGffDenseSegRecord::SetSourceLocation(
    const CSeq_id& id,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    id.GetLabel( &m_strId, CSeq_id::eContent );
    m_peStrand = new ENa_strand( eStrand );
}

//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::SetTargetLocation(
    const CSeq_id& id,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    if ( ! m_strAttributes.empty() ) {
        m_strAttributes += ";";
    }
    string strTarget;
    id.GetLabel( &strTarget, CSeq_id::eContent );
    strTarget += " ";
    strTarget += NStr::UIntToString( m_targetRange.GetFrom()+1 );
    strTarget += " ";
    strTarget += NStr::UIntToString( m_targetRange.GetTo()+1 );
    strTarget += " ";
    strTarget += (eStrand == eNa_strand_plus) ? "+" : "-";
    m_strAttributes += ( "Target=" + strTarget );
    CWriteUtil::GetIdType(id, m_strSource);
}

//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::AddInsertion(
    const CAlnMap::TSignedRange& targetPiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = targetPiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    m_bIsTrivial = false;
    m_targetRange += targetPiece;

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "I";
    m_strAlignment += NStr::IntToString( uSize );
}
    
//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::AddDeletion(
    const CAlnMap::TSignedRange& sourcePiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = sourcePiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    m_bIsTrivial = false;
    m_sourceRange += sourcePiece;
    m_uSeqStart = m_sourceRange.GetFrom();
    m_uSeqStop = m_sourceRange.GetTo();

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "D";
    m_strAlignment += NStr::IntToString( uSize );
}
    
//  ----------------------------------------------------------------------------
void CGffDenseSegRecord::AddMatch(
    const CAlnMap::TSignedRange& sourcePiece,
    const CAlnMap::TSignedRange& targetPiece )
//  ----------------------------------------------------------------------------
{
    unsigned int uSize = sourcePiece.GetLength();
    if ( 0 == uSize ) {
        return;
    }
    m_sourceRange += sourcePiece;
    m_targetRange += targetPiece;
    m_uSeqStart = m_sourceRange.GetFrom();
    m_uSeqStop = m_sourceRange.GetTo();

    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += " ";
    }
    m_strAlignment += "M";
    m_strAlignment += NStr::IntToString( uSize );
}

END_objects_SCOPE
END_NCBI_SCOPE
