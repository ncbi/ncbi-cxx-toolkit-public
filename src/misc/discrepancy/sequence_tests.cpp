/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <util/xregexp/regexp.hpp>

#include "discrepancy_core.hpp"
#include "utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(sequence_tests);


// DUP_DEFLINE

const string kUniqueDeflines = "[n] definition line[s] [is] unique";
const string kIdenticalDeflines = "[n] definition line[s] [is] identical";
const string kAllUniqueDeflines = "All deflines are unique";
const string kSomeIdenticalDeflines = "Defline Problem Report";


DISCREPANCY_CASE(DUP_DEFLINE, SEQUENCE, eOncaller, "Definition lines should be unique")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa() && bioseq.IsSetDescr()) {
        for (auto& desc : context.GetSeqdesc()) { // not searching on the parend nodes!
            if (desc.IsTitle()) {
                m_Objs[desc.GetTitle()].Add(*context.SeqdescObjRef(desc), false);
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DUP_DEFLINE)
{
    if (m_Objs.empty()) {
        return;
    }
    bool all_unique = true;
    CReportNode tmp;
    for (auto& it : m_Objs.GetMap()) {
        TReportObjectList& list = it.second->GetObjects();
        if (list.size() == 1) {
            tmp[kSomeIdenticalDeflines][kUniqueDeflines].Add(list).Severity(CReportItem::eSeverity_info);
        }
        else if (list.size() > 1) {
            tmp[kSomeIdenticalDeflines][kIdenticalDeflines + "[*" + it.first + "*]"].Add(list);
            all_unique = false;
        }
    }
    if (all_unique) {
        tmp.clear();
        tmp[kAllUniqueDeflines].Severity(CReportItem::eSeverity_info);
    }
    m_ReportItems = tmp.Export(*this)->GetSubitems();
}


// TERMINAL_NS

DISCREPANCY_CASE(TERMINAL_NS, SEQUENCE, eDisc | eSmart | eBig | eFatal, "Ns at end of sequences")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (sum.StartsWithGap || sum.EndsWithGap) {
            m_Objs["[n] sequence[s] [has] terminal Ns"].Fatal().Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(TERMINAL_NS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SHORT_PROT_SEQUENCES

DISCREPANCY_CASE(SHORT_PROT_SEQUENCES, SEQUENCE, eDisc | eOncaller | eSmart, "Protein sequences should be at least 50 aa, unless they are partial")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsAa() && bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() < 50) {
        const CSeqdesc* molinfo = context.GetMolinfo();
        if (!molinfo || !molinfo->GetMolinfo().IsSetCompleteness() || molinfo->GetMolinfo().GetCompleteness() == CMolInfo::eCompleteness_unknown || molinfo->GetMolinfo().GetCompleteness() == CMolInfo::eCompleteness_complete) {
            m_Objs["[n] protein sequences are shorter than 50 aa."].Add(*context.BioseqObjRef(), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(SHORT_PROT_SEQUENCES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COMMENT_PRESENT

DISCREPANCY_CASE(COMMENT_PRESENT, DESC, eOncaller, "Comment descriptor present")
{
    for (auto& desc : context.GetSeqdesc()) {
        if (desc.IsComment()) {
            m_Objs[desc.GetComment()].Add(*context.SeqdescObjRef(desc));
        }
    }
}


DISCREPANCY_SUMMARIZE(COMMENT_PRESENT)
{
    if (!m_Objs.empty()) {
        CReportNode rep;
        string label = m_Objs.GetMap().size() == 1 ? "[n] comment descriptor[s] were found (all same)" : "[n] comment descriptor[s] were found (some different)";
        for (auto it: m_Objs.GetMap()) {
            for (auto obj: it.second->GetObjects()) {
                rep[label].Add(*obj);
            }
        }
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


// MRNA_ON_WRONG_SEQUENCE_TYPE

DISCREPANCY_CASE(MRNA_ON_WRONG_SEQUENCE_TYPE, SEQUENCE, eDisc | eOncaller, "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (!bioseq.IsSetInst() || !bioseq.GetInst().IsSetMol() || bioseq.GetInst().GetMol() != CSeq_inst::eMol_dna) {
        return;
    }
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (!molinfo || !molinfo->GetMolinfo().IsSetBiomol() || molinfo->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_genomic) {
        return;
    }
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || !biosrc->GetSource().IsSetGenome() || 
            biosrc->GetSource().GetGenome() == CBioSource::eGenome_macronuclear || biosrc->GetSource().GetGenome() == CBioSource::eGenome_unknown ||
            biosrc->GetSource().GetGenome() == CBioSource::eGenome_genomic || biosrc->GetSource().GetGenome() == CBioSource::eGenome_chromosome ||
            !context.IsEukaryotic(&biosrc->GetSource())) {
        return;
    }
    for (auto& feat : context.FeatMRNAs()) {
        m_Objs["[n] mRNA[s] [is] located on eukaryotic sequence[s] that [does] not have genomic or plasmid source[s]"].Add(*context.SeqFeatObjRef(*feat));
    }
}


DISCREPANCY_SUMMARIZE(MRNA_ON_WRONG_SEQUENCE_TYPE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DISC_GAPS

const string kSequencesWithGaps = "[n] sequence[s] contain[S] gaps";

DISCREPANCY_CASE(GAPS, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "Sequences with gaps")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsSetRepr() && bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
        const CSeqSummary& sum = context.GetSeqSummary();
        bool has_gaps = !!sum.Gaps;
        if (!has_gaps) {
            const CSeq_annot* annot = nullptr;
            for (auto it : bioseq.GetAnnot()) {
                if (it->IsFtable()) {
                    annot = it;
                    break;
                }
            }
            if (annot) {
                for (auto feat : annot->GetData().GetFtable()) {
                    if (feat->IsSetData() && feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_gap) {
                        has_gaps = true;
                        break;
                    }
                }
            }
        }
        if (has_gaps) {
            m_Objs[kSequencesWithGaps].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(GAPS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BIOPROJECT_ID

DISCREPANCY_CASE(BIOPROJECT_ID, SEQUENCE, eOncaller, "Sequences with BioProject IDs")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.IsSetData() && user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "DBLink") {
                    for (auto& user_field : user.GetData()) {
                        if (user_field->IsSetLabel() && user_field->GetLabel().IsStr() && user_field->GetLabel().GetStr() == "BioProject" && user_field->IsSetData() && user_field->GetData().IsStrs()) {
                            const CUser_field::C_Data::TStrs& strs = user_field->GetData().GetStrs();
                            if (!strs.empty() && !strs[0].empty()) {
                                m_Objs["[n] sequence[s] contain[S] BioProject IDs"].Add(*context.BioseqObjRef());
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BIOPROJECT_ID)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_DEFLINES

DISCREPANCY_CASE(MISSING_DEFLINES, SEQUENCE, eOncaller, "Missing definition lines")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa() && !context.GetTitle()) {
        m_Objs["[n] bioseq[s] [has] no definition line"].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(MISSING_DEFLINES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// N_RUNS_14

DISCREPANCY_CASE(N_RUNS_14, SEQUENCE, eDisc | eTSA, "Runs of more than 14 Ns")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (sum.MaxN > 14) {
            m_Objs["[n] sequence[s] [has] runs of 15 or more Ns"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(N_RUNS_14)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// EXTERNAL_REFERENCE

DISCREPANCY_CASE(EXTERNAL_REFERENCE, SEQUENCE, eDisc | eOncaller, "Sequence has external reference")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (sum.HasRef) {
            m_Objs["[n] sequence[s] [has] external references"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(EXTERNAL_REFERENCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// 10_PERCENTN

DISCREPANCY_CASE(10_PERCENTN, SEQUENCE, eDisc | eSubmitter | eSmart | eTSA, "Greater than 10 percent Ns")
{
    const double MIN_N_PERCENTAGE = 10.0;

    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (!sum.HasRef && sum.N * 100. / sum.Len > MIN_N_PERCENTAGE) {
            m_Objs["[n] sequence[s] [has] > 10% Ns"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(10_PERCENTN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_COUNT

DISCREPANCY_CASE(FEATURE_COUNT, FEAT, eOncaller | eSubmitter | eSmart, "Count features present or missing from sequences")
{
    // context.SetGui(true); // for debug only!
    for (auto& feat : context.GetFeat()) {
        if (!feat.IsSetData() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_prot) {
            continue;
        }
        string key = feat.GetData().IsGene() ? "gene" : feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
        m_Objs[key + ": [n] present"].Info().Incr();
    }
    if (context.IsGui() && context.IsBioseq()) {
        const CBioseq& bioseq = context.CurrentBioseq();
        bool na = false;
        if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
            na = true;
        }
        CRef<CReportObj> rep(context.BioseqObjRef());
        for (auto& feat : context.GetAllFeat()) {
            if (!feat.IsSetData() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_prot) {
                continue;
            }
            string key = feat.GetData().IsGene() ? "gene" : feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
            key = to_string(feat.GetData().GetSubtype()) + " " + key;
            m_Objs[kEmptyCStr][key].Add(*rep, false);
        }
        m_Objs[kEmptyCStr][na ? "N" : "A"].Add(*rep);
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_COUNT)
{
    if (context.IsGui()) {
        for (auto& it : m_Objs[kEmptyCStr].GetMap()) {
            if (it.first == "N" || it.first == "A") {
                continue;
            }
            size_t n = it.first.find(' ');
            string key = it.first.substr(n + 1);
            CSeqFeatData::EFeatureLocationAllowed allow = CSeqFeatData::AllowedFeatureLocation((CSeqFeatData::ESubtype)stoi(it.first.substr(0, n)));
            string label = key + ": [n] present";
            map<CReportObj*, size_t> obj2num;
            if (allow == CSeqFeatData::eFeatureLocationAllowed_Any || allow == CSeqFeatData::eFeatureLocationAllowed_NucOnly) {
                for (auto& obj : m_Objs[kEmptyStr]["N"].GetObjects()) {
                    obj2num[&*obj] = 0;
                }
            }
            if (allow == CSeqFeatData::eFeatureLocationAllowed_Any || allow == CSeqFeatData::eFeatureLocationAllowed_ProtOnly) {
                for (auto& obj : m_Objs[kEmptyStr]["A"].GetObjects()) {
                    obj2num[&*obj] = 0;
                }
            }
            for (auto& obj : m_Objs[kEmptyStr][it.first].GetObjects()) {
                obj2num[&*obj]++;
            }
            for (auto& pp : obj2num) {
                m_Objs[label]["[n] bioseq[s] [has] [(]" + to_string(pp.second) + "[)] " + key + " features"].Info().Add(*pp.first);
            }
        }
        m_Objs.GetMap().erase(kEmptyCStr);
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// EXON_ON_MRNA

DISCREPANCY_CASE(EXON_ON_MRNA, SEQUENCE, eOncaller | eSmart, "mRNA sequences should not have exons")
{
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
        if (context.FeatExons().size()) {
            m_Objs["[n] mRNA bioseq[s] [has] exon features"].Add(*context.BioseqObjRef(CDiscrepancyContext::eFixSet));
        }
    }
}


DISCREPANCY_SUMMARIZE(EXON_ON_MRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(EXON_ON_MRNA)
{
    unsigned int n = 0;
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    CBioseq_EditHandle handle = context.GetBioseqHandle(*seq);
    CFeat_CI ci(handle);
    while (ci) {
        CSeq_feat_EditHandle eh;
        if (ci->IsSetData() && ci->GetData().GetSubtype() == CSeqFeatData::eSubtype_exon) {
            eh = CSeq_feat_EditHandle(context.GetScope().GetSeq_featHandle(ci->GetMappedFeature()));
        }
        ++ci;
        if (eh) {
            eh.Remove();
            n++;
        }
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(n ? new CAutofixReport("EXON_ON_MRNA: [n] exon[s] removed", n) : nullptr);
}


// INCONSISTENT_MOLINFO_TECH

DISCREPANCY_CASE(INCONSISTENT_MOLINFO_TECH, SEQUENCE, eDisc | eSmart, "Inconsistent Molinfo Techniques")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa()) {
        string moltype;
        const CSeqdesc* molinfo = context.GetMolinfo();
        if (molinfo) {
            if (molinfo->GetMolinfo().IsSetTech()) {
                m_Objs[to_string(molinfo->GetMolinfo().GetTech())].Add(*context.BioseqObjRef());
            }
            else {
                m_Objs[kEmptyStr].Add(*context.BioseqObjRef());
            }
        }
    }
}


static const string kInconsistentMolinfoTechSummary = "Molinfo Technique Report";
static const string kInconsistentMolinfoTech = "[n] Molinfo[s] [is] missing field technique";

DISCREPANCY_SUMMARIZE(INCONSISTENT_MOLINFO_TECH)
{
    if (m_Objs.empty()) {
        return;
    }

    CReportNode report;

    CReportNode::TNodeMap& the_map = m_Objs.GetMap();

    bool same = true;
    string tech;

    size_t num_of_missing = 0,
           num_of_bioseqs = 0;

    for (auto it: the_map) {
        num_of_bioseqs += it.second->GetObjects().size();
        if (it.first.empty()) {
            num_of_missing += it.second->GetObjects().size();
            continue;
        }
        if (tech.empty()) {
            tech = it.first;
        }
        else if (tech != it.first) {
            same = false;
        }
    }
    string summary = kInconsistentMolinfoTechSummary + " (";
    if (num_of_missing == num_of_bioseqs || (same && !num_of_missing)) {
        return;
    }
    summary += num_of_missing ? "some missing, " : "all present, ";
    summary += same ? "all same)" : "some different)";
    if (num_of_missing) {
        if (num_of_missing == num_of_bioseqs) {
            report[summary].SetCount(num_of_missing);
        }
        else {
            report[summary][kInconsistentMolinfoTech].Add(the_map[kEmptyStr]->GetObjects());
        }
    }

    m_ReportItems = report.Export(*this)->GetSubitems();
}


// TITLE_ENDS_WITH_SEQUENCE

static bool IsATGC(char ch)
{
    return (ch == 'A' || ch == 'T' || ch == 'G' || ch == 'C');
}


static bool EndsWithSequence(const string& title)
{
    static const size_t MIN_TITLE_SEQ_LEN = 19; // 19 was just copied from C-toolkit

    size_t count = 0;
    for (string::const_reverse_iterator it = title.rbegin(); it != title.rend(); ++it) {
        if (IsATGC(*it)) {
            ++count;
        }
        else
            break;

        if (count >= MIN_TITLE_SEQ_LEN) {
            break;
        }
    }

    return count >= MIN_TITLE_SEQ_LEN;
}


DISCREPANCY_CASE(TITLE_ENDS_WITH_SEQUENCE, DESC, eDisc | eSubmitter | eSmart | eBig, "Sequence characters at end of defline")
{
    for (auto& desc : context.GetSeqdesc()) {
        if (desc.IsTitle() && EndsWithSequence(desc.GetTitle())) {
            m_Objs["[n] defline[s] appear[S] to end with sequence characters"].Add(*context.SeqdescObjRef(desc));
        }
    }
}


DISCREPANCY_SUMMARIZE(TITLE_ENDS_WITH_SEQUENCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_MOLTYPE_MISMATCH

DISCREPANCY_CASE(FEATURE_MOLTYPE_MISMATCH, SEQUENCE, eOncaller, "Sequences with rRNA or misc_RNA features should be genomic DNA")
{
    bool is_dna = false;
    bool is_genomic = false;
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        is_dna = true;
    }
    auto molinfo = context.GetMolinfo();
    if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_genomic) {
        is_genomic = true;
    }
    if (!is_genomic || !is_dna) {
        const CSeq_annot* annot = nullptr;
        if (bioseq.IsSetAnnot()) {
            for (auto& annot_it : bioseq.GetAnnot()) {
                if (annot_it->IsFtable()) {
                    annot = annot_it;
                    break;
                }
            }
        }
        if (annot) {
            for (auto& feat : annot->GetData().GetFtable()) {
                if (feat->IsSetData()) {
                    CSeqFeatData::ESubtype subtype = feat->GetData().GetSubtype();
                    if (subtype == CSeqFeatData::eSubtype_rRNA || subtype == CSeqFeatData::eSubtype_otherRNA) {
                        m_Objs["[n] sequence[s] [has] rRNA or misc_RNA features but [is] not genomic DNA"].Add(*context.BioseqObjRef(CDiscrepancyContext::eFixSelf));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_MOLTYPE_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(FEATURE_MOLTYPE_MISMATCH)
{
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    CBioseq_EditHandle edit_handle = context.GetBioseqHandle(*seq);
    edit_handle.SetInst_Mol(CSeq_inst::eMol_dna);
    CSeq_descr& descrs = edit_handle.SetDescr();
    CMolInfo* molinfo = nullptr;
    if (descrs.IsSet()) {
        for (auto descr : descrs.Set()) {
            if (descr->IsMolinfo()) {
                molinfo = &(descr->SetMolinfo());
                break;
            }
        }
    }
    if (molinfo == nullptr) {
        CRef<CSeqdesc> new_descr(new CSeqdesc);
        molinfo = &(new_descr->SetMolinfo());
        descrs.Set().push_back(new_descr);
    }
    if (molinfo == nullptr) {
        return CRef<CAutofixReport>();
    }
    molinfo->SetBiomol(CMolInfo::eBiomol_genomic);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("FEATURE_MOLTYPE_MISMATCH: Moltype was set to genomic for [n] bioseq[s]", 1));
}


// INCONSISTENT_DBLINK

const string kMissingDBLink = "[n] Bioseq [is] missing DBLink object";
const string kDBLinkObjectList = "DBLink Objects";
const string kDBLinkFieldCountTop = "DBLink Fields";
const string kDBLinkCollect = "DBLink Collection";

string GetFieldValueAsString(const CUser_field& field)
{
    string value;

    if (field.GetData().IsStr()) {
        value = field.GetData().GetStr();
    } else if (field.GetData().IsStrs()) {
        for (const string& s : field.GetData().GetStrs()) {
            if (!NStr::IsBlank(value)) {
                value += "; ";
            }
            value += s;
        }
    }
    return value;
}


const string& kPreviouslySeenFields = "Previously Seen Fields";
const string& kPreviouslySeenFieldsThis = "Previously Seen Fields This";
const string& kPreviouslySeenObjects = "Previously Seen Objects";

void AddUserObjectFieldItems
(//CConstRef<CSeqdesc> desc,
    const CSeqdesc* desc,
 //CConstRef<CBioseq> seq,
 //const CSeqSummary* info,
 CReportObj& rep_seq,
 CReportNode& collector,
 CReportNode& previously_seen,
 CDiscrepancyContext& context,
 const string& object_name,
 const string& field_prefix = kEmptyStr)
{
    if (!desc) {
        // add missing for all previously seen fields
        for (auto& obj : previously_seen[kPreviouslySeenFields].GetMap()) {
            for (auto& z : obj.second->GetMap()) {
                collector[field_prefix + z.first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z.first]
                    //.Add(*context.NewBioseqObj(seq, info, eKeepRef), false);
                    //.Add(*context.NewBioseqObj(seq, info));
                    .Add(rep_seq);
            }
        }
        return;
    }
    
    bool already_seen = previously_seen[kPreviouslySeenObjects].Exist(object_name);
    for (auto& f : desc->GetUser().GetData()) {
        if (f->IsSetLabel() && f->GetLabel().IsStr() && f->IsSetData()) {
            string field_name = field_prefix + f->GetLabel().GetStr();
            // add missing field to all previous objects that do not have this field
            if (already_seen && !collector.Exist(field_name)) {
                //ITERATE(TReportObjectList, ro, previously_seen[kPreviouslySeenObjects][object_name].GetObjects()) {
                for (auto& ro: previously_seen[kPreviouslySeenObjects][object_name].GetObjects()) {
                    string missing_label = "[n] " + object_name + "[s] [is] missing field " + field_name;
                    //CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(ro->GetNCPointer()));
                    //collector[field_name][missing_label].Add(*seq_disc_obj, false);
                    //collector[field_name][missing_label].Add(*seq_disc_obj);
                    collector[field_name][missing_label].Add(*ro);
                }
            }
            collector[field_name]["[n] " + object_name + "[s] [has] field " + field_name + " value '" + GetFieldValueAsString(*f) + "'"].Add(*context.SeqdescObjRef(*desc), false);
            previously_seen[kPreviouslySeenFieldsThis][f->GetLabel().GetStr()].Add(*context.SeqdescObjRef(*desc), false);
            previously_seen[kPreviouslySeenFields][object_name][f->GetLabel().GetStr()].Add(*context.SeqdescObjRef(*desc), false);
        }
    }

    // add missing for all previously seen fields not on this object
    for (auto& z : previously_seen[kPreviouslySeenFields][object_name].GetMap()) {
        if (!previously_seen[kPreviouslySeenFieldsThis].Exist(z.first)) {
            collector[field_prefix + z.first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z.first].Add(*context.SeqdescObjRef(*desc));
        }
    }

    // maintain object list for missing fields
    //CRef<CDiscrepancyObject> this_disc_obj(context.NewSeqdescObj(d, context.GetCurrentBioseqLabel(), eKeepRef));
    CRef<CDiscrepancyObject> this_disc_obj(context.SeqdescObjRef(*desc));
    //previously_seen[kPreviouslySeenObjects][object_name].Add(*this_disc_obj, false);
    previously_seen[kPreviouslySeenObjects][object_name].Add(*this_disc_obj);
    previously_seen[kPreviouslySeenFieldsThis].clear();
}


DISCREPANCY_CASE(INCONSISTENT_DBLINK, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "Inconsistent DBLink fields")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        bool found = false;
        auto rep_seq = context.BioseqObjRef();
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.GetObjectType() == CUser_object::eObjectType_DBLink) {
                    found = true;
                    AddUserObjectFieldItems(&desc, *rep_seq, m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
                }
            }
        }
        if (!found) {
            m_Objs[kMissingDBLink].Add(*rep_seq);
            AddUserObjectFieldItems(nullptr, *rep_seq, m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
        }
    }
}


void AnalyzeField(CReportNode& node, bool& all_present, bool& all_same)
{
    all_present = true;
    all_same = true;
    size_t num_values = 0;
    string value = kEmptyStr;
    bool first = true;
    for (auto& s : node.GetMap()) {
        if (NStr::Find(s.first, " missing field ") != string::npos) {
            all_present = false;
        } else {
            size_t pos = NStr::Find(s.first, " value '");
            if (pos != string::npos) {
                if (first) {
                    value = s.first.substr(pos);
                    num_values++;
                    first = false;
                } else if (!NStr::Equal(s.first.substr(pos), value)) {
                    num_values++;
                }
            }
        }
        if (num_values > 1) {
            all_same = false;
            if (!all_present) {
                // have all the info we need
                break;
            }
        }
    }
}


void AnalyzeFieldReport(CReportNode& node, bool& all_present, bool& all_same)
{
    all_present = true;
    all_same = true;
    for (auto& s : node.GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*s.second, this_present, this_same);
        all_present &= this_present;
        all_same &= this_same;
        if (!all_present && !all_same) {
            break;
        }
    }
}


string GetSummaryLabel(bool all_present, bool all_same)
{
    string summary = "(";
    if (all_present) {
        summary += "all present";
    } else {
        summary += "some missing";
    }
    summary += ", ";
    if (all_same) {
        summary += "all same";
    } else {
        summary += "inconsistent";
    }
    summary += ")";
    return summary;
}


void CopyNode(CReportNode& new_home, CReportNode& original)
{
    for (auto& s : original.GetMap()){
        for (auto q : s.second->GetObjects()) {
            new_home[s.first].Add(*q);
        }
    }
    for (auto& q : original.GetObjects()) {
        new_home.Add(*q);
    }
}


string AdjustDBLinkFieldName(const string& orig_field_name)
{
    if (NStr::Equal(orig_field_name, "BioSample")) {
        return "     " + orig_field_name;
    } else if (NStr::Equal(orig_field_name, "ProbeDB")) {
        return "    " + orig_field_name;
    } else if (NStr::Equal(orig_field_name, "Sequence Read Archive")) {
        return "   " + orig_field_name;
    } else if (NStr::Equal(orig_field_name, "BioProject")) {
        return "  " + orig_field_name;
    } else if (NStr::Equal(orig_field_name, "Assembly")) {
        return " " + orig_field_name;
    } else {
        return orig_field_name;
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_DBLINK)
{
    m_Objs.GetMap().erase(kDBLinkObjectList);
    m_Objs.GetMap().erase(kDBLinkFieldCountTop);
    if (m_Objs.empty()) {
        return;
    }

    // add top-level category, rename field values
    bool all_present = true;
    bool all_same = true;
    AnalyzeFieldReport(m_Objs[kDBLinkCollect], all_present, all_same);
    if (all_present && all_same) {
        m_Objs.clear();
        return;
    }
    string top_label = "DBLink Report " + GetSummaryLabel(all_present, all_same);

    CReportNode::TNodeMap::iterator it = m_Objs.GetMap().begin();
    while (it != m_Objs.GetMap().end()) {
        if (!NStr::Equal(it->first, top_label)
            && !NStr::Equal(it->first, kDBLinkCollect)) {
            CopyNode(m_Objs[top_label]["      " + it->first], *it->second);
            it = m_Objs.GetMap().erase(it);
        } else {
            ++it;
        }
    }

    for (auto& it : m_Objs[kDBLinkCollect].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*it.second, this_present, this_same);
        string new_label = AdjustDBLinkFieldName(it.first) + " " + GetSummaryLabel(this_present, this_same);
        for (auto& s : it.second->GetMap()){
            for (auto& q : s.second->GetObjects()) {
                m_Objs[top_label][new_label][s.first].Add(*q);
            }
        }
    }
    m_Objs.GetMap().erase(kDBLinkCollect);

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_STRUCTURED_COMMENTS
const string kStructuredCommentsSeqs = "sequences";
const string kStructuredCommentObservedPrefixes = "observed prefixes";
const string kStructuredCommentObservedPrefixesThis = "observed prefixes this";
const string kStructuredCommentReport = "collection";
const string kStructuredCommentPrevious = "previous";
const string kStructuredCommentFieldPrefix = "structured comment field ";

DISCREPANCY_CASE(INCONSISTENT_STRUCTURED_COMMENTS, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "Inconsistent structured comments")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        auto rep_seq = context.BioseqObjRef();
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.GetObjectType() == CUser_object::eObjectType_StructuredComment) {
                    string prefix = CComment_rule::GetStructuredCommentPrefix(user);
                    if (NStr::IsBlank(prefix)) {
                        prefix = "unnamed";
                    }
                    m_Objs[kStructuredCommentObservedPrefixesThis][prefix].Add(*context.SeqdescObjRef(desc));
                    AddUserObjectFieldItems(&desc, *rep_seq, m_Objs[kStructuredCommentReport], m_Objs[kStructuredCommentPrevious], context, prefix + " structured comment", kStructuredCommentFieldPrefix);
                }
            }
        }
        //report prefixes seen previously, not found on this sequence
        for (auto& it : m_Objs[kStructuredCommentObservedPrefixes].GetMap()) {
            if (!m_Objs[kStructuredCommentObservedPrefixesThis].Exist(it.first)) {
                m_Objs["[n] Bioseq[s] [is] missing " + it.first + " structured comment"].Add(*rep_seq);
                AddUserObjectFieldItems(nullptr, *rep_seq, m_Objs[kStructuredCommentReport], m_Objs[kStructuredCommentPrevious], context, it.first + " structured comment", kStructuredCommentFieldPrefix);
            }
        }
        // report prefixes found on this sequence but not on previous sequences
        for (auto& it : m_Objs[kStructuredCommentObservedPrefixesThis].GetMap()) {
            if (!m_Objs[kStructuredCommentObservedPrefixes].Exist(it.first)) {
                for (auto ro : m_Objs[kStructuredCommentsSeqs].GetObjects()) {
                    m_Objs["[n] Bioseq[s] [is] missing " + it.first + " structured comment"].Add(*ro);
                    AddUserObjectFieldItems(nullptr, *ro, m_Objs[kStructuredCommentReport], m_Objs[kStructuredCommentPrevious], context, it.first + " structured comment", kStructuredCommentFieldPrefix);
                }
            }
            m_Objs[kStructuredCommentObservedPrefixes][it.first].Add(*context.BioseqObjRef());
        }
        m_Objs[kStructuredCommentObservedPrefixesThis].clear();
        m_Objs[kStructuredCommentsSeqs].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_STRUCTURED_COMMENTS)
{
    m_Objs.GetMap().erase(kStructuredCommentObservedPrefixesThis);
    m_Objs.GetMap().erase(kStructuredCommentsSeqs);
    m_Objs.GetMap().erase(kStructuredCommentObservedPrefixes);
    m_Objs.GetMap().erase(kStructuredCommentPrevious);

    m_Objs[kStructuredCommentReport].GetMap().erase(kStructuredCommentFieldPrefix + "StructuredCommentPrefix");
    m_Objs[kStructuredCommentReport].GetMap().erase(kStructuredCommentFieldPrefix + "StructuredCommentSuffix");

    if (m_Objs.empty()) {
        return;
    }

    // add top-level category, rename field values
    bool all_present = true;
    bool all_same = true;
    AnalyzeFieldReport(m_Objs[kStructuredCommentReport], all_present, all_same);
    if (all_present && all_same) {
        return;
    }

    string top_label = "Structured Comment Report " + GetSummaryLabel(all_present, all_same);

    CReportNode::TNodeMap::iterator it = m_Objs.GetMap().begin();
    while (it != m_Objs.GetMap().end()) {
        if (!NStr::Equal(it->first, top_label)
            && !NStr::Equal(it->first, kStructuredCommentReport)) {
            CopyNode(m_Objs[top_label]["      " + it->first], *it->second);
            it = m_Objs.GetMap().erase(it);
        } else {
            ++it;
        }
    }

    for (auto& it : m_Objs[kStructuredCommentReport].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*it.second, this_present, this_same);
        string new_label = it.first + " " + GetSummaryLabel(this_present, this_same);
        for (auto& s : it.second->GetMap()) {
            string sub_label = s.first;
            if (this_present && this_same) {
                NStr::ReplaceInPlace(sub_label, "[n]", "All");
            }
            for (auto& q : s.second->GetObjects()) {
                m_Objs[top_label][new_label][sub_label].Add(*q);
            }
        }
    }
    m_Objs.GetMap().erase(kStructuredCommentReport);

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_STRUCTURED_COMMENT

DISCREPANCY_CASE(MISSING_STRUCTURED_COMMENT, SEQUENCE, eDisc | eTSA, "Structured comment not included")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.GetObjectType() == CUser_object::eObjectType_StructuredComment) {
                    return;
                }
            }
        }
        m_Objs["[n] sequence[s] [does] not include structured comments."].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(MISSING_STRUCTURED_COMMENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_PROJECT

DISCREPANCY_CASE(MISSING_PROJECT, SEQUENCE, eDisc | eTSA, "Project not included")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.GetObjectType() == CUser_object::eObjectType_DBLink) {
                    if (user.IsSetData()) {
                        for (auto& it : user.GetData()) {
                            if (it->IsSetLabel() && it->GetLabel().IsStr() && NStr::Equal(it->GetLabel().GetStr(), "BioProject")) {
                                return;
                            }
                        }
                    }
                }
                else if (user.IsSetType() && user.GetType().IsStr() && NStr::Equal(user.GetType().GetStr(), "GenomeProjectsDB")) {
                    return;
                }
            }
        }
        m_Objs["[n] sequence[s] [does] not include project."].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(MISSING_PROJECT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_UNVERIFIED

DISCREPANCY_CASE(COUNT_UNVERIFIED, SEQUENCE, eOncaller, "Count number of unverified sequences")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.GetObjectType() == CUser_object::eObjectType_Unverified) {
                    m_Objs["[n] sequence[s] [is] unverified"].Add(*context.BioseqObjRef(), false);
                    return;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(COUNT_UNVERIFIED)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DEFLINE_PRESENT

const string kDeflineExists = "[n] Bioseq[s] [has] definition line";

DISCREPANCY_CASE(DEFLINE_PRESENT, SEQUENCE, eDisc, "Test defline existence")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa() && context.GetTitle()) {
        m_Objs[kDeflineExists].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(DEFLINE_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNUSUAL_NT

DISCREPANCY_CASE(UNUSUAL_NT, SEQUENCE, eDisc | eSubmitter | eSmart, "Sequence contains unusual nucleotides")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (sum.Other) {
            m_Objs["[n] sequence[s] contain[S] nucleotides that are not ATCG or N"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(UNUSUAL_NT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TAXNAME_NOT_IN_DEFLINE

const string kNoTaxnameInDefline = "[n] defline[s] [does] not contain the complete taxname";

DISCREPANCY_CASE(TAXNAME_NOT_IN_DEFLINE, SEQUENCE, eDisc | eOncaller, "Complete taxname should be present in definition line")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa()) {
        const CSeqdesc* source = context.GetBiosource();
        const CSeqdesc* title = context.GetTitle();
        if (source && source->IsSource() && source->GetSource().IsSetOrg() && source->GetSource().GetOrg().IsSetTaxname() && title) {
            string taxname = source->GetSource().GetOrg().GetTaxname();
            if (NStr::EqualNocase(taxname, "Human immunodeficiency virus 1")) {
                taxname = "HIV-1";
            }
            else if (NStr::EqualNocase(taxname, "Human immunodeficiency virus 2")) {
                taxname = "HIV-2";
            }
            bool no_taxname_in_defline = false;
            SIZE_TYPE taxname_pos = NStr::FindNoCase(title->GetTitle(), taxname);
            if (taxname_pos == NPOS) {
                no_taxname_in_defline = true;
            }
            else {
                //capitalization must match for all but the first letter
                no_taxname_in_defline = NStr::CompareCase(title->GetTitle().c_str() + taxname_pos, 1, taxname.size() - 1, taxname.c_str() + 1) != 0;
                if (taxname_pos > 0 && !isspace(title->GetTitle()[taxname_pos - 1]) && !ispunct(title->GetTitle()[taxname_pos - 1])) {
                    no_taxname_in_defline = true;
                }
            }
            if (no_taxname_in_defline) {
                m_Objs[kNoTaxnameInDefline].Add(*context.SeqdescObjRef(*title));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(TAXNAME_NOT_IN_DEFLINE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// HAS_PROJECT_ID

static string GetProjectID(const CUser_object& user)
{
    string res;
    if (user.IsSetData()) {
        for (auto field: user.GetData()) {
            if (field->IsSetData() && field->GetData().IsInt() && field->IsSetLabel() && field->GetLabel().IsStr() && field->GetLabel().GetStr() == "ProjectID") {
                return NStr::IntToString(field->GetData().GetInt());
            }
        }
    }
    return res;
}


DISCREPANCY_CASE(HAS_PROJECT_ID, SEQUENCE, eOncaller | eSmart, "Sequences with project IDs (looks for genome project IDs)")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser()) {
                const CUser_object& user = desc.GetUser();
                if (user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "GenomeProjectsDB") {
                    string proj_id = GetProjectID(user);
                    if (!proj_id.empty()) {
                        m_Objs[proj_id][bioseq.IsNa() ? "N" : "A"].Add(*context.BioseqObjRef());
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(HAS_PROJECT_ID)
{
    if (m_Objs.empty()) {
        return;
    }
    CReportNode res;
    string all = "[n] sequence[s] [has] project IDs ";
    string prots = "[n] protein sequence[s] [has] project IDs ";
    string nucs = "[n] nucleotide sequence[s] [has] project IDs ";
    auto& projects = m_Objs.GetMap();
    all += projects.size() > 1 ? "(some different)" : "(all same)";
    size_t count_prots = 0;
    size_t count_nucs = 0;
    for (auto it: projects) {
        auto& M = it.second->GetMap();
        if (M.find("A") != M.end()) {
            count_prots++;
        }
        if (M.find("N") != M.end()) {
            count_nucs++;
        }
    }
    prots += count_prots > 1 ? "(some different)" : "(all same)";
    nucs += count_nucs > 1 ? "(some different)" : "(all same)";
    for (auto it : projects) {
        auto& M = it.second->GetMap();
        if (M.find("A") != M.end()) {
            for (auto obj : M["A"]->GetObjects()) {
                res[all][prots].Add(*obj);
            }
        }
        if (M.find("N") != M.end()) {
            for (auto obj : M["N"]->GetObjects()) {
                res[all][nucs].Add(*obj);
            }
        }
    }

    m_ReportItems = res.Export(*this)->GetSubitems();
}


// MULTIPLE_CDS_ON_MRNA

DISCREPANCY_CASE(MULTIPLE_CDS_ON_MRNA, SEQUENCE, eOncaller | eSubmitter | eSmart, "Multiple CDS on mRNA")
{
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
        auto& cds = context.FeatCDS();
        if (cds.size() < 2) {
            return;
        }
        size_t count_pseudo = 0;
        size_t count_disrupt = 0;
        for (auto feat : cds) {
            if (feat->IsSetComment() && NStr::Find(feat->GetComment(), "coding region disrupted by sequencing gap") != string::npos) {
                count_disrupt++;
            }
            if (context.IsPseudo(*feat)) {
                count_pseudo++;
            }
        }
        if (count_disrupt != cds.size() && count_pseudo != cds.size()) {
            m_Objs["[n] mRNA bioseq[s] [has] multiple CDS features"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(MULTIPLE_CDS_ON_MRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MRNA_SEQUENCE_MINUS_STRAND_FEATURES

static const string kMrnaSequenceMinusStrandFeatures = "[n] mRNA sequences have features on the complement strand.";

DISCREPANCY_CASE(MRNA_SEQUENCE_MINUS_STRAND_FEATURES, SEQUENCE, eOncaller, "mRNA sequences have CDS/gene on the complement strand")
{
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
        auto& cds = context.FeatCDS();
        size_t count_plus = 0;
        size_t count_minus = 0;
        for (auto& feat : cds) {
            if (feat->GetLocation().GetStrand() != eNa_strand_minus || feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind) {
                count_plus++;
            }
            else {
                count_minus++;
            }
        }
        if (count_minus) {
            if (!count_plus) {
                m_Objs[kMrnaSequenceMinusStrandFeatures].Add(*context.BioseqObjRef(CDiscrepancyContext::eFixSet));
            }
            else {
                m_Objs[kMrnaSequenceMinusStrandFeatures].Add(*context.BioseqObjRef());
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MRNA_SEQUENCE_MINUS_STRAND_FEATURES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(MRNA_SEQUENCE_MINUS_STRAND_FEATURES)
{
    unsigned int n = 0;
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    CBioseq_EditHandle bioseq(context.GetBioseqHandle(*seq));
    vector<CSeq_feat*> features;
    CFeat_CI feat_ci(bioseq, CSeqFeatData::e_Cdregion);
    for (; feat_ci; ++feat_ci) {
        features.push_back(const_cast<CSeq_feat*>(&*feat_ci->GetSeq_feat()));
    }

    CRef<objects::CSeq_inst> new_inst(new objects::CSeq_inst());
    new_inst->Assign(bioseq.GetInst());
    ReverseComplement(*new_inst, &context.GetScope());
    bioseq.SetInst(*new_inst);

    for (auto& feat : features) {
        edit::ReverseComplementFeature(*feat, context.GetScope());
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(n ? new CAutofixReport("MRNA_SEQUENCE_MINUS_STRAND_FEATURES: [n] sequence[s] [is] converted to reverse complement[s]", n) : nullptr);
}


// LOW_QUALITY_REGION

DISCREPANCY_CASE(LOW_QUALITY_REGION, SEQUENCE, eDisc | eSubmitter | eSmart, "Sequence contains regions of low quality")
{
    const size_t MAX_N_IN_SEQ = 7; // 25% of the sequence
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (sum.MinQ > MAX_N_IN_SEQ) {
            m_Objs["[n] sequence[s] contain[S] low quality region"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(LOW_QUALITY_REGION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DEFLINE_ON_SET

DISCREPANCY_CASE(DEFLINE_ON_SET, SEQ_SET, eOncaller, "Titles on sets")
{
    const CBioseq_set& set = context.CurrentBioseq_set();
    if (set.IsSetDescr()) {
        for (auto descr : set.GetDescr().Get()) {
            if (descr->IsTitle()) {
                m_Objs["[n] title[s] on sets were found"].Add(*context.SeqdescObjRef(*descr));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DEFLINE_ON_SET)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// MITOCHONDRION_REQUIRED

DISCREPANCY_CASE(MITOCHONDRION_REQUIRED, SEQUENCE, eDisc | eOncaller, "If D-loop or control region misc_feat is present, source must be mitochondrial")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || biosrc->GetSource().GetGenome() != CBioSource::eGenome_mitochondrion) {
        auto& all = context.FeatAll();
        bool has_D_loop = false;
        bool has_misc_feat_with_control_region = false;
        for (auto& feat : all) {
            if (feat->IsSetData()) {
                if (feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_D_loop) {
                    has_D_loop = true;
                    break;
                }
                else if (feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
                    if (feat->IsSetComment() && NStr::FindNoCase(feat->GetComment(), "control region") != NPOS) {
                        has_misc_feat_with_control_region = true;
                        break;
                    }
                }
            }
        }
        if (has_D_loop || has_misc_feat_with_control_region) {
            m_Objs["[n] bioseq[s] [has] D-loop or control region misc_feature, but [is] do not have mitochondrial source"].Add(*context.BioseqObjRef(CDiscrepancyContext::eFixSet));
        }
    }
}


DISCREPANCY_SUMMARIZE(MITOCHONDRION_REQUIRED)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


static bool FixGenome(const CBioseq& bioseq, CScope& scope)
{
    CBioseq_Handle seq_h = scope.GetBioseqHandle(bioseq);
    CSeqdesc_CI biosrc(seq_h, CSeqdesc::e_Source);
    if (biosrc) {
        CSeqdesc& edit_biosrc = const_cast<CSeqdesc&>(*biosrc);
        edit_biosrc.SetSource().SetGenome(CBioSource::eGenome_mitochondrion);
        return true;
    }

    return false;
}


DISCREPANCY_AUTOFIX(MITOCHONDRION_REQUIRED)
{
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    CBioseq_EditHandle seq_h = context.GetBioseqHandle(*seq);
    CSeqdesc_CI biosrc(seq_h, CSeqdesc::e_Source);
    if (biosrc) {
        CSeqdesc& edit_biosrc = const_cast<CSeqdesc&>(*biosrc);
        edit_biosrc.SetSource().SetGenome(CBioSource::eGenome_mitochondrion);
        obj->SetFixed();
        return CRef<CAutofixReport>(new CAutofixReport("MITOCHONDRION_REQUIRED: Genome was set to mitochondrion for [n] bioseq[s]", 1));
    }
    return CRef<CAutofixReport>();
}



// SEQ_SHORTER_THAN_50bp

static bool IsMolProd(int biomol) { return biomol == CMolInfo::eBiomol_mRNA || biomol == CMolInfo::eBiomol_ncRNA || biomol == CMolInfo::eBiomol_rRNA || biomol == CMolInfo::eBiomol_pre_RNA || biomol == CMolInfo::eBiomol_tRNA; }

DISCREPANCY_CASE(SEQ_SHORTER_THAN_50bp, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "Find Short Sequences")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa() && bioseq.IsSetLength() && bioseq.GetLength() < 50) {
        if (context.InGenProdSet() && bioseq.IsSetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == objects::CSeq_inst::eMol_rna) {
            const CSeqdesc* molinfo = context.GetMolinfo();
            if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && IsMolProd(molinfo->GetMolinfo().GetBiomol())) {
                return;
            }
        }
        m_Objs["[n] sequence[s] [is] shorter than 50 nt"].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(SEQ_SHORTER_THAN_50bp)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}




// SEQ_SHORTER_THAN_200bp

DISCREPANCY_CASE(SEQ_SHORTER_THAN_200bp, SEQUENCE, eDisc | eSubmitter | eSmart | eBig | eTSA, "Short Contig")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa() && bioseq.IsSetLength() && bioseq.GetLength() < 200) {
        if (context.InGenProdSet() && bioseq.IsSetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == objects::CSeq_inst::eMol_rna) {
            const CSeqdesc* molinfo = context.GetMolinfo();
            if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && IsMolProd(molinfo->GetMolinfo().GetBiomol())) {
                return;
            }
        }
        CDiscrepancyContext::EFixType fix = CDiscrepancyContext::eFixParent;
        if (bioseq.IsSetAnnot()) {
            for (auto& annot_it : bioseq.GetAnnot()) {
                if (annot_it->IsFtable()) {
                    fix = CDiscrepancyContext::eFixNone;
                }
            }
        }
        m_Objs["[n] contig[s] [is] shorter than 200 nt"].Add(*context.BioseqObjRef(fix));
    }
}


DISCREPANCY_SUMMARIZE(SEQ_SHORTER_THAN_200bp)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


DISCREPANCY_AUTOFIX(SEQ_SHORTER_THAN_200bp)
{
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    CBioseq_EditHandle bioseq_edit(context.GetBioseqHandle(*seq));
    bioseq_edit.Remove();
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("SEQ_SHORTER_THAN_200bp: [n] short bioseq[s] [is] removed", 1));
}


// RNA_PROVIRAL

DISCREPANCY_CASE(RNA_PROVIRAL, SEQUENCE, eOncaller, "RNA bioseqs are proviral")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
        const CSeqdesc* biosrc = context.GetBiosource();
        if (biosrc && biosrc->GetSource().IsSetOrg() && biosrc->GetSource().IsSetGenome() && biosrc->GetSource().GetGenome() == CBioSource::eGenome_proviral) {
            m_Objs["[n] RNA bioseq[s] [is] proviral"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(RNA_PROVIRAL)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SMALL_GENOME_SET_PROBLEM

typedef bool (CBioSource::*FnIsSet)() const;
typedef const string& (CBioSource::*FnGet)() const;

static bool CompareOrGetString(const CBioSource& bio_src, FnIsSet is_set_fn, FnGet get_fn, string& val)
{
    bool ret = true;
    if ((bio_src.*is_set_fn)()) {
        if (val.empty()) {
            val = (bio_src.*get_fn)();
        }
        else if (val != (bio_src.*get_fn)()) {
            ret = false;
        }
    }
    return ret;
}


static bool CompareOrgModValue(const CBioSource& bio_src, COrgMod::TSubtype subtype, string& val)
{
    bool ret = true;
    if (bio_src.IsSetOrgMod()) {
        for (auto& mod : bio_src.GetOrgname().GetMod()) {
            if (mod->IsSetSubtype() && mod->GetSubtype() == subtype && mod->IsSetSubname()) {
                if (val.empty()) {
                    val = mod->GetSubname();
                }
                else {
                    if (mod->GetSubname() != val) {
                        ret = false;
                    }
                }
                break;
            }
        }
    }
    return ret;
}


static bool IsSegmentSubtype(const CBioSource& bio_src)
{
    bool ret = false;
    if (bio_src.IsSetSubtype()) {
        for (auto& subtype : bio_src.GetSubtype()) {
            if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_segment) {
                ret = true;
                break;
            }
        }
    }
    return ret;
}


DISCREPANCY_CASE(SMALL_GENOME_SET_PROBLEM, SEQ_SET, eOncaller, "Problems with small genome sets")
{
    const CBioseq_set& set = context.CurrentBioseq_set();
    if (set.IsSetClass() && set.GetClass() == CBioseq_set::eClass_small_genome_set) {
        string taxname, isolate, strain;
        bool all_taxname_same = true, all_isolate_same = true, all_strain_same = true;
        for (auto& descr_bio_src : context.GetSetBiosources()) {
            const CBioSource& bio_src = descr_bio_src->GetSource();
            if (context.HasLineage(bio_src, "", "Viruses")) {
                if (!IsSegmentSubtype(bio_src)) {
                    m_Objs["[n] biosource[s] should have segment qualifier but [does] not"].Add(*context.SeqdescObjRef(*descr_bio_src));
                }
            }
            if (all_taxname_same) {
                all_taxname_same = CompareOrGetString(bio_src, &CBioSource::IsSetTaxname, &CBioSource::GetTaxname, taxname);
            }
            if (all_isolate_same) {
                all_isolate_same = CompareOrgModValue(bio_src, COrgMod::eSubtype_isolate, isolate);
            }
            if (all_strain_same) {
                all_strain_same = CompareOrgModValue(bio_src, COrgMod::eSubtype_strain, strain);
            }
        }
        if (!all_taxname_same) {
            m_Objs["Not all biosources have same taxname"];
        }
        if (!all_isolate_same) {
            m_Objs["Not all biosources have same isolate"];
        }
        if (!all_strain_same) {
            m_Objs["Not all biosources have same strain"];
        }
    }
}


DISCREPANCY_SUMMARIZE(SMALL_GENOME_SET_PROBLEM)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// UNWANTED_SET_WRAPPER

static bool IsMicroSatellite(const CSeq_feat& feat)
{
    if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_repeat_region) {
        if (feat.IsSetQual()) {
            for (auto& qual : feat.GetQual()) {
                if (qual->IsSetQual() && qual->IsSetVal() && NStr::EqualNocase("satellite", qual->GetQual()) && NStr::StartsWith(qual->GetVal(), "microsatellite", NStr::eNocase)) {
                    return true;
                }
            }
        }
    }
    return false;
}


DISCREPANCY_CASE(UNWANTED_SET_WRAPPER, FEAT, eOncaller, "Set wrapper on microsatellites or rearranged genes")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && biosrc->GetSource().IsSetSubtype()) {
        for (auto& subtype : biosrc->GetSource().GetSubtype()) {
            if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_rearranged) {
                context.PropagateFlags(CDiscrepancyContext::eHasRearranged);
                break;
            }
        }
    }
    for (auto& feat : context.GetFeat()) {
        if (IsMicroSatellite(feat)) {
            context.PropagateFlags(CDiscrepancyContext::eHasSatFeat);
        }
        else {
            context.PropagateFlags(CDiscrepancyContext::eHasNonSatFeat);
        }
    }
    if (!context.IsBioseq()) {
        const CBioseq_set& set = context.CurrentBioseq_set();
        if (set.IsSetClass()) {
            CBioseq_set::EClass bio_set_class = set.GetClass();
            if (bio_set_class == CBioseq_set::eClass_eco_set || bio_set_class == CBioseq_set::eClass_mut_set || bio_set_class == CBioseq_set::eClass_phy_set || bio_set_class == CBioseq_set::eClass_pop_set) {
                unsigned char flags = context.ReadFlags();
                if ((flags & CDiscrepancyContext::eHasRearranged) || ((flags & CDiscrepancyContext::eHasSatFeat) && !(flags & CDiscrepancyContext::eHasNonSatFeat))) {
                    m_Objs["[n] unwanted set wrapper[s]"].Add(*context.BioseqSetObjRef());
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNWANTED_SET_WRAPPER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FLATFILE_FIND
struct SpellFixData
{
    const char* m_misspell;
    const char* m_correct;
    bool m_whole_word;
};

static SpellFixData kSpellFixes[] = {
    { "Agricultutral", "agricultural", false },
    { "Bacilllus", "Bacillus", false },
    { "Enviromental", "Environmental", false },
    { "Insitiute", "institute", false },
    { "Instutite", "institute", false },
    { "Instutute", "Institute", false },
    { "P.R.Chian", "P.R. China", false },
    { "PRChian", "PR China", false },
    { "Scieces", "Sciences", false },
    { "agricultral", "agricultural", false },
    { "agriculturral", "agricultural", false },
    { "biotechnlogy", "biotechnology", false },
    { "Biotechnlogy", "Biotechnology", false },
    { "biotechnolgy", "biotechnology", false },
    { "biotechology", "biotechnology", false },
    { "caputre", "capture", true },
    { "casette", "cassette", true },
    { "catalize", "catalyze", false },
    { "charaterization", "characterization", false },
    { "clonging", "cloning", false },
    { "consevered", "conserved", false },
    { "cotaining", "containing", false },
    { "cytochome", "cytochrome", true },
    { "diveristy", "diversity", true },
    { "enivronment", "environment", false },
    { "enviroment", "environment", false },
    { "genone", "genome", true },
    { "homologue", "homolog", true },
    { "hypotethical", "hypothetical", false },
    { "hypotetical", "hypothetical", false },
    { "hypothetcial", "hypothetical", false },
    { "hypothteical", "hypothetical", false },
    { "indepedent", "independent", false },
    { "insititute", "institute", false },
    { "insitute", "institute", false },
    { "institue", "institute", false },
    { "instute", "institute", false },
    { "muesum", "museum", true },
    { "musuem", "museum", true },
    { "nuclear shutting", "nuclear shuttling", true },
    { "phylogentic", "phylogenetic", false },
    { "protien", "protein", false },
    { "puatative", "putative", false },
    { "putaitve", "putative", false },
    { "putaive", "putative", false },
    { "putataive", "putative", false },
    { "putatitve", "putative", false },
    { "putatuve", "putative", false },
    { "putatvie", "putative", false },
    { "pylogeny", "phylogeny", false },
    { "resaerch", "research", false },
    { "reseach", "research", false },
    { "reserach", "research", true },
    { "reserch", "research", false },
    { "ribosoml", "ribosomal", false },
    { "ribossomal", "ribosomal", false },
    { "scencies", "sciences", false },
    { "scinece", "science", false },
    { "simmilar", "similar", false },
    { "structual", "structural", false },
    { "subitilus", "subtilis", false },
    { "sulfer", "sulfur", false },
    { "technlogy", "technology", false },
    { "technolgy", "technology", false },
    { "Technlogy", "Technology", false },
    { "Veterinry", "Veterinary", false },
    { "Argricultural", "Agricultural", false },
    { "transcirbed", "transcribed", false },
    { "transcirption", "transcription", true },
    { "uiniversity", "university", false },
    { "uinversity", "university", false },
    { "univercity", "university", false },
    { "univerisity", "university", false },
    { "univeristy", "university", false },
    { "univesity", "university", false },
    { "unversity", "university", true },
    { "uviversity", "university", false },
    { "anaemia", nullptr, false },
    { "haem", nullptr, false },
    { "haemagglutination", nullptr, false },
    { "heam", nullptr, false },
    { "mithocon", nullptr, false },
};

static const size_t kSpellFixesSize = sizeof(kSpellFixes) / sizeof(kSpellFixes[0]);
static const string kFixable = "Fixable";
static const string kNonFixable = "Non-fixable";


//static void FindFlatfileText(const unsigned char* p, bool *result)
//{
//#define _FSM_REPORT(x, y) result[x] = true, false
//#include "FLATFILE_FIND.inc"
//#undef _FSM_REPORT
//}


static void FindFlatfileText(const char* str, bool *result)
{
#define _FSM_EMIT static bool emit[]
#define _FSM_HITS static map<size_t, vector<size_t>> hits
#define _FSM_STATES static size_t states[]
#include "FLATFILE_FIND.inc"
#undef _FSM_EMIT
#undef _FSM_HITS
#undef _FSM_STATES
    CMultipatternSearch::Search(str, states, emit, hits, [result](size_t n){ result[n] = true; });
}


/// Checking that FLATFILE_FIND.inc is in sync with kSpellFixes
/// If the array is changed, need to regenerate FLATFILE_FIND.inc:
/// multipattern.exe -i FLATFILE_FIND.txt > FLATFILE_FIND.inc
void UnitTest_FLATFILE_FIND()
{
    bool Found[kSpellFixesSize];
    string error = "String not found: ";
    for (size_t i = 0; i < kSpellFixesSize; i++) {
        fill(Found, Found + kSpellFixesSize, 0);
        FindFlatfileText(kSpellFixes[i].m_misspell, Found);
        if (!Found[i]) {
            error += kSpellFixes[i].m_misspell;
            NCBI_THROW(CException, eUnknown, error);
        }
    }
}


DISCREPANCY_CASE(FLATFILE_FIND, SEQUENCE, eOncaller, "Flatfile representation of object contains suspect text")
{
    bool Found[kSpellFixesSize];
    for (auto& desc : context.GetAllSeqdesc()) {
        fill(Found, Found + kSpellFixesSize, 0);
        for (CStdTypeConstIterator<string> it(desc); it; ++it) {
            FindFlatfileText(it->c_str(), Found);
        }
        for (size_t i = 0; i < kSpellFixesSize; i++) {
            if (Found[i]) {
                string subitem = string("[n] object[s] contain[S] ") + kSpellFixes[i].m_misspell;
                bool autofix = kSpellFixes[i].m_correct != nullptr;
                const string& fixable = (autofix ? kFixable : kNonFixable);
                m_Objs[fixable][subitem].Add(*context.SeqdescObjRef(desc, &desc));
            }
        }
    }
    for (auto& feat: context.FeatAll()) {
        fill(Found, Found + kSpellFixesSize, 0);
        for (CStdTypeConstIterator<string> it(*feat); it; ++it) {
            FindFlatfileText(it->c_str(), Found);
        }
        for (size_t i = 0; i < kSpellFixesSize; i++) {
            if (Found[i]) {
                string subitem = string("[n] object[s] contain[S] ") + kSpellFixes[i].m_misspell;
                bool autofix = kSpellFixes[i].m_correct != nullptr;
                const string& fixable = (autofix ? kFixable : kNonFixable);
                m_Objs[fixable][subitem].Add(*context.SeqFeatObjRef(*feat, feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FLATFILE_FIND)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


static bool FixTextInObject(CSerialObject* obj, size_t misspell_idx)
{
    bool ret = false;
    const SpellFixData& fix_data = kSpellFixes[misspell_idx];
    for (CStdTypeIterator<string> it(*obj); it; ++it) {
        if (NStr::Find(*it, fix_data.m_misspell) != NPOS) {
            NStr::ReplaceInPlace(*it, fix_data.m_misspell, fix_data.m_correct, 0, -1);
            ret = true;
        }
    }
    return ret;
}


DISCREPANCY_AUTOFIX(FLATFILE_FIND)
{
    unsigned int n = 0;
    bool Found[kSpellFixesSize];
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    fill(Found, Found + kSpellFixesSize, 0);
    if (feat) {
        for (CStdTypeConstIterator<string> it(*feat); it; ++it) {
            FindFlatfileText(it->c_str(), Found);
        }
        for (size_t i = 0; i < kSpellFixesSize; i++) {
            if (Found[i]) {
                if (FixTextInObject(const_cast<CSeq_feat*>(feat), i)) {
                    ++n;
                }
            }
        }
    }
    if (desc) {
        for (CStdTypeConstIterator<string> it(*desc); it; ++it) {
            FindFlatfileText(it->c_str(), Found);
        }
        for (size_t i = 0; i < kSpellFixesSize; i++) {
            if (Found[i]) {
                if (FixTextInObject(const_cast<CSeqdesc*>(desc), i)) {
                    ++n;
                }
            }
        }
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("FLATFILE_FIND: [n] suspect text[s] [is] fixed", n));
}


DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER);
DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER_UNFIXABLE);
DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER_FIXABLE);


// ALL_SEQS_SHORTER_THAN_20kb

static const size_t MIN_SEQUENCE_LEN = 20000;


DISCREPANCY_CASE(ALL_SEQS_SHORTER_THAN_20kb, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "Short sequences test")
{
    if (context.CurrentBioseqSummary().Len > MIN_SEQUENCE_LEN) {
        m_Objs[kEmptyStr];
    }
}


DISCREPANCY_SUMMARIZE(ALL_SEQS_SHORTER_THAN_20kb)
{
    if (m_Objs.GetMap().find(kEmptyStr) == m_Objs.GetMap().end()) {
        // no sequences longer than 20000 nt
        m_Objs["No sequences longer than 20,000 nt found"];
    }
    else {
        m_Objs.GetMap().erase(kEmptyStr);
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}



DISCREPANCY_CASE(ALL_SEQS_CIRCULAR, SEQUENCE, eDisc | eSubmitter | eSmart, "All sequences circular")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        if (m_Objs["N"].GetCount()) {
            return;
        }
        if (bioseq.GetInst().CanGetTopology() && bioseq.GetInst().GetTopology() == CSeq_inst::eTopology_circular) {
            const CSeqdesc* biosrc = context.GetBiosource();
            if (biosrc && biosrc->GetSource().IsSetGenome() && (biosrc->GetSource().GetGenome() == CBioSource::eGenome_plasmid || biosrc->GetSource().GetGenome() == CBioSource::eGenome_chromosome)) {
                return;
            }
            m_Objs["C"].Incr();
            if (!m_Objs["F"].GetCount()) {
                if (bioseq.IsSetId()) {
                    for (auto id : bioseq.GetId()) {
                        const CTextseq_id* txt = id->GetTextseq_Id();
                        if (txt && txt->IsSetAccession()) {
                            CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession(txt->GetAccession());
                            if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs) {
                                m_Objs["F"].Incr();
                                return;
                            }
                        }
                    }
                }
                if (bioseq.IsSetDescr() && bioseq.GetDescr().IsSet()) {
                    for (auto descr: bioseq.GetDescr().Get()) {
                        if (descr->IsMolinfo() && descr->GetMolinfo().CanGetTech()) {
                            if (descr->GetMolinfo().GetTech() == CMolInfo::eTech_wgs || descr->GetMolinfo().GetTech() == CMolInfo::eTech_tsa || descr->GetMolinfo().GetTech() == CMolInfo::eTech_targeted) {
                                m_Objs["F"].Incr();
                                return;
                            }
                        }
                    }
                }
            }
        }
        else {
            m_Objs["N"].Incr();
        }
    }
}


DISCREPANCY_SUMMARIZE(ALL_SEQS_CIRCULAR)
{
    CReportNode rep;
    if (m_Objs["C"].GetCount() && !m_Objs["N"].GetCount()) {
        rep["All ([n]) sequences are circular"].Severity(m_Objs["F"].GetCount() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning).SetCount(m_Objs["C"].GetCount());
        m_ReportItems = rep.Export(*this, false)->GetSubitems();
    }
}


// SUSPICIOUS_SEQUENCE_ID

static bool SuspiciousId(const string& s)
{
    static CRegexp regexp("chromosome|plasmid|mito|chloroplast|apicoplast|plastid|^chr|^lg|\\bNW_|\\bNZ_|\\bNM_|\\bNC_|\\bAC_|CP\\d\\d\\d\\d\\d\\d", CRegexp::fCompile_ignore_case);
    return regexp.IsMatch(s);
}

DISCREPANCY_CASE(SUSPICIOUS_SEQUENCE_ID, SEQUENCE, eOncaller | eSubmitter | eSmart | eBig, "Suspicious sequence identifiers")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetId()) {
        bool report = false;
        for (auto& id : bioseq.GetId()) {
            if (id->IsLocal()) {
                if (id->GetLocal().IsStr() && SuspiciousId(id->GetLocal().GetStr())) {
                    report = true;
                    break;
                }
            }
            else if (id->IsGeneral()) {
                if (id->GetGeneral().IsSetDb() && SuspiciousId(id->GetGeneral().GetDb())) {
                    report = true;
                    break;
                }
                if (id->GetGeneral().IsSetTag() && id->GetGeneral().GetTag().IsStr() && SuspiciousId(id->GetGeneral().GetTag().GetStr())) {
                    report = true;
                    break;
                }
            }
        }
        if (report) {
            m_Objs["[n] sequence[s] [has] suspicious identifiers"].Add(*context.BioseqSetObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(SUSPICIOUS_SEQUENCE_ID)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// CHROMOSOME_PRESENT

static const CSubSource::ESubtype eSubtype_unknown = static_cast<CSubSource::ESubtype>(0);

static bool s_areCompatible(CBioSource::EGenome Location, CSubSource::ESubtype Qualifier)
{
    if (Qualifier == CSubSource::eSubtype_plasmid_name) {
        return true; // always OK by this test; might be handled elsewhere
    }

    switch (Location)
    {
    case CBioSource::eGenome_chromosome:
        return false;
    case CBioSource::eGenome_unknown: // not present
    case CBioSource::eGenome_genomic:
        switch (Qualifier)
        {
        case CSubSource::eSubtype_chromosome:
        case CSubSource::eSubtype_linkage_group:
            return false;
        case eSubtype_unknown: // not present
        default:
            return true;
        }
    case CBioSource::eGenome_plasmid: // always OK by this test; might be handled elsewhere
    default:
        return true;
    }

}

DISCREPANCY_CASE(CHROMOSOME_PRESENT, SEQ_SET, eSubmitter | eSmart | eFatal, "Chromosome present")
{
    const CBioseq_set& set = context.CurrentBioseq_set();
    if (set.IsSetSeq_set() && set.IsSetClass() && set.GetClass() == CBioseq_set::eClass_genbank) {
        for (const auto& se : set.GetSeq_set()) {
            if (!se->IsSetDescr()) {
                continue;
            }

            for (const auto& descr : se->GetDescr().Get()) {
                if (!descr->IsSource()) {
                    continue;
                }
                const CBioSource& bio_src = descr->GetSource();

                CBioSource::EGenome Location = CBioSource::eGenome_unknown;
                if (bio_src.IsSetGenome()) {
                    Location = static_cast<CBioSource::EGenome>(bio_src.GetGenome());
                }
                // shortcut
                if (Location == CBioSource::eGenome_plasmid) {
                    continue; // always OK by this test; might be handled elsewhere
                }

                if (bio_src.IsSetSubtype()) {
                    for (const auto& subtype : bio_src.GetSubtype()) {
                        CSubSource::ESubtype Qualifier = eSubtype_unknown;
                        if (subtype->IsSetSubtype()) {
                            Qualifier = static_cast<CSubSource::ESubtype>(subtype->GetSubtype());
                        }
                        if (!s_areCompatible(Location, Qualifier)) {
                            m_Objs["[n] chromosome[s] [is] present"].Add(*context.BioseqSetObjRef()).Fatal();
                        }
                    }
                } else {
                    if (!s_areCompatible(Location, eSubtype_unknown)) {
                        m_Objs["[n] chromosome[s] [is] present"].Add(*context.BioseqSetObjRef()).Fatal();
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CHROMOSOME_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
