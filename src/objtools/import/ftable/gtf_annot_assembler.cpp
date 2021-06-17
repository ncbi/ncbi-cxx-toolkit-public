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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/import/import_error.hpp>
#include "feat_util.hpp"
#include "featid_generator.hpp"
#include "gtf_annot_assembler.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ========================================================================
class CFeatureMap
//  ========================================================================
{
protected:
    using FEATKEY = pair<string, string>;
    using TYPEMAP = map<FEATKEY, CRef<CSeq_feat> >;
    using FEATMAP = map<string, TYPEMAP>;
    FEATMAP mFeatMap;

public:
    CFeatureMap() {};
    ~CFeatureMap() {};

    void
    AddFeature(
        const CGtfImportData& record,
        CRef<CSeq_feat>& pFeat)
    {
        auto recType = xGetCookedFeatureType(record);
        auto featKey = FeatKeyOf(record);
        AddFeature(recType, featKey, pFeat);
    };

    void
    AddFeature(
        const string& recType,
        const pair<string, string>& featKey,
        CRef<CSeq_feat>& pFeat) 
    {
        auto typeIt = mFeatMap.find(recType);
        if (typeIt == mFeatMap.end()) {
            mFeatMap[recType] = TYPEMAP();
        }
        mFeatMap[recType][featKey] = pFeat;
    };

    CRef<CSeq_feat>
    FindFeature(
        const CGtfImportData& record) const
    {
        auto recType = xGetCookedFeatureType(record);
        auto featKey = FeatKeyOf(record);

        auto typeIt = mFeatMap.find(recType);
        if (typeIt == mFeatMap.end()) {
            return CRef<CSeq_feat>();
        }
        auto& features = typeIt->second;
        auto featIt = features.find(featKey);
        if (featIt == features.end()) {
            return CRef<CSeq_feat>();
        }
        return featIt->second;
    };

    CRef<CSeq_feat>
    FindGeneParent(
        const CGtfImportData& record) const
    {
        auto geneKey = FeatKeyOf(record);
        geneKey.second = "";
        auto geneEntries = mFeatMap.find("gene");
        if (geneEntries == mFeatMap.end()) {
            return CRef<CSeq_feat>();
        }
        auto geneParent = geneEntries->second.find(geneKey);
        if (geneParent == geneEntries->second.end()) {
            return CRef<CSeq_feat>();
        }
        return geneParent->second;
    }

    CRef<CSeq_feat>
        FindMrnaParent(
            const CGtfImportData& record) const
    {
        auto mrnaKey = FeatKeyOf(record);
        auto mrnaEntries = mFeatMap.find("mrna");
        if (mrnaEntries == mFeatMap.end()) {
            return CRef<CSeq_feat>();
        }
        auto mrnaParent = mrnaEntries->second.find(mrnaKey);
        if (mrnaParent == mrnaEntries->second.end()) {
            return CRef<CSeq_feat>();
        }
        return mrnaParent->second;
    }

    const TYPEMAP&
    GetTypeMapFor(
        const string& type)
    {
        static const auto emptyMap = TYPEMAP();
        auto typeIt = mFeatMap.find(type);
        if (typeIt == mFeatMap.end()) {
            return emptyMap;
        }
        return typeIt->second;
    }

    static FEATKEY
    FeatKeyOf(
        const CGtfImportData& record)
    {
        if (record.Type() == "gene") {
            return FEATKEY(record.GeneId(), "");
        }
        return FEATKEY(record.GeneId(), record.TranscriptId());
    }

    static string
    xGetCookedFeatureType(
        const CGtfImportData& record)
    {
        map<string, string> typeMap = {
            {"5utr", "mrna"},
            {"3utr", "mrna"},
            {"exon", "mrna"},
            {"initial", "mrna"},
            {"internal", "mrna"},
            {"terminal", "mrna"},
            {"start_codon", "cds"},
            {"stop_codon", "cds"},
        };
        auto recType = record.Type();
        auto it = typeMap.find(recType);
        if (it != typeMap.end()) {
            return it->second;
        }
        return recType;
    }
};

//  ============================================================================
CGtfAnnotAssembler::CGtfAnnotAssembler(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatAnnotAssembler(errorReporter)
{
    mpFeatureMap.reset(new CFeatureMap);
    mpIdGenerator.reset(new CFeatureIdGenerator);
}

//  ============================================================================
CGtfAnnotAssembler::~CGtfAnnotAssembler()
//  ============================================================================
{
}

//  ============================================================================
void
CGtfAnnotAssembler::ProcessRecord(
    const CFeatImportData& record_,
    CSeq_annot& annot)
//  ============================================================================
{
    assert(dynamic_cast<const CGtfImportData*>(&record_));
    const CGtfImportData& record = static_cast<const CGtfImportData&>(record_);

    CImportError errorUnknownFeatureType(
        CImportError::ERROR, "Unknown GTF feature type");

    vector<string> ignoredFeatureTypes = {
        "inter", "inter_cns", "intron", "intron_cns",
    };
    if (find(ignoredFeatureTypes.begin(), ignoredFeatureTypes.end(), 
            record.Type()) != ignoredFeatureTypes.end()) {
        return;
    }
 
    typedef void (CGtfAnnotAssembler::*HANDLER)(const CGtfImportData&, CSeq_annot&);
    static map<string, HANDLER> handlerMap = {
        {"5utr", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"3utr", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"gene", &CGtfAnnotAssembler::xProcessRecordGene},
        {"mrna", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"exon", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"initial", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"internal", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"terminal", &CGtfAnnotAssembler::xProcessRecordMrna},
        {"cds", &CGtfAnnotAssembler::xProcessRecordCds},
        {"start_codon", &CGtfAnnotAssembler::xProcessRecordCds},
        {"stop_codon", &CGtfAnnotAssembler::xProcessRecordCds},
    };
    auto handlerIt = handlerMap.find(record.Type());
    if (handlerIt == handlerMap.end()) {
        errorUnknownFeatureType.AmendMessage(record.Type());
        throw errorUnknownFeatureType;
    }
    (this->*(handlerIt->second))(record, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordGene(
    const CGtfImportData& record,
    CSeq_annot& annot)
//  ============================================================================
{
    CRef<CSeq_feat> pGene = mpFeatureMap->FindFeature(record);
    pGene ? xUpdateGene(record, pGene, annot) : xCreateGene(record, pGene, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordMrna(
    const CGtfImportData& record,
    CSeq_annot& annot)
    //  ============================================================================
{
    CGtfImportData impliedRecord(record);
    if (impliedRecord.Type() != "mrna") {
        impliedRecord.AdjustFeatureType("exon");
    }
    CRef<CSeq_feat> pGene = mpFeatureMap->FindGeneParent(impliedRecord);
    pGene ? 
        xUpdateGene(impliedRecord, pGene, annot) : 
        xCreateGene(impliedRecord, pGene, annot);

    CRef<CSeq_feat> pRna = mpFeatureMap->FindFeature(impliedRecord);
    pRna ? 
        xUpdateMrna(impliedRecord, pRna, annot) : 
        xCreateMrna(impliedRecord, pRna, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordCds(
    const CGtfImportData& record,
    CSeq_annot& annot)
//  ============================================================================
{
    CGtfImportData impliedRecord(record);

    CRef<CSeq_feat> pGene = mpFeatureMap->FindGeneParent(impliedRecord);
    pGene ? 
        xUpdateGene(impliedRecord, pGene, annot) : 
        xCreateGene(impliedRecord, pGene, annot);

    impliedRecord.AdjustFeatureType("exon");
    CRef<CSeq_feat> pRna = mpFeatureMap->FindMrnaParent(impliedRecord);
    pRna ? 
        xUpdateMrna(impliedRecord, pRna, annot) : 
        xCreateMrna(impliedRecord, pRna, annot);

    impliedRecord.AdjustFeatureType("cds");
    CRef<CSeq_feat> pCds = mpFeatureMap->FindFeature(impliedRecord);
    pCds ? 
        xUpdateCds(impliedRecord, pCds, annot) : 
        xCreateCds(impliedRecord, pCds, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateGene(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pGene,
    CSeq_annot& annot)
    //  ============================================================================
{
    pGene.Reset(new CSeq_feat);
    xFeatureSetGene(record, pGene);
    xFeatureSetLocation(record, pGene);
    xFeatureSetQualifiers(record, pGene);

    CGtfImportData impliedRecord(record);
    impliedRecord.AdjustFeatureType("gene");
    xFeatureSetFeatId(impliedRecord, pGene);
    xAnnotAddFeature(impliedRecord, pGene, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateGene(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pGene,
    CSeq_annot&)
    //  ============================================================================
{
    assert(record.Location().IsInt());

    if (record.Type() == "gene") {
        xFeatureUpdateLocation(record, pGene);
        return;
    }
    const auto& recordInt = record.Location().GetInt();
    const auto& featureInt = pGene->GetLocation().GetInt();

    pGene->SetLocation().SetInt().SetFrom(
        min(recordInt.GetFrom(), featureInt.GetFrom()));
    pGene->SetLocation().SetInt().SetTo(
        max(recordInt.GetTo(), featureInt.GetTo()));
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateMrna(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pRna,
    CSeq_annot& annot)
    //  ============================================================================
{
    pRna.Reset(new CSeq_feat);
    xFeatureSetMrna(record, pRna);
    xFeatureSetLocation(record, pRna);
    xFeatureSetQualifiers(record, pRna);
    xFeatureSetFeatId(record, pRna);
    xAnnotAddFeature(record, pRna, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateMrna(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pRna,
    CSeq_annot&)
    //  ============================================================================
{
    xFeatureUpdateLocation(record, pRna);
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateCds(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pCds,
    CSeq_annot& annot)
    //  ============================================================================
{
    pCds.Reset(new CSeq_feat);
    xFeatureSetCds(record, pCds);
    xFeatureSetLocation(record, pCds);
    xFeatureSetQualifiers(record, pCds);

    CGtfImportData impliedRecord(record);
    impliedRecord.AdjustFeatureType("cds");
    xFeatureSetFeatId(impliedRecord, pCds);
    xAnnotAddFeature(impliedRecord, pCds, annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateCds(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pCds,
    CSeq_annot&)
    //  ============================================================================
{
    xFeatureUpdateLocation(record, pCds);

    const auto& recordLoc = record.Location().GetInt();
    const auto& cdsLoc = pCds->GetLocation();
    auto& cdsRef = pCds->SetData().SetCdregion();

    if (cdsLoc.GetStrand() == eNa_strand_plus) {
        auto cdsStart = cdsLoc.GetStart(eExtreme_Positional);
        if (cdsStart == recordLoc.GetFrom()) {
            cdsRef.SetFrame(record.Frame());
        }
    }
    else if (cdsLoc.GetStrand() == eNa_strand_minus) {
        auto cdsStop = cdsLoc.GetStop(eExtreme_Positional);
        if (cdsStop == recordLoc.GetTo()) {
            cdsRef.SetFrame(record.Frame());
        }
    }
}

//  ============================================================================
void
CGtfAnnotAssembler::xAnnotAddFeature(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature,
    CSeq_annot& annot)
//  ============================================================================
{
    annot.SetData().SetFtable().push_back(pFeature);
    mpFeatureMap->AddFeature(record, pFeature);
}

//  ============================================================================
void
CGtfAnnotAssembler::FinalizeAnnot(
    const CAnnotImportData& annotData,
    CSeq_annot& annot)
//  ============================================================================
{
    xAnnotGenerateXrefs(annot);
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetGene(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    auto& geneRef = pFeature->SetData().SetGene();
    auto locusTag = record.AttributeValueOf("locus_tag");
    if (!locusTag.empty()) {
        geneRef.SetLocus_tag(locusTag);
    }
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetMrna(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
    //  ============================================================================
{
    pFeature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetCds(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
    //  ============================================================================
{
    pFeature->SetData().SetCdregion();
    if (record.Frame() != CCdregion::eFrame_not_set) {
        pFeature->SetData().SetCdregion().SetFrame(record.Frame());
    }
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetFeatId(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    map<string, string> typeMap = {
        {"exon", "mrna"},
        {"initial", "mrna"},
        {"internal", "mrna"},
        {"terminal", "mrna"},
        {"start_codon", "cds"},
        {"stop_codon", "cds"},
    };
    string mappedType(record.Type());
    auto it = typeMap.find(record.Type());
    if (it != typeMap.end()) {
        mappedType = it->second;
    }
    pFeature->SetId(*mpIdGenerator->GetIdFor(mappedType));
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetLocation(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    record.Type() == "mrna" ?
        pFeature->SetLocation().SetNull() :
        pFeature->SetLocation().Assign(record.Location());
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureUpdateLocation(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    CRef<CSeq_loc> pUpdatedLocation = FeatUtil::AddLocations(
        pFeature->GetLocation(), record.Location());
    pFeature->SetLocation().Assign(*pUpdatedLocation);
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetQualifiers(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    vector<string> ignoredKeys = {
        "exon_number", "locus_tag",
    };

    pFeature->AddQualifier("gene_id", record.GeneId());
    if (!pFeature->GetData().IsGene()) {
        pFeature->AddQualifier("transcript_id", record.TranscriptId());
    }
    for (auto entry: record.Attributes()) {
        auto key = entry.first;
        if (find(ignoredKeys.begin(), ignoredKeys.end(), key)
                != ignoredKeys.end()) {
            continue;
        }
        for (auto value: entry.second) {
            const auto& existingQuals = pFeature->GetQual();
            bool isDuplicate = false;
            for (auto qualRef: existingQuals) {
                if (qualRef->GetQual() == key  &&  qualRef->GetVal() == value) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                pFeature->AddQualifier(key, value);
            }
        }
    }
}

//  ============================================================================
void
CGtfAnnotAssembler::xAnnotGenerateXrefs(
    CSeq_annot& annot)
//  ============================================================================
{
    auto geneEntries = mpFeatureMap->GetTypeMapFor("gene");
    auto mrnaEntries = mpFeatureMap->GetTypeMapFor("mrna");
    auto cdsEntries = mpFeatureMap->GetTypeMapFor("cds");

    for (auto mrnaEntry: mrnaEntries) {
        // mrna <--> cds
        auto mrnaKey = mrnaEntry.first;
        for (auto cdsEntry: cdsEntries) {
            if (mrnaKey == cdsEntry.first) {
                cdsEntry.second->AddSeqFeatXref(mrnaEntry.second->GetId());
                mrnaEntry.second->AddSeqFeatXref(cdsEntry.second->GetId());
            }
        }
        // mrna <--> gene
        mrnaKey.second = "";
        for (auto geneEntry: geneEntries) {
            if (mrnaKey == geneEntry.first) {
                geneEntry.second->AddSeqFeatXref(mrnaEntry.second->GetId());
                mrnaEntry.second->AddSeqFeatXref(geneEntry.second->GetId());
            }
        }
    }
    // cds <--> gene
    for (auto cdsEntry: cdsEntries) {
        auto cdsKey = cdsEntry.first;
        cdsKey.second = "";
        for (auto geneEntry: geneEntries) {
            if (cdsKey == geneEntry.first) {
                geneEntry.second->AddSeqFeatXref(cdsEntry.second->GetId());
                cdsEntry.second->AddSeqFeatXref(geneEntry.second->GetId());
            }
        }
    }
}

