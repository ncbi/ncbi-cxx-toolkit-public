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
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_source_data.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>
#include <objtools/writers/gff3_denseg_record.hpp>
#include <objtools/writers/gff3_splicedseg_record.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objtools/writers/gff3_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//bool bDebugHere = false;

#define IS_INSERTION(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define IS_DELETION(sf, tf) \
    ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define IS_MATCH(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ----------------------------------------------------------------------------
bool sIsTransspliced(const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    return (mf.IsSetExcept_text()  &&  mf.GetExcept_text() == "trans-splicing");
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedEndpoints(
    const CSeq_loc& loc, 
    unsigned int& inPoint,
    unsigned int& outPoint)
//  start determined by the minimum start of any sub interval
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    inPoint = packedInt.GetStart(eExtreme_Biological);
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetFrom() < inPoint) {
            inPoint = intv.GetFrom();
        }
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedOutPoint(const CSeq_loc& loc, unsigned int& outPoint)
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
sInheritScores(
    const CSeq_align& alignFrom,
    CSeq_align& alignTo)
//  Idea: Inherit down, but only in a score of the same key/id does not already
//    exist.
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CScore> > SCORES;

    if (!alignFrom.IsSetScore()) {
        return true;
    }
    const SCORES& scoresFrom = alignFrom.GetScore();
    for (SCORES::const_iterator itFrom = scoresFrom.begin(); 
            itFrom != scoresFrom.end(); ++itFrom) {

        const CScore& scoreFrom = **itFrom;

        if (scoreFrom.GetId().IsStr()) {
            const string& keyFrom = scoreFrom.GetId().GetStr(); 
            const SCORES& scoresTo = alignTo.GetScore();
            SCORES::const_iterator itTo;
            for (itTo = scoresTo.begin(); itTo != scoresTo.end(); ++itTo) {
                const CScore& scoreTo = **itTo;
                if (scoreTo.GetId().IsStr()) {
                    const string& keyTo = scoreTo.GetId().GetStr();
                    if (keyTo == keyFrom) {
                        break;
                    }
                }
            }
            if (itTo == scoresTo.end()) {
                alignTo.SetScore().push_back(*itFrom);
            }
        }

        if (scoreFrom.GetId().IsId()) {
            const CObject_id& idFrom = scoreFrom.GetId();
            const SCORES& scoresTo = alignFrom.GetScore();
            SCORES::const_iterator itTo;
            for (itTo = scoresTo.begin(); itTo != scoresTo.end(); ++itTo) {
                const CScore& scoreTo = **itTo;
                if (scoreTo.GetId().IsId()) {
                    const CObject_id& idTo = scoreTo.GetId();
                    if (idTo.Match(idFrom)) {
                        break;
                    }
                }
            }
            if (itTo == scoresTo.end()) {
                alignTo.SetScore().push_back(*itFrom);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
string sBestMatchType(
    const CSeq_id& source,
    const CSeq_id& target)
//  ----------------------------------------------------------------------------
{
    const char* strProtMatch     = "nucleotide_to_protein_match";
    const char* strEstMatch      = "EST_match";
    const char* strTransNucMatch = "translated_nucleotide_match";
    const char* strCdnaMatch     = "cDNA_match";

    CSeq_id::EAccessionInfo targetInfo = source.IdentifyAccession();
    CSeq_id::EAccessionInfo sourceInfo =target.IdentifyAccession();


    if (targetInfo & CSeq_id::fAcc_prot) {
        return strProtMatch;
    }
    if ((targetInfo & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) {
        return strEstMatch;
    }
    if ((targetInfo & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_mrna) {
        return strCdnaMatch;
    }
    if ((targetInfo & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_tsa) {
        return strCdnaMatch;
    }
    if (sourceInfo & CSeq_id::fAcc_prot) {
        return strTransNucMatch;
    }
    return "match";
}
    
//  ----------------------------------------------------------------------------
bool sFeatureHasChildOfSubtype(
    CMappedFeat mf,
    CSeqFeatData::ESubtype subtype,
    feature::CFeatTree* pTree = 0)
    //  ----------------------------------------------------------------------------
{
    bool bTreeIsMine = false;
    if (!pTree) {
        pTree = new feature::CFeatTree;
        pTree->AddFeaturesFor(mf, subtype, mf.GetFeatSubtype());
        bTreeIsMine = true;
    }
    vector<CMappedFeat> c = pTree->GetChildren(mf);
    for (vector<CMappedFeat>::iterator it = c.begin(); it != c.end(); it++) {
        CMappedFeat f = *it;
        if (f.GetFeatSubtype() == subtype) {
            return true;
        }
        else {
            if (sFeatureHasChildOfSubtype(f, subtype, pTree)) {
                return true;
            }
        }
    }
    if (bTreeIsMine) {
        delete pTree;
    }
    return false;
}


//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CScope& scope, 
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags ),
    m_sDefaultMethod("")
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
    m_uPendingTrnaId = 0;
    m_uPendingCdsId = 0;
    m_uPendingGenericId = 0;
    m_uPendingAlignId = 0;
};

//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags )
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
    m_uPendingCdsId = 0;
    m_uPendingTrnaId = 0;
    m_uPendingGenericId = 0;
    m_uPendingAlignId = 0;
};

//  ----------------------------------------------------------------------------
CGff3Writer::~CGff3Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3Writer::WriteAlign(
    const CSeq_align& align,
    const string& strAssName,
    const string& strAssAcc )
//  ----------------------------------------------------------------------------
{
    try {
        align.Validate(true);
    }
    catch(CException& e) {
        string msg("Inconsistent alignment data ");
        msg += ("\"\"\"" + e.GetMsg() + "\"\"\"");
        NCBI_THROW(CObjWriterException, eBadInput, msg);
    }
    if ( ! x_WriteAssemblyInfo( strAssName, strAssAcc ) ) {
        return false;
    }
    if ( ! xWriteAlign( align ) ) {
        return false;
    }
    m_uRecordId++;
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSeqAnnotHandle(
    CSeq_annot_Handle sah )
//  ----------------------------------------------------------------------------
{
    CConstRef<CSeq_annot> pAnnot = sah.GetCompleteSeq_annot();

    if ( pAnnot->IsAlign() ) {
        for ( CAlign_CI it( sah ); it; ++it ) {
            if ( ! xWriteAlign( *it ) ) {
                return false;
            }
        }
		m_uRecordId++;
        return true;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(sah, sel);
    CGffFeatureContext fc(feat_iter, CBioseq_Handle(), sah);
    for ( /*0*/; feat_iter; ++feat_iter ) {
        if ( ! xWriteFeature( fc, *feat_iter ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlign( 
    const CSeq_align& align,
    const string& alignId)
//  ----------------------------------------------------------------------------
{
    if (!align.IsSetSegs()) {
        cerr << "Object type not supported." << endl;
        return true;
    }
    
    string id = alignId;
    if (id.empty()) {
        if (align.IsSetId()) {
            const CSeq_align::TId& ids = align.GetId();
            for (CSeq_align::TId::const_iterator it = ids.begin();
                    it != ids.end(); ++it) {
                if ((*it)->IsStr()) {
                    id = (*it)->GetStr();
                    break;
                }
            }
        }
    }
    if (id.empty()) {
        id = xNextAlignId();
    }

    switch(align.GetSegs().Which()) {
        default:
            break;
        case CSeq_align::TSegs::e_Denseg:
            return xWriteAlignDenseg(align, id);
        case CSeq_align::TSegs::e_Spliced:
            return xWriteAlignSpliced(align, id);
        case CSeq_align::TSegs::e_Disc:
            return xWriteAlignDisc(align, id);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignDisc( 
    const CSeq_align& align,
    const string& alignId)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_align> > ALIGNS;

    const ALIGNS& data = align.GetSegs().GetDisc().Get();
    for (ALIGNS::const_iterator cit = data.begin(); cit != data.end(); ++cit) {

        CRef<CSeq_align> pA(new CSeq_align);
        pA->Assign(**cit);
        if (!sInheritScores(align, *pA)) {
            return false;
        }
        if (!xWriteAlign(*pA, alignId)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignSpliced( 
    const CSeq_align& align,
    const string& alignId)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());

    typedef list<CRef<CSpliced_exon> > EXONS;
    const EXONS& exons = align.GetSegs().GetSpliced().GetExons();

    const CSpliced_seg& spliced = align.GetSegs().GetSpliced();
    //string recordId = xNextAlignId();
    for (EXONS::const_iterator cit = exons.begin(); cit != exons.end(); ++cit) {
        const CSpliced_exon& exon = **cit;
        CRef<CGffAlignRecord> pRecord(new CGffAlignRecord(alignId));      
        if (!xAssignAlignmentSpliced(*pRecord, spliced, exon)) {
            return false;
        }  
        if (!xAssignAlignmentScores(*pRecord, align)) {
            return false;
        }
        if (!xWriteRecord(*pRecord)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedPhase(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    //phase is meaningless for alignments
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedAttributes(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    //nothing here --- yet
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedSeqId(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string seqId;
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
         genomicId, *m_pScope, sequence::eGetId_Best);
    bestH.GetSeqId()->GetLabel(&seqId, CSeq_id::eContent);
    record.SetSeqId(seqId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedMethod(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    //const CSeq_id& genomicId = spliced.GetGenomic_id();
    //const CSeq_id& productId = spliced.GetProduct_id();
    string method;

    //following order of resolution is from mss-265:
    
    //if feature has a ModelEvidence user object, use that
    // this is an alignment, not a feature, hence does not apply

    //use source database of the target
    if (spliced.IsSetProduct_id()) {
        const CSeq_id& productId = spliced.GetProduct_id();
        CSeq_id_Handle bestH = sequence::GetId(
            productId, *m_pScope, sequence::eGetId_Best);
        const CSeq_id& bestId = *bestH.GetSeqId();
        CWriteUtil::GetIdType(bestId, method);
        record.SetMethod(method);
        return true;
    }

    //if parent has a ModelEvidence user objcet, use that
    // this is an alignment, not a feature, hence does not apply

    // use the default method if one has been set
    if (!m_sDefaultMethod.empty()) {
        record.SetMethod(m_sDefaultMethod);
        return true;
    }

    // finally, look at the type of accession
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
        genomicId, *m_pScope, sequence::eGetId_Best);
    const CSeq_id& bestId = *bestH.GetSeqId();
    CWriteUtil::GetIdType(bestId, method);
    record.SetMethod(method);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedType(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle genomicH = sequence::GetId(
        spliced.GetGenomic_id(), *m_pScope, sequence::eGetId_Best);
    CSeq_id_Handle productH = sequence::GetId(
        spliced.GetProduct_id(), *m_pScope, sequence::eGetId_Best);
    if (!genomicH || !productH) {
        // MSS-225: There _are_ accessions that are not in ID (yet). 
        return true;
    }
    const CSeq_id& genomicId = *genomicH.GetSeqId();
    const CSeq_id& productId =*productH.GetSeqId();
    record.SetType(sBestMatchType(productId, genomicId));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedLocation(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = exon.GetGenomic_start();
    unsigned int seqStop = exon.GetGenomic_end();
    ENa_strand seqStrand = eNa_strand_plus;
    if (exon.IsSetGenomic_strand()) {
        seqStrand = exon.GetGenomic_strand();
    }
    else if (spliced.IsSetGenomic_strand()) {
        seqStrand = spliced.GetGenomic_strand();
    }
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedScores(
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
            record.SetScore(**cit);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedGap(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    CSeq_id::EAccessionInfo productInfo;
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        productInfo = bestH.GetSeqId()->IdentifyAccession();
    }
    else {
        productInfo = productId.IdentifyAccession();
    }
    const int tgtWidth = (productInfo & CSeq_id::fAcc_prot) ? 3 : 1;

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
            // Round to next multiple of tgtWidth to account for reverse frameshifts
            record.AddMatch((chunk.GetDiag()+tgtWidth-1)/tgtWidth);
            break;
        case CSpliced_exon_chunk::e_Match:
            // Round to next multiple of tgtWidth to account for reverse framshifts
            record.AddMatch((chunk.GetMatch()+tgtWidth-1)/tgtWidth); 
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            {
                const int del_length = chunk.GetGenomic_ins()/tgtWidth;
                if (del_length > 0) {
                    record.AddDeletion(del_length);
                }
            }
            if (tgtWidth > 1) {
                const int forward_shift = chunk.GetGenomic_ins()%tgtWidth;
                if (forward_shift >  0) {
                    record.AddForwardShift(forward_shift);
                }
            }
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            if (tgtWidth > 1) { // Can only occur when target is prot
                const int reverse_shift = chunk.GetProduct_ins()%tgtWidth;
                if (reverse_shift > 0) { 
                    record.AddReverseShift(reverse_shift);
                }
            }
            {
                const int insert_length = chunk.GetProduct_ins()/tgtWidth;
                if (insert_length > 0) { 
                    record.AddInsertion(insert_length);
                }
            }
            break;
        }
    }
    record.FinalizeMatches();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedTarget(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    CSeq_id::EAccessionInfo productInfo;
    string target;
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&target, CSeq_id::eContent);
        productInfo = bestH.GetSeqId()->IdentifyAccession();
    }
    else {
        productId.GetLabel(&target, CSeq_id::eContent);
        productInfo = productId.IdentifyAccession();
    }

    const int tgtWidth = (productInfo & CSeq_id::fAcc_prot) ? 3 : 1;


    string seqStart = NStr::IntToString(exon.GetProduct_start().AsSeqPos()/tgtWidth+1);
    string seqStop = NStr::IntToString(exon.GetProduct_end().AsSeqPos()/tgtWidth+1);
    string seqStrand = "+";
    if (spliced.CanGetProduct_strand()  &&  
            spliced.GetProduct_strand() == objects::eNa_strand_minus) {
         seqStrand = "-";
    }
    target += " " + seqStart;
    target += " " + seqStop;
    target += " " + seqStrand;
    record.SetAttribute("Target", target); 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSpliced(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    return (xAssignAlignmentSplicedSeqId(record, spliced, exon)  &&
        xAssignAlignmentSplicedMethod(record, spliced, exon)  &&
        xAssignAlignmentSplicedType(record, spliced, exon)  &&
        xAssignAlignmentSplicedLocation(record, spliced, exon)  &&
        xAssignAlignmentSplicedScores(record, spliced, exon)  &&
        xAssignAlignmentSplicedPhase(record, spliced, exon)  &&
        xAssignAlignmentSplicedTarget(record, spliced, exon)  &&
        xAssignAlignmentSplicedAttributes(record, spliced, exon)  &&
        xAssignAlignmentSplicedGap(record, spliced, exon));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentScores(
    CGffAlignRecord& record,
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CScore> > SCORES;
    if (!align.IsSetScore()) {
        return true;
    }
    const SCORES& scores = align.GetScore();
    for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); 
            ++cit) {
        record.SetScore(**cit);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignDenseg( 
    const CSeq_align& align,
    const string& alignId)
//  ----------------------------------------------------------------------------
{
    CRef<CDense_seg> dsFilled = align.GetSegs().GetDenseg().FillUnaligned();
    CAlnMap alnMap(*dsFilled);

    //const CSeq_id& sourceId = align.GetSeq_id(0);
    const CSeq_id& sourceId = alnMap.GetSeqId(0);
    CBioseq_Handle sourceH = m_pScope->GetBioseqHandle(sourceId);

    //string nextAlignId = xNextAlignId();
    for (CAlnMap::TDim sourceRow = 1; sourceRow < alnMap.GetNumRows(); ++sourceRow) {
        CRef<CGffAlignRecord> pSource(new CGffAlignRecord(alignId));
        const CSeq_id& targetId = alnMap.GetSeqId(sourceRow);
        CBioseq_Handle targetH = m_pScope->GetBioseqHandle(targetId);
        if (!xAssignAlignmentScores(*pSource, align)) {
            return false;
        }
        if (!xAssignAlignmentDenseg(*pSource, alnMap, sourceRow)) {
            return false;
        }
        return xWriteRecord(*pSource);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDensegSeqId(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& targetId = alnMap.GetSeqId(srcRow);
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
bool CGff3Writer::xAssignAlignmentDensegScores(
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
        record.SetScore(**cit);
    }        
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDensegType(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& sourceId = alnMap.GetSeqId(0);
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

    const CSeq_id& targetId = alnMap.GetSeqId(srcRow);
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
    record.SetType(sBestMatchType(*pSourceId, *pTargetId));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDensegMethod(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& sourceId = alnMap.GetSeqId(0);
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

    string method;
    if (!m_sDefaultMethod.empty()) {
        record.SetMethod(m_sDefaultMethod);
        return true;
    }
    CWriteUtil::GetIdType(*pSourceId, method);
    record.SetMethod(method);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDensegTarget(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& sourceId = alnMap.GetSeqId(0);
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
        (alnMap.StrandSign(0) == -1) ? eNa_strand_minus : eNa_strand_plus;
    int numSegs = alnMap.GetNumSegs();

    int start2 = -1;
    int start_seg = 0;
    while (start2 < 0 && start_seg < numSegs) { // Skip over -1 start coords
        start2 = alnMap.GetStart(0, start_seg++);
    }

    int stop2 = -1;
    int stop_seg = numSegs-1;
    while (stop2 < 0 && stop_seg >= 0) { // Skip over -1 stop coords
        stop2 = alnMap.GetStart(0, stop_seg--);
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
bool CGff3Writer::xAssignAlignmentDensegGap(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CDense_seg& denseSeg = alnMap.GetDenseg();

    int tgtWidth; //could be 1 or 3, depending on nuc or prot
    if (0 < denseSeg.GetWidths().size()) {
        tgtWidth = denseSeg.GetWidths()[0];
    } else {
        const CSeq_id& tgtId = alnMap.GetSeqId(0);
        CBioseq_Handle tgtH = m_pScope->GetBioseqHandle(tgtId);
        CSeq_id_Handle tgtIdH = tgtH.GetSeq_id_Handle();
        try {
            CSeq_id_Handle best = sequence::GetId(
                tgtH, sequence::eGetId_ForceAcc);
            if (best) {
                tgtIdH = best;
            }  
        }
        catch(std::exception&) {};
        CSeq_id::EAccessionInfo tgtInfo = tgtIdH.GetSeqId()->IdentifyAccession();
        tgtWidth = (tgtInfo & CSeq_id::fAcc_prot) ? 3 : 1;
    } 


    int numSegs = alnMap.GetNumSegs();
    for (int seg = 0; seg < numSegs; ++seg) {
        CAlnMap::TSegTypeFlags srcFlags = alnMap.GetSegType(srcRow, seg);
        CAlnMap::TSegTypeFlags tgtFlags = alnMap.GetSegType(0, seg);

        if (IS_INSERTION(tgtFlags, srcFlags)) {
            CRange<int> tgtPiece = alnMap.GetRange(0, seg);

            if (tgtWidth > 1) {
                const int reverse_shift = tgtPiece.GetLength()%tgtWidth;
                if (reverse_shift > 0) { // Can only occur when target is prot
                    record.AddReverseShift(reverse_shift);
                }
            }

            const int insert_length = tgtPiece.GetLength()/tgtWidth;
            if (insert_length > 0) { 
                record.AddInsertion(insert_length);
            }
        }

        if (IS_DELETION(tgtFlags, srcFlags)) {
            CRange<int> srcPiece = alnMap.GetRange(srcRow, seg);

            const int del_length = srcPiece.GetLength()/tgtWidth;
            if (del_length > 0) {
                record.AddDeletion(del_length);
            }

            if (tgtWidth > 1) {
                const int forward_shift = srcPiece.GetLength()%tgtWidth;
                if (forward_shift > 0) {
                    record.AddForwardShift(forward_shift);
                }
            }
        }

        if (IS_MATCH(tgtFlags, srcFlags)) {
            CRange<int> tgtPiece = alnMap.GetRange(0, seg); //either will work
            record.AddMatch((tgtPiece.GetLength()+tgtWidth-1)/tgtWidth);
        } 
    }
    record.FinalizeMatches();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDensegLocation(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = alnMap.GetSeqStart(srcRow);
    unsigned int seqStop = alnMap.GetSeqStop(srcRow);
    ENa_strand seqStrand = (alnMap.StrandSign(srcRow) == 1 ? 
        eNa_strand_plus : 
        eNa_strand_minus);
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentDenseg(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    return (xAssignAlignmentDensegSeqId(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegMethod(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegType(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegScores(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegLocation(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegTarget(record, alnMap, srcRow)  &&
        xAssignAlignmentDensegGap(record, alnMap, srcRow));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << '\n';
        m_Os << "#!gff-spec-version 1.21" << '\n';
        m_Os << "#!processor NCBI annotwriter" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSequenceHeader(
    CSeq_id_Handle idh)
//  ----------------------------------------------------------------------------
{
    string id;
    if (!CWriteUtil::GetBestId(idh, *m_pScope, id)) {
        id = "<unknown>";
    }
    m_Os << "##sequence-region " << id << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSequenceHeader(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    //sequence-region
    string id;
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if ( pId ) {
        if (!CWriteUtil::GetBestId(CSeq_id_Handle::GetHandle(*pId), bsh.GetScope(), id)) {
            id = "<unknown>";
        }
    }

    string start = "1";
    string stop = NStr::IntToString(bsh.GetBioseqLength());
    m_Os << "##sequence-region " << id << " " << start << " " << stop << '\n';

    //species
    const string base_url = 
        "https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?";
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (sdi) {
        const CBioSource& bs = sdi->GetSource();
        if (bs.IsSetOrg()) {
            string tax_id = NStr::IntToString(bs.GetOrg().GetTaxId());
            m_Os << "##species " << base_url << "id=" << tax_id << '\n';
        }
        else if (bs.IsSetOrgname()) {
            string orgname = NStr::URLEncode(bs.GetTaxname());
            m_Os << "##species " << base_url << "name=" << orgname << '\n';        
        }
    }

    //genome build
//    for(CSeqdesc_CI udi(bsh.GetParentEntry(), CSeqdesc::e_User, 0); udi; ++udi) {
//        const CUser_object& uo = udi->GetUser();
//        if (!uo.IsSetType()  ||  uo.GetType().IsStr()  ||  
//                uo.GetType().GetStr() != "GenomeBuild" ) {
//            continue;
//        }
//        //awaiting specific instructions here ...
//        break;
//    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteBioseqHandle(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    if (!xWriteSequenceHeader(bsh) ) {
        return false;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(bsh, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    if (!xWriteSource(fc)) {
        return false;
    }
    for ( ; feat_iter; ++feat_iter) {
        if (!xWriteFeature(fc, *feat_iter)) {
            return false;
        }
    }
    for (CAlign_CI align_it(bsh, sel);  align_it;  ++ align_it) {
        xWriteAlign(*align_it);
        m_uRecordId++;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSource(
    CGffFeatureContext& fc )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle bsh = fc.BioseqHandle();
    CSeqdesc_CI sdi(bsh.GetParentEntry(), CSeqdesc::e_Source, 0);
    if (!sdi) {
        return true; 
    }
    CRef<CGffSourceRecord> pSource(new CGffSourceRecord(xNextGenericId()));
    if (!xAssignSource(*pSource, fc, bsh)) {
        return false;
    }
    return xWriteRecord(*pSource);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeature(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    //CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
    try {
        switch( mf.GetFeatSubtype() ) {
            default:
                if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                    return xWriteFeatureRna( fc, mf );
                }
                return xWriteFeatureGeneric( fc, mf );
            case CSeqFeatData::eSubtype_C_region:
            case CSeqFeatData::eSubtype_D_segment:
            case CSeqFeatData::eSubtype_J_segment:
            case CSeqFeatData::eSubtype_V_segment:
                return xWriteFeatureCDJVSegment( fc, mf );
            case CSeqFeatData::eSubtype_gene: 
                return xWriteFeatureGene( fc, mf );
            case CSeqFeatData::eSubtype_cdregion:
                return xWriteFeatureCds( fc, mf );
            case CSeqFeatData::eSubtype_tRNA:
                return xWriteFeatureTrna( fc, mf );

            case CSeqFeatData::eSubtype_pub:
                return true; //ignore
        }
    }
    catch (CException& e) {
        cerr << "CGff3Writer: Unsupported feature type encountered: Removed." << endl;
        cerr << mf.GetFeatType() << "\t" << mf.GetFeatSubtype() << endl;
        cerr << "  exc: " << e.ReportAll() << endl;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureTrna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{

    const string rnaId = xNextTrnaId();

    CRef<CGffFeatureRecord> pRna( new CGffFeatureRecord(rnaId) );
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
	}


    if(sIsTransspliced(mf)){    
        xAssignFeatureAttributeParentGene(*pRna, fc, mf);
        TSeqPos seqlength = 0;
        if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
            seqlength = fc.BioseqHandle().GetInst().GetLength();

        if (!xWriteFeatureRecords( *pRna, mf.GetLocation(), seqlength ) ) {
            return false;
        }
    }else{
        if(!xWriteRecord(*pRna)){
            return false;
        }
    }

	const CSeq_loc& PackedInt = pRna->Location();

    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffFeatureRecord> pChild(new CGffFeatureRecord(*pRna));
            pChild->SetRecordId(xNextGenericId());
            pChild->SetType("exon");
            pChild->SetLocation(subint);
            pChild->SetParent(rnaId);
            if ( ! xWriteRecord(*pChild ) ) {
                return false;
            }
        }
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    static CSafeStatic<CSofaMap> SOFAMAP;

    if (!mf.IsSetData()) {
        record.SetType(SOFAMAP->DefaultName());
        return true;
    }
    record.SetType(
        SOFAMAP->MappedName(mf.GetFeatType(), mf.GetFeatSubtype()));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureSeqId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string bestId;
    if (!CWriteUtil::GetBestId(mf, bestId)) {
        bestId = ".";
    }
    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetMethodFromModelEvidence(
    CMappedFeat mf,
    string& method)
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> me = CWriteUtil::GetModelEvidence( mf);
    if (!me  || !me->HasField("Method")) {
        return false;
    }
    const CUser_field& uf = me->GetField("Method");
    if (!uf.IsSetData()  ||  !uf.GetData().IsStr()) {
        return false;
    }
    method = uf.GetData().GetStr();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureMethod(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string method(".");

    //if feature got a ModelEvidence object, try to get metgod from there
    if (sGetMethodFromModelEvidence(mf, method)) {
        record.SetMethod(method);
        return true;
    }
    
    //if parent feature got a ModelEvidence object, use that.
    try {
        CMappedFeat parent = fc.FeatTree().GetParent(mf);
        if (parent && sGetMethodFromModelEvidence(parent, method)) {
            record.SetMethod(method);
            return true;
        }
    }
    catch (const CException&) {};
    
    //if a default method has been set, use that.
    if (!m_sDefaultMethod.empty()) {
        record.SetMethod(m_sDefaultMethod);
        return true;
    }

    //last resort: derive method from ID.
    CBioseq_Handle bsh = fc.BioseqHandle();
    if (bsh) {
        if (!CWriteUtil::GetIdType(bsh, method)) {
            return false;
        }
    }
    else {
        CSeq_id_Handle idh = mf.GetLocationId();
        if (!CWriteUtil::GetIdType(*idh.GetSeqId(), method)) {
            return false;
        }
    }
    if (method == "Local") {
        method = ".";
    }
    record.SetMethod(method);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureEndpoints(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CGffBaseRecord& baseRecord = record;

    unsigned int seqStart(0);
    unsigned int seqStop(0);

    if (sIsTransspliced(mf)) {
        if (!sGetTranssplicedEndpoints(record.Location(), 
                seqStart, seqStop)) {
            return false;
        }
        baseRecord.SetLocation(seqStart, seqStop);
        //return true;
    }
    else {
        seqStart = record.Location().GetStart(eExtreme_Positional);
        seqStop = record.Location().GetStop(eExtreme_Positional);
        string min = NStr::IntToString(seqStart + 1);
        string max = NStr::IntToString(seqStop + 1);
        if (record.Location().IsPartialStart(eExtreme_Biological)) {
            if (record.Location().GetStrand() == eNa_strand_minus) {
                record.SetAttribute("end_range", max + string(",."));
            }
            else {
                record.SetAttribute("start_range", string(".,") + min);
            }
        }
        if (record.Location().IsPartialStop(eExtreme_Biological)) {
            if (record.Location().GetStrand() == eNa_strand_minus) {
                record.SetAttribute("start_range", string(".,") + min);
            }
            else {
                record.SetAttribute("end_range", max + string(",."));
            }
        }
        baseRecord.SetLocation(seqStart, seqStop);
        //return true;
    }

    CBioseq_Handle bsh = fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }

    unsigned int bstart = record.Location().GetStart( eExtreme_Biological );
    unsigned int bstop = record.Location().GetStop( eExtreme_Biological );

    ENa_strand strand = record.Location().GetStrand();
    if (strand == eNa_strand_minus) {
        if (seqStart < bstop) {
            seqStart += bsh.GetInst().GetLength();
        }
        if (seqStop < bstop) {
            seqStop += bsh.GetInst().GetLength();
        }
        baseRecord.SetLocation(seqStart, seqStop);
        return true;
    }
    //everything else considered eNa_strand_plus
    if (seqStart < bstart) {
        seqStart += bsh.GetInst().GetLength();
    }
    if (seqStop < bstart) {
        seqStop += bsh.GetInst().GetLength();
    }
    baseRecord.SetLocation(seqStart, seqStop);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureScore(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureStrand(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    record.SetStrand(mf.GetLocation().GetStrand());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeaturePhase(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatSubtype() == CSeq_feat::TData::eSubtype_cdregion) {
        record.SetPhase(0);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributes(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!xAssignFeatureAttributeGbKey(record, fc, mf)) {
        return false;
    }

    //attributes common to all feature types:
    if (!xAssignFeatureAttributeProduct(record, fc, mf) ||
            !xAssignFeatureAttributeParent(record, fc, mf)  ||
            !xAssignFeatureAttributePseudoGene(record, fc, mf) ||
            !xAssignFeatureAttributePartial(record, fc, mf) ||
            !xAssignFeatureAttributeException(record, fc, mf) ||
            !xAssignFeatureAttributeExonNumber(record, fc, mf)  ||
            !xAssignFeatureAttributePseudo(record, fc, mf)  ||
            !xAssignFeatureAttributeDbXref(record, fc, mf)  ||
            !xAssignFeatureAttributeGene(record, fc, mf)  ||
            !xAssignFeatureAttributeNote(record, fc, mf)  ||
            !xAssignFeatureAttributeModelEvidence(record, fc, mf)  ||
            !xAssignFeatureAttributeIsOrdered(record, fc, mf)  ||
            !xAssignFeatureAttributeTranscriptId(record, fc, mf)) {
        return false;
    }

    //attributes specific to certain feature types:
    switch(mf.GetData().GetSubtype()) {
        default:
            break;

        case CSeqFeatData::eSubtype_gene:
            if (!xAssignFeatureAttributeLocusTag(record, fc, mf)  ||
                    !xAssignFeatureAttributeGeneSynonym(record, fc, mf)  ||
                    !xAssignFeatureAttributeOldLocusTag(record, fc, mf)  ||
                    !xAssignFeatureAttributeGeneDesc(record, fc, mf)  ||
                    !xAssignFeatureAttributeGeneBiotype(record, fc, mf) ||
                    !xAssignFeatureAttributeMapLoc(record, fc, mf)) {
                return false;
            }
            break;

        case CSeqFeatData::eSubtype_mRNA:
            break;

        case CSeqFeatData::eSubtype_cdregion:
            if (!xAssignFeatureAttributeProteinId(record, fc, mf)  ||
                    !xAssignFeatureAttributeTranslationTable(record, fc, mf)  ||
                    !xAssignFeatureAttributeCodeBreak(record, fc, mf)) {
                return false;
            }
            break;

        case CSeqFeatData::eSubtype_ncRNA:
            if (!xAssignFeatureAttributeNcrnaClass(record, fc, mf)) {
                return false;
            }
            break;
    }

    //  deriviate attributes --- depend on other attributes. Hence need to be
    //  done last: 
    if (!xAssignFeatureAttributeName(record, fc, mf)) {
        return false;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeIsOrdered(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::IsLocationOrdered(mf.GetLocation())) {
        record.SetAttribute("is_ordered", "true");
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeExonNumber(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return true;
    }
    const CSeq_feat::TQual& quals = mf.GetQual();
    for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
        cit != quals.end(); 
        ++cit ) {
        const CGb_qual& qual = **cit;
        if (qual.IsSetQual()  &&  qual.GetQual() == "number") {
            record.SetAttribute("exon_number", qual.GetVal());
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributePseudo(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetPseudo()) {
        return true;
    }
    if (mf.GetPseudo() == true) {
        record.SetAttribute("pseudo", "true");
        return true; 
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeDbXref(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::E_Choice choice = mf.GetData().Which();

    if (mf.IsSetDbxref()) {
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref();
        for (size_t i=0; i < dbxrefs.size(); ++i) {
            string tag;
            if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                record.AddAttribute("Dbxref", tag);
            }
        }
    }

    switch (choice) {
        default: {
            CMappedFeat parent;
            try {
                parent = fc.FeatTree().GetParent( mf );
            }
            catch(...) {
            }
            if (parent  &&  parent.IsSetDbxref()) {
                const CSeq_feat::TDbxref& more_dbxrefs = parent.GetDbxref();
                for (size_t i=0; i < more_dbxrefs.size(); ++i) {
                    string tag;
                    if (CWriteUtil::GetDbTag(*more_dbxrefs[i], tag)) {
                        record.AddAttribute("Dbxref", tag);
                    }
                }
            }
            return true;
        }

        case CSeq_feat::TData::e_Rna:
        case CSeq_feat::TData::e_Cdregion: {
            if (mf.IsSetProduct()) {
                CSeq_id_Handle idh = sequence::GetId( 
                    mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceAcc);
                if (idh) {
                    string str;
                    idh.GetSeqId()->GetLabel(&str, CSeq_id::eContent);
                    if (NPOS != str.find('_')) { //nucleotide
                        str = string("Genbank:") + str;
                    }
                    else { //protein
                        str = string("NCBI_GP:") + str;
                    }
                    record.AddAttribute("Dbxref", str);
                }
                else {
                    idh = sequence::GetId(
                        mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceGi);
                    if (idh) {
                        string str;
                        idh.GetSeqId()->GetLabel(&str, CSeq_id::eContent);
                        str = string("NCBI_gi:") + str;
                        record.AddAttribute("Dbxref", str);
                    }
                }
            }
            CMappedFeat gene_feat = fc.FeatTree().GetParent(mf, CSeqFeatData::e_Gene);
            if (gene_feat  &&  !gene_feat.GetData().GetGene().IsSuppressed()  
                    &&  gene_feat.IsSetDbxref()) {
                const CSeq_feat::TDbxref& dbxrefs = gene_feat.GetDbxref();
                for ( size_t i=0; i < dbxrefs.size(); ++i ) {
                    string tag;
                    if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                        record.AddAttribute("Dbxref", tag);
                    }
                }
            }
        }
        break;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string strGene;
    if (mf.GetData().Which() == CSeq_feat::TData::e_Gene) {
        const CGene_ref& gene_ref = mf.GetData().GetGene();
        CWriteUtil::GetGeneRefGene(gene_ref, strGene);
        record.SetAttribute("gene", strGene);
        return true;
    }

    if (mf.IsSetXref()) {
        const vector<CRef<CSeqFeatXref> > xrefs = mf.GetXref();
        for (vector<CRef<CSeqFeatXref> >::const_iterator it = xrefs.begin();
                it != xrefs.end();
                ++it) {
            const CSeqFeatXref& xref = **it;
            if (xref.CanGetData() && xref.GetData().IsGene()) {
                CWriteUtil::GetGeneRefGene(xref.GetData().GetGene(), strGene);
                record.SetAttribute("gene", strGene);
                return true;
            }
        }
    }

    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (gene.IsSetData()  &&  gene.GetData().IsGene()) {
        CWriteUtil::GetGeneRefGene(gene.GetData().GetGene(), strGene);
        record.SetAttribute("gene", strGene);
        return true; 
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeNote(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetComment()  ||  mf.GetComment().empty()) {
        return true;
    }
    record.SetAttribute("Note", mf.GetComment());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeException(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsSetExcept_text()) {
        record.SetAttribute("exception", mf.GetExcept_text());
        return true;
    }
    if (mf.IsSetExcept()) {
        // what should I do?
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeModelEvidence(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    string modelEvidence;
    if (!CWriteUtil::GetStringForModelEvidence(mf, modelEvidence)) {
        return true;
    }
    if (!modelEvidence.empty()) {
        record.SetAttribute("model_evidence", modelEvidence);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeTranscriptId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatType() != CSeqFeatData::e_Rna) {
        return true;
    }
    const CSeq_feat::TQual& quals = mf.GetQual();
    for (CSeq_feat::TQual::const_iterator cit = quals.begin(); 
      cit != quals.end(); ++cit) {
        if ((*cit)->GetQual() == "transcript_id") {
            record.SetAttribute("transcript_id", (*cit)->GetVal());
            return true;
        }
    }

    if (mf.IsSetProduct()) {
        string transcript_id;
        if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), transcript_id)) {
            record.SetAttribute("transcript_id", transcript_id);
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGbKey(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    record.SetAttribute("gbkey", mf.GetData().GetKey());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGeneDesc(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetDesc()) {
        return true;
    }
    record.SetAttribute("description", gene_ref.GetDesc());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGeneSynonym(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetSyn()) {
        return true;
    }

    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if (!gene_ref.IsSetLocus() && !gene_ref.IsSetLocus_tag()) {
        ++it;
    }    
    if (it == syns.end()) {
        return true;
    }
    while (it != syns.end()) {
        record.AddAttribute("gene_synonym", *(it++));
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeOldLocusTag(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return true;
    }
    string old_locus_tags;
    vector<CRef<CGb_qual> > quals = mf.GetQual();
    for (vector<CRef<CGb_qual> >::const_iterator it = quals.begin();
            it != quals.end(); ++it) {
        if ((**it).IsSetQual() && (**it).IsSetVal()) {
            string qual = (**it).GetQual();
            if (qual != "old_locus_tag") {
                continue;
            }
            if (!old_locus_tags.empty()) {
                old_locus_tags += ",";
            }
            old_locus_tags += (**it).GetVal();
        }
    }
    if (!old_locus_tags.empty()) {
        record.SetAttribute("old_locus_tag", old_locus_tags);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributePseudoGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string pseudoGene = mf.GetNamedQual("pseudogene");
    if (!pseudoGene.empty()) {
        record.SetAttribute("pseudogene", pseudoGene);
        return true;
    }
    if (!CSeqFeatData::IsLegalQualifier(
            mf.GetFeatSubtype(), CSeqFeatData::eQual_pseudogene)) {
        return true;
    }
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return true;
    }
    pseudoGene = gene.GetNamedQual("pseudogene");
    if (!pseudoGene.empty()) {
        record.SetAttribute("pseudogene", pseudoGene);
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeMapLoc(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetMaploc()) {
        return true;
    }
    record.SetAttribute("map", gene_ref.GetMaploc());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeName(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    vector<string> value;   
    switch (mf.GetFeatSubtype()) {
        default:
            break;

        case CSeqFeatData::eSubtype_gene:
            if (record.GetAttributes("gene", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            if (record.GetAttributes("locus_tag", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            return true;

        case CSeqFeatData::eSubtype_cdregion:
            if (record.GetAttributes("protein_id", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            return true;
    }

    if (record.GetAttributes("transcript_id", value)) {
        record.SetAttribute("Name", value.front());
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeLocusTag(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetLocus_tag()) {
        return true;
    }
    record.SetAttribute("locus_tag", gene_ref.GetLocus_tag());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributePartial(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsSetPartial()) {
        if (mf.GetPartial() == true) {
            record.SetAttribute("partial", "true");
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeProteinId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetProduct()) {
        return true;
    }
    string protein_id;
    if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), protein_id)) {
        record.SetAttribute("protein_id", protein_id);
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeTranslationTable(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode()) {
        return true;
    }
    int id = cds.GetCode().GetId();
    if (id != 1  &&  id != 255) {
        record.SetAttribute("transl_table", NStr::IntToString(id));
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeCodeBreak(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode_break()) {
        return true;
    }
    const list<CRef<CCode_break> >& code_breaks = cds.GetCode_break();
    list<CRef<CCode_break> >::const_iterator it = code_breaks.begin();
    for (; it != code_breaks.end(); ++it) {
        string cbString;
        if (CWriteUtil::GetCodeBreak(**it, cbString)) {
            record.AddAttribute("transl_except", cbString);
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeProduct(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    if (subtype == CSeqFeatData::eSubtype_cdregion) {

        // Possibility 1:
        // Product name comes from a prot-ref which stored in the seqfeat's 
        // xrefs:
        const CProt_ref* pProtRef = mf.GetProtXref();
        if ( pProtRef && pProtRef->IsSetName() ) {
            const list<string>& names = pProtRef->GetName();
            record.SetAttribute("product", names.front());
            return true;
        }

        // Possibility 2:
        // Product name is from the prot-ref refered to by the seqfeat's 
        // data.product:
        if (mf.IsSetProduct()) {
            const CSeq_id* pId = mf.GetProduct().GetId();
            if (pId) {
                CBioseq_Handle bsh = mf.GetScope().GetBioseqHandle(*pId); 
                if (bsh) {
                    SAnnotSelector sel(CSeqFeatData::eSubtype_prot);
                    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
                    CFeat_CI it(bsh, sel);
                    if (it  &&  it->IsSetData() 
                            &&  it->GetData().GetProt().IsSetName()
                            &&  !it->GetData().GetProt().GetName().empty()) {
                        record.SetAttribute("product",
                            it->GetData().GetProt().GetName().front());
                        return true;
                    }
                }
            }
            
            string product;
            if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), product)) {
                record.SetAttribute("product", product);
                return true;
            }
        }
    }

    CSeqFeatData::E_Choice type = mf.GetFeatType();
    if (type == CSeqFeatData::e_Rna) {
        const CRNA_ref& rna = mf.GetData().GetRna();

        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (rna.IsSetExt()  &&  rna.GetExt().IsTRNA()) {
                const CTrna_ext& trna = rna.GetExt().GetTRNA();
                string anticodon;
                if (CWriteUtil::GetTrnaAntiCodon(trna, anticodon)) {
                    record.SetAttribute("anticodon", anticodon);
                }
                string codons;
                if (CWriteUtil::GetTrnaCodons(trna, codons)) {
                    record.SetAttribute("codons", codons);
                }
                string aa;
                if (CWriteUtil::GetTrnaProductName(trna, aa)) {
                    record.SetAttribute("product", aa);
                    return true;
                }
            }
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsName()) {
            record.SetAttribute("product", rna.GetExt().GetName());
            return true;
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsGen()  &&  
                rna.GetExt().GetGen().IsSetProduct() ) {
            record.SetAttribute("product", rna.GetExt().GetGen().GetProduct());
            return true;
        }
    }

    // finally, look for gb_qual
    if (mf.IsSetQual()) {
        const CSeq_feat::TQual& quals = mf.GetQual();
        for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
                cit != quals.end(); ++cit) {
            if ((*cit)->IsSetQual()  &&  (*cit)->IsSetVal()  &&  
                    (*cit)->GetQual() == "product") {
                record.SetAttribute("product", (*cit)->GetVal());
                return true;
            }
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeNcrnaClass(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            (mf.GetData().GetSubtype() != CSeqFeatData::eSubtype_ncRNA)) {
        return true;
    }
    const CSeqFeatData::TRna& rna = mf.GetData().GetRna();
    if (!rna.IsSetExt()) {
        return true;
    }
    const CRNA_ref::TExt& ext = rna.GetExt();
    if (!ext.IsGen()  ||  !ext.GetGen().IsSetClass()) {
        return true;
    }
    record.SetAttribute("ncrna_class", ext.GetGen().GetClass());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeParent(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
        xAssignFeatureAttributeParentGene(record, fc, mf);
        return true;
    }
    switch (mf.GetFeatSubtype()) {
    default:
        return true;

    case CSeqFeatData::eSubtype_cdregion:
    case CSeqFeatData::eSubtype_exon:
        //mss-275:
        //  we just write the data given to us we don't check it.
        //  if there is a feature that should have a parent but doesn't
        //    then so be it.
        xAssignFeatureAttributeParentMrna(record, fc,mf)  ||
            xAssignFeatureAttributeParentGene(record, fc, mf);
        return true;
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_C_region:
    case CSeqFeatData::eSubtype_D_segment:
    case CSeqFeatData::eSubtype_J_segment:
    case CSeqFeatData::eSubtype_V_segment:
        xAssignFeatureAttributeParentGene(record, fc,mf);
        return true;
    case CSeqFeatData::eSubtype_gene:
        //genes have no parents
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGeneBiotype(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //applies only to genomic records
    if (!fc.IsSequenceGenomicRecord()) {
        return true;
    }

    string biotype;
    if (!feature::GetFeatureGeneBiotype(fc.FeatTree(), mf, biotype)) {
        return true;
    }
    record.SetAttribute("gene_biotype", biotype);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureBasic(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    return (xAssignFeatureType(record, fc, mf)  &&
        xAssignFeatureSeqId(record, fc, mf)  &&
        xAssignFeatureMethod(record, fc, mf)  &&
        xAssignFeatureEndpoints(record, fc, mf)  &&
        xAssignFeatureScore(record, fc, mf)  &&
        xAssignFeatureStrand(record, fc, mf)  &&
        xAssignFeaturePhase(record, fc, mf)  &&
        xAssignFeatureAttributes(record, fc, mf));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeature(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pLoc(new CSeq_loc(CSeq_loc::e_Mix));
    pLoc->Add(mf.GetLocation());
    CWriteUtil::ChangeToPackedInt(*pLoc);

    CBioseq_Handle bsh = fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        record.InitLocation(*pLoc);
        return xAssignFeatureBasic(record, fc, mf);
    }

    //  intervals wrapping around the origin extend beyond the sequence length
    //  instead of breaking and restarting at the origin.
    //
    unsigned int len = bsh.GetInst().GetLength();
    list< CRef< CSeq_interval > >& sublocs = pLoc->SetPacked_int().Set();
    list< CRef<CSeq_interval> >::iterator it;
    list< CRef<CSeq_interval> >::iterator it_ceil=sublocs.end(); 
    list< CRef<CSeq_interval> >::iterator it_floor=sublocs.end();
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        //fix intervals broken in two for crossing the origin to extend
        //  into virtual space instead
        CSeq_interval& subint = **it;
        if (subint.IsSetFrom()  &&  subint.GetFrom() == 0) {
            it_floor = it;
        }
        if (subint.IsSetTo()  &&  subint.GetTo() == len-1) {
            it_ceil = it;
        }
        if (it_floor != sublocs.end()  &&  it_ceil != sublocs.end()) {
            break;
        } 
    }
    if ( it_ceil != sublocs.end()  &&  it_floor != sublocs.end() ) {
        (*it_ceil)->SetTo( (*it_ceil)->GetTo() + (*it_floor)->GetTo() + 1 );
        sublocs.erase(it_floor);
    }

    record.InitLocation(*pLoc);
    return xAssignFeatureBasic(record, fc, mf);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSource(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    return (xAssignSourceType(record, fc, bsh)  &&
        xAssignSourceSeqId(record, fc, bsh)  &&
        xAssignSourceMethod(record, fc, bsh)  &&
        xAssignSourceEndpoints(record, fc, bsh)  &&
        xAssignSourceAttributes(record, fc, bsh));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceType(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    record.SetType("region");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceSeqId(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    const string defaultId(".");
    string bestId;

    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if (!pId) {
        record.SetSeqId(defaultId);
        return true;
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*pId);
    if (!CWriteUtil::GetBestId(idh, bsh.GetScope(), bestId)) {
        record.SetSeqId(defaultId);
        return true;
    }

    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceMethod(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    string method(".");
    CWriteUtil::GetIdType(bsh, method);
    record.SetMethod(method);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceEndpoints(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = 0;//always for source
    unsigned int seqStop = bsh.GetBioseqLength() - 1;
    ENa_strand seqStrand = eNa_strand_plus;
    if (bsh.CanGetInst_Strand()) {
        //now that's nuts- how should we act on GetInst_Strand() ???
    }
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributes(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    return (xAssignSourceAttributeGbKey(record, fc, bsh)  &&
        xAssignSourceAttributeMolType(record, fc, bsh)  &&
        xAssignSourceAttributeIsCircular(record, fc, bsh)  &&
        xAssignSourceAttributesBioSource(record, fc, bsh)); 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGbKey(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    record.SetAttribute("gbkey", "Src");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeMolType(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    string molType;
    if (!CWriteUtil::GetBiomol(bsh, molType)) {
        return true;
    }
    record.SetAttribute("mol_type", molType);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeIsCircular(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }
    record.SetAttribute("Is_circular", "true");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesBioSource(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (!sdi) {
        return true;
    }
    const CBioSource& bioSrc = sdi->GetSource();
    return (xAssignSourceAttributeGenome(record, fc, bioSrc)  &&
        xAssignSourceAttributeName(record, fc, bioSrc)  &&
        xAssignSourceAttributeDbxref(record, fc, bioSrc)  &&
        xAssignSourceAttributesOrgMod(record, fc, bioSrc)  &&
        xAssignSourceAttributesSubSource(record, fc, bioSrc));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGenome(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    string genome;
    if (!CWriteUtil::GetGenomeString(bioSrc, genome)) {
        return true;
    }
    record.SetAttribute("genome", genome);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeName(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    string name = bioSrc.GetRepliconName();
    if (name.empty()) {
        return true;
    }
    record.SetAttribute("Name", name);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeDbxref(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CDbtag> > DBTAGS;

    if (!bioSrc.IsSetOrg()) {
        return true;
    }
    const COrg_ref& orgRef = bioSrc.GetOrg();
    if (!orgRef.IsSetDb()) {
        return true;
    }
    const DBTAGS& tags = orgRef.GetDb();
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        string tag;
        if (CWriteUtil::GetDbTag(**cit, tag)) {
            record.AddAttribute("Dbxref", tag);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesOrgMod(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<COrgMod> > MODS;

    if (!bioSrc.IsSetOrg()) {
        return true;
    }
    const COrg_ref& orgRef = bioSrc.GetOrg();
    if (!orgRef.IsSetOrgname()) {
        return true;
    }
    const COrgName& orgName = orgRef.GetOrgname();
    if (!orgName.IsSetMod()) {
        return true;
    }
    const MODS& mods = orgName.GetMod();
    for (MODS::const_iterator cit = mods.begin(); cit != mods.end(); ++cit) {
        string key, value;
        if (CWriteUtil::GetOrgModSubType(**cit, key, value)) {
            record.SetAttribute(key, value);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesSubSource(
    CGffSourceRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSubSource> > SUBS;

    if (!bioSrc.IsSetSubtype()) {
        return true;
    }
    const SUBS& subs = bioSrc.GetSubtype();
    for (SUBS::const_iterator cit = subs.begin(); cit != subs.end(); ++cit) {
        string key, value;
        if (CWriteUtil::GetSubSourceSubType(**cit, key, value)) {
            record.SetAttribute(key, value);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureGene(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pRecord( 
        new CGffFeatureRecord(this->xNextGeneId()));
    if (!xAssignFeature(*pRecord, fc, mf)) {
        return false;
    }
    m_GeneMapNew[mf] = pRecord;
    return xWriteFeatureRecords(*pRecord, pRecord->Location(), 0);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureCds(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pCds(new CGffFeatureRecord());
    if (!xAssignFeature(*pCds, fc, mf)) {
        return false;
    }

    const CSeq_loc& PackedInt = pCds->Location();
    bool bStrandAdjust = PackedInt.IsSetStrand()  
        &&  (PackedInt.GetStrand()  ==  eNa_strand_minus);
    int /*CCdregion::EFrame*/ iPhase = 0;
    if (mf.GetData().GetCdregion().IsSetFrame()) {
        iPhase = max(mf.GetData().GetCdregion().GetFrame()-1, 0);
    }
    int iTotSize = -iPhase;
    if (bStrandAdjust && iPhase) {
        iPhase = 3-iPhase;
    }
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > > sublocs( PackedInt.GetPacked_int().Get() );
        list< CRef< CSeq_interval > >::const_iterator it;
        string cdsId = xNextCdsId();
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffFeatureRecord> pExon(
                new CGffFeatureRecord(*pCds));
            pExon->SetRecordId(cdsId);
            pExon->SetType("CDS");
            pExon->DropAttributes("start_range");
            pExon->DropAttributes("end_range");
            pExon->SetLocation(subint);
            pExon->SetPhase( bStrandAdjust ? (3-iPhase) : iPhase );
            if (!xWriteRecord(*pExon)) {
                return false;
            }
            iTotSize = (iTotSize + subint.GetLength());
            if (!bStrandAdjust) {
                iPhase = (3-iTotSize)%3; 
            }
            else {
                iPhase = iTotSize%3;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureRna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string rnaId = xNextTrnaId();
    CRef<CGffFeatureRecord> pRna(new CGffFeatureRecord(rnaId));
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
    }

    if (!xWriteRecord(*pRna)) {
        return false;
    }
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_MrnaMapNew[mf] = pRna;
    }    

    const CSeq_loc& PackedInt = pRna->Location();
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffFeatureRecord> pChild(
                new CGffFeatureRecord(*pRna));
            pChild->SetRecordId(xNextGenericId());
            pChild->DropAttributes("Name"); //explicitely not inherited
            pChild->DropAttributes("start_range");
            pChild->DropAttributes("end_range");
            pChild->SetAttribute("Parent", rnaId);
            pChild->SetType("exon");
            pChild->SetLocation(subint);
            if (!xWriteRecord(*pChild)) {
                return false;
            }
        }
        return true;
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureCDJVSegment(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string id = xNextGenericId();

    CRef<CGffFeatureRecord> pSegment(new CGffFeatureRecord(id));

    if (!xAssignFeature(*pSegment, fc, mf)) {
        return false;
    }

    if (!xWriteRecord(*pSegment)) {
        return false;
    }

    const CSeq_loc& PackedInt = pSegment->Location();

    if (PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        list< CRef< CSeq_interval> >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffFeatureRecord> pChild(
                new CGffFeatureRecord(*pSegment));
            pChild->SetRecordId(xNextGenericId());
            pChild->DropAttributes("Name");
            pChild->DropAttributes("start_range");
            pChild->DropAttributes("end_range");
            pChild->SetAttribute("Parent", id);
            pChild->SetType("exon");
            pChild->SetLocation(subint);
            if (!xWriteRecord(*pChild)) {
                return false;
            }
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureGeneric(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pParent(new CGffFeatureRecord(xNextGenericId()));
    if (!xAssignFeature(*pParent, fc, mf)) {
        return false;
    }

    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    return xWriteFeatureRecords( *pParent, mf.GetLocation(), seqlength );
}

//  ============================================================================
bool CGff3Writer::xWriteFeatureRecords(
    const CGffFeatureRecord& record,
    const CSeq_loc& location,
    unsigned int seqLength )
//  ============================================================================
{
    const CSeq_loc& loc = record.Location();
    if (!loc.IsPacked_int()  ||  !loc.GetPacked_int().CanGet()) {
        return xWriteRecord(record);
    }
    const list<CRef<CSeq_interval> >& sublocs = loc.GetPacked_int().Get();
    if (sublocs.size() == 1) {
        return xWriteRecord(record);
    }//<<
    list<CRef<CSeq_interval> >::const_iterator it;
    string totIntervals = string("/") + NStr::NumericToString(sublocs.size());
    unsigned int curInterval = 1;
    for (it = sublocs.begin(); it != sublocs.end(); ++it) {
        const CSeq_interval& subint = **it;
        CRef<CGffFeatureRecord> pChild(new CGffFeatureRecord(record));
        pChild->SetLocation(subint);
        string part = NStr::IntToString(curInterval++) + totIntervals;
        pChild->SetAttribute("part", part);
        if (!xWriteRecord(*pChild)) {
            return false;
        }
    }
    return true;
}

//  ============================================================================
void CGff3Writer::xWriteAlignment( 
    const CGffAlignmentRecord& record )
//  ============================================================================
{
    m_Os << record.StrId() << '\t';
    m_Os << record.StrSource() << '\t';
    m_Os << record.StrType() << '\t';
    m_Os << record.StrSeqStart() << '\t';
    m_Os << record.StrSeqStop() << '\t';
    m_Os << record.StrScore() << '\t';
    m_Os << record.StrStrand() << '\t';
    m_Os << record.StrPhase() << '\t';
    m_Os << record.StrAttributes() << '\n';
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ============================================================================
{
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return false;
    }
    TGeneMapNew::iterator it = m_GeneMapNew.find(gene);
    if (it == m_GeneMapNew.end()) {
        return false;
    }
    vector<string> parentId;
    if (!it->second->GetAttributes("ID", parentId)) {
        return false;
    }
    record.SetParent(parentId.front());
    return true;
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentMrna(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ============================================================================
{
    CMappedFeat mrna;
    switch (mf.GetFeatSubtype()) {
        default:
            mrna = feature::GetBestParentForFeat(
                mf, CSeqFeatData::eSubtype_mRNA, &fc.FeatTree());
            break;
        case CSeqFeatData::eSubtype_cdregion:
            mrna = feature::GetBestMrnaForCds(mf, &fc.FeatTree());
            break;
    }
    TMrnaMapNew::iterator it = m_MrnaMapNew.find(mrna);
    if (it == m_MrnaMapNew.end()) {
        return false;
    }
    vector<string> parentId;
    if (!it->second->GetAttributes("ID", parentId)) {
        return false;
    }
    record.SetParent(parentId.front());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteRecord( 
    const CGffBaseRecord& record )
//  ----------------------------------------------------------------------------
{
//    if (record.StrType() == "gene"  &&  record.StrSeqStart() == "15956") {
//        cerr << "";
//        bDebugHere = true;
//    }
    m_Os << record.StrSeqId() << '\t';
    m_Os << record.StrMethod() << '\t';
    m_Os << record.StrType() << '\t';
    m_Os << record.StrSeqStart() << '\t';
    m_Os << record.StrSeqStop() << '\t';
    m_Os << record.StrScore() << '\t';
    m_Os << record.StrStrand() << '\t';
    m_Os << record.StrPhase() << '\t';
    m_Os << record.StrAttributes() << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextGenericId()
//  ----------------------------------------------------------------------------
{
    string nextId = string("id") + NStr::UIntToString(m_uPendingGenericId++);
    return nextId;
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextGeneId()
//  ----------------------------------------------------------------------------
{
    return (string("gene") + NStr::UIntToString(m_uPendingGeneId++));
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextAlignId()
//  ----------------------------------------------------------------------------
{
    string nextId = string("aln") + NStr::UIntToString(m_uPendingAlignId++);
    return nextId;
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextCdsId()
//  ----------------------------------------------------------------------------
{
    return (string("cds") + NStr::UIntToString(m_uPendingCdsId++));
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextTrnaId()
//  ----------------------------------------------------------------------------
{
    return (string("rna") + NStr::UIntToString(m_uPendingTrnaId++));
}

END_NCBI_SCOPE

