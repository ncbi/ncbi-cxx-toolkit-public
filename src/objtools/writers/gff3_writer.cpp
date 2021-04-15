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
#include <objects/seq/so_map.hpp>
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
#include <objects/seqfeat/Imp_feat.hpp>
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
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>
#include <objmgr/util/weight.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

#include <array>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define IS_INSERTION(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define IS_DELETION(sf, tf) \
    ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define IS_MATCH(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

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
    const CSeq_id& source)
//  ----------------------------------------------------------------------------
{
    const char* strProtMatch     = "protein_match";
    const char* strEstMatch      = "EST_match";
    const char* strCdnaMatch     = "cDNA_match";

    CSeq_id::EAccessionInfo sourceInfo = source.IdentifyAccession();

    if (sourceInfo & CSeq_id::fAcc_prot) {
        return strProtMatch;
    }

    if ((sourceInfo & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) {
        return strEstMatch;
    }

    return strCdnaMatch;

}
    

//  ----------------------------------------------------------------------------
bool sFeatureHasChildOfSubtype(
    const CMappedFeat& mf,
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
    unsigned int uFlags,
    bool sortAlignments) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags ),
    m_sDefaultMethod(""),
    m_SortAlignments(sortAlignments),
    m_BioseqHandle(CBioseq_Handle())
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
    unsigned int uFlags,
    bool sortAlignments) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags ),
    m_SortAlignments(false),
    m_BioseqHandle(CBioseq_Handle())
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
SAnnotSelector& CGff3Writer::xSetJunkFilteringAnnotSelector(void)
//  ----------------------------------------------------------------------------
{
    auto& selector = CGff2Writer::SetAnnotSelector();
    selector.ExcludeFeatSubtype(CSeqFeatData::eSubtype_pub)
        .ExcludeFeatSubtype(CSeqFeatData::eSubtype_rsite)
        .ExcludeFeatSubtype(CSeqFeatData::eSubtype_seq)
        .ExcludeFeatSubtype(CSeqFeatData::eSubtype_non_std_residue);
    selector.ExcludeFeatType(CSeqFeatData::e_Biosrc);
    if (!(this->m_uFlags & CGff3Writer::fIncludeProts)) {
        selector.ExcludeFeatSubtype(CSeqFeatData::eSubtype_prot);
    }
    return selector;
}


//  ----------------------------------------------------------------------------
void CGff3Writer::SetBioseqHandle(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    m_BioseqHandle = bsh;
}


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
//    m_uRecordId++;
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSeqAnnotHandle(
    CSeq_annot_Handle sah )
//  ----------------------------------------------------------------------------
{
    CConstRef<CSeq_annot> pAnnot = sah.GetCompleteSeq_annot();

    if ( pAnnot->IsAlign() ) {
        for ( CAlign_CI it( sah ); it; ++it ) {  // Could restrict the range here
            if ( ! xWriteAlign( *it ) ) {
                return false;
            }
        }
        return true;
    }

    SAnnotSelector sel = xSetJunkFilteringAnnotSelector();
    sel.SetLimitSeqAnnot(sah).SetResolveNone();
    CRef<CSeq_loc> loc = Ref(new CSeq_loc());
    loc->SetWhole();
    sel.SetSourceLoc(*loc);

    CFeat_CI feat_iter(sah, sel);

    CGffFeatureContext fc(feat_iter, CBioseq_Handle(), sah);
    return x_WriteFeatureContext(fc);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlign(
    CAlign_CI align_it) 
//  ----------------------------------------------------------------------------
{
    if (!align_it) {
        return false;
    }

    return xWriteAlign(*align_it);
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
    for (EXONS::const_iterator cit = exons.begin(); cit != exons.end(); ++cit) {
        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }
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
bool CGff3Writer::xSplicedSegHasProteinProd(
    const CSpliced_seg& spliced) 
//  ----------------------------------------------------------------------------
{
    if (spliced.IsSetProduct_type() ) {
        return (spliced.GetProduct_type() == CSpliced_seg::eProduct_type_protein);
    }
    // The following lines of code should never be called since 
    // the product type should always be specified
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, *m_pScope, sequence::eGetId_Best);

    CSeq_id::EAccessionInfo productInfo;
    if (bestH) {
        productInfo = bestH.GetSeqId()->IdentifyAccession();
    }
    else {
        productInfo = productId.IdentifyAccession();
    }
    
    return (productInfo & CSeq_id::fAcc_prot);
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
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&seqId, CSeq_id::eContent);
    }
    else {
        genomicId.GetLabel(&seqId, CSeq_id::eContent);
    }
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
        if (bestH) {
            const CSeq_id& bestId = *bestH.GetSeqId();
            CWriteUtil::GetIdType(bestId, method);
            record.SetMethod(method);
            return true;
        }
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
    if (bestH) {
        const CSeq_id& bestId = *bestH.GetSeqId();
        CWriteUtil::GetIdType(bestId, method);
        record.SetMethod(method);
    }
    // give up and move on
    record.SetMethod(".");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignAlignmentSplicedType(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    if (spliced.IsSetProduct_type() &&
        spliced.GetProduct_type() == CSpliced_seg::eProduct_type_protein) {
        record.SetType("protein_match");
        return true;
    }

    CSeq_id_Handle genomicH = sequence::GetId(
        spliced.GetGenomic_id(), *m_pScope, sequence::eGetId_Best);
    CSeq_id_Handle productH = sequence::GetId(
        spliced.GetProduct_id(), *m_pScope, sequence::eGetId_Best);
    if (!genomicH || !productH) {
        // MSS-225: There _are_ accessions that are not in ID (yet). 
        return true;
    }
    const CSeq_id& genomicId = *genomicH.GetSeqId();
    record.SetType(sBestMatchType(genomicId));
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
    const bool isProteinProd = xSplicedSegHasProteinProd(spliced);
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
            // Round to next multiple of tgtWidth to account for reverse frameshifts
            record.AddMatch((chunk.GetDiag()+tgtWidth-1)/tgtWidth);
            break;
        case CSpliced_exon_chunk::e_Match:
            // Round to next multiple of tgtWidth to account for reverse framshifts
            record.AddMatch((chunk.GetMatch()+tgtWidth-1)/tgtWidth); 
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            {
                const unsigned int del_length = chunk.GetGenomic_ins()/tgtWidth;
                if (del_length > 0) {
                    record.AddDeletion(del_length);
                }
            }
            if (isProteinProd) {
                const unsigned int forward_shift = chunk.GetGenomic_ins()%tgtWidth;
                if (forward_shift >  0) {
                    record.AddForwardShift(forward_shift);
                }
            }
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            if (isProteinProd) { 
                const unsigned int reverse_shift = chunk.GetProduct_ins()%tgtWidth;
                if (reverse_shift > 0) { 
                    record.AddReverseShift(reverse_shift);
                }
            }
            {
                const unsigned int insert_length = chunk.GetProduct_ins()/tgtWidth;
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
    string target;
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&target, CSeq_id::eContent);
    }
    else {
        productId.GetLabel(&target, CSeq_id::eContent);
    }

    const bool isProteinProd = xSplicedSegHasProteinProd(spliced);
    const unsigned int tgtWidth = isProteinProd ? 3 : 1;


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

    for (CAlnMap::TDim sourceRow = 1; sourceRow < alnMap.GetNumRows(); ++sourceRow) {
        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }
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
    record.SetType("match");
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
    const unsigned int tgtWidth = (sourceInfo & CSeq_id::fAcc_prot) ? 3 : 1;

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

    unsigned int tgtWidth; //could be 1 or 3, depending on nuc or prot
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
                const unsigned int reverse_shift = tgtPiece.GetLength()%tgtWidth;
                if (reverse_shift > 0) { // Can only occur when target is prot
                    record.AddReverseShift(reverse_shift);
                }
            }

            const unsigned int insert_length = tgtPiece.GetLength()/tgtWidth;
            if (insert_length > 0) { 
                record.AddInsertion(insert_length);
            }
        }

        if (IS_DELETION(tgtFlags, srcFlags)) {
            CRange<int> srcPiece = alnMap.GetRange(srcRow, seg);

            const unsigned int del_length = srcPiece.GetLength()/tgtWidth;
            if (del_length > 0) {
                record.AddDeletion(del_length);
            }

            if (tgtWidth > 1) {
                const unsigned int forward_shift = srcPiece.GetLength()%tgtWidth;
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
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    //sequence-region
    string id;
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if ( pId ) {
        if (!CGenbankIdResolve::Get().GetBestId(
                CSeq_id_Handle::GetHandle(*pId),
                bsh.GetScope(), 
                id)) {
            id = "<unknown>";
        }
    }

    TSeqPos start = 1;
    TSeqPos stop = bsh.GetBioseqLength();
    if (!m_Range.IsWhole()) {
        start = m_Range.GetFrom() + 1;
        stop = m_Range.GetTo() + 1;
    }
    m_Os << "##sequence-region " << id << " " << start << " " << stop << '\n';

    //species
    const string base_url = 
        "https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?";
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (sdi) {
        const CBioSource& bs = sdi->GetSource();
        if (bs.IsSetOrg()  &&  bs.GetOrg().GetTaxId() != ZERO_TAX_ID) {
            string tax_id = NStr::NumericToString(bs.GetOrg().GetTaxId());
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

struct SCompareAlignments {

    CScope& m_Scope;

    SCompareAlignments(CScope& scope) : m_Scope(scope) {}

    bool operator()(
            const pair<CConstRef<CSeq_align>, string>& p1,
            const pair<CConstRef<CSeq_align>, string>& p2) 
    {

        CConstRef<CSeq_align> align1 = p1.first;
        CConstRef<CSeq_align> align2 = p2.first;

        if (!align1 && align2) {
             return true;
        }

        if ((align1 && !align2) ||
            (!align1 && !align2) ) {
            return false;
        }


        auto make_key = [](const pair<CConstRef<CSeq_align>, string>& p, CScope& scope) {
            const CSeq_align& align = *(p.first);
            const string alignId = p.second;

            string subject_accession;
            try {
                subject_accession = sequence::GetAccessionForId(align.GetSeq_id(1), scope);
            } catch (...) {
            }

            string target_accession;
            try {
                target_accession = sequence::GetAccessionForId(align.GetSeq_id(0), scope);
            } catch (...) {
            }

            return make_tuple(
                subject_accession,
                align.GetSeqStart(1), 
                align.GetSeqStop(1),
                align.GetSeqStrand(1),
                target_accession,
                align.GetSeqStart(0),
                align.GetSeqStop(0),
                align.GetSeqStrand(0),
                alignId
                );
        };

        return (make_key(p1, m_Scope) < make_key(p2, m_Scope));
    }
};

//  ----------------------------------------------------------------------------
void CGff3Writer::x_SortAlignments(TAlignCache& alignCache,
                                   CScope& scope)
//  ----------------------------------------------------------------------------
{
    alignCache.sort(SCompareAlignments(scope));
}


string s_GetAlignID(const CSeq_align& align) {
    if (align.IsSetId()) {
        const CSeq_align::TId& ids = align.GetId();
        for (CSeq_align::TId::const_iterator it = ids.begin();
            it != ids.end(); ++it) {
            if ((*it)->IsStr()) {
                return (*it)->GetStr();
            }
        }
    }
    return "";
}


//  ----------------------------------------------------------------------------
bool s_RangeContains(const CRange<TSeqPos>& range, const TSeqPos pos) 
//  ----------------------------------------------------------------------------
{
    if ((range.GetFrom() <= pos) &&
        (range.GetTo() >= pos)) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::xPassesFilterByViewMode(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if ((m_uFlags & fIncludeProts)  &&  !(m_uFlags & fExcludeNucs)) {
        // after all, if we are seeing it here then it must be nuc or prot,
        //  whether it is marked as such or not. 
        return true;
    }

    if (!(m_uFlags & fExcludeNucs)) {
        return CWriteUtil::IsNucleotideSequence(bsh);
    }
    if (m_uFlags & fIncludeProts) {
        return CWriteUtil::IsProteinSequence(bsh);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteBioseqHandle(
    CBioseq_Handle bsh) 
//  ----------------------------------------------------------------------------
{
    if (!xPassesFilterByViewMode(bsh)) {
        return true; //nothing to do
    }

    xSetJunkFilteringAnnotSelector();
        
    if (!xWriteSequenceHeader(bsh) ) {
        return false;
    }
    if (!xWriteSource(bsh)) {
        return false;
    }

    CAnnot_CI aci(bsh, SetAnnotSelector());
    if (aci) {
        if (!xWriteSequence(bsh)) {
            return false;
        }
    }
    else {
        const auto& cc = bsh.GetCompleteBioseq();
        if (!cc->IsSetAnnot()) {
            return true;
        }
        const auto& annots = cc->GetAnnot();
        if (annots.empty()) {
            return true;
        }
        const auto& data = cc->GetAnnot().front();
        auto ah = m_pScope->GetObjectHandle(*data);
        if (!x_WriteSeqAnnotHandle(ah)) {
            return false;
        }
    }
    SAnnotSelector sel = SetAnnotSelector();
    const auto& display_range = GetRange();
    if ( m_SortAlignments ) {
        TAlignCache alignCache;

        for (CAlign_CI align_it(bsh, display_range, sel); align_it; ++align_it) {
            const string alignId = s_GetAlignID(*align_it); // Might be an empty string
            CConstRef<CSeq_align> pAlign = ConstRef(&(*align_it));
            alignCache.push_back(make_pair(pAlign,alignId));

            string target_accession = sequence::GetAccessionForId(align_it->GetSeq_id(0), m_pScope.GetNCObject());
        }

        x_SortAlignments(alignCache, m_pScope.GetNCObject());

        for (auto alignPair : alignCache) {
            xWriteAlign(*(alignPair.first), alignPair.second);
        }
        return true;
    }

    CAlign_CI align_it(bsh, display_range, sel);
    WriteAlignments(align_it);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAllChildren(
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    feature::CFeatTree& featTree = fc.FeatTree();
    vector<CMappedFeat> vChildren;
    featTree.GetChildrenTo(mf, vChildren);
    for (auto cit = vChildren.begin(); cit != vChildren.end(); ++cit) {
        CMappedFeat mChild = *cit;
        if (!xWriteNucleotideFeature(fc, mChild)) {
            return false;
        }
        if (!xWriteAllChildren(fc, mChild)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSource(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    CSeqdesc_CI sdi(bsh.GetParentEntry(), CSeqdesc::e_Source, 0);
    if (!sdi) {
        return true; 
    }
    CRef<CGff3SourceRecord> pSource(new CGff3SourceRecord());
    if (!xAssignSource(*pSource, bsh)) {
        return false;
    }
    return xWriteRecord(*pSource);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeature(
    CFeat_CI feat_it)
//  ----------------------------------------------------------------------------
{
    if (!feat_it) {
        return false;
    }

    CGffFeatureContext fc(feat_it, m_BioseqHandle, feat_it.GetAnnot());

    return xWriteNucleotideFeature(fc, *feat_it);
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSequence(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::IsProteinSequence(bsh)) {
        return xWriteProteinSequence(bsh);
    }
    return xWriteNucleotideSequence(bsh);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteProteinSequence(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = SetAnnotSelector();
    sel.IncludeFeatType(CSeqFeatData::e_Prot);
    const auto& display_range = GetRange();
    CFeat_CI feat_iter(bsh, display_range, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    while (feat_iter) {
        CMappedFeat mf = *feat_iter;
        xWriteProteinFeature(fc, mf);
        ++feat_iter;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureContext(
    CGffFeatureContext& fc)
//  ----------------------------------------------------------------------------
{
    vector<CMappedFeat> vRoots = fc.FeatTree().GetRootFeatures();
    std::sort(vRoots.begin(), vRoots.end(), CWriteUtil::CompareLocations);
    for (auto pit = vRoots.begin(); pit != vRoots.end(); ++pit) {
        CMappedFeat mRoot = *pit;
        fc.AssignShouldInheritPseudo(false);
        if (!xWriteNucleotideFeature(fc, mRoot)) {
            // error!
            continue;
        }
        xWriteAllChildren(fc, mRoot);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteNucleotideSequence(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = SetAnnotSelector();
    const auto& display_range = GetRange();
    CFeat_CI feat_iter(bsh, display_range, sel);
    //CFeat_CI feat_iter(bsh);
    CGffFeatureContext fc(feat_iter, bsh);
    return x_WriteFeatureContext(fc);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteProteinFeature(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (IsCanceled()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "Processing terminated by user");
    }

    // Skip feature if it lies outside the display interval - RW-158
    if (!GetRange().IsWhole() &&
        mf.GetLocation().GetTotalRange().IntersectionWith(GetRange()).Empty()) {
        return true;
    }

    CRef<CGff3FeatureRecord> pRecord(new CGff3FeatureRecord());
    if (!xAssignFeature(*pRecord, fc, mf)) {
        return false;
    }
    if (mf.GetData().IsProt()) {
        if (mf.GetData().GetProt().IsSetName()) {
            pRecord->AddAttribute("product", mf.GetData().GetProt().GetName().front());
        }
        auto weight = GetProteinWeight(mf.GetOriginalFeature(), *m_pScope, nullptr, 0);
        pRecord->AddAttribute(
            "calculated_mol_wt", NStr::NumericToString(int(weight+0.5)));
    }
    return xWriteRecord(*pRecord);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteNucleotideFeature(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (IsCanceled()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "Processing terminated by user");
    }

    // Skip feature if it lies outside the display interval - RW-158
    if (!GetRange().IsWhole() &&
        mf.GetLocation().GetTotalRange().IntersectionWith(GetRange()).Empty()) {
        return true;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    try {
        switch(subtype) {
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
            case CSeqFeatData::eSubtype_cdregion: {
                return xWriteFeatureCds( fc, mf );
            }
            case CSeqFeatData::eSubtype_tRNA:
                return xWriteFeatureTrna( fc, mf );

            case CSeqFeatData::eSubtype_pub:
                return true; //ignore
            case CSeqFeatData::eSubtype_prot:
            case CSeqFeatData::eSubtype_preprotein:
            case CSeqFeatData::eSubtype_sig_peptide:
            case CSeqFeatData::eSubtype_sig_peptide_aa:
            case CSeqFeatData::eSubtype_transit_peptide:
            case CSeqFeatData::eSubtype_transit_peptide_aa:
            case CSeqFeatData::eSubtype_mat_peptide:
            case CSeqFeatData::eSubtype_mat_peptide_aa: {
                return true; //already handled in context of cds
            }
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
void
sGetWrapInfo(
    const list<CRef<CSeq_interval> >& subInts,
    CGffFeatureContext& fc,
    unsigned int& wrapSize,
    unsigned int& wrapPoint)
//  ----------------------------------------------------------------------------
{
    wrapSize = wrapPoint = 0;
    if (subInts.empty()) {
        return;
    }
    if (!fc.BioseqHandle().CanGetInst_Length()) {
        return;
    }
    wrapSize = fc.BioseqHandle().GetInst_Length();
    const auto& front = *subInts.front();
    wrapPoint = (front.CanGetStrand()  &&  front.GetStrand() == eNa_strand_minus) ?
        subInts.back()->GetFrom() :
        subInts.front()->GetFrom();
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureTrna(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{

    CRef<CGff3FeatureRecord> pRna( new CGff3FeatureRecord() );
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
	}


    if(CWriteUtil::IsTransspliced(mf)){    
        xAssignFeatureAttributeParentGene(*pRna, fc, mf);
        TSeqPos seqlength = 0;
        if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
            seqlength = fc.BioseqHandle().GetInst().GetLength();

        if (!xWriteFeatureRecords( *pRna, mf.GetLocation(), seqlength ) ) {
            return false;
        }
    }
    else {
        if(!xWriteRecord(*pRna)){
            return false;
        }
    }
    const auto rnaId = pRna->Id();
	const CSeq_loc& PackedInt = pRna->Location();

    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();

        unsigned int wrapSize(0), wrapPoint(0);
        sGetWrapInfo(sublocs, fc, wrapSize, wrapPoint);

        int partNum = 1;
        bool useParts = xIntervalsNeedPartNumbers(sublocs);

        for ( auto it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3FeatureRecord> pChild(new CGff3FeatureRecord(*pRna));
            pChild->SetRecordId(m_idGenerator.GetNextGffExonId(rnaId));
            pChild->SetType("exon");
            pChild->SetLocation(subint, wrapSize, wrapPoint);
            pChild->SetParent(rnaId);
            if (useParts) {
                pChild->SetAttribute("part", NStr::NumericToString(partNum++));
            }
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
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    //rw-340: attempt to use so_map API:
    const auto& feature = mf.GetOriginalFeature();
    string so_type;
    if (CSoMap::FeatureToSoType(feature, so_type)) {
        record.SetType(so_type);
        return true;
    }

    //fallback
    record.SetType("region");
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetMethodFromModelEvidence(
    const CMappedFeat& mf,
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
    const CMappedFeat& mf )
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
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CGffBaseRecord& baseRecord = record;

    unsigned int seqStart(0);
    unsigned int seqStop(0);

    if (CWriteUtil::IsTransspliced(mf)) {
        if (!CWriteUtil::GetTranssplicedEndpoints(record.Location(), 
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
bool CGff3Writer::xAssignFeatureStrand(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    record.SetStrand(mf.GetLocation().GetStrand());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeaturePhase(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatSubtype() == CSeq_feat::TData::eSubtype_cdregion) {
        record.SetPhase(0);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributesFormatIndependent(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (!CGff2Writer::xAssignFeatureAttributesFormatIndependent(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureAttributeTranscriptId(record, mf)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributesFormatSpecific(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    CGff3FeatureRecord& record = dynamic_cast<CGff3FeatureRecord&>(rec);
    return (
        xAssignFeatureAttributeID(record, fc, mf)  &&
        xAssignFeatureAttributeParent(record, fc, mf)  &&
        xAssignFeatureAttributeName(record, mf)); //must come last!
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeDbxref(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
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
            if (parent  &&  parent.IsSetData()  &&  parent.GetData().IsGene()) {
                const auto& geneRef = mf.GetGeneXref();
                if (geneRef  &&  geneRef->IsSuppressed()) {
                    return true;
                }
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
bool CGff3Writer::xAssignFeatureAttributeNote(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    string note;
    CSeqFeatData::ESubtype st = mf.GetFeatSubtype();

    vector<string> acceptedClasses = {
        "antisense_RNA",
        "autocatalytically_spliced_intron",
        "guide_RNA",
        "hammerhead_ribozyme",
        "lncRNA",
        "miRNA",
        "ncRNA",
        "other",
        "piRNA",
        "rasiRNA",
        "ribozyme",
        "RNase_MRP_RNA",
        "RNase_P_RNA",
        "scRNA",
        "siRNA",
        "snoRNA",
        "snRNA",
        "SRP_RNA",
        "telomerase_RNA",
        "vault_RNA",
        "Y_RNA"};

    if (st == CSeqFeatData::eSubtype_ncRNA) {
        string ncrna_class = mf.GetNamedQual("ncRNA_class");
        if (ncrna_class.empty()) {
            if (mf.IsSetData()  &&
                    mf.GetData().IsRna()  &&
                    mf.GetData().GetRna().IsSetExt()  &&
                    mf.GetData().GetRna().GetExt().IsGen()  &&
                    mf.GetData().GetRna().GetExt().GetGen().IsSetClass()) {
                ncrna_class = mf.GetData().GetRna().GetExt().GetGen().GetClass();
                if (ncrna_class == "classRNA") {
                    ncrna_class = "";
                }
            }
        }
        if (ncrna_class.empty()) {
            if (mf.IsSetData()  &&
                    mf.GetData().IsRna()  &&
                    mf.GetData().GetRna().IsSetType()) {
                auto ncrna_type = mf.GetData().GetRna().GetType();
                ncrna_class = CRNA_ref::GetRnaTypeName(ncrna_type);
            }
        }
        const auto cit = std::find(
            acceptedClasses.begin(), acceptedClasses.end(), ncrna_class);
        if (cit == acceptedClasses.end()) {
            note = ncrna_class;
        }
    }
    if (st == CSeqFeatData::eSubtype_misc_recomb) {
        string recomb_class = mf.GetNamedQual("recombination_class");
        if (!recomb_class.empty()  &&  recomb_class != "other") {
            auto validClasses = CSeqFeatData::GetRecombinationClassList();
            auto cit = std::find(validClasses.begin(), validClasses.end(), recomb_class);
            if (cit == validClasses.end()) {
                note = recomb_class;
            }
        }
    }
    if (st == CSeqFeatData::eSubtype_regulatory) {
        string regulatory_class = mf.GetNamedQual("regulatory_class");
        if (!regulatory_class.empty()  && regulatory_class != "other") {
            auto validClasses = CSeqFeatData::GetRegulatoryClassList();
            auto cit = std::find(validClasses.begin(), validClasses.end(), regulatory_class);
            if (cit == validClasses.end()) {
                note = regulatory_class;
            }
        }
    }        

    string comment;
    if (mf.IsSetComment()) {
        comment = mf.GetComment();
    }
    if (!note.empty()) {
        if (!comment.empty()) {
            note += "; " + comment;
        }
    }
    else {
        note = comment;
    }
    if (!note.empty()) {
        record.SetAttribute("Note", note);
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeTranscriptId(
    CGffFeatureRecord& record,
    const CMappedFeat& mf )
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
        if (CGenbankIdResolve::Get().GetBestId(
                mf.GetProductId(), 
                mf.GetScope(), 
                transcript_id)) {
            record.SetAttribute("transcript_id", transcript_id);
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeName(
    CGffFeatureRecord& record,
    const CMappedFeat& mf )
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

        case CSeqFeatData::eSubtype_region:
            record.SetAttribute("Name", mf.GetData().GetRegion());
            return true;
    }

    if (record.GetAttributes("transcript_id", value)) {
        record.SetAttribute("Name", value.front());
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeNcrnaClass(
    CGffFeatureRecord& record,
    const CMappedFeat& mf )
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
bool CGff3Writer::xAssignFeatureAttributeID(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    auto rawId = m_idGenerator.GetGffId(mf, fc);
    record.SetRecordId(rawId);
    return true;
}



//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeParent(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
        if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_ncRNA &&
            xAssignFeatureAttributeParentpreRNA(record, fc, mf)) {
            return true;
        }
        xAssignFeatureAttributeParentGene(record, fc, mf);
        return true;
    }


    switch (mf.GetFeatSubtype()) {
    default: {
        return true; // by default: no Parent assigned
    }

    case CSeqFeatData::eSubtype_ncRNA:
        return xAssignFeatureAttributeParentpreRNA(record, fc, mf)  ||
            xAssignFeatureAttributeParentGene(record, fc, mf);

    case CSeqFeatData::eSubtype_cdregion:
    case CSeqFeatData::eSubtype_exon:
        //mss-275:
        //  we just write the data given to us we don't check it.
        //  if there is a feature that should have a parent but doesn't
        //    then so be it.
        return xAssignFeatureAttributeParentVDJsegmentCregion(record, fc, mf) ||
            xAssignFeatureAttributeParentMrna(record, fc,mf)  ||
            xAssignFeatureAttributeParentGene(record, fc, mf);

    case CSeqFeatData::eSubtype_transit_peptide:
    case CSeqFeatData::eSubtype_transit_peptide_aa:
    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_mat_peptide_aa:
    case CSeqFeatData::eSubtype_sig_peptide:
    case CSeqFeatData::eSubtype_sig_peptide_aa:
    case CSeqFeatData::eSubtype_propeptide:
        return xAssignFeatureAttributeParentCds(record, fc, mf);

    case CSeqFeatData::eSubtype_intron:
    case CSeqFeatData::eSubtype_N_region:
    case CSeqFeatData::eSubtype_polyA_site:
    case CSeqFeatData::eSubtype_S_region:
    case CSeqFeatData::eSubtype_V_region:
    case CSeqFeatData::eSubtype_5UTR:
    case CSeqFeatData::eSubtype_3UTR:
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_C_region:
    case CSeqFeatData::eSubtype_D_segment:
    case CSeqFeatData::eSubtype_J_segment:
    case CSeqFeatData::eSubtype_V_segment:
        return xAssignFeatureAttributeParentGene(record, fc, mf);

    case CSeqFeatData::eSubtype_regulatory:
        return xAssignFeatureAttributeParentGene(record, fc, mf)  ||
            xAssignFeatureAttributeParentRegion(record, fc, mf);

    case CSeqFeatData::eSubtype_misc_feature:
    case CSeqFeatData::eSubtype_protein_bind:
    case CSeqFeatData::eSubtype_repeat_region:
    case CSeqFeatData::eSubtype_misc_recomb:
    case CSeqFeatData::eSubtype_mobile_element:
    case CSeqFeatData::eSubtype_rep_origin:
    case CSeqFeatData::eSubtype_misc_structure:
    case CSeqFeatData::eSubtype_stem_loop:
        return xAssignFeatureAttributeParentRegion(record, fc, mf);
    }

    return true; 
    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributesQualifiers(
    CGffFeatureRecord& rec,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    //FIX_ME
    CGff3FeatureRecord& record = dynamic_cast<CGff3FeatureRecord&>(rec);
    static set<string> gff3_attributes = 
    {"ID", "Name", "Alias", "Parent", "Target", "Gap", "Derives_from",
     "Note", "Dbxref", "Ontology_term", "Is_circular"};
    

    const CSeq_feat::TQual& quals = mf.GetQual();
    for (const auto& qual: quals) {
        if (!qual->IsSetQual()  ||  !qual->IsSetVal()) {
            continue;
        }
        string key = qual->GetQual();
        const string& value = qual->GetVal();
        if (key == "SO_type") { // RW-469
            continue;
        }
        if (key == "ID") {
            record.SetRecordId(value);
            continue;
        }
        if (key == "Parent") {
            record.SetParent(value);
            continue;
        }
        if (isupper(key.front()) && 
            gff3_attributes.find(key) == gff3_attributes.end()) {
            NStr::ToLower(key);
        }

        //CSeqFeatData::EQualifier equal = CSeqFeatData::GetQualifierType(key);
        //for now, retain all random junk:
        //if (!CSeqFeatData::IsLegalQualifier(subtype, equal)) {
        //    continue;
        //}
        record.SetAttribute(key, value);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeature(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pLoc(new CSeq_loc());
    try {
        if (mf.GetLocation().IsWhole()) {
            CSeq_loc whole;
            whole.SetInt().SetId().Assign(*mf.GetLocation().GetId());
            whole.SetInt().SetFrom(0);
            whole.SetInt().SetTo(fc.BioseqHandle().GetInst_Length()-1);
            pLoc->Assign(whole);
        } 
        else {
            pLoc->Assign(mf.GetLocation());
        }
    }
    catch(CException&) {
        NCBI_THROW(CObjWriterException, eBadInput, 
            "CGff3Writer: Unable to assign record location.\n");
    }
    
    auto display_range = GetRange();
    if (!display_range.IsWhole()) {
        pLoc->Assign(*sequence::CFeatTrim::Apply(*pLoc, display_range));
    }

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
    if (sublocs.size() > 1) {
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
    }
    record.InitLocation(*pLoc);
    return xAssignFeatureBasic(record, fc, mf);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSource(
    CGff3SourceRecord& record,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    return (xAssignSourceType(record)   &&
        xAssignSourceSeqId(record, bsh)  &&
        xAssignSourceMethod(record, bsh)  &&
        xAssignSourceEndpoints(record, bsh)  &&
        xAssignSourceAttributes(record, bsh));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceType(
    CGff3SourceRecord& record)
//  ----------------------------------------------------------------------------
{
    record.SetType("region");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceSeqId(
    CGff3SourceRecord& record,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    const string defaultId(".");
    string bestId;

    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if (!pId) {
        auto ids = bsh.GetId();
        if (!ids.empty()) {
            auto id = ids.front();
            CGenbankIdResolve::Get().GetBestId(
                id, 
                bsh.GetScope(), 
                bestId);
            record.SetSeqId(bestId);
            return true;
        }
        record.SetSeqId(defaultId);
        return true;
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*pId);
    if (!CGenbankIdResolve::Get().GetBestId(
            idh, 
            bsh.GetScope(), 
            bestId)) {
        record.SetSeqId(defaultId);
        return true;
    }

    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceMethod(
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = 0;//always for source
    unsigned int seqStop = bsh.GetBioseqLength() - 1;
    if (!m_Range.IsWhole()) {
        seqStart = m_Range.GetFrom();
        seqStop = m_Range.GetTo();
    } 
    ENa_strand seqStrand = eNa_strand_plus;
    if (bsh.CanGetInst_Strand()) {
        //now that's nuts- how should we act on GetInst_Strand() ???
    }
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributes(
    CGff3SourceRecord& record,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    record.SetRecordId(m_idGenerator.GetGffSourceId(bsh));
    return (xAssignSourceAttributeGbKey(record)  &&
        xAssignSourceAttributeMolType(record, bsh)  &&
        xAssignSourceAttributeIsCircular(record, bsh)  &&
        xAssignSourceAttributesBioSource(record, bsh)); 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGbKey(
    CGff3SourceRecord& record)
//  ----------------------------------------------------------------------------
{
    record.SetAttribute("gbkey", "Src");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeMolType(
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (!sdi) {
        return true;
    }
    const CBioSource& bioSrc = sdi->GetSource();
    return (xAssignSourceAttributeGenome(record, bioSrc)  &&
        xAssignSourceAttributeName(record, bioSrc)  &&
        xAssignSourceAttributeDbxref(record, bioSrc)  &&
        xAssignSourceAttributesOrgMod(record, bioSrc)  &&
        xAssignSourceAttributesSubSource(record, bioSrc));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGenome(
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
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
    CGff3SourceRecord& record,
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
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3FeatureRecord> pRecord(new CGff3FeatureRecord());
    if (!xAssignFeature(*pRecord, fc, mf)) {
        return false;
    }
    m_GeneMapNew[mf] = pRecord;
    return xWriteFeatureRecords(*pRecord, pRecord->Location(), 0);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureCds(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CMappedFeat tf = xGenerateMissingTranscript(fc, mf);
    if (tf  && !xWriteNucleotideFeature(fc, tf)) {
        return false;
    }

    CRef<CGff3FeatureRecord> pCds(new CGff3FeatureRecord());
    if (!xAssignFeature(*pCds, fc, mf)) {
        return false;
    }

    const CSeq_feat& feature = mf.GetMappedFeature();
    const CSeq_loc& PackedInt = pCds->Location();
    int /*CCdregion::EFrame*/ iPhase = 0;
    const CRange<TSeqPos>& display_range = GetRange();
    if (display_range.IsWhole())  {
        if (feature.GetData().GetCdregion().IsSetFrame()) {
            iPhase = max(feature.GetData().GetCdregion().GetFrame()-1, 0);
        }
    }
    else {
        iPhase = max(sequence::CFeatTrim::GetCdsFrame(feature, display_range)-1, 0);
    }

    int iTotSize = -iPhase;
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > > sublocs( PackedInt.GetPacked_int().Get() );
        list< CRef< CSeq_interval > >::const_iterator it;
        string cdsId = pCds->Id();
        int partNum = 1;
        bool useParts = xIntervalsNeedPartNumbers(sublocs);

        unsigned int wrapSize(0), wrapPoint(0);
        sGetWrapInfo(sublocs, fc, wrapSize, wrapPoint);

        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3FeatureRecord> pExon(new CGff3FeatureRecord(*pCds));
            pExon->SetRecordId(cdsId);
            pExon->SetType("CDS");
            pExon->DropAttributes("start_range");
            pExon->DropAttributes("end_range");
            pExon->SetLocation(subint, wrapSize, wrapPoint);
            pExon->SetPhase(iPhase);
            if (useParts) {
                pExon->SetAttribute("part", NStr::NumericToString(partNum++));
            }
            if (!xWriteRecord(*pExon)) {
                return false;
            }
            iTotSize = (iTotSize + subint.GetLength());
            const int posInCodon = (3+iTotSize)%3;
            iPhase = posInCodon ? 3-posInCodon : 0;
        }
    }
    m_MrnaMapNew[mf] = pCds;

    if (!fc.BioseqHandle()  ||  !mf.IsSetProduct()) {
        return true;
    }
    CConstRef<CSeq_id> protId(mf.GetProduct().GetId());
    CBioseq_Handle protein_h = m_pScope->GetBioseqHandleFromTSE(*protId, fc.BioseqHandle());
    if (!protein_h) {
        return true;
    }
    CFeat_CI it(protein_h);
    fc.FeatTree().AddFeatures(it);
    for (; it; ++it) {
        if (!it->GetData().IsProt()) {
            continue;
        }
        xWriteFeatureProtein(fc, mf, *it);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureRna(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3FeatureRecord> pRna(new CGff3FeatureRecord());
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
    }

    if (!xWriteRecord(*pRna)) {
        return false;
    }
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_MrnaMapNew[mf] = pRna;
    }   
    else
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_preRNA) {
        m_PrernaMapNew[mf] = pRna;
    } 

    const CSeq_loc& PackedInt = pRna->Location();
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        auto parentId = pRna->Id();
        list< CRef< CSeq_interval > >::const_iterator it;
        int partNum = 1;
        bool useParts = xIntervalsNeedPartNumbers(sublocs);

        unsigned int wrapSize(0), wrapPoint(0);
        sGetWrapInfo(sublocs, fc, wrapSize, wrapPoint);

        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3FeatureRecord> pChild(new CGff3FeatureRecord(*pRna));
            pChild->SetRecordId(m_idGenerator.GetNextGffExonId(parentId));
            pChild->DropAttributes("Name"); //explicitely not inherited
            pChild->DropAttributes("start_range");
            pChild->DropAttributes("end_range");
            pChild->DropAttributes("model_evidence");
            pChild->SetParent(parentId);
            pChild->SetType("exon");
            pChild->SetLocation(subint, wrapSize, wrapPoint);
            if (useParts) {
                pChild->SetAttribute("part", NStr::NumericToString(partNum++));
            }
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
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3FeatureRecord> pSegment(new CGff3FeatureRecord());

    if (!xAssignFeature(*pSegment, fc, mf)) {
        return false;
    }

    if (!xWriteRecord(*pSegment)) {
        return false;
    }

    // if mf is VDJ segment or C_region
    switch(mf.GetFeatSubtype()) {
    default:
        break;
    case CSeqFeatData::eSubtype_C_region:
    case CSeqFeatData::eSubtype_V_segment:
    case CSeqFeatData::eSubtype_D_segment:
    case CSeqFeatData::eSubtype_J_segment:
        {
            m_VDJsegmentCregionMapNew[mf] = pSegment;
        }
    }
    
    const CSeq_loc& PackedInt = pSegment->Location();
    const auto parentId = pSegment->Id();
    if (PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();

        unsigned int wrapSize(0), wrapPoint(0);
        sGetWrapInfo(sublocs, fc, wrapSize, wrapPoint);

        for (auto it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3FeatureRecord> pChild(new CGff3FeatureRecord(*pSegment));
            pChild->SetRecordId(m_idGenerator.GetNextGffExonId(parentId));
            pChild->DropAttributes("Name");
            pChild->DropAttributes("start_range");
            pChild->DropAttributes("end_range");
            pChild->SetParent(parentId);
            pChild->SetType("exon");
            pChild->SetLocation(subint, wrapSize, wrapPoint);
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
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3FeatureRecord> pParent(new CGff3FeatureRecord());
    if (!xAssignFeature(*pParent, fc, mf)) {
        return false;
    }

    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    return xWriteFeatureRecords( *pParent, mf.GetLocation(), seqlength );
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureProtein(
    CGffFeatureContext& fc,
    const CMappedFeat& cds,
    const CMappedFeat& protein )
//  ----------------------------------------------------------------------------
{
    auto subtype = protein.GetFeatSubtype();
    //const auto& location = protein.GetLocation().GetInt();

    if (subtype == CSeqFeatData::eSubtype_prot) {
        return true;
    }

    CRef<CGff3FeatureRecord> pRecord(new CGff3FeatureRecord());
    if (!xAssignFeature(*pRecord, fc, protein)) {
        return false;
    }

    // edit some feature types that for some reason are named differently
    //  once a feature gets mapped onto the cds (rw-1096):
    // note: if these proliferate then we have to find an somap mechanism
    //  to take care of this.
    map<string, string> proteinOnCdsFixups = {
        { "mature_protein_region", "mature_protein_region_of_CDS"},
        { "immature_peptide_region", "propeptide_region_of_CDS"},
        { "signal_peptide", "signal_peptide_region_of_CDS"},
        { "transit_peptide", "transit_peptide_region_of_CDS"},
    };
    auto fixupIt = proteinOnCdsFixups.find(pRecord->StrType());
    if (fixupIt != proteinOnCdsFixups.end()) {
        pRecord->SetType(fixupIt->second);
    }

    const auto& parentIt = m_MrnaMapNew.find(cds);
    if (parentIt != m_MrnaMapNew.end()) {
        string parentId = parentIt->second->Id();
        pRecord->AddAttribute("Parent", parentId);
    } 
    if (protein.IsSetProduct()) {
        string proteinId;
        CGenbankIdResolve::Get().GetBestId(protein.GetProduct(), proteinId);
        pRecord->AddAttribute("protein_id", proteinId);
    }
    const auto& prot = protein.GetData().GetProt();
    if (prot.IsSetName()) {
        pRecord->AddAttribute("product", prot.GetName().front());
    }
    // map location to cds coordinates (id and span):
    xAssignFeatureSeqId(*pRecord, fc, cds);
    CSeq_loc_Mapper prot_to_cds(cds.GetOriginalFeature(), 
        CSeq_loc_Mapper::eProductToLocation, m_pScope);
    prot_to_cds.SetFuzzOption(CSeq_loc_Mapper::fFuzzOption_CStyle);
    CRef<CSeq_loc> pMappedLoc(prot_to_cds.Map(protein.GetLocation()));
    auto& packedInt = *pMappedLoc;
    CWriteUtil::ChangeToPackedInt(packedInt);
    _ASSERT(packedInt.IsPacked_int() && packedInt.GetPacked_int().CanGet());
    
    list< CRef< CSeq_interval > > sublocs( packedInt.GetPacked_int().Get() );

    unsigned int wrapSize(0), wrapPoint(0);
    sGetWrapInfo(sublocs, fc, wrapSize, wrapPoint);

    for ( auto it = sublocs.begin(); it != sublocs.end(); ++it ) {
        const CSeq_interval& subint = **it;
        CRef<CGff3FeatureRecord> pExon(new CGff3FeatureRecord(*pRecord));
        pExon->SetLocation(subint, wrapSize, wrapPoint);
        if (!xWriteRecord(*pExon)) {
            return false;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureRecords(
    const CGffFeatureRecord& record,
    const CSeq_loc& location,
    unsigned int seqLength )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3FeatureRecord> pRecord(new CGff3FeatureRecord(
        dynamic_cast<const CGff3FeatureRecord&>(record)));
    _ASSERT(pRecord);

    const CSeq_loc& loc = record.Location();
    if (!loc.IsPacked_int()  ||  !loc.GetPacked_int().CanGet()) {
        return xWriteRecord(record);
    }
    const list<CRef<CSeq_interval> >& sublocs = loc.GetPacked_int().Get();
    if (sublocs.size() == 1) {
        return xWriteRecord(record);
    }

    unsigned int curInterval = 1;
    bool useParts = xIntervalsNeedPartNumbers(sublocs);
    for (auto it = sublocs.begin(); it != sublocs.end(); ++it) {
        const CSeq_interval& subint = **it;
        CRef<CGffFeatureRecord> pChild(new CGff3FeatureRecord(*pRecord));
        pChild->SetLocation(subint, 0);
        string part = NStr::IntToString(curInterval++);
        if (useParts) {
            pChild->SetAttribute("part", part);
        }
        if (!xWriteRecord(*pChild)) {
            return false;
        }
    }
    return true;
}

//  ============================================================================
void CGff3Writer::xWriteAlignment( 
    const CGffAlignRecord& record )
//  ============================================================================
{
    m_Os << record.StrId() << '\t';
    m_Os << record.StrMethod() << '\t';
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
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ============================================================================
{
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return true; //nothing to do
    }
    TGeneMapNew::iterator it = m_GeneMapNew.find(gene);
    if (it == m_GeneMapNew.end()) {
        return false;
    }
    record.SetParent(it->second->Id());
    return true;
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentMrna(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
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
    record.SetParent(it->second->Id());
    return true;
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentCds(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ============================================================================
{
    CMappedFeat cds = feature::GetBestParentForFeat(
        mf, CSeqFeatData::eSubtype_cdregion, &fc.FeatTree());
    if (!cds) {
        return true; // nothing to do
    }
    TCdsMapNew::iterator it = m_CdsMapNew.find(cds);
    if (it == m_CdsMapNew.end()) {
        return false; // not good - but at least preserve feature
    }
    record.SetParent(it->second->Id());
    return true;
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentRegion(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ============================================================================
{
    CMappedFeat region = feature::GetBestParentForFeat(
        mf, CSeqFeatData::eSubtype_region, &fc.FeatTree());
    if (!region) {
        return true; // nothing to assign
    }
    TCdsMapNew::iterator it = m_RegionMapNew.find(region);
    if (it == m_RegionMapNew.end()) {
        return true; // not good - but let's save the feature
    }
    record.SetParent(it->second->Id());
    return true;
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentpreRNA(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ============================================================================
{
    CMappedFeat parent = feature::GetBestParentForFeat(
                mf, CSeqFeatData::eSubtype_preRNA, &fc.FeatTree());
    if (!parent) {
        return false;
    }

    TMrnaMapNew::iterator it = m_PrernaMapNew.find(parent);
    if (it == m_PrernaMapNew.end()) {
        return false;
    }
    record.SetParent(it->second->Id());
    return true;
}


//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentVDJsegmentCregion(
    CGff3FeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ============================================================================
{
    static array<CSeqFeatData::ESubtype, 4> parent_types =
    { CSeqFeatData::eSubtype_C_region,
      CSeqFeatData::eSubtype_D_segment,
      CSeqFeatData::eSubtype_J_segment,
      CSeqFeatData::eSubtype_V_segment 
    };


    for (const auto& parent_type : parent_types) {
        auto parent = feature::GetBestParentForFeat(
            mf, parent_type, &fc.FeatTree());
        if (parent) {
            auto it = m_VDJsegmentCregionMapNew.find(parent);
            if (it != m_VDJsegmentCregionMapNew.end()) {
                record.SetParent(it->second->Id());
                return true; 
            }
        }
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteRecord( 
    const CGffBaseRecord& record )
//  ----------------------------------------------------------------------------
{
    auto id = record.StrSeqId();
    if (id == "."  &&  record.CanGetLocation()) {//one last desperate attempt--- 
        id = "";
        const CSeq_loc& loc = record.GetLocation();
        auto idh = sequence::GetIdHandle(loc, m_pScope);
        if (!CGenbankIdResolve::Get().GetBestId(
                idh, *m_pScope, id)) {
            id = ".";
        }
    }
    if (id == ".") {//all hope gone here
        NCBI_THROW(CObjWriterException, eBadInput, 
            "CGff3Writer::xWriteRecord: GFF3 reord is missing mandatory SeqID assignment.\n"
            "Identifying information:\n"
            "    SeqStart: " + record.StrSeqStart() + "\n"
            "    SeqStop : " + record.StrSeqStop() + "\n"
            "    Gff3Type: " + record.StrType() + "\n\n");    
    }
    m_Os << id  << '\t';
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
string CGff3Writer::xNextAlignId()
//  ----------------------------------------------------------------------------
{
    return string("aln") + NStr::UIntToString(m_uPendingAlignId++);
}

END_NCBI_SCOPE

