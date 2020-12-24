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
* Author: Frank Ludwig, NCBI
*
* File Description:
*   Convenience wrapper around some of the other feature processing functions
*   in the xobjedit library.
*/

#include <ncbi_pch.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/feattable_edit.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <corelib/ncbi_message.hpp>
#include <objtools/edit/edit_error.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/edit/cds_fix.hpp>

#include <objects/general/Dbtag.hpp>

#include <unordered_set>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> sProductFromString(
    const string str)
    //  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pProduct(new CSeq_loc(CSeq_loc::e_Whole));
    CRef<CSeq_id> pId(new CSeq_id(CSeq_id::e_Local, str));
    pProduct->SetId(*pId);
    return pProduct;
}

//  ----------------------------------------------------------------------------
string sGetCdsProductName(
    const CSeq_feat& cds,
    CScope& scope)
    //  ----------------------------------------------------------------------------
{
    string productName;
    if (cds.IsSetProduct()) {
        CBioseq_Handle bsh = sequence::GetBioseqFromSeqLoc(cds.GetProduct(), scope);
        if (bsh) {
            CFeat_CI protCi(bsh, CSeqFeatData::e_Prot);
            if (protCi) {
                const CProt_ref& prot = protCi->GetOriginalFeature().GetData().GetProt();
                if (prot.IsSetName() && prot.GetName().size() > 0) {
                    productName = prot.GetName().front();
                }
            }
            return productName;
        }
    }
    if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if (xref.IsSetData() && xref.GetData().IsProt()) {
                const CProt_ref& prot = xref.GetData().GetProt();
                if (prot.IsSetName() && prot.GetName().size() > 0) {
                    productName = prot.GetName().front();
                }
            }
            return productName;
        }
    }
    return productName;
}

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
    const string& locusTagPrefix,
    unsigned int locusTagNumber,
    unsigned int startingFeatId,
    IObjtoolsListener* pMessageListener) :
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mSequenceSize(0),
    mpMessageListener(pMessageListener),
    mNextFeatId(startingFeatId),
    mLocusTagNumber(locusTagNumber),
    mLocusTagPrefix(locusTagPrefix)
{
    mpScope.Reset(new CScope(*CObjectManager::GetInstance()));
    mpScope->AddDefaults();
    mHandle = mpScope->AddSeq_annot(mAnnot);
    mEditHandle = mpScope->GetEditHandle(mHandle);
    mTree = feature::CFeatTree(mHandle);
};


//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
    unsigned int sequenceSize,
    const string& locusTagPrefix,
    unsigned int locusTagNumber,
    unsigned int startingFeatId,
    IObjtoolsListener* pMessageListener) :
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mSequenceSize(sequenceSize),
    mpMessageListener(pMessageListener),
    mNextFeatId(startingFeatId),
    mLocusTagNumber(locusTagNumber),
    mLocusTagPrefix(locusTagPrefix)
{
    mpScope.Reset(new CScope(*CObjectManager::GetInstance()));
    mpScope->AddDefaults();
    mHandle = mpScope->AddSeq_annot(mAnnot);
    mEditHandle = mpScope->GetEditHandle(mHandle);
    mTree = feature::CFeatTree(mHandle);
};


//  -------------------------------------------------------------------------
CFeatTableEdit::~CFeatTableEdit() = default;
//  -------------------------------------------------------------------------

//  -------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingGeneForCds()
//  -------------------------------------------------------------------------
{
    xGenerateMissingGeneForSubtype(CSeqFeatData::eSubtype_cdregion);
}

//  -------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingMrnaForCds()
//  -------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for (; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CConstRef<CSeq_feat> pOverlappingRna =
            sequence::GetBestOverlappingFeat(
                cds.GetLocation(),
                CSeqFeatData::e_Rna,
                sequence::eOverlap_CheckIntRev,
                *mpScope);
        if (pOverlappingRna) {
            continue;
        }
        CRef<CSeq_feat> pRna(new CSeq_feat);
        pRna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        pRna->SetLocation().Assign(cds.GetLocation());
        pRna->SetLocation().SetPartialStart(false, eExtreme_Positional);
        pRna->SetLocation().SetPartialStop(false, eExtreme_Positional);
        pRna->ResetPartial();
        //product name
        pRna->SetData().SetRna().SetExt().SetName(
            sGetCdsProductName(cds, *mpScope));

        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);

        //add rna xref to cds
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);

        //add cds xref to rna
        CRef<CFeat_id> pFeatId(new CFeat_id);
        pFeatId->Assign(cds.GetId());
        CRef<CSeqFeatXref> pRnaXref(new CSeqFeatXref);
        pRnaXref->SetId(*pFeatId);
        pRna->SetXref().push_back(pRnaXref);

        CMappedFeat parentGene = feature::GetBestGeneForFeat(*it, &mTree);
        if (parentGene) {
            //if gene exists, add gene xref to rna
            CSeq_feat_EditHandle geh(mpScope->GetObjectHandle(
                parentGene.GetOriginalFeature()));
            geh.AddFeatXref(rnaId);
            //if gene exists, add rna xref to gene
            CRef<CFeat_id> pGeneId(new CFeat_id);
            pGeneId->Assign(parentGene.GetId());
            CRef<CSeqFeatXref> pRnaXref(new CSeqFeatXref);
            pRnaXref->SetId(*pGeneId);
            pRna->SetXref().push_back(pRnaXref);
        }

        //add new rna to feature table
        mEditHandle.AddFeat(*pRna);
        mTree.AddFeature(mpScope->GetObjectHandle(*pRna));
    }
}

//  -------------------------------------------------------------------------
void CFeatTableEdit::InferParentMrnas()
//  -------------------------------------------------------------------------
{
    GenerateMissingMrnaForCds();
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingGeneForMrna()
//  ---------------------------------------------------------------------------
{
    xGenerateMissingGeneForSubtype(CSeqFeatData::eSubtype_mRNA);
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    GenerateMissingGeneForMrna();
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::InferPartials()
//  ----------------------------------------------------------------------------
{
    bool infered(false);
    edit::CLocationEditPolicy editPolicy(
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd,
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd,
        false, //extend 5'
        false, //extend 3'
        edit::CLocationEditPolicy::eMergePolicy_NoChange);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for (; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pEditedCds(new CSeq_feat);
        pEditedCds->Assign(cds);
        infered = editPolicy.ApplyPolicyToFeature(*pEditedCds, *mpScope);
        if (!infered) {
            continue;
        }
        CSeq_feat_EditHandle cdsEh(mpScope->GetObjectHandle(cds));
        cdsEh.Replace(*pEditedCds);

        // make sure the parent rna is partial as well
        CMappedFeat parentRna = feature::GetBestMrnaForCds(*it, &mTree);
        if (parentRna &&
            !(parentRna.IsSetPartial() && parentRna.GetPartial())) {
            CRef<CSeq_feat> pEditedRna(new CSeq_feat);
            pEditedRna->Assign(parentRna.GetOriginalFeature());
            pEditedRna->SetPartial(true);
            CSeq_feat_EditHandle rnaEh(
                mpScope->GetObjectHandle(parentRna.GetOriginalFeature()));
            rnaEh.Replace(*pEditedRna);
        }

        // make sure the gene parent is partial as well
        CMappedFeat parentGene = feature::GetBestGeneForCds(*it);
        if (parentGene &&
            !(parentGene.IsSetPartial() && parentGene.GetPartial())) {
            CRef<CSeq_feat> pEditedGene(new CSeq_feat);
            pEditedGene->Assign(parentGene.GetOriginalFeature());
            pEditedGene->SetPartial(true);
            CSeq_feat_EditHandle geneEh(
                mpScope->GetObjectHandle(parentGene.GetOriginalFeature()));
            geneEh.Replace(*pEditedGene);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::SubmitFixProducts()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    //sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    for (CFeat_CI it(mHandle, sel); it; ++it) {
        CMappedFeat mf = *it;
        if (mf.IsSetProduct()) {
            continue;
        }
        //debug CSeqFeatData::ESubtype st = mf.GetFeatSubtype();
        string product = mf.GetNamedQual("Product");
        CRef<CSeq_feat> pEditedFeature(new CSeq_feat);
        pEditedFeature->Assign(mf.GetOriginalFeature());
        pEditedFeature->ResetProduct();
        if (!product.empty()) {
            pEditedFeature->AddQualifier("product", product);
            pEditedFeature->RemoveQualifier("Product");
        }
        CSeq_feat_EditHandle feh(mf);
        feh.Replace(*pEditedFeature);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::EliminateBadQualifiers()
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual QUALS;

    vector<string> specialQuals{
        "Protein", "protein",
        "go_function", "go_component", "go_process" };

    CFeat_CI it(mHandle);
    for (; it; ++it) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (it)->GetOriginalFeature()));
        const QUALS& quals = (*it).GetQual();
        vector<string> badQuals;
        for (QUALS::const_iterator qual = quals.begin(); qual != quals.end();
            ++qual) {
            string qualKey = (*qual)->GetQual();
            if (std::find(specialQuals.begin(), specialQuals.end(), qualKey)
                != specialQuals.end()) {
                continue;
            }
            if (subtype == CSeqFeatData::eSubtype_cdregion ||
                subtype == CSeqFeatData::eSubtype_mRNA) {
                if (qualKey == "protein_id" || qualKey == "transcript_id") {
                    continue;
                }
                if (qualKey == "orig_protein_id" || qualKey == "orig_transcript_id") {
                    continue;
                }
            }
            if (subtype != CSeqFeatData::eSubtype_gene  &&  qualKey == "gene") {
                //rw-570: remove gene qualifiers fro non-gene features before cleanup sees them
                badQuals.push_back(qualKey);
                continue;
            }
            CSeqFeatData::EQualifier qualType = CSeqFeatData::GetQualifierType(qualKey);
            if (CSeqFeatData::IsLegalQualifier(subtype, qualType)) {
                continue;
            }
            badQuals.push_back(qualKey);
        }
        for (vector<string>::const_iterator badIt = badQuals.begin();
            badIt != badQuals.end(); ++badIt) {
            feh.RemoveQualifier(*badIt);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::ProcessCodonRecognized()
//  ----------------------------------------------------------------------------
{
    static map<char, list<char>> sIUPACmap {
        {'A', list<char>({'A'})},
        {'G', list<char>({'G'})},
        {'C', list<char>({'C'})},
        {'T', list<char>({'T'})},
        {'U', list<char>({'U'})},
        {'M', list<char>({'A', 'C'})},
        {'R', list<char>({'A', 'G'})},
        {'W', list<char>({'A', 'T'})},
        {'S', list<char>({'C', 'G'})},
        {'Y', list<char>({'C', 'T'})},
        {'K', list<char>({'G', 'T'})},
        {'V', list<char>({'A', 'C', 'G'})},
        {'H', list<char>({'A', 'C', 'T'})},
        {'D', list<char>({'A', 'G', 'T'})},
        {'B', list<char>({'C', 'G', 'T'})},
        {'N', list<char>({'A', 'C', 'G', 'T'})}
    };

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_tRNA);
    CFeat_CI it(mHandle, sel);
    for (; it; ++it) {
        CMappedFeat mf = *it;
        auto codonRecognized = mf.GetNamedQual("codon_recognized");
        if (codonRecognized.empty()) {
            continue;
        }
        if (codonRecognized.size() != 3) {
            xPutErrorBadCodonRecognized(codonRecognized);
            return;
        }
        NStr::ToUpper(codonRecognized);

        const CSeq_feat& origFeat = mf.GetOriginalFeature();

        CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
        pEditedFeat->Assign(origFeat);
        CRNA_ref::C_Ext::TTRNA & extTrna = pEditedFeat->SetData().SetRna().SetExt().SetTRNA();

        set<int> codons;
        try {
            for (char char1 : sIUPACmap.at(codonRecognized[0])) {
                for (char char2 : sIUPACmap.at(codonRecognized[1])) {
                    for (char char3 : sIUPACmap.at(codonRecognized[2])) {
                        const auto codonIndex = CGen_code_table::CodonToIndex(char1, char2, char3);
                        codons.insert(codonIndex);
                    }
                }
            }
        }
        catch(CException&) {
            xPutErrorBadCodonRecognized(codonRecognized);
            return;
        }
        if (!codons.empty()) {
            for (const auto codonIndex : codons) {
                extTrna.SetCodon().push_back(codonIndex);
            }
            CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
            feh.Replace(*pEditedFeat);
            feh.RemoveQualifier("codon_recognized");
        }
    }
}

// ---------------------------------------------------------------------------
void CFeatTableEdit::GenerateProteinAndTranscriptIds()
// ---------------------------------------------------------------------------
{
    mProcessedMrnas.clear();

    { {
            SAnnotSelector sel;
            sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
            for (CFeat_CI it(mHandle, sel); it; ++it) {
                CMappedFeat cds = *it;
                xAddTranscriptAndProteinIdsToCdsAndParentMrna(cds);
            }
        }}

    { {
            SAnnotSelector sel;
            sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
            for (CFeat_CI it(mHandle, sel); it; ++it) {
                CMappedFeat mrna = *it;
                xAddTranscriptAndProteinIdsToUnmatchedMrna(mrna);
            }
        }}

}


// ---------------------------------------------------------------------------
static bool s_ShouldConvertToGeneral(const string& id)
// ---------------------------------------------------------------------------
{
    return (!NStr::StartsWith(id, "gb|") &&
        !NStr::StartsWith(id, "gnl|") &&
        !NStr::StartsWith(id, "cds.gnl|") &&
        !NStr::StartsWith(id, "mrna.gnl|"));
}


// ---------------------------------------------------------------------------
static string s_GetTranscriptIdFromMrna(const CMappedFeat& mrna)
// ---------------------------------------------------------------------------
{
    string transcript_id = mrna.GetNamedQual("transcript_id");
    if (NStr::IsBlank(transcript_id)) {
        return mrna.GetNamedQual("ID");
    }

    return transcript_id;
}


// ---------------------------------------------------------------------------
void CFeatTableEdit::xAddTranscriptAndProteinIdsToCdsAndParentMrna(CMappedFeat& cds)
// ---------------------------------------------------------------------------
{
    CMappedFeat mrna = feature::GetBestMrnaForCds(cds, &mTree);
    string protein_id = cds.GetNamedQual("protein_id");
    const bool no_protein_id_qual = NStr::IsBlank(protein_id);
    if (no_protein_id_qual) { // no protein_id qual on CDS - check mRNA and ID qual
        if (mrna) {
            protein_id = mrna.GetNamedQual("protein_id");
        }
        if (NStr::IsBlank(protein_id)) {
            protein_id = cds.GetNamedQual("ID");
        }
    }
    bool is_genbank_protein = NStr::StartsWith(protein_id, "gb|");

    string transcript_id = cds.GetNamedQual("transcript_id");
    const bool no_transcript_id_qual = NStr::IsBlank(transcript_id);
    if (no_transcript_id_qual) { // no transcript_id qual on CDS - check mRNA
        if (mrna) {
            transcript_id = s_GetTranscriptIdFromMrna(mrna);
        }
    }
    bool is_genbank_transcript = NStr::StartsWith(transcript_id, "gb|");


    if ((is_genbank_protein ||
        NStr::StartsWith(protein_id, "gnl|")) &&
        (is_genbank_transcript ||
            NStr::StartsWith(transcript_id, "gnl|"))) { // No further processing of ids is required - simply assign to features
        if (no_protein_id_qual) { // protein_id from ID qualifier or mRNA => need to add protein_id qual to CDS
            xFeatureSetQualifier(cds, "protein_id", protein_id);
        }

        if (no_transcript_id_qual) { // transcript_id from mRNA => need to add transcript_id qual to CDS
            xFeatureSetQualifier(cds, "transcript_id", transcript_id);
        }

        if (mrna) {
            xAddTranscriptAndProteinIdsToMrna(transcript_id,
                protein_id,
                mrna);
        }
        return;
    }

    // else need to generate and/or process ids before we add them to features
    bool has_protein_id = !NStr::IsBlank(protein_id);
    bool has_transcript_id = !NStr::IsBlank(transcript_id);

    if (has_protein_id &&
        has_transcript_id) {
        if (!is_genbank_protein &&
            (transcript_id == protein_id)) {
            protein_id = "cds." + protein_id;
        }
    }
    else {
        if (has_protein_id &&
            !is_genbank_protein) {
            transcript_id = "mrna." + protein_id;
            has_transcript_id = true;

        }
        else
            if (has_transcript_id &&
                !is_genbank_transcript) {
                protein_id = "cds." + transcript_id;
                has_protein_id = true;
            }

        // Generate new transcript_id and protein_id if necessary
        if (!has_transcript_id) {
            transcript_id = xNextTranscriptId(cds);
        }

        if (!has_protein_id) {
            protein_id = xNextProteinId(cds);
        }
    }

    xConvertToGeneralIds(cds, transcript_id, protein_id);

    if (mrna) {
        xAddTranscriptAndProteinIdsToMrna(transcript_id,
            protein_id,
            mrna);
    }


    xFeatureSetQualifier(cds, "transcript_id", transcript_id);
    xFeatureSetQualifier(cds, "protein_id", protein_id);
}


// ---------------------------------------------------------------------------
void CFeatTableEdit::xAddTranscriptAndProteinIdsToMrna(const string& cds_transcript_id,
    const string& cds_protein_id,
    CMappedFeat& mrna)
    // ---------------------------------------------------------------------------
{
    if (mProcessedMrnas.find(mrna) != mProcessedMrnas.end()) {
        return;
    }

    bool use_local_ids = false; // 

    string transcript_id = mrna.GetNamedQual("transcript_id");
    if (NStr::IsBlank(transcript_id)) {
        transcript_id = cds_transcript_id;
    }
    else {
        use_local_ids = true;
    }

    string protein_id = mrna.GetNamedQual("protein_id");
    if (NStr::IsBlank(protein_id)) {
        protein_id = cds_protein_id;
    }
    else {
        if ((protein_id == transcript_id) &&
            !NStr::StartsWith(protein_id, "gb|")) {
            protein_id = "cds." + protein_id;
        }
        use_local_ids = true;
    }

    if (use_local_ids) { // protein id and/or transcript id specified on mRNA.
                         // Process these ids and check that the processed quals match the qualifiers on the child CDS feature
        xConvertToGeneralIds(mrna, transcript_id, protein_id);

        if (transcript_id != cds_transcript_id) {
            xPutErrorDifferingTranscriptIds(mrna);
        }

        if (protein_id != cds_protein_id) {
            xPutErrorDifferingProteinIds(mrna);
        }
    }

    xFeatureSetQualifier(mrna, "transcript_id", transcript_id);
    xFeatureSetQualifier(mrna, "protein_id", protein_id);

    mProcessedMrnas.insert(mrna);
}


// ---------------------------------------------------------------------------
void CFeatTableEdit::xAddTranscriptAndProteinIdsToUnmatchedMrna(CMappedFeat& mrna)
// ---------------------------------------------------------------------------
{
    if (mProcessedMrnas.find(mrna) != mProcessedMrnas.end()) {
        return;
    }

    string transcript_id = mrna.GetNamedQual("transcript_id");
    bool no_transcript_id_qual = NStr::IsBlank(transcript_id);
    if (no_transcript_id_qual) {
        transcript_id = mrna.GetNamedQual("ID");
    }
    const bool is_genbank_transcript = NStr::StartsWith(transcript_id, "gb|");

    string protein_id = mrna.GetNamedQual("protein_id");
    const bool is_genbank_protein = NStr::StartsWith(protein_id, "gb|");

    if ((is_genbank_protein ||
        NStr::StartsWith(protein_id, "gnl|")) &&
        (is_genbank_transcript ||
            NStr::StartsWith(transcript_id, "gnl|"))) {
        if (no_transcript_id_qual) {
            xFeatureSetQualifier(mrna, "transcript_id", transcript_id);
        }
        return;
    }

    if (!NStr::IsBlank(protein_id) &&
        !NStr::IsBlank(transcript_id)) {
        if ((transcript_id == protein_id) &&
            !is_genbank_transcript) {
            protein_id = "cds." + protein_id;
        }
    }
    else
        if (!is_genbank_protein && !NStr::IsBlank(protein_id)) {
            transcript_id = "mrna." + protein_id;
        }
        else
            if (!is_genbank_transcript && !NStr::IsBlank(transcript_id)) {
                protein_id = "cds." + transcript_id;
            }

    if (NStr::IsBlank(protein_id)) {
        protein_id = xNextProteinId(mrna);
    }
    if (NStr::IsBlank(transcript_id)) {
        transcript_id = xNextTranscriptId(mrna);
    }

    xConvertToGeneralIds(mrna, transcript_id, protein_id);

    xFeatureSetQualifier(mrna, "transcript_id", transcript_id);
    xFeatureSetQualifier(mrna, "protein_id", protein_id);

    mProcessedMrnas.insert(mrna);
}


void CFeatTableEdit::xConvertToGeneralIds(const CMappedFeat& mf,
    string& transcript_id,
    string& protein_id)
{
    const bool update_protein_id = s_ShouldConvertToGeneral(protein_id);
    const bool update_transcript_id = s_ShouldConvertToGeneral(transcript_id);

    string locus_tag_prefix;
    if (update_protein_id || update_transcript_id) {
        locus_tag_prefix = xGetCurrentLocusTagPrefix(mf);
        if (!NStr::IsBlank(locus_tag_prefix)) {
            if (update_protein_id) {
                protein_id = "gnl|" + locus_tag_prefix + "|" + protein_id;
            }

            if (update_transcript_id) {
                transcript_id = "gnl|" + locus_tag_prefix + "|" + transcript_id;
            }
        }
        else {
            string seq_label;
            mf.GetLocation().GetId()->GetLabel(&seq_label, CSeq_id::eContent);

            if (update_protein_id) {
                protein_id = "gnl|" + seq_label + "|" + protein_id;
            }

            if (update_transcript_id) {
                transcript_id = "gnl|" + seq_label + "|" + transcript_id;
            }
        }
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::InstantiateProducts()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel1;
    sel1.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel1.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    for (CFeat_CI it(mHandle, sel1); it; ++it) {
        CMappedFeat mf = *it;

        auto tid = mf.GetNamedQual("transcript_id");
        if (!tid.empty()) {
            xFeatureRemoveQualifier(mf, "transcript_id");
            xFeatureAddQualifier(mf, "orig_transcript_id", tid);
        }

        if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion) {
            auto pid = mf.GetNamedQual("protein_id");
            if (!pid.empty()) {
                if (!mf.IsSetProduct()) {
                    xFeatureSetProduct(mf, pid);
                }
                xFeatureRemoveQualifier(mf, "protein_id");
            }
        }
    }
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::xGenerateMissingGeneForChoice(
    CSeqFeatData::E_Choice choice)
    //  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatType(choice);
    CFeat_CI it(mHandle, sel);
    for (; it; ++it) {
        CMappedFeat mf = *it;
        if (xCreateMissingParentGene(mf)) {
            xAdjustExistingParentGene(mf);
        }
    }
}


//  ---------------------------------------------------------------------------
void CFeatTableEdit::xGenerateMissingGeneForSubtype(
    CSeqFeatData::ESubtype subType)
    //  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(subType);
    CFeat_CI it(mHandle, sel);
    for (; it; ++it) {
        CMappedFeat mf = *it;
        if (xCreateMissingParentGene(mf)) {
            xAdjustExistingParentGene(mf);
        }
    }
}

//  ----------------------------------------------------------------------------
bool
CFeatTableEdit::xAdjustExistingParentGene(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    if (!mf.IsSetPartial() || !mf.GetPartial()) {
        return true;
    }
    CMappedFeat parentGene = feature::GetBestGeneForFeat(mf, &mTree);
    if (!parentGene) {
        return false;
    }

    if (parentGene.IsSetPartial() && parentGene.GetPartial()) {
        return true;
    }
    CRef<CSeq_feat> pEditedGene(new CSeq_feat);
    pEditedGene->Assign(parentGene.GetOriginalFeature());
    pEditedGene->SetPartial(true);
    CSeq_feat_EditHandle geneEH(
        mpScope->GetObjectHandle(parentGene.GetOriginalFeature()));
    geneEH.Replace(*pEditedGene);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CFeatTableEdit::xCreateMissingParentGene(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene = xMakeGeneForFeature(mf);
    if (!pGene) {
        return false;
    }
    // missing gene was created. now attach ids and xrefs:
    string geneId(xNextFeatId());
    pGene->SetId().SetLocal().SetStr(geneId);
    CSeq_feat_EditHandle feh(
        mpScope->GetObjectHandle(mf.GetOriginalFeature()));
    feh.AddFeatXref(geneId);

    CRef<CFeat_id> pFeatId(new CFeat_id);
    pFeatId->Assign(mf.GetId());
    CRef<CSeqFeatXref> pGeneXref(new CSeqFeatXref);
    pGeneXref->SetId(*pFeatId);
    pGene->SetXref().push_back(pGeneXref);

    mEditHandle.AddFeat(*pGene);
    mTree.AddFeature(mpScope->GetObjectHandle(*pGene));
    return true;
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdMrna(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for mRNA:
    // no mRNA survives with an orig_protein_id
    // almost all mRNA features should have protein_ids
    // if one exists already then police it
    // if none exists then if possible inherit it from the decendent CDS

    auto orig_tid = mf.GetNamedQual("orig_protein_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_protein_id");
    }
    auto pid = mf.GetNamedQual("protein_id");
    if (NStr::StartsWith(pid, "gb|") || NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    CMappedFeat child = feature::GetBestCdsForMrna(mf, &mTree);
    if (!child) {
        // only permitted case of an mRNA without a protein_id
        return;
    }
    pid = child.GetNamedQual("protein_id");
    xFeatureAddQualifier(mf, "protein_id", pid);
    return;
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdCds(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for CDS:
    // no CDS survives with an orig_protein_id
    // all CDS features should have transcript_ids
    // if one exists already then police it
    // if it doen't have one then generate one following a strict set of rules

    auto orig_pid = mf.GetNamedQual("orig_protein_id");
    if (!orig_pid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_protein_id");
    }

    auto pid = mf.GetNamedQual("protein_id");
    if (NStr::StartsWith(pid, "gb|") || NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    auto id = mf.GetNamedQual("ID");
    if (!id.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + id;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    auto tid = mf.GetNamedQual("transcript_id");
    if (!tid.empty()) {
        CMappedFeat parent = feature::GetBestMrnaForCds(mf, &mTree);
        auto mRnaTid = parent.GetNamedQual("transcript_id");
        if (tid == mRnaTid) {
            tid = string("cds.") + tid;
        }
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    pid = xNextProteinId(mf);
    if (!pid.empty()) {
        xFeatureSetQualifier(mf, "protein_id", pid);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdDefault(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for non CDS, non mRNA:
    // we won't touch orig_protein_id
    // we don't generate any protein_ids
    // if it comes with a protein_id then we add it to the "gnl|locus_tag|"
    //  namespace if necessary.

    auto pid = mf.GetNamedQual("protein_id");
    if (pid.empty()) {
        return;
    }
    if (NStr::StartsWith(pid, "gb|") || NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    //pid = xGenerateTranscriptOrProteinId(mf, pid);
    pid = xNextProteinId(mf);
    if (!pid.empty()) {
        xFeatureSetQualifier(mf, "protein_id", pid);
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdMrna(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for mRNA:
    // no mRNA survives with an orig_transcript_id
    // every mRNA must have a transcript_id.
    // if it already got one then police it.
    // if it doen't have one then generate one following a strict set of rules

    auto orig_tid = mf.GetNamedQual("orig_transcript_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_transcript_id");
    }

    auto tid = mf.GetNamedQual("transcript_id");
    if (NStr::StartsWith(tid, "gb|") || NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }
    else {
    }



    auto id = mf.GetNamedQual("ID");
    if (!id.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + id;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    //tid = xGenerateTranscriptOrProteinId(mf, tid);
    tid = xNextTranscriptId(mf);
    if (!tid.empty()) {
        xFeatureSetQualifier(mf, "transcript_id", tid);
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdCds(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for CDS:
    // no CDS survives with an orig_transcript_id
    // almost all CDS features should have transcript_ids
    // if one exists already then police it
    // if none exists then if possible inherit it from the parent mRNA

    auto orig_tid = mf.GetNamedQual("orig_transcript_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_transcript_id");
    }

    auto tid = mf.GetNamedQual("transcript_id");
    // otherwise, we need to police the existing transcript_id:
    if (NStr::StartsWith(tid, "gb|") || NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    CMappedFeat parent = feature::GetBestMrnaForCds(mf, &mTree);
    if (!parent) {
        // only permitted case of a CDS without a transcript_id
        return;
    }
    tid = parent.GetNamedQual("transcript_id");
    xFeatureAddQualifier(mf, "transcript_id", tid);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdDefault(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for non CDS, non mRNA:
    // we won't touch orig_transcript_id
    // we don't generate any transcript_ids
    // if it comes with a transcript_id then we add it to the "gnl|locus_tag|"
    //  namespace if necessary.

    auto tid = mf.GetNamedQual("transcript_id");
    if (tid.empty()) {
        return;
    }
    if (NStr::StartsWith(tid, "gb|") || NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    tid = this->xNextTranscriptId(mf);
    if (!tid.empty()) {
        xFeatureSetQualifier(mf, "transcript_id", tid);
    }
}


//  ----------------------------------------------------------------------------
string CFeatTableEdit::xGenerateTranscriptOrProteinId(
    CMappedFeat mf,
    const string& rawId)
    //  ----------------------------------------------------------------------------
{
    //weed out badly formatted original tags:
    if (string::npos != rawId.find("|")) {
        xPutError(
            "Feature " + xGetIdStr(mf) +
            " does not have a usable transcript_ or protein_id.");
        return "";
    }

    //make sure we got the locus tag prefix necessary for tag generation:
    auto locusTagPrefix = xGetCurrentLocusTagPrefix(mf);
    if (locusTagPrefix.empty()) {
        xPutError(
            "Cannot generate transcript_/protein_id for feature " + xGetIdStr(mf) +
            " without a locus tag.");
        return "";
    }

    //reformat any tags we already have:
    if (!rawId.empty()) {
        return string("gnl|") + locusTagPrefix + "|" + rawId;
    }

    //attempt to make transcript_id from protein_id, or protein_id from transcript_id:
    switch (mf.GetFeatSubtype()) {
    case CSeqFeatData::eSubtype_mRNA: {
        auto id = mf.GetNamedQual("protein_id");
        if (id.empty()) {
            id = mf.GetNamedQual("ID");
        }
        if (id.empty()) {
            if (mf.GetId().IsLocal()) {
                id = mf.GetId().GetLocal().GetStr();
            }
        }
        if (!id.empty()) {
            return string("gnl|") + locusTagPrefix + "|mrna." + id;
        }
    }
                                      break;

    case CSeqFeatData::eSubtype_cdregion: {
        auto id = mf.GetNamedQual("transcript_id");
        if (id.empty()) {
            id = mf.GetNamedQual("ID");
        }
        if (!id.empty()) {
            return string("gnl|") + locusTagPrefix + "|cds." + id;
        }
    }
                                          break;

    default:
        break;
    }


    xPutError(
        "Cannot generate transcript_/protein_id for feature " + xGetIdStr(mf) +
        " - insufficient context.");
    return "";
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusIds()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_imp);

    if (mLocusTagPrefix.empty()) {
        return xGenerateLocusIdsUseExisting();
    }
    else {
        return xGenerateLocusIdsRegenerate();
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xGenerateLocusIdsRegenerate()
//  ----------------------------------------------------------------------------
{
    //mss-362:
    // blow away any locus_tag, protein_id, transcript_id attributes and
    // regenerate from scratch using mLocusTagPrefix

    //make sure genes got proper locus tags
    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    selGenes.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    for (CFeat_CI it(mHandle, selGenes); it; ++it) {
        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);
        CRef<CSeq_feat> pReplacement(new CSeq_feat);
        pReplacement->Assign(*mf.GetSeq_feat());
        pReplacement->SetData().SetGene().SetLocus_tag(xNextLocusTag());
        feh.Replace(*pReplacement);
    }

    //make sure all locus related junk is removed and that rnas are
    // labeled properly
    SAnnotSelector selOthers;
    selOthers.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    for (CFeat_CI it(mHandle, selOthers); it; ++it) {
        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);

        feh.RemoveQualifier("orig_protein_id");
        feh.RemoveQualifier("orig_transcript_id");

        CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
        switch (subtype) {
        case CSeqFeatData::eSubtype_mRNA: {
            string proteinId = xNextProteinId(mf);
            feh.AddQualifier("orig_protein_id", proteinId);
            string transcriptId = xNextTranscriptId(mf);
            feh.AddQualifier("orig_transcript_id", transcriptId);
            break;
        }
        default: {
            break;
        }
        }
    }

    //finally, down inherit transcript ids from the mrna's to the cdregions
    SAnnotSelector selCds;
    selCds.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);

    for (CFeat_CI it(mHandle, selCds); it; ++it) {

        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);
        CMappedFeat rna = feature::GetBestMrnaForCds(mf);
        string transcriptId = rna.GetNamedQual("transcript_id");
        feh.AddQualifier("orig_transcript_id", transcriptId);
        string proteinId = rna.GetNamedQual("protein_id");
        feh.AddQualifier("orig_protein_id", proteinId);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xGenerateLocusIdsUseExisting()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_imp);

    for (CFeat_CI it(mHandle, sel); it; ++it) {
        //mss-362: every feature that needs them must come with a complete set
        // of locus_tag, protein_id, and transcript_id.

        CMappedFeat mf = *it;
        CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();

        switch (subtype) {
        case CSeqFeatData::eSubtype_gene: {
            if (!mf.GetData().GetGene().IsSetLocus_tag()) {
                xPutErrorMissingLocustag(mf);
            }
            break;
        }
        case CSeqFeatData::eSubtype_mRNA: {
            string transcriptId = mf.GetNamedQual("transcript_id");
            if (transcriptId.empty()) {
                xPutErrorMissingTranscriptId(mf);
            }
            string proteinId = mf.GetNamedQual("protein_id");
            if (proteinId.empty()) {
                xPutErrorMissingProteinId(mf);
            }
            break;
        }
        case CSeqFeatData::eSubtype_cdregion: {
            string transcriptId = mf.GetNamedQual("transcript_id");
            if (transcriptId.empty()) {
                xPutErrorMissingTranscriptId(mf);
            }
            break;
        }
        default: {
            break;
        }
        }
    }
}

bool idAlpha(const CSeq_id_Handle& idh1, const CSeq_id_Handle idh2) {
    return (idh1.AsString() < idh2.AsString());
}

//  ----------------------------------------------------------------------------
bool CFeatTableEdit::AnnotHasAllLocusTags() const
//  ----------------------------------------------------------------------------
{
    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for (; itGenes; ++itGenes) {
        CSeq_feat_Handle fh = *itGenes;
        if (!fh.GetData().GetGene().IsSetLocus_tag()) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusTags()
//  ----------------------------------------------------------------------------
{
    if (mLocusTagPrefix.empty()) {
        return;
    }

    CRef<CGb_qual> pLocusTag;
    SAnnotSelector selGenes;
    vector<CSeq_id_Handle> annotIds;
    selGenes.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for (; itGenes; ++itGenes) {
        CSeq_feat_Handle fh = *itGenes;
        CSeq_id_Handle idh = fh.GetLocationId();
        vector<CSeq_id_Handle>::const_iterator compIt;
        for (compIt = annotIds.begin();
            compIt != annotIds.end();
            ++compIt) {
            if (*compIt == idh) {
                break;
            }
        }
        if (compIt == annotIds.end()) {
            annotIds.push_back(idh);
        }
    }
    std::sort(annotIds.begin(), annotIds.end(), idAlpha);

    for (vector<CSeq_id_Handle>::const_iterator idIt = annotIds.begin();
        idIt != annotIds.end();
        ++idIt) {
        CSeq_id_Handle curId = *idIt;

        CRef<CGb_qual> pLocusTag;
        SAnnotSelector selGenes;
        selGenes.SetSortOrder(SAnnotSelector::eSortOrder_None);
        selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
        CFeat_CI itGenes(mHandle, selGenes);
        for (; itGenes; ++itGenes) {
            CSeq_feat_Handle fh = *itGenes;
            string id1 = fh.GetLocationId().AsString();
            string id2 = curId.AsString();
            if (fh.GetLocationId() != curId) {
                continue;
            }
            CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
                itGenes->GetOriginalFeature()));
            CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
            pEditedFeat->Assign(itGenes->GetOriginalFeature());
            pEditedFeat->RemoveQualifier("locus_tag");
            pEditedFeat->SetData().SetGene().SetLocus_tag(xNextLocusTag());
            feh.Replace(*pEditedFeat);
        }
    }
    SAnnotSelector selOther;
    selOther.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itOther(mHandle, selOther);

    // mss-315:
    //  only the genes get the locus_tags. they are inherited down
    //  to the features that live on them when we generate a flat file,
    //  but we don't want them here in the ASN.1
    for (; itOther; ++itOther) {
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itOther)->GetOriginalFeature()));
        feh.RemoveQualifier("locus_tag");
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeatures(
    bool forEukaryote)
    //  ----------------------------------------------------------------------------
{
    if (forEukaryote) {
        GenerateMissingParentFeaturesForEukaryote();
    }
    else {
        GenerateMissingParentFeaturesForProkaryote();
    }
    mTree = feature::CFeatTree(mHandle);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeaturesForEukaryote()
//  ----------------------------------------------------------------------------
{
    GenerateMissingMrnaForCds();
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Rna);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeaturesForProkaryote()
//  ----------------------------------------------------------------------------
{
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Cdregion);
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Rna);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddQualifier(
    CMappedFeat mf,
    const string& qualKey,
    const string& qualVal)
    //  ----------------------------------------------------------------------------
{
    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.AddQualifier(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureRemoveQualifier(
    CMappedFeat mf,
    const string& qualKey)
    //  ----------------------------------------------------------------------------
{
    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.RemoveQualifier(qualKey);
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureSetQualifier(
    CMappedFeat mf,
    const string& qualKey,
    const string& qualVal)
    //  ----------------------------------------------------------------------------
{
    auto existing = mf.GetNamedQual(qualKey);
    if (!existing.empty()) {
        xFeatureRemoveQualifier(mf, qualKey);
    }
    xFeatureAddQualifier(mf, qualKey, qualVal);
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureSetProduct(
    CMappedFeat mf,
    const string& proteinIdStr)
    //  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pProteinId(
        new CSeq_id(proteinIdStr,
            CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK));

    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
    pEditedFeat->Assign(origFeat);
    pEditedFeat->SetProduct().SetWhole(*pProteinId);
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.Replace(*pEditedFeat);
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextFeatId()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mNextFeatId++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH - suffix.size()) + suffix;
    }
    string nextTag("auto");
    return nextTag + suffix;
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextLocusTag()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mLocusTagNumber++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH - suffix.size()) + suffix;
    }
    string nextTag = mLocusTagPrefix + "_" + suffix;
    return nextTag;
}

//	----------------------------------------------------------------------------
string CFeatTableEdit::xNextProteinId(
    const CMappedFeat& mf)
    //	----------------------------------------------------------------------------
{
    const string dbPrefix("gnl|");

    // format: mLocusTagPrefix|<locus tag of gene or hash>[_numeric disambiguation]
    CMappedFeat parentGene = feature::GetBestGeneForFeat(mf, &mTree);
    if (!parentGene ||
        !parentGene.GetData().GetGene().IsSetLocus_tag()) {
        xPutErrorMissingLocustag(mf);
        return "";
    }

    int offset = 0;
    string locusTag = parentGene.GetData().GetGene().GetLocus_tag();
    map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
    if (it == mMapProtIdCounts.end()) {
        mMapProtIdCounts[locusTag] = 0;
    }
    else {
        ++mMapProtIdCounts[locusTag];
        offset = mMapProtIdCounts[locusTag];
    }
    string db = mLocusTagPrefix;
    if (locusTag.empty() &&
        db.empty()) {
        xPutErrorMissingLocustag(mf);
    }

    if (db.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        db = prefix;
    }
    string proteinId = dbPrefix + db + "|" + GetIdHashOrValue(locusTag, offset);
    return proteinId;
}

//	----------------------------------------------------------------------------
string CFeatTableEdit::xNextTranscriptId(
    const CMappedFeat& cds)
    //	----------------------------------------------------------------------------
{
    const string dbPrefix("gnl|");


    // format: mLocusTagPrefix|mrna.<locus tag of gene or hash>[_numeric disambiguation]
    CMappedFeat parentGene = feature::GetBestGeneForFeat(cds, &mTree);
    if (!parentGene ||
        !parentGene.GetData().GetGene().IsSetLocus_tag()) {
        xPutErrorMissingLocustag(cds);
        return "";
    }


    string locusTag = parentGene.GetData().GetGene().GetLocus_tag();
    int offset = 0;
    map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
    if (it != mMapProtIdCounts.end() && mMapProtIdCounts[locusTag] != 0) {
        offset = mMapProtIdCounts[locusTag];
    }
    string db = mLocusTagPrefix;

    if (locusTag.empty() &&
        db.empty()) {
        xPutErrorMissingLocustag(cds);
    }


    if (db.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        db = prefix;
    }

    string transcriptId = dbPrefix + db + "|mrna." + GetIdHashOrValue(locusTag, offset);
    return transcriptId;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> CFeatTableEdit::xGetGeneLocation(
    const CSeq_loc& baseLoc)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pEnvelope(new CSeq_loc);

    auto baseStart = baseLoc.GetStart(eExtreme_Positional);
    auto baseStop = baseLoc.GetStop(eExtreme_Positional);
    auto& baseId = *baseLoc.GetId();

    if (mSequenceSize == 0  ||  baseStart <= baseStop) {
        pEnvelope->SetInt();
        pEnvelope->SetId(baseId);
        pEnvelope->SetInt().SetFrom(baseStart);
        pEnvelope->SetInt().SetTo(baseStop);
        pEnvelope->SetInt().SetStrand(baseLoc.GetStrand());
    }
    else {
        if (baseLoc.GetStrand() != eNa_strand_minus) {
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(baseId);
            pTop->SetFrom(baseStart);
            pTop->SetTo(mSequenceSize);
            pTop->SetStrand(baseLoc.GetStrand());
            pEnvelope->SetPacked_int().AddInterval(*pTop);
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(baseId);
            pBottom->SetFrom(0);
            pBottom->SetTo(baseStop);
            pBottom->SetStrand(baseLoc.GetStrand());
            pEnvelope->SetPacked_int().AddInterval(*pBottom);
            pEnvelope->ChangeToMix();
        }
        else {
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(baseId);
            pBottom->SetFrom(0);
            pBottom->SetTo(baseStop);
            pBottom->SetStrand(baseLoc.GetStrand());
            pEnvelope->SetPacked_int().AddInterval(*pBottom);
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(baseId);
            pTop->SetFrom(baseStart);
            pTop->SetTo(mSequenceSize);
            pTop->SetStrand(baseLoc.GetStrand());
            pEnvelope->SetPacked_int().AddInterval(*pTop);
            pEnvelope->ChangeToMix();
        }
    }
    return pEnvelope;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForFeature(
    const CMappedFeat& rna)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    //const CSeq_loc& loc = rna.GetOriginalFeature().GetLocation();
    CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(rna.GetOriginalFeature());
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CMappedFeat existingGene = feature::GetBestGeneForFeat(rna, &mTree);
    if (existingGene) {
        return pGene;
    }
    pGene.Reset(new CSeq_feat);
    pGene->SetLocation(*xGetGeneLocation(rna.GetLocation()));
    pGene->SetData().SetGene();
    return pGene;
}


//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutError(
    const string& message)
    //  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    mpMessageListener->PutMessage(
        CObjEditMessage(message, eDiag_Error));
}


//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingLocustag(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    string message = subName + " feature is missing locus tag.";

    xPutError(message);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingTranscriptId(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    string message = subName + " feature is missing transcript ID.";

    xPutError(message);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorBadCodonRecognized(
    const string codonRecognized)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }
    string message = "tRNA with bad codon recognized attribute \"" +
        codonRecognized + "\".";
    xPutError(message);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingProteinId(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    string message = subName + " feature is missing protein ID.";
    xPutError(message);
}


//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorDifferingProteinIds(
    const CMappedFeat& mrna)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    if (mrna.GetFeatSubtype() != CSeqFeatData::eSubtype_mRNA) {
        return;
    }
    xPutError("Protein ID on mRNA feature differs from protein ID on child CDS.");

    //    xPutError(ILineError::eProblem_InconsistentQualifiers,
    //        "Protein ID on mRNA feature differs from protein ID on child CDS.");
}

void
CFeatTableEdit::xPutErrorDifferingTranscriptIds(
    const CMappedFeat& mrna)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    if (mrna.GetFeatSubtype() != CSeqFeatData::eSubtype_mRNA) {
        return;
    }

    xPutError("Transcript ID on mRNA feature differs from transcript ID on child CDS.");
}


//  ----------------------------------------------------------------------------
string
CFeatTableEdit::xGetIdStr(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    stringstream strstr;
    auto& id = mf.GetId();
    switch (id.Which()) {
    default:
        return "\"UNKNOWN ID\"";
    case CFeat_id::e_Local:
        id.GetLocal().AsString(strstr);
        return strstr.str();
    }
}

//  ----------------------------------------------------------------------------
string
CFeatTableEdit::xGetCurrentLocusTagPrefix(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    if (!mLocusTagPrefix.empty()) {
        return mLocusTagPrefix;
    }
    CMappedFeat geneFeature = mf;
    if (geneFeature.GetFeatSubtype() != CSeqFeatData::eSubtype_gene) {
        geneFeature = feature::GetBestGeneForFeat(mf, &mTree);
    }
    if (!geneFeature) {
        return "";
    }
    const auto& geneRef = geneFeature.GetData().GetGene();
    if (geneRef.IsSetLocus_tag()) {
        const auto& locusTag = geneFeature.GetData().GetGene().GetLocus_tag();
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        return prefix;
    }
    auto locusTagFromQualifier = geneFeature.GetNamedQual("locus_tag");
    if (!locusTagFromQualifier.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTagFromQualifier, "_", prefix, suffix);
        return prefix;
    }
    return "";
}

CConstRef<CSeq_feat> CFeatTableEdit::xGetLinkedFeature(const CSeq_feat& cd_feature, bool gene)
{
    CConstRef<CSeq_feat> feature;
    if (feature.Empty())
    {

        CMappedFeat cds(mpScope->GetSeq_featHandle(cd_feature));
        CMappedFeat other;
        if (gene)
            other = feature::GetBestGeneForCds(cds, &mTree);
        else
            other = feature::GetBestMrnaForCds(cds, &mTree);

        if (other)
            feature.Reset(&other.GetOriginalFeature());

    }
    return feature;
}


static void s_AppendProtRefInfo(CProt_ref& current_ref, const CProt_ref& other_ref)
{

    auto append_nonduplicated_item = [](list<string>& current_list,
        const list<string>& other_list)
    {
        unordered_set<string> current_set;
        for (const auto& item : current_list) {
            current_set.insert(item);
        }

        for (const auto& item : other_list) {
            if (current_set.find(item) == current_set.end()) {
                current_list.push_back(item);
            }
        }
    };

    if (other_ref.IsSetName()) {
        append_nonduplicated_item(current_ref.SetName(),
            other_ref.GetName());
    }

    if (other_ref.IsSetDesc()) {
        current_ref.SetDesc() = other_ref.GetDesc();
    }

    if (other_ref.IsSetEc()) {
        append_nonduplicated_item(current_ref.SetEc(),
            other_ref.GetEc());
    }

    if (other_ref.IsSetActivity()) {
        append_nonduplicated_item(current_ref.SetActivity(),
            other_ref.GetActivity());
    }

    if (other_ref.IsSetDb()) {
        for (const auto pDBtag : other_ref.GetDb()) {
            current_ref.SetDb().push_back(pDBtag);
        }
    }

    if (current_ref.GetProcessed() == CProt_ref::eProcessed_not_set) {
        const auto processed = other_ref.GetProcessed();
        if (processed != CProt_ref::eProcessed_not_set) {
            current_ref.SetProcessed(processed);
        }
    }
}

static void s_SetProtRef(const CSeq_feat& cds,
    CConstRef<CSeq_feat> pMrna,
    CProt_ref& prot_ref)
{
    const CProt_ref* pProtXref = cds.GetProtXref();
    if (pProtXref) {
        s_AppendProtRefInfo(prot_ref, *pProtXref);
    }


    if (!prot_ref.IsSetName()) {
        const string& product_name = cds.GetNamedQual("product");
        if (product_name != kEmptyStr) {
            prot_ref.SetName().push_back(product_name);
        }
    }

    if (pMrna.Empty()) { // Nothing more we can do here
        return;
    }

    if (prot_ref.IsSetName()) {
        for (auto& prot_name : prot_ref.SetName()) {
            if (NStr::CompareNocase(prot_name, "hypothetical protein") == 0) {
                if (pMrna->GetData().GetRna().IsSetExt() &&
                    pMrna->GetData().GetRna().GetExt().IsName()) {
                    prot_name = pMrna->GetData().GetRna().GetExt().GetName();
                    break;
                }
            }
        }
    } // prot_ref.IsSetName() 
}

static bool AssignLocalIdIfEmpty(ncbi::objects::CSeq_feat& feature, unsigned& id)
{
    if (feature.IsSetId())
        return true;
    else
    {
        feature.SetId().SetLocal().SetId(id++);
        return false;
    }
}

void CFeatTableEdit::InstantiateProductsNames()
{
    for (auto feat : mAnnot.GetData().GetFtable())
    {
        if (feat.NotEmpty() && feat->IsSetData() && feat->GetData().IsCdregion())
        {
            xGenerate_mRNA_Product(*feat);
        }
    }
}

void CFeatTableEdit::xGenerate_mRNA_Product(CSeq_feat& cd_feature)
{
    if (sequence::IsPseudo(cd_feature, *mpScope))
        return; // CRef<CSeq_entry>();

#if 0
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(bioseq);

    CConstRef<CSeq_entry> replacement = LocateProtein(m_replacement_protein, cd_feature);
#endif

    CConstRef<CSeq_feat> mrna = xGetLinkedFeature(cd_feature, false);
    CConstRef<CSeq_feat> gene = xGetLinkedFeature(cd_feature, true);

#if 0
    CRef<CBioseq> protein;
    bool was_extended = false;
    if (replacement.Empty())
    {
        was_extended = CCleanup::ExtendToStopIfShortAndNotPartial(cd_feature, bsh);

        protein = CSeqTranslator::TranslateToProtein(cd_feature, *m_scope);

        if (protein.Empty())
            return CRef<CSeq_entry>();
    }
    else
    {
        protein.Reset(new CBioseq());
        protein->Assign(replacement->GetSeq());
    }

    CRef<CSeq_entry> protein_entry(new CSeq_entry);
    protein_entry->SetSeq(*protein);

    CAutoAddDesc molinfo_desc(protein->SetDescr(), CSeqdesc::e_Molinfo);
    molinfo_desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    molinfo_desc.Set().SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
    feature::AdjustProteinMolInfoToMatchCDS(molinfo_desc.Set().SetMolinfo(), cd_feature);

    string org_name;
    CTable2AsnContext::GetOrgName(org_name, *top_entry_h.GetCompleteObject());

    CTempString locustag;
    if (!gene.Empty() && gene->IsSetData() && gene->GetData().IsGene() && gene->GetData().GetGene().IsSetLocus_tag())
    {
        locustag = gene->GetData().GetGene().GetLocus_tag();
    }

    string base_name;
    CRef<CSeq_id> newid;
    CTempString qual_to_remove;

    if (protein->GetId().empty())
    {
        const string* protein_ids = 0;

        qual_to_remove = "protein_id";
        protein_ids = &cd_feature.GetNamedQual(qual_to_remove);

        if (protein_ids->empty())
        {
            qual_to_remove = "orig_protein_id";
            protein_ids = &cd_feature.GetNamedQual(qual_to_remove);
        }

        if (protein_ids->empty())
        {
            if (mrna.NotEmpty())
                protein_ids = &mrna->GetNamedQual("protein_id");
        }

        if (protein_ids->empty())
        {
            protein_ids = &cd_feature.GetNamedQual("product_id");
        }

        if (!protein_ids->empty())
        {
            CBioseq::TId new_ids;
            CSeq_id::ParseIDs(new_ids, *protein_ids, CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK);

            MergeSeqIds(*protein, new_ids);
            cd_feature.RemoveQualifier(qual_to_remove);
        }
    }
    else {
        cd_feature.RemoveQualifier("protein_id");
        cd_feature.RemoveQualifier("orig_protein_id");
    }

    if (protein->GetId().empty())
    {
        if (base_name.empty() && !bioseq.GetId().empty())
        {
            bioseq.GetId().front()->GetLabel(&base_name, CSeq_id::eContent);
        }

        newid = GetNewProteinId(*m_scope, base_name);
        protein->SetId().push_back(newid);
    }

#endif

#if 0
    CSeq_feat& prot_feat = CreateOrSetFTable(*protein);
    CProt_ref& prot_ref = prot_feat.SetData().SetProt();
#else
    CRef<CProt_ref> p_prot_ref(new CProt_ref);
    CProt_ref& prot_ref = *p_prot_ref;
#endif

    s_SetProtRef(cd_feature, mrna, prot_ref);
    if ((!prot_ref.IsSetName() || prot_ref.GetName().empty()) && m_use_hypothetic_protein)
    {
        prot_ref.SetName().push_back("hypothetical protein");
        cd_feature.SetProtXref().SetName().clear();
        cd_feature.SetProtXref().SetName().push_back("hypothetical protein");
        //cd_feature.RemoveQualifier("product");
    }


#if 0
    prot_feat.SetLocation().SetInt().SetFrom(0);
    prot_feat.SetLocation().SetInt().SetTo(protein->GetInst().GetLength() - 1);
    prot_feat.SetLocation().SetInt().SetId().Assign(*GetAccessionId(protein->GetId()));
    feature::CopyFeaturePartials(prot_feat, cd_feature);

    if (!cd_feature.IsSetProduct())
        cd_feature.SetProduct().SetWhole().Assign(*GetAccessionId(protein->GetId()));

    CBioseq_Handle protein_handle = m_scope->AddBioseq(*protein);

    AssignLocalIdIfEmpty(cd_feature, m_local_id_counter);
    if (gene.NotEmpty() && mrna.NotEmpty())
        cd_feature.SetXref().clear();

    if (gene.NotEmpty())
    {
        CSeq_feat& gene_feature = (CSeq_feat&)*gene;
        AssignLocalIdIfEmpty(gene_feature, m_local_id_counter);
        cd_feature.AddSeqFeatXref(gene_feature.GetId());
        gene_feature.AddSeqFeatXref(cd_feature.GetId());
    }
#endif

    if (mrna.NotEmpty())
    {
        CSeq_feat& mrna_feature = (CSeq_feat&)*mrna;

        AssignLocalIdIfEmpty(mrna_feature, mNextFeatId);

        if (prot_ref.IsSetName() && !prot_ref.GetName().empty())
        {
            auto& ext = mrna_feature.SetData().SetRna().SetExt();
            if (ext.Which() == CRNA_ref::C_Ext::e_not_set ||
                (ext.IsName() && ext.SetName().empty()))
                ext.SetName() = prot_ref.GetName().front();
        }
        mrna_feature.AddSeqFeatXref(cd_feature.GetId());
        cd_feature.AddSeqFeatXref(mrna_feature.GetId());
    }

#if 0
    if (prot_ref.IsSetName() && !prot_ref.GetName().empty())
    {
        //cd_feature.ResetProduct();
        cd_feature.SetProtXref().SetName().clear();
        cd_feature.SetProtXref().SetName().push_back(prot_ref.GetName().front());
        cd_feature.RemoveQualifier("product");
    }


    if (was_extended)
    {
        if (!mrna.Empty() && mrna->IsSetLocation() && CCleanup::LocationMayBeExtendedToMatch(mrna->GetLocation(), cd_feature.GetLocation()))
            CCleanup::ExtendStopPosition((CSeq_feat&)*mrna, &cd_feature);
        if (!gene.Empty() && gene->IsSetLocation() && CCleanup::LocationMayBeExtendedToMatch(gene->GetLocation(), cd_feature.GetLocation()))
            CCleanup::ExtendStopPosition((CSeq_feat&)*gene, &cd_feature);
    }

    return protein_entry;
#endif
}

//  ============================================================================
string
sGetFeatMapKey(const CObject_id& objectId)
//  ============================================================================
{
    if (objectId.IsStr()) {
        return objectId.GetStr();
    }
    return string("id:") + NStr::NumericToString(objectId.GetId());
}

//  ============================================================================
void
CFeatTableEdit::MergeFeatures(
    const CSeq_annot::TData::TFtable& otherFeatures)
//  ============================================================================
{
    FeatMap featMap;
    
    auto& thisFeatures = mAnnot.SetData().SetFtable();
    for (auto pThisFeat: thisFeatures) {
    //(assumption: only local IDs are at issue)
        if (!pThisFeat->IsSetId()  ||  !pThisFeat->GetId().IsLocal()) {
            continue;
        }
        const auto& this_oid = pThisFeat->GetId().GetLocal();
        featMap[sGetFeatMapKey(this_oid)] = pThisFeat;
    }
    for (auto pOtherFeat: otherFeatures) {
        if (!pOtherFeat->IsSetId()  ||  !pOtherFeat->GetId().IsLocal()) {
            thisFeatures.push_back(pOtherFeat);
            continue;
        }
        const auto& other_oid = pOtherFeat->GetId().GetLocal();
        if (featMap.find(sGetFeatMapKey(other_oid)) == featMap.end()) {
            thisFeatures.push_back(pOtherFeat);
            continue;
        }
        xRenameFeatureId(other_oid, featMap);
        thisFeatures.push_back(pOtherFeat);
    }
}

//  ==============================================================================
bool OjectIdsAreEqual (
//  ==============================================================================
    const CObject_id& lhs,
    const CObject_id& rhs)
{
    if (lhs.Which() != rhs.Which()) {
        return false;
    }
    if (lhs.IsStr()) {
        return (lhs.GetStr() == rhs.GetStr());
    }
    return (lhs.GetId() == rhs.GetId());
};

//  =============================================================================
void
CFeatTableEdit::xRenameFeatureId(
    const CObject_id& oldFeatId,
    FeatMap& featMap)
//  =============================================================================
{
    string oldFeatIdAsStr;
    if (oldFeatId.IsStr()) {
        oldFeatIdAsStr = oldFeatId.GetStr();
    }
    else {
        oldFeatIdAsStr = NStr::NumericToString(oldFeatId.GetId());
    }
    
    string newFeatIdAsStr = oldFeatIdAsStr + "x";
    CRef<CObject_id> pNewFeatId(new CObject_id);
    pNewFeatId->SetStr(oldFeatIdAsStr + "x");
    while (featMap.find(sGetFeatMapKey(*pNewFeatId)) != featMap.end()) {
        pNewFeatId->SetStr() += "x";
    }

    auto pBaseFeat = featMap[sGetFeatMapKey(oldFeatId)];
    pBaseFeat->SetId().SetLocal(*pNewFeatId);
    featMap.erase(sGetFeatMapKey(oldFeatId));
    featMap[sGetFeatMapKey(*pNewFeatId)] = pBaseFeat;

    for (auto it = featMap.begin(); it != featMap.end(); ++it) {
        auto pFeat = it->second;
        for (auto& pXref: pFeat->SetXref()) {
            if (!pXref->IsSetId()  ||  !pXref->GetId().IsLocal()) {
                continue;
            }
            if (OjectIdsAreEqual(pXref->GetId().GetLocal(), oldFeatId)) {
                pXref->SetId().SetLocal(*pNewFeatId);
            }
        }
    }
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

