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
#include <objects/general/Object_id.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/import/feat_import_error.hpp>

#include "gtf_annot_assembler.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CFeatureMap
//  ============================================================================
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
class CFeatureIdGenerator
//  ============================================================================
{
protected:
    map<string, int> mIdCounter;

public:
    CFeatureIdGenerator() {};
    ~CFeatureIdGenerator() {};

    //  ------------------------------------------------------------------------
    CRef<CFeat_id>
    GetIdFor(
        const CGtfImportData& record)
    //  ------------------------------------------------------------------------
    {
        return GetIdFor(record.Type());
    };

    //  ------------------------------------------------------------------------
    CRef<CFeat_id>
    GetIdFor(
        const string& recType)
    //  ------------------------------------------------------------------------
    {
        map<string, string> typeMap = {
            {"exon", "mrna"},
            {"initial", "mrna"},
            {"internal", "mrna"},
            {"terminal", "mrna"},
            {"start_codon", "cds"},
            {"stop_codon", "cds"},
        };
        string mappedType(recType);
        auto it = typeMap.find(recType);
        if (it != typeMap.end()) {
            mappedType = it->second;
        }

        auto typeIt = mIdCounter.find(mappedType);
        if (typeIt == mIdCounter.end()) {
            mIdCounter[mappedType] = 0;
        }
        ++mIdCounter[mappedType];

        CRef<CFeat_id> pId(new CFeat_id);
        pId->SetLocal().SetStr(mappedType + "_" + NStr::IntToString(
            mIdCounter[mappedType]));
        return pId;
    };
};

//  ============================================================================
CGtfAnnotAssembler::CGtfAnnotAssembler(
    CSeq_annot& annot):
//  ============================================================================
    mAnnot(annot)
{
    mAnnot.SetData().SetFtable();
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
    const CGtfImportData& record)
//  ============================================================================
{
    CFeatureImportError errorUnknownFeatureType(
        CFeatureImportError::ERROR, "Unknown GTF feature type");

    vector<string> ignoredFeatureTypes = {
        "intron",
    };
    if (find(ignoredFeatureTypes.begin(), ignoredFeatureTypes.end(), 
            record.Type()) != ignoredFeatureTypes.end()) {
        return;
    }
 
    typedef void (CGtfAnnotAssembler::*HANDLER)(const CGtfImportData&);
    static map<string, HANDLER> handlerMap = {
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
        throw errorUnknownFeatureType;
    }
    (this->*(handlerIt->second))(record);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordGene(
    const CGtfImportData& record)
//  ============================================================================
{
    CRef<CSeq_feat> pGene = mpFeatureMap->FindFeature(record);
    pGene ? xUpdateGene(record, pGene) : xCreateGene(record, pGene);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordMrna(
    const CGtfImportData& record)
    //  ============================================================================
{
    CRef<CSeq_feat> pGene = mpFeatureMap->FindGeneParent(record);
    pGene ? xUpdateGene(record, pGene) : xCreateGene(record, pGene);

    CRef<CSeq_feat> pRna = mpFeatureMap->FindFeature(record);
    pRna ? xUpdateMrna(record, pRna) : xCreateMrna(record, pRna);
}

//  ============================================================================
void
CGtfAnnotAssembler::xProcessRecordCds(
    const CGtfImportData& record)
//  ============================================================================
{
    CRef<CSeq_feat> pGene = mpFeatureMap->FindGeneParent(record);
    pGene ? xUpdateGene(record, pGene) : xCreateGene(record, pGene);

    CRef<CSeq_feat> pRna = mpFeatureMap->FindMrnaParent(record);
    pRna ? xUpdateMrna(record, pRna) : xCreateMrna(record, pRna);

    CRef<CSeq_feat> pCds = mpFeatureMap->FindFeature(record);
    pCds ? xUpdateCds(record, pCds) : xCreateCds(record, pCds);
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateGene(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pGene)
    //  ============================================================================
{
    pGene.Reset(new CSeq_feat);
    xFeatureSetGene(record, pGene);
    xFeatureSetLocation(record, pGene);
    xFeatureSetQualifiers(record, pGene);

    CGtfImportData impliedRecord(record);
    impliedRecord.AdjustFeatureType("gene");
    xFeatureSetFeatId(impliedRecord, pGene);
    xAnnotAddFeature(impliedRecord, pGene);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateGene(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pGene)
    //  ============================================================================
{
    if (record.Type() == "gene") {
        xFeatureUpdateLocation(record, pGene);
        return;
    }
    const auto& featureInt = pGene->GetLocation().GetInt();
    pGene->SetLocation().SetInt().SetFrom(
        min(record.SeqStart(), featureInt.GetFrom()));
    pGene->SetLocation().SetInt().SetTo(
        max(record.SeqStop(), featureInt.GetTo()));
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateMrna(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pRna)
    //  ============================================================================
{
    pRna.Reset(new CSeq_feat);
    xFeatureSetMrna(record, pRna);
    xFeatureSetLocation(record, pRna);
    xFeatureSetQualifiers(record, pRna);

    CGtfImportData impliedRecord(record);
    impliedRecord.AdjustFeatureType("exon");
    xFeatureSetFeatId(impliedRecord, pRna);
    xAnnotAddFeature(impliedRecord, pRna);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateMrna(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pRna)
    //  ============================================================================
{
    xFeatureUpdateLocation(record, pRna);
}

//  ============================================================================
void
CGtfAnnotAssembler::xCreateCds(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pCds)
    //  ============================================================================
{
    pCds.Reset(new CSeq_feat);
    xFeatureSetCds(record, pCds);
    xFeatureSetLocation(record, pCds);
    xFeatureSetQualifiers(record, pCds);

    CGtfImportData impliedRecord(record);
    impliedRecord.AdjustFeatureType("cds");
    xFeatureSetFeatId(impliedRecord, pCds);
    xAnnotAddFeature(impliedRecord, pCds);
}

//  ============================================================================
void
CGtfAnnotAssembler::xUpdateCds(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pCds)
    //  ============================================================================
{
    xFeatureUpdateLocation(record, pCds);
}

//  ============================================================================
void
CGtfAnnotAssembler::xAnnotAddFeature(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    mAnnot.SetData().SetFtable().push_back(pFeature);
    mpFeatureMap->AddFeature(record, pFeature);
}

//  ============================================================================
void
CGtfAnnotAssembler::FinalizeAnnot()
//  ============================================================================
{
    xAnnotGenerateXrefs();
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
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureSetFeatId(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    pFeature->SetId(*mpIdGenerator->GetIdFor(record));
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
        pFeature->SetLocation().Assign(*record.LocationRef());
}

//  ============================================================================
void
CGtfAnnotAssembler::xFeatureUpdateLocation(
    const CGtfImportData& record,
    CRef<CSeq_feat>& pFeature)
//  ============================================================================
{
    if (pFeature->GetLocation().IsNull()) {
        xFeatureSetLocation(record, pFeature);
        return;
    }
    const auto& debug = pFeature->GetLocation();
    CRef<CSeq_loc> pRecLocation = record.LocationRef();
    CRef<CSeq_loc> pUpdatedLocation = pFeature->GetLocation().Add(
        *pRecLocation, CSeq_loc::fSortAndMerge_All, nullptr);
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
CGtfAnnotAssembler::xAnnotGenerateXrefs()
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

