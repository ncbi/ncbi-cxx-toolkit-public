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
void CGffAlignmentRecord::SetTargetLocation(
    const CSeq_id& id,
    TSeqPos uFrom,
    TSeqPos uTo,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    if ( ! m_strAttributes.empty() ) {
        m_strAttributes += ";";
    }
    string strTarget;
    id.GetLabel( &strTarget, CSeq_id::eContent );
    strTarget += " ";
    strTarget += NStr::UIntToString( uFrom+1 );
    strTarget += " ";
    strTarget += NStr::UIntToString( uTo+1 );
    strTarget += " ";
    strTarget += (eStrand == eNa_strand_plus) ? "+" : "-";
    m_strAttributes += ( "Target=\"" + strTarget + "\"" );
    
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
    };
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetSourceLocation(
    const CSeq_id& id,
    TSeqPos uFrom,
    TSeqPos uTo,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    id.GetLabel( &m_strId, CSeq_id::eContent );
    m_peStrand = new ENa_strand( eStrand );
    m_uSeqStart = uFrom;
    m_uSeqStop = uTo;
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetScore(
    const CScore& score )
//  ----------------------------------------------------------------------------
{
    if ( score.IsSetId() && score.GetId().IsStr() && score.IsSetValue() ) {
        if ( score.GetId().GetStr() == "score" ) {
            if ( score.GetValue().IsInt() ) {
                m_pdScore = new double( score.GetValue().GetInt() );
            }
            else {
                m_pdScore = new double( score.GetValue().GetReal() );
            }
        }
        else {
            cerr << "FIXME: " << "CGffAlignmentRecord::SetScore" << endl;
        }
    }
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetPhase(
    unsigned int uPhase )
//  ----------------------------------------------------------------------------
{
    m_puPhase = new unsigned int( uPhase );
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::AddAlignmentPiece(
    char cType,
    unsigned int uSize )
//  ----------------------------------------------------------------------------
{
    if ( 0 == uSize ) {
        return;
    }
    if ( ! m_strAlignment.empty() ) {
        m_strAlignment += "+";
    }
    m_strAlignment += cType;
    m_strAlignment += NStr::IntToString( uSize );
}

END_objects_SCOPE
END_NCBI_SCOPE
