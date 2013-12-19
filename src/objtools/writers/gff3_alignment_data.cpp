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

#include <objtools/writers/gff3_alignment_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

CGffFeatureContext CGffAlignmentRecord::sDummyContext;

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
void CGffAlignmentRecord::SetScore(
    const CScore& score )
//  ----------------------------------------------------------------------------
{
    if ( !score.IsSetId() || !score.GetId().IsStr() || !score.IsSetValue() ) {
        return;
    }
    string key = score.GetId().GetStr();
    string value;
    if ( score.GetValue().IsInt() ) {
            value = NStr::IntToString(score.GetValue().GetInt());
    }
    else {
        value = NStr::DoubleToString(score.GetValue().GetReal());
    }
    if ( key == "score" ) {
        m_pScore = new string(value);
    }
    else {
        if ( ! m_strOtherScores.empty() ) {
            m_strOtherScores += ";";
        }
        m_strOtherScores += key;
        m_strOtherScores += "=";
        m_strOtherScores += value;
    }
}

//  ----------------------------------------------------------------------------
void CGffAlignmentRecord::SetMatchType(
    const CSeq_id& source,
    const CSeq_id& target)
//  ----------------------------------------------------------------------------
{
    const char* strProtMatch     = "protein_match";
    const char* strEstMatch      = "EST_match";
    const char* strTransNucMatch = "translated_nucleotide_match";
    const char* strCdnaMatch     = "cDNA_match";

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
    //m_strType = "match";
}

END_objects_SCOPE
END_NCBI_SCOPE
