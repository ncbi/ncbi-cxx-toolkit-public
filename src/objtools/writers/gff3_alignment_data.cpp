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
            // "for historical reasons"
            m_strSource = "RefSeq";
            return;
        case CSeq_id::e_Ddbj:
            m_strSource = "DDBJ";
            return;
        case CSeq_id::e_Embl:
            m_strSource = "EMBL";
            return;
        case CSeq_id::e_Pir:
            m_strSource = "PIR";
            return;
        case CSeq_id::e_Prf:
            m_strSource = "PRF";
            return;
        case CSeq_id::e_Pdb:
            m_strSource = "PDB";
            return;
        case CSeq_id::e_Tpg:
            m_strSource = "tpg";
            return;
        case CSeq_id::e_Tpe:
            m_strSource = "tpe";
            return;
        case CSeq_id::e_Tpd:
            m_strSource = "tpd";
            return;
        case CSeq_id::e_Gpipe:
            m_strSource = "gpipe";
            return;
        case CSeq_id::e_Named_annot_track :
            m_strSource = "NADB";
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

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetIdSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();
    m_strId.clear();
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
         genomicId, scope, sequence::eGetId_Best);
    bestH.GetSeqId()->GetLabel(&m_strId, CSeq_id::eContent);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetMethodSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

    //maybe look at ModelEvidence first?

#define EMIT(str) { m_strSource = str; break; }
    m_strSource.clear();
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
        genomicId, scope, sequence::eGetId_Best);
    const CSeq_id& bestId = *bestH.GetSeqId();
    switch(bestId.Which()) {
    default:
        break;
    case CSeq_id::e_Local: EMIT("Local");
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Gi: EMIT("GenInfo");
    case CSeq_id::e_Genbank: EMIT("Genbank");
    case CSeq_id::e_Swissprot: EMIT("SwissProt");
    case CSeq_id::e_Patent: EMIT("Patent");
    case CSeq_id::e_Other: EMIT("RefSeq");
    case CSeq_id::e_General: 
        EMIT(genomicId.GetGeneral().GetDb());
    }
    if (m_strSource.empty()) {
        m_strSource = CSeq_id::SelectionName(genomicId.Which());
        NStr::ToUpper(m_strSource);
    }
#undef EMIT
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetTypeSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

    const CSeq_id& genomicId = *sequence::GetId(
        spliced.GetGenomic_id(), scope, sequence::eGetId_Best).GetSeqId();
    const CSeq_id& productId =*sequence::GetId(
        spliced.GetProduct_id(), scope, sequence::eGetId_Best).GetSeqId();
    SetMatchType(genomicId, productId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetLocationSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

    m_uSeqStart = exon.GetGenomic_start();
    m_uSeqStop = exon.GetGenomic_end();
    try {
        this->m_peStrand = new ENa_strand(exon.GetGenomic_strand());
    }
    catch(std::exception&) {
        try {
            this->m_peStrand = new ENa_strand(spliced.GetGenomic_strand());
        }
        catch(std::exception&) {}
    };
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetPhaseSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    //meaningless for alignments

    //_ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    //const CSpliced_seg& spliced = align.GetSegs().GetSpliced();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetAttributesSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    //note: depends on xSetLocationSpliced already been called

    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

    //Target:
    string targetStr("");
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, scope, sequence::eGetId_Best);
    bestH.GetSeqId()->GetLabel(&targetStr, CSeq_id::eContent);

    string start = NStr::IntToString(exon.GetProduct_start().AsSeqPos()+1);
    string stop = NStr::IntToString(exon.GetProduct_end().AsSeqPos()+1);
    string strand = "+";
    m_strAttributes += 
        ";Target=" + targetStr + " " + start + " " + stop + " " + strand;

    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetScoresSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

    map<string, string> scoreAttrs;
    if (align.IsSetScore()) {
        typedef vector<CRef<CScore> > SCORES;

        const SCORES& scores = align.GetScore();
        for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); ++cit) {
            const CScore& score = **cit;
            if (!score.IsSetId() || !score.GetId().IsStr()  ||  !score.IsSetValue()) {
                continue;
            }
            const string& key = score.GetId().GetStr();
            const CScore::TValue& value = score.GetValue();
            switch(value.Which()) {
            default:
                continue;
            case CScore::TValue::e_Int:
                scoreAttrs[key] = NStr::IntToString(value.GetInt());
                continue;
            case CScore::TValue::e_Real:
                scoreAttrs[key] = NStr::DoubleToString(value.GetReal());
                continue;
            }
        }
    }
    if (exon.IsSetScores()) {
        typedef list<CRef<CScore> > SCORES;

        const SCORES& scores = exon.GetScores().Get();
        for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); ++cit) {
            const CScore& score = **cit;
            if (!score.IsSetId() || !score.GetId().IsStr()  ||  !score.IsSetValue()) {
                continue;
            }
            const string& key = score.GetId().GetStr();
            const CScore::TValue& value = score.GetValue();
            if (key == "score") {
                switch(value.Which()) {
                default:
                    continue;
                case CScore::TValue::e_Int:
                    m_pdScore = new double(value.GetInt());
                    continue;
                case CScore::TValue::e_Real:
                    m_pdScore = new double(value.GetReal());
                    continue;
                }
            }
            else {
                switch(value.Which()) {
                default:
                    continue;
                case CScore::TValue::e_Int:
                    scoreAttrs[key] = NStr::IntToString(value.GetInt());
                    continue;
                case CScore::TValue::e_Real:
                    scoreAttrs[key] = NStr::DoubleToString(value.GetReal());
                    continue;
                }
            }
        }
    }
    for (map<string, string>::const_iterator cit = scoreAttrs.begin(); 
            cit != scoreAttrs.end(); ++cit) {
        if (!m_strOtherScores.empty()) {
            m_strOtherScores += ";";
        }
        m_strOtherScores += cit->first + "=" + cit->second;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffAlignmentRecord::xSetAlignmentSpliced(
    CScope& scope,
    const CSeq_align& align,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());
    if (!exon.IsSetParts()) {
        return true;
    }

    typedef list<CRef<CSpliced_exon_chunk> > CHUNKS;

    const CHUNKS& chunks = exon.GetParts();
    m_strAlignment.clear();
    m_bIsTrivial = true;
    for (CHUNKS::const_iterator cit = chunks.begin(); cit != chunks.end(); ++cit) {
        const CSpliced_exon_chunk& chunk = **cit;
        switch (chunk.Which()) {
        default:
            break;
        case CSpliced_exon_chunk::e_Match:
            m_strAlignment += "M" + NStr::IntToString(chunk.GetMatch());
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            m_strAlignment += "D" + NStr::IntToString(chunk.GetGenomic_ins());
            m_bIsTrivial = false;
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            m_strAlignment += "I" + NStr::IntToString(chunk.GetProduct_ins());
            m_bIsTrivial = false;
            break;
        }
        m_strAlignment += " ";
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
