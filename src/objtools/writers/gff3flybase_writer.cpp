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
#include <objmgr/util/create_defline.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/writers/gff3flybase_writer.hpp>
#include <objects/seqalign/Prot_pos.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )


//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xIsNeededScore(
    const string& seqId,
    const CScore& score) const
//  ----------------------------------------------------------------------------
{
    static const vector<string> supportedScores{
        "Gap", "ambiguous_orientation", "consensus_splices",
        "pct_coverage", "pct_identity_gap", "pct_identity_ungap",
        "rank", "score"
    };
    static const vector<string> coreScores{
        "ID", "Target", "Gap"
    };

    if (!score.IsSetId()  ||  !score.GetId().IsStr()) {
        return false;
    }
    string key = score.GetId().GetStr();
    if (seqId == mCurrentIdForAttributes  &&  
            std::find(coreScores.begin(), coreScores.end(), key) == coreScores.end()) {
        return false;
    }
    if (std::find(supportedScores.begin(), supportedScores.end(), key)
            == supportedScores.end()) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignTaxid(
    CBioseq_Handle bsh,
    CGffAlignRecord& record)
//  ----------------------------------------------------------------------------
{
    const auto& seqId = record.StrSeqId();
    const auto& taxIdIt = mTaxidMap.find(seqId);
    if (taxIdIt != mTaxidMap.end()) {
        record.SetAttribute("taxid", taxIdIt->second);
        return true;
    }
    if (!bsh) {
        return false;
    }

    string taxonIdStr;
    for (CSeqdesc_CI sdit(bsh, CSeqdesc::e_Source); sdit; ++sdit) {
        const CBioSource& src = sdit->GetSource();
        if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDb()) {
            continue;
        }
        const auto& tags = src.GetOrg().GetDb();
        for (auto cit = tags.begin(); 
                taxonIdStr.empty()  &&  cit != tags.end(); ++cit) {
            const auto& tag = **cit;
            if (!tag.IsSetDb()  ||  tag.GetDb() != "taxon") {
                continue;
            }
            const auto& objid = tag.GetTag();
            switch (objid.Which()) {
                default:
                    break;
                case CObject_id::e_Str:
                    if (!objid.GetStr().empty()) {
                        taxonIdStr = objid.GetStr();
                    }
                    break;
                case CObject_id::e_Id:
                    taxonIdStr = NStr::IntToString(objid.GetId());
                    break;
            }
        }
    }
    if (!taxonIdStr.empty()) {
        record.SetAttribute("taxid", taxonIdStr);
        mTaxidMap[seqId] = taxonIdStr;
        return true;
    }
    return false;
}

 
//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignDefline(
    CBioseq_Handle bsh,
    CGffAlignRecord& record)
//  ----------------------------------------------------------------------------
{
    const auto& seqId = record.StrSeqId();
    const auto& deflineIt = mDeflineMap.find(seqId);
    if (deflineIt != mDeflineMap.end()) {
        record.SetAttribute("def", deflineIt->second);
        return true;
    }
    if (!bsh) {
        return false;
    }
    auto defline = sequence::CDeflineGenerator().GenerateDefline(bsh);
    record.SetAttribute("def", defline);
    mDeflineMap[seqId] = defline;
    return false;
}

    
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
        m_Os << "##gff-version 3" << '\n';
        m_Os << "#!gff-spec-version 1.20" << '\n';
        m_Os << "##!gff-variant flybase" << '\n';
        m_Os << "# This variant of GFF3 interprets ambiguities in the" << '\n';
        m_Os << "# GFF3 specifications in accordance with the views of Flybase." << '\n';
        m_Os << "# This impacts the feature tag set, and meaning of the phase." << '\n';
        m_Os << "#!processor NCBI annotwriter" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xWriteAlignDisc(
    const CSeq_align& align,
    const string& aid)
//  ----------------------------------------------------------------------------
{
    if (!CGff3Writer::xWriteAlignDisc(align, aid)) {
        return false;
    }
    m_Os << "###" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedLocation(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    bool isProteinProd = xSplicedSegHasProteinProd(spliced);

    const unsigned int tgtWidth = isProteinProd ? 3 : 1;

    const unsigned int seqStart = exon.GetProduct_start().AsSeqPos()/tgtWidth;
    const unsigned int seqStop = exon.GetProduct_end().AsSeqPos()/tgtWidth;


    ENa_strand seqStrand = eNa_strand_plus;
    if (spliced.CanGetProduct_strand()  &&  
            spliced.GetProduct_strand() == objects::eNa_strand_minus) {
         seqStrand = eNa_strand_minus;
    }
    record.SetLocation(seqStart, seqStop, seqStrand);

    if (seqStrand == eNa_strand_minus) {
        if (exon.GetProduct_end().IsProtpos() &&
            exon.GetProduct_end().GetProtpos().IsSetFrame()) {
            const TSeqPos frame = exon.GetProduct_end().GetProtpos().GetFrame();
            record.SetPhase(3-frame);
        }
        return true;
    }

    // Works for + strand 
    if (exon.GetProduct_start().IsProtpos() &&
        exon.GetProduct_start().GetProtpos().IsSetFrame()) {
        const TSeqPos frame = exon.GetProduct_start().GetProtpos().GetFrame();
        record.SetPhase(frame-1);
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedTarget(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string genomicLabel;
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
        genomicId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&genomicLabel, CSeq_id::eContent);
    }
    else {
        genomicId.GetLabel(&genomicLabel, CSeq_id::eContent);
    }

    string seqStart = NStr::IntToString(exon.GetGenomic_start()+1);
    string seqStop = NStr::IntToString(exon.GetGenomic_end()+1);
    string seqStrand = "+";
    if (spliced.IsSetGenomic_strand()  &&  
            spliced.GetGenomic_strand() == objects::eNa_strand_minus) {
         seqStrand = "-";
    }

    string target = genomicLabel;
    target += " " + seqStart;
    target += " " + seqStop;
    target += " " + seqStrand;
    record.SetAttribute("Target", target); 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedSeqId(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string seqId;
    const CSeq_id& genomicId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
         genomicId, *m_pScope, sequence::eGetId_Best);
    bestH.GetSeqId()->GetLabel(&seqId, CSeq_id::eContent);
    record.SetSeqId(seqId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedScores(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    if (exon.IsSetScores()) {
        typedef list<CRef<CScore> > SCORES;

        const SCORES& scores = exon.GetScores().Get();
        for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); 
                ++cit) {
            const CScore& score = **cit;
            if (!score.IsSetId()  ||  !score.GetId().IsStr()) {
                continue;
            }
            const string& key = score.GetId().GetStr();
            if (key == "score"  ||  xIsNeededScore(record.StrSeqId(), score)) {
                record.SetScore(score);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentScores(
    CGffAlignRecord& record,
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(record.StrSeqId());
    CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
    if (mCurrentIdForAttributes != record.StrSeqId()) {
        xAssignTaxid(bsh, record);
        xAssignDefline(bsh, record);
    }

    typedef vector<CRef<CScore> > SCORES;
    if (align.IsSetScore()) {
        const SCORES& scores = align.GetScore();
        for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); 
                ++cit) {
            const CScore& score = **cit;
            if (!xIsNeededScore(record.StrSeqId(), score)) {
                continue;
            }
            record.SetScore(**cit);
        }
    }
    mCurrentIdForAttributes = record.StrSeqId();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegSeqId(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    //const CSeq_id& targetId = alnMap.GetSeqId(srcRow);
    const CSeq_id& targetId = alnMap.GetSeqId(0);
    CBioseq_Handle targetH = m_pScope->GetBioseqHandle(targetId);
    CSeq_id_Handle targetIdH = targetH.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(
            targetH, sequence::eGetId_ForceAcc);
        if (best) {
            targetIdH = best;
        }
    }
    catch(std::exception&) {};
    CConstRef<CSeq_id> pTargetId = targetIdH.GetSeqId();
    string seqId;
    pTargetId->GetLabel( &seqId, CSeq_id::eContent );
    record.SetSeqId(seqId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegTarget(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& sourceId = alnMap.GetSeqId(srcRow);
    CBioseq_Handle sourceH = m_pScope->GetBioseqHandle(sourceId);
    CSeq_id_Handle sourceIdH = sourceH.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(
            sourceH, sequence::eGetId_ForceAcc);
        if (best) {
            sourceIdH = best;
        }
    }
    catch(std::exception&) {};
    CConstRef<CSeq_id> pSourceId = sourceIdH.GetSeqId();

    string target;
    pSourceId->GetLabel(&target, CSeq_id::eContent);

    ENa_strand strand = 
        (alnMap.StrandSign(srcRow) == -1) ? eNa_strand_minus : eNa_strand_plus;
    int numSegs = alnMap.GetNumSegs();

    int start2 = -1;
    int start_seg = 0;
    while (start2 < 0 && start_seg < numSegs) { // Skip over -1 start coords
        start2 = alnMap.GetStart(srcRow, start_seg++);
    }

    int stop2 = -1;
    int stop_seg = numSegs-1;
    while (stop2 < 0 && stop_seg >= 0) { // Skip over -1 stop coords
        stop2 = alnMap.GetStart(srcRow, stop_seg--);
    }

    if (strand == eNa_strand_minus) {
        swap(start2, stop2);
        stop2 += alnMap.GetLen(start_seg-1)-1;
    } 
    else {
        stop2 += alnMap.GetLen(stop_seg+1)-1;
    }


    CSeq_id::EAccessionInfo sourceInfo = pSourceId->IdentifyAccession();
    const int tgtWidth = (sourceInfo & CSeq_id::fAcc_prot) ? 3 : 1;

    target += " " + NStr::IntToString(start2/tgtWidth + 1);
    target += " " + NStr::IntToString(stop2/tgtWidth + 1);
    target += " " + string(strand == eNa_strand_plus ? "+" : "-");
    record.SetAttribute("Target", target); 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegLocation(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = alnMap.GetSeqStart(0);
    unsigned int seqStop = alnMap.GetSeqStop(0);
    ENa_strand seqStrand = (alnMap.StrandSign(0) == 1 ? 
        eNa_strand_plus : 
        eNa_strand_minus);
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegScores(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CScore> > SCORES;
    const CDense_seg& denseSeg = alnMap.GetDenseg();
    if (!denseSeg.IsSetScores()) {
        return true;
    }
    const SCORES& scores = denseSeg.GetScores();
    for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); 
            ++cit) {
        const CScore& score = **cit;
        if (!score.IsSetId()  ||  !score.GetId().IsStr()) {
            continue;
        }
        const string& key = score.GetId().GetStr();
        if (key == "score"  ||  xIsNeededScore(record.StrSeqId(), score)) {
            record.SetScore(score);
        }
    }        
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedGap(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    bool isProteinProd = xSplicedSegHasProteinProd(spliced);
    const unsigned int tgtWidth = isProteinProd ? 3 : 1;

    typedef list<CRef<CSpliced_exon_chunk> > CHUNKS;

    const CHUNKS& chunks = exon.GetParts();
    for (CHUNKS::const_iterator cit = chunks.begin(); cit != chunks.end(); ++cit) {
        const CSpliced_exon_chunk& chunk = **cit;
        switch (chunk.Which()) {
        default:
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            record.AddMatch(chunk.GetMismatch()); 
            break;
        case CSpliced_exon_chunk::e_Diag:
            record.AddMatch(chunk.GetDiag()/tgtWidth); 
            break;
        case CSpliced_exon_chunk::e_Match:
            record.AddMatch(chunk.GetMatch()/tgtWidth); 
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            {
                const unsigned int del_length = chunk.GetGenomic_ins()/tgtWidth;
                if (del_length > 0) {
                    record.AddInsertion(del_length);
                }
                if (isProteinProd) {
                    const unsigned int forward_shift = chunk.GetGenomic_ins()%tgtWidth;
                    if (forward_shift >  0) {
                        record.AddForwardShift(forward_shift);
                    }
                }
            }
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            {
                const unsigned int insert_length = chunk.GetProduct_ins()/tgtWidth;
                if (insert_length > 0) { 
                    record.AddDeletion(insert_length);
                }
                if (isProteinProd) { 
                    const unsigned int reverse_shift = chunk.GetProduct_ins()%tgtWidth;
                    if (reverse_shift > 0) { 
                        record.AddReverseShift(reverse_shift);
                    }
                }
            }
            break;
        }
    }
    record.FinalizeMatches();
    return true;
}

END_NCBI_SCOPE
