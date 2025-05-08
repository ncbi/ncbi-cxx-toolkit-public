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
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seq/so_map.hpp>

#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/feature_edit.hpp>

#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

#include "exon_number_assignment.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CScope&scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
    mIdGenerator.Reset(); //may be flag dependent some day
};

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags )
{
    mIdGenerator.Reset(); //may be flag dependent some day
};

//  ----------------------------------------------------------------------------
CGtfWriter::~CGtfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteBioseqHandle(
    CBioseq_Handle bsh )
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = SetAnnotSelector();
    const auto& display_range = GetRange();
    CFeat_CI feat_iter(bsh, display_range, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    vector<CMappedFeat> vRoots = fc.FeatTree().GetRootFeatures();
    std::sort(vRoots.begin(), vRoots.end(), CWriteUtil::CompareFeatures);
    for (auto pit = vRoots.begin(); pit != vRoots.end(); ++pit) {
        CMappedFeat mRoot = *pit;
        fc.AssignShouldInheritPseudo(false);
        if (!xWriteFeature(fc, mRoot)) {
            // error!
            continue;
        }
        xWriteAllChildren(fc, mRoot);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "#gtf-version 2.2" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecord(
    const CGffWriteRecord* pRecord )
//  ----------------------------------------------------------------------------
{
    m_Os << pRecord->StrSeqId() << '\t';
    m_Os << pRecord->StrMethod() << '\t';
    m_Os << pRecord->StrType() << '\t';
    m_Os << pRecord->StrSeqStart() << '\t';
    m_Os << pRecord->StrSeqStop() << '\t';
    m_Os << pRecord->StrScore() << '\t';
    m_Os << pRecord->StrStrand() << '\t';
    m_Os << pRecord->StrPhase() << '\t';

    if ( m_uFlags & fStructibutes ) {
        m_Os << pRecord->StrStructibutes() << '\n';
    }
    else {
        m_Os << pRecord->StrAttributes() << '\n';
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeature(
    CGffFeatureContext& context,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    if (IsCanceled()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "Processing terminated by user");
    }
    auto subtype = mf.GetFeatSubtype();

    //const auto& feat = mf.GetMappedFeature();
    //if (subtype == CSeqFeatData::eSubtype_tRNA) {
    //    auto from = feat.GetLocation().GetStart(ESeqLocExtremes::eExtreme_Biological);
    //    if (from == 48738) {
    //        cerr << "";
    //    }
    //}

    switch(subtype) {
        default:
            if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                return xWriteRecordsTranscript(context, mf);
            }
            // GTF is not interested --- ignore
            return true;
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_V_segment:
            return xWriteRecordsTranscript(context, mf);
        case CSeqFeatData::eSubtype_gene:
            return xWriteRecordsGene(context, mf);
        case CSeqFeatData::eSubtype_cdregion:
            return xWriteRecordsCds(context, mf);
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsGene(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (m_uFlags & fNoGeneFeatures) {
        return true;
    }

    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesGene(records, context, mf)) {
        return false;
    }

    for (const auto& record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsCds(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptIdPreAssigned)
//  ----------------------------------------------------------------------------
{
    string transcriptId(transcriptIdPreAssigned);

    CMappedFeat tf = xGenerateMissingTranscript(context, mf);
    if (tf) {
        if (!xWriteRecordsTranscript(context, tf, transcriptId)) {
            return false;
        }
    }
    if (!tf) {
        tf = context.FeatTree().GetParent(mf);
    }
    if (tf) {
        auto featIt = this->mFeatMap.find(tf);
        if (featIt != mFeatMap.end()) {
            transcriptId = featIt->second;
        }
    }
    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesCds(records, context, mf, transcriptId)) {
        return false;
    }

    for (const auto& record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsTranscript(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptIdPreAssigned )
//  ----------------------------------------------------------------------------
{
    string transcriptId(transcriptIdPreAssigned);

    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesTranscript(records, context, mf, transcriptId)) {
        return false;
    }
    for (const auto& record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return xWriteFeatureExons(context, mf, transcriptId);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesGene(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mf.GetLocation());
    mfLocAsPackedInt.ChangeToPackedInt();

    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();
    bool needsPartNumbers = (sublocs.size() > 1);
    unsigned int partNum = 1;
    for ( auto it = sublocs.begin(); it != sublocs.end(); it++ ) {
        const CSeq_interval& intv = **it;
        const auto strand = intv.IsSetStrand() ? intv.GetStrand() : eNa_strand_plus;
        CRef<CGtfRecord> pRecord(
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetEndpoints(intv.GetFrom(), intv.GetTo(), strand);
        if (needsPartNumbers) {
            pRecord->SetPartNumber(partNum++);
        }
        recordList.push_back(pRecord);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesTranscript(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptId)
//  ----------------------------------------------------------------------------
{
    // note: GTF transcript locations are the minimum covering interval of all
    //  its exons. Hence it's either a single interval, or two intervals if the
    //  covering interval wraps around the origin.

    CRef<CGtfRecord> pRecord(
        new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
    if (!transcriptId.empty()) {
        pRecord->SetTranscriptId(transcriptId);
    }
    if (!xAssignFeature(*pRecord, context, mf)) {
        return false;
    }

    const auto& mfLoc = mf.GetLocation();
    auto mfStrand = mfLoc.IsSetStrand() ?
        mfLoc.GetStrand() :
        eNa_strand_plus;

    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mfLoc);
    mfLocAsPackedInt.ChangeToPackedInt();

    bool isTransSpliced = mf.IsSetExcept()  &&  mf.GetExcept()  &&  mf.IsSetExcept_text()  &&
        (NStr::Find(mf.GetExcept_text(), "trans-splicing") != NPOS);
    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();
    if (isTransSpliced) {
        auto rnaStart = sublocs.front()->GetFrom();
        auto rnaStop = sublocs.front()->GetTo();
        for (const auto& loc : sublocs) {
            auto start = loc->GetFrom();
            if (start < rnaStart) {
                rnaStart = start;
            }
            auto stop = loc->GetTo();
            if (stop > rnaStop) {
                rnaStop = stop;
            }
        }
        pRecord->SetEndpoints(rnaStart, rnaStop, mfStrand);
    }
    else {
        TSeqPos lastFrom = sublocs.front()->GetFrom();
        TSeqPos lastTo = sublocs.front()->GetTo();
        auto it = sublocs.begin();
        bool iterationDone(false);
        for ( it++; it != sublocs.end()  &&  !iterationDone; it++ ) {
            const CSeq_interval& intv = **it;

            switch (mfStrand) {
                case eNa_strand_minus: {
                    if (intv.GetTo() <= lastFrom) {
                        lastFrom = intv.GetFrom();
                    }
                    else {
                        pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
                        recordList.push_back(pRecord);
                        pRecord.Reset(new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
                        if (!transcriptId.empty()) {
                            pRecord->SetTranscriptId(transcriptId);
                        }
                        if (!xAssignFeature(*pRecord, context, mf)) {
                            return false;
                        }
                        lastFrom = intv.GetFrom();
                        lastTo = intv.GetTo();
                    }
                }
                break;

                case eNa_strand_other: {
                    //feature contains parts from both strands
                    // for now, don't even attempt to origin wrap these things (rw-1299).
                    iterationDone = true;
                }
                break;

                default: {
                    if (intv.GetFrom() >= lastTo) {
                        lastTo = intv.GetTo();
                    }
                    else { //wrapping back to 0
                        pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
                        recordList.push_back(pRecord);
                        pRecord.Reset(new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
                        if (!transcriptId.empty()) {
                            pRecord->SetTranscriptId(transcriptId);
                        }
                        if (!xAssignFeature(*pRecord, context, mf)) {
                            return false;
                        }
                        lastFrom = 0;
                        lastTo = intv.GetTo();
                    }
                }
                break;
            }
        }
        pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
    }
    recordList.push_back(pRecord);

    bool needPartNumbers = (recordList.size() > 1);
    unsigned int partNum = 1;
    for (auto& record: recordList) {
        if (needPartNumbers) {
            record->SetPartNumber(partNum++);
        }
        record->SetType("transcript");
        record->SetGbKeyFrom(mf);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesCds(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& parentTranscriptId)
//  ----------------------------------------------------------------------------
{
    string transcriptId(parentTranscriptId);

    const auto& mfLoc = mf.GetLocation();

    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mfLoc);
    mfLocAsPackedInt.ChangeToPackedInt();
    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();

    bool needsPartNumbers = xIntervalsNeedPartNumbers(sublocs);

    int phase {0};
    const auto& cdsFeat = mf.GetMappedFeature();
    if (cdsFeat.GetData().GetCdregion().IsSetFrame()) {
        phase = max(cdsFeat.GetData().GetCdregion().GetFrame()-1, 0);
    }
    
    unsigned int partNum = 1;
    for (auto pInterval : sublocs) {
        const CSeq_interval& intv = *pInterval;
        auto strand = intv.IsSetStrand() ? intv.GetStrand() : eNa_strand_plus;
        CRef<CGtfRecord> pRecord(
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetEndpoints(intv.GetFrom(), intv.GetTo(), strand);
        if (needsPartNumbers) {
            pRecord->SetAttribute("part", NStr::NumericToString(partNum++));
        }
        if (!transcriptId.empty()) {
            pRecord->SetTranscriptId(transcriptId);
        }
        else {
            transcriptId = pRecord->TranscriptId();
        }
        pRecord->SetCdsPhase(phase);
        recordList.push_back(pRecord);
        phase = (3 - ((intv.GetLength()+3 - phase)%3))%3;
    }

    // subtract stop_codon in the end:
    unsigned int basesToLose = 3;
    while (basesToLose > 0 && !recordList.empty()) {
        auto pLastRecord = recordList.back();
        auto lastSize = pLastRecord->SeqStop() - pLastRecord->SeqStart() + 1;
        auto lastStrand = pLastRecord->SeqStrand();
        if (lastSize > basesToLose) {
            if (mfLoc.GetStrand() == eNa_strand_minus) {
                pLastRecord->SetEndpoints(
                    pLastRecord->SeqStart() + basesToLose, pLastRecord->SeqStop(), lastStrand);
            }
            else {
                pLastRecord->SetEndpoints(
                    pLastRecord->SeqStart(), pLastRecord->SeqStop() - basesToLose, lastStrand);
            }
            basesToLose = 0;
        }
        else {
            recordList.erase(--recordList.end());
            basesToLose -= lastSize;
        }
    }

    // generate start codon:
    if (!mfLoc.IsPartialStart(eExtreme_Biological)) {
        int basePairsNeeded = 3;
        auto currentIt = sublocs.begin();
        unsigned int partNumber = 1;
        unsigned int baseCount = 0;

        while (basePairsNeeded > 0 && currentIt != sublocs.end()) {
            const CSeq_interval& currentLoc = **currentIt;
            auto currentFrom = currentLoc.GetFrom();
            auto currentTo = currentLoc.GetTo();
            auto currentStrand = currentLoc.IsSetStrand() ? currentLoc.GetStrand() : eNa_strand_plus;

            CRef<CGtfRecord> pRecord(
                new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
            if (!xAssignFeature(*pRecord, context, mf)) {
                return false;
            }
            pRecord->SetType("start_codon");

            if (currentTo >= currentFrom + basePairsNeeded -1) {
                if (currentStrand == eNa_strand_minus) {
                    pRecord->SetEndpoints(currentTo - basePairsNeeded + 1, currentTo,  currentStrand);
                }
                else {
                    pRecord->SetEndpoints(currentFrom, currentFrom + basePairsNeeded -1, currentStrand);
                }
                basePairsNeeded = 0;
            }
            else {
                pRecord->SetEndpoints(currentFrom, currentTo, currentStrand);
                basePairsNeeded = basePairsNeeded - (currentTo - currentFrom + 1);
            }

            if (partNumber > 1  || basePairsNeeded > 0) {
                pRecord->SetPartNumber(partNumber++);
            }
            if (!transcriptId.empty()) {
                pRecord->SetTranscriptId(transcriptId);
            }
            _ASSERT(baseCount < 3);
            pRecord->SetCdsPhase((3-baseCount)%3);
            baseCount += pRecord->GetExtent();
            recordList.push_back(pRecord);
            currentIt++;
        }
    }

    // generate stop codon:
    if (!mfLoc.IsPartialStop(eExtreme_Biological)) {
        list<CRef<CGtfRecord>> stopCodonParts;
        int basePairsNeeded = 3;
        auto currentIt = sublocs.rbegin();
        while (basePairsNeeded > 0 && currentIt != sublocs.rend()) {
            const CSeq_interval& currentLoc = **currentIt;
            auto currentFrom = currentLoc.GetFrom();
            auto currentTo = currentLoc.GetTo();
            auto currentStrand = currentLoc.IsSetStrand() ? currentLoc.GetStrand() : eNa_strand_plus;

            CRef<CGtfRecord> pRecord(
                new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
            if (!xAssignFeature(*pRecord, context, mf)) {
                return false;
            }
            pRecord->SetType("stop_codon");

            if (currentTo >= currentFrom + basePairsNeeded - 1) {
                if (currentStrand == eNa_strand_minus) {
                    pRecord->SetEndpoints(currentFrom, currentFrom + basePairsNeeded - 1,  currentStrand);
                }
                else {
                    pRecord->SetEndpoints(currentTo - basePairsNeeded + 1, currentTo, currentStrand);
                }
                basePairsNeeded = 0;
            }
            else {
                pRecord->SetEndpoints(currentFrom, currentTo, currentStrand);
                basePairsNeeded = basePairsNeeded - (currentTo - currentFrom + 1);
            }

            if (!transcriptId.empty()) {
                pRecord->SetTranscriptId(transcriptId);
            }
            stopCodonParts.push_front(pRecord);
            currentIt++;
        }
        unsigned int partNumber = 1;
        bool needPartNumbers = (stopCodonParts.size() > 1);
        unsigned int baseCount = 0;
        for (auto& pRecord: stopCodonParts) {
            if (needPartNumbers) {
                pRecord->SetPartNumber(partNumber++);
            }
            _ASSERT(baseCount < 3);
            pRecord->SetCdsPhase((3-baseCount)%3);
            baseCount += pRecord->GetExtent();
            recordList.push_back(pRecord);
        }
    }

    // assign exon numbers:
    CExonNumberAssigner exonNumberAssigner(mf);
    if (exonNumberAssigner.CdsNeedsExonNumbers()) {
        for (auto& pRecord: recordList) {
            exonNumberAssigner.AssignExonNumberTo(*pRecord);
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureExons(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptId)
//  ----------------------------------------------------------------------------
{
    CRef<CGtfRecord> pMrna( new CGtfRecord( context ) );
    if (!transcriptId.empty()) {
        pMrna->SetTranscriptId(transcriptId);
    }
    if (!xAssignFeature(*pMrna, context, mf)) {
        return false;
    }
    pMrna->CorrectType("exon");

    const CSeq_loc& loc = mf.GetLocation();
    unsigned int uExonNumber = 1;

    CRef< CSeq_loc > pLocMrna(new CSeq_loc(CSeq_loc::e_Mix));
    pLocMrna->Add( loc );
    pLocMrna->ChangeToPackedInt();
    if (!pLocMrna->GetPacked_int().CanGet()) {
        return false;
    }

    const list<CRef<CSeq_interval>>& sublocs = pLocMrna->GetPacked_int().Get();
    for (auto it = sublocs.begin(); it != sublocs.end(); ++it) {
        const CSeq_interval& subint = **it;
        CRef<CGtfRecord> pExon(
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        pExon->MakeChildRecord(*pMrna, subint, uExonNumber++);
        pExon->DropAttributes("gbkey");
        xWriteRecord(pExon);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetType("region");

    if (mf.IsSetQual()) {
        const auto& quals = mf.GetQual();
        auto it = quals.begin();
        for( ; it != quals.end(); ++it) {
            if (!(*it)->CanGetQual() || !(*it)->CanGetVal()) {
                continue;
            }
            if ((*it)->GetQual() == "standard_name") {
                record.SetType((*it)->GetVal());
                return true;
            }
        }
    }
    switch ( mf.GetFeatSubtype() ) {
    default:
        break;
    case CSeq_feat::TData::eSubtype_cdregion:
        record.SetType("CDS");
        break;
    case CSeq_feat::TData::eSubtype_exon:
        record.SetType("exon");
        break;
    case CSeq_feat::TData::eSubtype_misc_RNA:
        record.SetType("transcript");
        break;
    case CSeq_feat::TData::eSubtype_gene:
        record.SetType("gene");
        break;
    case CSeq_feat::TData::eSubtype_mRNA:
        record.SetType("mRNA");
        break;
    case CSeq_feat::TData::eSubtype_scRNA:
        record.SetType("scRNA");
        break;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureMethod(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetMethod(".");

    if (mf.IsSetQual()) {
        const auto& quals = mf.GetQual();
        auto it = quals.begin();
        for (; it != quals.end(); ++it) {
            if (!(*it)->CanGetQual() || !(*it)->CanGetVal()) {
                continue;
            }
            if ((*it)->GetQual() == "gff_source") {
                record.SetMethod((*it)->GetVal());
                return true;
            }
        }
    }

    if (mf.IsSetExt()) {
        CConstRef<CUser_object> model_evidence = CWriteUtil::GetUserObjectByType(
            mf.GetExt(), "ModelEvidence");
        if (model_evidence) {
            string strMethod;
            if (model_evidence->HasField("Method") ) {
                record.SetMethod(
                    model_evidence->GetField("Method").GetData().GetStr());
                return true;
            }
        }
    }

    if (mf.IsSetExts()) {
        CConstRef<CUser_object> model_evidence = CWriteUtil::GetUserObjectByType(
            mf.GetExts(), "ModelEvidence");
        if (model_evidence) {
            string strMethod;
            if (model_evidence->HasField("Method")) {
                record.SetMethod(
                    model_evidence->GetField("Method").GetData().GetStr());
                return true;
            }
        }
    }

    CScope& scope = mf.GetScope();
    CSeq_id_Handle idh = sequence::GetIdHandle(mf.GetLocation(), &mf.GetScope());
    string typeFromId;
    CWriteUtil::GetIdType(scope.GetBioseqHandle(idh), typeFromId);
    if (!typeFromId.empty()) {
        record.SetMethod(typeFromId);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributesFormatSpecific(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    return (
        xAssignFeatureAttributeGeneId(record, fc, mf)  &&
        xAssignFeatureAttributeTranscriptId(record, fc, mf)  &&
        xAssignFeatureAttributeTranscriptBiotype(record, fc, mf));
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributesQualifiers(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    const vector<string> specialCases = {
        "ID",
        "Parent",
        "gff_type",
        "transcript_id",
        "gene_id",
    };

    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    auto quals = mf.GetQual();
    for (auto qual: quals) {
        if (!qual->IsSetQual()  ||  !qual->IsSetVal()) {
            continue;
        }
        auto specialCase = std::find(
            specialCases.begin(), specialCases.end(), qual->GetQual());
        if (specialCase != specialCases.end()) {
            continue;
        }
        record.AddAttribute(qual->GetQual(), qual->GetVal());
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeDbxref(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    return CGff2Writer::xAssignFeatureAttributeDbxref(rec, fc, "db_xref", mf);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeNote(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (!mf.IsSetComment()) {
        return true;
    }
    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    record.SetAttribute("note", mf.GetComment());
    return true;
}

//  ----------------------------------------------------------------------------
string CGtfWriter::xGenericGeneId(
    const CMappedFeat& mf,
    CGffFeatureContext& context)
//  ----------------------------------------------------------------------------
{
    auto featIt = mMapFeatToGeneId.find(mf);
    if (featIt != mMapFeatToGeneId.end()) {
        return featIt->second;
    }

    auto parent = context.FeatTree().GetParent(mf);
    featIt = mMapFeatToGeneId.find(parent);
    if (featIt != mMapFeatToGeneId.end()) {
        return featIt->second;
    }

    auto children = context.FeatTree().GetChildren(mf);
    for (auto child : children) {
        featIt = mMapFeatToGeneId.find(child);
        if (featIt != mMapFeatToGeneId.end()) {
            return featIt->second;
        }
    }

    string geneId = mIdGenerator.NextId("unassigned_gene");
    mMapFeatToGeneId[mf] = geneId;
    return geneId;
}

//  ----------------------------------------------------------------------------
string CGtfWriter::xGenericTranscriptId(
    const CMappedFeat& mf)
    //  ----------------------------------------------------------------------------
{
    return mIdGenerator.NextId("unassigned_transcript");
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeTranscriptBiotype(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    static list<CSeqFeatData::ESubtype> nonRnaTranscripts = {
        CSeqFeatData::eSubtype_mRNA,
        CSeqFeatData::eSubtype_otherRNA,
        CSeqFeatData::eSubtype_C_region,
        CSeqFeatData::eSubtype_D_segment,
        CSeqFeatData::eSubtype_J_segment,
        CSeqFeatData::eSubtype_V_segment
    };
    auto featSubtype = mf.GetFeatSubtype();
    if (!mf.GetData().IsRna()) {
        auto it = std::find(
            nonRnaTranscripts.begin(), nonRnaTranscripts.end(), featSubtype);
        if (it == nonRnaTranscripts.end()) {
            return true;
        }
    }
    const auto& feature = mf.GetOriginalFeature();
    string so_type;
    if (!CSoMap::FeatureToSoType(feature, so_type)) {
        return true;
    }

    record.SetAttribute("transcript_biotype", so_type);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeTranscriptId(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!record.TranscriptId().empty()) {
        return true; //special case hence already assigned
    }

    const auto mfIt = mFeatMap.find(mf);
    if (mFeatMap.end() != mfIt) {
        record.SetTranscriptId(mfIt->second);
        return true;
    }

    CMappedFeat mrnaFeat;
    auto featSubtype = mf.GetFeatSubtype();
    switch(featSubtype) {
        default:
            mrnaFeat = mf;
            break;
        case CSeq_feat::TData::eSubtype_mRNA:
        case CSeq_feat::TData::eSubtype_C_region:
        case CSeq_feat::TData::eSubtype_D_segment:
        case CSeq_feat::TData::eSubtype_J_segment:
        case CSeq_feat::TData::eSubtype_V_segment:
            mrnaFeat = mf;
            break;
        case CSeq_feat::TData::eSubtype_cdregion:
            if (HasAccaptableTranscriptParent(fc, mf)) {
                mrnaFeat = feature::GetParentFeature(mf);
            }
            else {
                mrnaFeat = mf;
                //there must be one somewhere, and that's the closest we can
                // get to it.
            }
            break;
        case CSeq_feat::TData::eSubtype_gene:
            return true;
    }

    if (!mrnaFeat) {
        //record.SetTranscriptId("dummy");
        return true;
    }

    const auto featIt = mFeatMap.find(mrnaFeat);
    if (mFeatMap.end() != featIt) {
        record.SetTranscriptId(featIt->second);
        return true;
    }

    FEAT_ID featId = mf.GetNamedQual("transcript_id");
    if (featId.empty()  &&  mf.GetData().IsRna()  &&  mf.IsSetProduct()) {
        if (!GetBestId(mf.GetProductId(), mf.GetScope(), featId)) {
            featId.clear();
        }
    }

    if (featId.empty()) {
        featId = mf.GetNamedQual("orig_transcript_id");
    }

    if (featId.empty()) {
        featId = xGenericTranscriptId(mf);
        //we know the ID is going to be unique if we get it this way
        // not point in further checking
        mUsedFeatIds.emplace(featId);
        mFeatMap[mf] = featId;
        record.SetTranscriptId(featId);
        return true;
    }
    //uniquify the ID we came up with
    auto cit = mUsedFeatIds.find(featId);
    if (mUsedFeatIds.end() == cit) {
        mUsedFeatIds.emplace(featId);
        mFeatMap[mf] = featId;
        record.SetTranscriptId(featId);
        return true;
    }
    unsigned int suffix = 1;
    featId += "_";
    while (true) {
        auto qualifiedId = featId + NStr::UIntToString(suffix);
		cit = mUsedFeatIds.find(qualifiedId);
        if (mUsedFeatIds.end() == cit) {
            mUsedFeatIds.emplace(qualifiedId);
            mFeatMap[mf] = qualifiedId;
            record.SetTranscriptId(qualifiedId);
            return true;
        }
        ++suffix;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeGeneId(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!record.GeneId().empty()) {
        return true;
    }
    CMappedFeat geneFeat = mf;
    if (mf.GetFeatSubtype() != CSeq_feat::TData::eSubtype_gene) {
        geneFeat = feature::GetBestGeneForFeat(mf, &fc.FeatTree());
    }
    if (!geneFeat) {
        const auto& geneIdQual = mf.GetNamedQual("gene_id");
        if (!geneIdQual.empty()) {
            record.SetGeneId(geneIdQual); // empty most times but still best effort
            return true;
        }
        auto geneId = xGenericGeneId(mf, fc);
        record.SetGeneId(geneId);
        return true;
    }

    auto  geneIt = mGeneMap.find(geneFeat);
    if (mGeneMap.end() != geneIt) {
        record.SetGeneId(geneIt->second);
        return true;
    }

    GENE_ID geneId;
    const auto& geneRef = geneFeat.GetData().GetGene();

    geneId = mf.GetNamedQual("gene_id");
    if (geneId.empty()  &&  geneRef.IsSetLocus_tag()) {
        geneId = geneRef.GetLocus_tag();
    }
    if (geneId.empty() &&  geneRef.IsSetLocus()) {
        geneId = geneRef.GetLocus();
    }
    if (geneId.empty() &&  geneRef.IsSetSyn() ) {
        geneId = geneRef.GetSyn().front();
    }
    if (geneId.empty()) {
        geneId = xGenericGeneId(mf, fc);
        //we know the ID is going to be unique if we get it this way
        // not point in further checking
        mUsedGeneIds.emplace(geneId);
        mGeneMap[mf] = geneId;
        record.SetGeneId(geneId);
        return true;
    } 

    auto cit = mUsedGeneIds.find(geneId);
    if (mUsedGeneIds.end() == cit) {
        mUsedGeneIds.emplace(geneId);
        mGeneMap[mf] = geneId;
        record.SetGeneId(geneId);
        return true;
    }

    unsigned int suffix = 1;
    geneId += "_";
    while (true) {
        GENE_ID qualifiedGeneId = geneId + NStr::UIntToString(suffix);
        cit = find(mUsedGeneIds.begin(), mUsedGeneIds.end(), qualifiedGeneId);
        if (mUsedGeneIds.end() == cit) {
            mUsedGeneIds.emplace(qualifiedGeneId);
            mGeneMap[mf] = qualifiedGeneId;
            record.SetGeneId(qualifiedGeneId);
            return true;
        }
        ++suffix;
    }
    return true;
}

END_NCBI_SCOPE
