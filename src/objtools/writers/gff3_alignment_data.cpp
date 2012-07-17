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

#include <objtools/writers/gff3_alignment_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string CGffAlignmentRecord::StrAttributes() const 
//  ----------------------------------------------------------------------------
{
    string str = m_strAttributes;
    if ( !m_strOtherScores.empty() ) {
        str += ";";
        str += m_strOtherScores;
    }
    if ( !m_bIsTrivial ) {
        str += ";Gap=";
        str += m_strAlignment;
    }
    return str; 
};

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetTargetLocation(
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
    
    // set source field
    switch( id.Which() ) {
        default:
            m_strSource = CSeq_id::SelectionName( id.Which() );
            NStr::ToUpper( m_strSource );
        
        case CSeq_id::e_Local: 
            m_strSource = "Local";
            return;
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
        case CSeq_id::e_Gi:
            m_strSource = "GenInfo";
            return;
        case CSeq_id::e_Genbank:
            m_strSource = "Genbank";
            return;
        case CSeq_id::e_Swissprot:
            m_strSource = "SwissProt";
            return;
        case CSeq_id::e_Patent:
            m_strSource = "Patent";
            return;
        case CSeq_id::e_Other:
            m_strSource = "RefSeq";
            return;
        case CSeq_id::e_General:
            m_strSource = id.GetGeneral().GetDb();
            return;
    }
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetSourceLocation(
    const CSeq_id& id,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    id.GetLabel( &m_strId, CSeq_id::eContent );
    m_peStrand = new ENa_strand( eStrand );
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetScore(
    const CScore& score )
//  ----------------------------------------------------------------------------
{
    if ( !score.IsSetId() || !score.GetId().IsStr() || !score.IsSetValue() ) {
        return;
    }
    string key = score.GetId().GetStr();
    double dValue( 0 );
    if ( score.GetValue().IsInt() ) {
            dValue = double( score.GetValue().GetInt() );
    }
    else {
        dValue = score.GetValue().GetReal();
    }
    if ( key == "score" ) {
        m_pdScore = new double( dValue );
    }
    else {
        if ( ! m_strOtherScores.empty() ) {
            m_strOtherScores += ";";
        }
        m_strOtherScores += key;
        m_strOtherScores += "=";
        m_strOtherScores += NStr::DoubleToString( dValue );
    }
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetMatchType(
    const CSeq_id& source,
    const CSeq_id& target)
//  ----------------------------------------------------------------------------
{
    static char* strProtMatch     = "protein_match";
    static char* strEstMatch      = "EST_match";
    static char* strTransNucMatch = "translated_nucleotide_match";
    static char* strCdnaMatch     = "cDNA_match";

    CSeq_id::EAccessionInfo source_info = source.IdentifyAccession();
    CSeq_id::EAccessionInfo target_info = target.IdentifyAccession();
    if (target_info & CSeq_id::fAcc_prot) {
        m_strType = strProtMatch;
        return;
    }
    if ((target_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) {
        m_strType = strEstMatch;
        return;
    }
    if ((target_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_mrna) {
        m_strType = strCdnaMatch;
        return;
    }
    if ((target_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_tsa) {
        m_strType = strCdnaMatch;
        return;
    }
    if (source_info & CSeq_id::fAcc_prot) {
        m_strType = strTransNucMatch;
        return;
    }
//    m_strType = "match";
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetPhase(
    unsigned int uPhase )
//  ----------------------------------------------------------------------------
{
    m_puPhase = new unsigned int( uPhase );
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::AddInsertion(
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
void CGffAlignmentRecord::AddDeletion(
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
void CGffAlignmentRecord::AddMatch(
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
