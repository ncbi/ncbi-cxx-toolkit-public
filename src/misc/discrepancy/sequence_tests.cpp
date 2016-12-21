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
#include <objects/general/Person_id.hpp>

#include "discrepancy_core.hpp"
#include "utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(sequence_tests);


// MISSING_GENOMEASSEMBLY_COMMENTS

const string kMissingGenomeAssemblyComments = "[n] bioseq[s] [is] missing GenomeAssembly structured comments";

DISCREPANCY_CASE(MISSING_GENOMEASSEMBLY_COMMENTS, CSeq_inst, eDisc | eSmart, "Bioseqs should have GenomeAssembly structured comments")
{
    if (obj.IsAa()) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) {
        return;
    }
    CBioseq_Handle b = context.GetScope().GetBioseqHandle(*seq);

    CSeqdesc_CI d(b, CSeqdesc::e_User);
    bool found = false;
    while (d && !found) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_StructuredComment &&
            NStr::Equal(CComment_rule::GetStructuredCommentPrefix(d->GetUser()), "Genome-Assembly-Data")) {
            found = true;
        }
        ++d;
    }

    if (!found) {
        m_Objs[kMissingGenomeAssemblyComments].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(MISSING_GENOMEASSEMBLY_COMMENTS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUP_DEFLINE

const string kUniqueDeflines = "[n] definition line[s] [is] unique";
const string kIdenticalDeflines = "[n] definition line[s] [is] identical";
const string kAllUniqueDeflines = "All deflines are unique";
const string kSomeIdenticalDeflines = "Defline Problem Report";


DISCREPANCY_CASE(DUP_DEFLINE, CSeq_inst, eOncaller, "Definition lines should be unique")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq || !seq->IsSetDescr() || obj.IsAa()) {
        return;
    }
    ITERATE (CBioseq::TDescr::Tdata, it, seq->GetDescr().Get()) {
        if ((*it)->IsTitle()) {
            m_Objs[(*it)->GetTitle()].Add(*context.NewSeqdescObj(*it, context.GetCurrentBioseqLabel()), false);
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
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, m_Objs.GetMap()) {
        TReportObjectList& list = it->second->GetObjects();
        if (list.size() == 1) {
            tmp[kSomeIdenticalDeflines][kUniqueDeflines].Add(list);
        }
        else if (list.size() > 1) {
            tmp[kSomeIdenticalDeflines][kIdenticalDeflines + "[*" + it->first + "*]"].Add(list);
            all_unique = false;
        }
    }
    if (all_unique) {
        tmp.clear();
        tmp[kAllUniqueDeflines];
    }
    m_ReportItems = tmp.Export(*this)->GetSubitems();
}


// TERMINAL_NS

const string kTerminalNs = "[n] sequence[s] [has] terminal Ns";

DISCREPANCY_CASE(TERMINAL_NS, CSeq_inst, eDisc | eSubmitter | eSmart, "Ns at end of sequences")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq || seq->IsAa()) {
        return;
    }

    bool has_terminal_n_or_gap = false;

    CRef<CSeq_loc> start(new CSeq_loc());
    start->SetInt().SetId().Assign(*(seq->GetId().front()));
    start->SetInt().SetFrom(0);
    start->SetInt().SetTo(0);

    CSeqVector start_vec(*start, context.GetScope(), CBioseq_Handle::eCoding_Iupac);
    CSeqVector::const_iterator vi_start = start_vec.begin();

    if (*vi_start == 'N' || vi_start.IsInGap()) {
        has_terminal_n_or_gap = true;
    } else {
        CRef<CSeq_loc> stop(new CSeq_loc());
        stop->SetInt().SetId().Assign(*(seq->GetId().front()));
        stop->SetInt().SetFrom(seq->GetLength() - 1);
        stop->SetInt().SetTo(seq->GetLength() - 1);

        CSeqVector stop_vec(*stop, context.GetScope(), CBioseq_Handle::eCoding_Iupac);
        CSeqVector::const_iterator vi_stop = stop_vec.begin();
        if (*vi_stop == 'N' || vi_stop.IsInGap()) {
            has_terminal_n_or_gap = true;
        }
    }

    if (has_terminal_n_or_gap) {
        m_Objs[kTerminalNs].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(TERMINAL_NS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SHORT_PROT_SEQUENCES
const string kShortProtSeqs = "[n] protein sequences are shorter than 50 aa.";

DISCREPANCY_CASE(SHORT_PROT_SEQUENCES, CSeq_inst, eDisc | eOncaller | eSmart, "Protein sequences should be at least 50 aa, unless they are partial")
{
    if (!obj.IsAa() || !obj.IsSetLength() || obj.GetLength() >= 50) {
        return;
    }

    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) {
        return;
    }

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    if (!bsh) {
        return;
    }
    CSeqdesc_CI mi(bsh, CSeqdesc::e_Molinfo);
    if (mi && mi->GetMolinfo().IsSetCompleteness() &&
        mi->GetMolinfo().GetCompleteness() != CMolInfo::eCompleteness_unknown &&
        mi->GetMolinfo().GetCompleteness() != CMolInfo::eCompleteness_complete) {
        return;
    }

    m_Objs[kShortProtSeqs].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false);
}


DISCREPANCY_SUMMARIZE(SHORT_PROT_SEQUENCES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COMMENT_PRESENT

DISCREPANCY_CASE(COMMENT_PRESENT, CSeqdesc, eOncaller, "Comment descriptor present")
{
    if (obj.IsComment()) {
        //CRef<CDiscrepancyObject> this_disc_obj(context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseq(), eKeepRef));
        m_Objs["comment"][obj.GetComment()].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), eKeepRef), false);
    }
}


DISCREPANCY_SUMMARIZE(COMMENT_PRESENT)
{
    if (m_Objs.empty()) {
        return;
    }

    bool all_same = false;
    if (m_Objs["comment"].GetMap().size() == 1) {
        all_same = true;
    }
    string label = all_same ? "[n] comment descriptor[s] were found (all same)" : "[n] comment descriptor[s] were found (some different)";

    CReportNode::TNodeMap::iterator it = m_Objs["comment"].GetMap().begin();
    while (it != m_Objs["comment"].GetMap().end()) {
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["comment"][it->first].GetObjects())
        {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeqdesc> comment_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));
            m_Objs[label].Add(*context.NewSeqdescObj(comment_desc, kEmptyStr), false);
        }
        ++it;
    }
    m_Objs.GetMap().erase("comment");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MRNA_ON_WRONG_SEQUENCE_TYPE

const string kmRNAOnWrongSequenceType = "[n] mRNA[s] [is] located on eukaryotic sequence[s] that [does] not have genomic or plasmid source[s]";

DISCREPANCY_CASE(MRNA_ON_WRONG_SEQUENCE_TYPE, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features")
{
    if (!obj.IsSetData() || obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_mRNA) {
        return;
    }
    if (!context.IsEukaryotic()) {
        return;
    }
    if (!context.GetCurrentBioseq() || !context.GetCurrentBioseq()->IsSetInst() ||
        !context.GetCurrentBioseq()->GetInst().IsSetMol() ||
        context.GetCurrentBioseq()->GetInst().GetMol() != CSeq_inst::eMol_dna) {
        return;
    }
    if (!context.GetCurrentMolInfo() || !context.GetCurrentMolInfo()->IsSetBiomol() ||
        context.GetCurrentMolInfo()->GetBiomol() != CMolInfo::eBiomol_genomic) {
        return;
    }
    const CBioSource* src = context.GetCurrentBiosource();
    if (!src || !src->IsSetGenome() ||
        src->GetGenome() == CBioSource::eGenome_macronuclear ||
        src->GetGenome() == CBioSource::eGenome_unknown || 
        src->GetGenome() == CBioSource::eGenome_genomic ||
        src->GetGenome() == CBioSource::eGenome_chromosome) {
        return;
    }
    m_Objs[kmRNAOnWrongSequenceType].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);

}


DISCREPANCY_SUMMARIZE(MRNA_ON_WRONG_SEQUENCE_TYPE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DISC_GAPS

const string kSequencesWithGaps = "[n] sequence[s] contain[S] gaps";

DISCREPANCY_CASE(GAPS, CSeq_inst, eDisc | eSubmitter | eSmart, "Sequences with gaps")
{
    if (obj.IsSetRepr() && obj.GetRepr() == CSeq_inst::eRepr_delta) {

        const CSeqSummary& sum = context.GetSeqSummary();
        bool has_gaps = !!sum.Gaps;

        if (!has_gaps) {
            CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
            if (!bioseq || !bioseq->IsSetAnnot()) {
                return;
            }

            const CSeq_annot* annot = nullptr;
            ITERATE (CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
                if ((*annot_it)->IsFtable()) {
                    annot = *annot_it;
                    break;
                }
            }

            if (annot) {
                ITERATE (CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {
                    if ((*feat)->IsSetData() && (*feat)->GetData().GetSubtype() == CSeqFeatData::eSubtype_gap) {
                        has_gaps = true;
                        break;
                    }
                }
            }
        }

        if (has_gaps) {
            m_Objs[kSequencesWithGaps].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(GAPS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ONCALLER_BIOPROJECT_ID

const string kSequencesWithBioProjectIDs = "[n] sequence[s] contain[S] BioProject IDs";

DISCREPANCY_CASE(BIOPROJECT_ID, CSeq_inst, eOncaller, "Sequences with BioProject IDs")
{
    CBioseq_Handle bioseq = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    CSeqdesc_CI user_desc_it(bioseq, CSeqdesc::E_Choice::e_User);
    for (; user_desc_it; ++user_desc_it) {
        const CUser_object& user = user_desc_it->GetUser();
        if (user.IsSetData() && user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "DBLink") {
            ITERATE (CUser_object::TData, user_field, user.GetData()) {
                if ((*user_field)->IsSetLabel() && (*user_field)->GetLabel().IsStr() && (*user_field)->GetLabel().GetStr() == "BioProject" &&
                    (*user_field)->IsSetData() && (*user_field)->GetData().IsStrs()) {
                    const CUser_field::C_Data::TStrs& strs = (*user_field)->GetData().GetStrs();
                    if (!strs.empty() && !strs[0].empty()) {
                        m_Objs[kSequencesWithBioProjectIDs].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
                        return;
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

const string kMissingDeflines = "[n] bioseq[s] [has] no definition line";

DISCREPANCY_CASE(MISSING_DEFLINES, CSeq_inst, eOncaller, "Missing definition lines")
{
    if (obj.IsAa()) {
        return;
    }

    bool has_title = false;
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (seq && seq->IsSetDescr()) {
        ITERATE (CBioseq::TDescr::Tdata, d, seq->GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                has_title = true;
                break;
            }
        }
    }
    if (!has_title) {
        m_Objs[kMissingDeflines].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(MISSING_DEFLINES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// N_RUNS_14

const string kMoreThan14NRuns = "[n] sequence[s] [has] runs of 15 or more Ns";
const TSeqPos MIN_BAD_RUN_LEN = 15;

DISCREPANCY_CASE(N_RUNS_14, CSeq_inst, eDisc, "Runs of more than 14 Ns")
{
    if (obj.IsNa()) {
        if (obj.IsSetSeq_data()) {
            vector<CRange<TSeqPos> > runs;
            FindNRuns(runs, obj.GetSeq_data(), obj.GetLength(), 0, MIN_BAD_RUN_LEN);
            if (!runs.empty()) {
                m_Objs[kMoreThan14NRuns].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
            }
        }
        else if (obj.IsSetExt() && obj.GetExt().IsDelta()) {
            const CSeq_ext::TDelta& deltas = obj.GetExt().GetDelta();
            if (deltas.IsSet()) {
                ITERATE (CDelta_ext::Tdata, delta, deltas.Get()) {
                    if ((*delta)->IsLiteral() && (*delta)->GetLiteral().IsSetSeq_data()) {
                        if ((*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_Gap || (*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_not_set) {
                            continue;
                        }
                        vector<CRange<TSeqPos> > runs;
                        FindNRuns(runs, (*delta)->GetLiteral().GetSeq_data(), 0, (*delta)->GetLiteral().GetLength(), MIN_BAD_RUN_LEN);
                        if (!runs.empty()) {
                            m_Objs[kMoreThan14NRuns].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
                            break;
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(N_RUNS_14)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// 10_PERCENTN

const string kMoreThan10PercentsN = "[n] sequence[s] [has] > 10%% Ns";
const double MIN_N_PERCENTAGE = 10.0;

DISCREPANCY_CASE(10_PERCENTN, CSeq_inst, eDisc | eSubmitter | eSmart, "Greater than 10 percent Ns")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    const CSeqSummary& sum = context.GetSeqSummary();
    if (sum.N * 100. / sum.Len > MIN_N_PERCENTAGE) {
        m_Objs[kMoreThan10PercentsN].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
}


DISCREPANCY_SUMMARIZE(10_PERCENTN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_COUNT

DISCREPANCY_CASE(FEATURE_COUNT, COverlappingFeatures, eOncaller | eSubmitter | eSmart, "Count features present or missing from sequences")
{
    string type = context.GetCurrentBioseq()->IsNa() ? "n" : "a";
    CRef<CReportObject> rep(context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    if (context.IsGui()) {
        m_Objs[type][kEmptyCStr].Add(*rep);
    }
    ITERATE (vector<CConstRef<CSeq_feat> >, feat, context.FeatAll()) {
        if ((*feat)->GetData().GetSubtype() == CSeqFeatData::eSubtype_prot) {
            continue;
        }
        string key = (*feat)->GetData().IsGene() ? "gene" : (*feat)->GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
        m_Objs[type][key].Add(*rep, false);
        m_Objs[kEmptyCStr][key];
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_COUNT)
{
    CReportNode out;
    CReportNode& aa = m_Objs["a"];
    CReportNode& na = m_Objs["n"];
    ITERATE (CReportNode::TNodeMap, it, m_Objs[kEmptyCStr].GetMap()) {
        const string& key = it->first;
        string label = key + ": " + NStr::NumericToString(aa[key].GetObjects().size() + na[key].GetObjects().size()) + " present";
        out[label];
        if (context.IsGui()) {
            if (na[key].GetObjects().size()) {
                map<CReportObj*, size_t> obj2num;
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["n"][kEmptyStr].GetObjects()) {
                    obj2num[&**obj] = 0;
                }
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["n"][key].GetObjects()) {
                    obj2num[&**obj]++;
                }
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["n"][kEmptyStr].GetObjects()) {
                    string str = "[n] bioseq[s] [has] " + NStr::NumericToString(obj2num[&**obj]) + " " + key + " features";
                    out[label][str].Add(**obj);
                }
            }
            if (aa[key].GetObjects().size()) {
                map<CReportObj*, size_t> obj2num;
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["a"][kEmptyStr].GetObjects()) {
                    obj2num[&**obj] = 0;
                }
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["a"][key].GetObjects()) {
                    obj2num[&**obj]++;
                }
                NON_CONST_ITERATE (vector<CRef<CReportObj> >, obj, m_Objs["a"][kEmptyStr].GetObjects()) {
                    string str = "[n] bioseq[s] [has] " + NStr::NumericToString(obj2num[&**obj]) + " " + key + " features";
                    out[label][str].Add(**obj);
                }
            }
        }
    }
    m_Objs.clear();
    m_ReportItems = out.Export(*this)->GetSubitems();
}


// EXON_ON_MRNA

const string kExonOnMrna = "[n] mRNA bioseq[s] [has] exon features";
const string kExons = "exon";
const string kExonBioseq = "exon bioseq";

void SummarizeExonCount(CReportNode& m_Objs)
{
    if (!m_Objs[kExons].GetObjects().empty()) {
        CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(m_Objs[kExonBioseq].GetObjects().back().GetNCPointer()));
        m_Objs[kExonOnMrna].Add(*seq_disc_obj, false);
        m_Objs[kExons].clear();
        m_Objs[kExonBioseq].clear();
    }
}


DISCREPANCY_CASE(EXON_ON_MRNA, CSeq_feat_BY_BIOSEQ, eOncaller | eSmart, "mRNA sequences should not have exons")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        SummarizeExonCount(m_Objs);
        CRef<CDiscrepancyObject> this_disc_obj(context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eKeepRef));
        m_Objs[kExonBioseq].Add(*this_disc_obj, false);
    }
    if (!context.IsCurrentSequenceMrna() || !obj.IsSetData() || obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_exon) {
        return;
    }
    m_Objs[kExons].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
}


DISCREPANCY_SUMMARIZE(EXON_ON_MRNA)
{
    SummarizeExonCount(m_Objs);
    m_Objs.GetMap().erase(kExons);
    m_Objs.GetMap().erase(kExonBioseq);
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_MOLINFO_TECH

static const string kMissingTech = "missing";

DISCREPANCY_CASE(INCONSISTENT_MOLINFO_TECH, CSeq_inst, eDisc | eSmart, "Inconsistent Molinfo Techniques")
{
    if (obj.IsAa()) {
        return;
    }

    CBioseq_Handle bioseq = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    
    for (CSeqdesc_CI molinfo_desc_it(bioseq, CSeqdesc::E_Choice::e_Molinfo); molinfo_desc_it; ++molinfo_desc_it) {
        const CMolInfo& mol_info = molinfo_desc_it->GetMolinfo();
        if (!mol_info.IsSetTech()) {
            m_Objs[kMissingTech].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
        }
        else {
            string tech = NStr::IntToString(mol_info.GetTech());
            m_Objs[tech].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
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

    CReportNode::TNodeMap::iterator missing_it = the_map.end();
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, the_map) {

        num_of_bioseqs += it->second->GetObjects().size();

        if (it->first == kMissingTech) {
            missing_it = it;
            num_of_missing += it->second->GetObjects().size();
            continue;
        }

        if (tech.empty()) {
            tech = it->first;
        }
        else if (tech != it->first) {
            same = false;
        }
    }

    string summary = kInconsistentMolinfoTechSummary + " (";
    summary += num_of_missing ? "some missing, " : "all present, ";
    summary += same ? "all same)" : "some different)";

    if (num_of_missing) {
        if (num_of_missing == num_of_bioseqs) {
            report[summary]["technique (all missing)"];
        }
        else {
            report[summary]["technique (some missing)"];
        }
    }

    if (missing_it != the_map.end()) {
        report[summary][kInconsistentMolinfoTech].Add(missing_it->second->GetObjects());
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

static const string kEndsWithSeq = "[n] defline[s] appear[S] to end with sequence characters";

DISCREPANCY_CASE(TITLE_ENDS_WITH_SEQUENCE, CSeqdesc, eDisc | eSubmitter | eSmart, "Sequence characters at end of defline")
{
    if (!obj.IsTitle()) {
        return;
    }
    if (EndsWithSequence(obj.GetTitle())) {
        m_Objs[kEndsWithSeq].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel()));
    }
}


DISCREPANCY_SUMMARIZE(TITLE_ENDS_WITH_SEQUENCE)
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_MOLTYPE_MISMATCH

static const string kMoltypeMismatch = "[n] sequence[s] [has] rRNA or misc_RNA features but [is] not genomic DNA";

DISCREPANCY_CASE(FEATURE_MOLTYPE_MISMATCH, CSeq_inst, eOncaller, "Sequences with rRNA or misc_RNA features should be genomic DNA")
{
    CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());

    bool is_genomic = false;
    const CMolInfo* molinfo = context.GetCurrentMolInfo();
    if (molinfo && molinfo->IsSetBiomol() && molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) {
        is_genomic = true;
    }
    bool is_dna = false;
    if (obj.IsSetMol() && obj.GetMol() == CSeq_inst::eMol_dna) {
        is_dna = true;
    }

    if (!is_genomic || !is_dna) {
        const CSeq_annot* annot = nullptr;
        CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
        if (bioseq->IsSetAnnot()) {
            ITERATE (CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
                if ((*annot_it)->IsFtable()) {
                    annot = *annot_it;
                    break;
                }
            }
        }

        if (annot) {
            ITERATE (CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {
                if ((*feat)->IsSetData()) {
                    CSeqFeatData::ESubtype subtype = (*feat)->GetData().GetSubtype();
                    if (subtype == CSeqFeatData::eSubtype_rRNA || subtype == CSeqFeatData::eSubtype_otherRNA) {
                        m_Objs[kMoltypeMismatch].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary(), eKeepRef, true));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_MOLTYPE_MISMATCH)
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool FixMoltype(const CBioseq& bioseq, CScope& scope)
{
    CBioseq_EditHandle edit_handle = scope.GetBioseqEditHandle(bioseq);
    edit_handle.SetInst_Mol(CSeq_inst::eMol_dna);

    CSeq_descr& descrs = edit_handle.SetDescr();
    CMolInfo* molinfo = nullptr;

    if (descrs.IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, descr, descrs.Set()) {
            if ((*descr)->IsMolinfo()) {
                molinfo = &((*descr)->SetMolinfo());
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
        return false;
    }

    molinfo->SetBiomol(CMolInfo::eBiomol_genomic);
    return true;
}

DISCREPANCY_AUTOFIX(FEATURE_MOLTYPE_MISMATCH)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        const CBioseq* bioseq = dynamic_cast<const CBioseq*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (bioseq && FixMoltype(*bioseq, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("FEATURE_MOLTYPE_MISMATCH: Moltype was set to genomic for [n] bioseq[s]", n) : 0);
}


// INCONSISTENT_DBLINK

const string kMissingDBLink = "[n] Bioseq [is] missing DBLink object";
const string kDBLinkObjectList = "DBLink Objects";
const string kDBLinkFieldCountTop = "DBLink Fields";
const string kDBLinkCollect = "DBLink Collection";

string GetFieldValueAsString(const CUser_field& field)
{
    string value = kEmptyStr;

    if (field.GetData().IsStr()) {
        value = field.GetData().GetStr();
    } else if (field.GetData().IsStrs()) {
        ITERATE (CUser_field::TData::TStrs, s, field.GetData().GetStrs()) {
            if (!NStr::IsBlank(value)) {
                value += "; ";
            }
            value += *s;
        }
    }
    return value;
}


const string& kPreviouslySeenFields = "Previously Seen Fields";
const string& kPreviouslySeenFieldsThis = "Previously Seen Fields This";
const string& kPreviouslySeenObjects = "Previously Seen Objects";

void AddUserObjectFieldItems
(CConstRef<CSeqdesc> d,
 CConstRef<CBioseq> seq,
 const CSeqSummary* info,
 CReportNode& collector,
 CReportNode& previously_seen,
 CDiscrepancyContext& context,
 const string& object_name,
 const string& field_prefix = kEmptyStr)
{
    if (!d) {
        // add missing for all previously seen fields
        NON_CONST_ITERATE (CReportNode::TNodeMap, obj, previously_seen[kPreviouslySeenFields].GetMap()) {
            ITERATE (CReportNode::TNodeMap, z, obj->second->GetMap()) {
                collector[field_prefix + z->first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z->first]
                    .Add(*context.NewBioseqObj(seq, info, eKeepRef), false);
            }
        }
        return;
    }
    
    bool already_seen = previously_seen[kPreviouslySeenObjects].Exist(object_name);
    ITERATE (CUser_object::TData, f, d->GetUser().GetData()) {
        if ((*f)->IsSetLabel() && (*f)->GetLabel().IsStr() && (*f)->IsSetData()) {
            string field_name = field_prefix + (*f)->GetLabel().GetStr();
            // add missing field to all previous objects that do not have this field
            if (already_seen && !collector.Exist(field_name)) {
                ITERATE (TReportObjectList, ro, previously_seen[kPreviouslySeenObjects][object_name].GetObjects()) {
                    string missing_label = "[n] " + object_name + "[s] [is] missing field " + field_name;
                    CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(ro->GetNCPointer()));
                    collector[field_name][missing_label].Add(*seq_disc_obj, false);
                }
            }
            collector[field_name]["[n] " + object_name + "[s] [has] field " + field_name + " value '" + GetFieldValueAsString(**f) + "'"].Add(*context.NewSeqdescObj(d, context.GetCurrentBioseqLabel()), false);
            previously_seen[kPreviouslySeenFieldsThis][(*f)->GetLabel().GetStr()].Add(*context.NewSeqdescObj(d, context.GetCurrentBioseqLabel()), false);
            previously_seen[kPreviouslySeenFields][object_name][(*f)->GetLabel().GetStr()].Add(*context.NewSeqdescObj(d, context.GetCurrentBioseqLabel()), false);
        }
    }

    // add missing for all previously seen fields not on this object
    ITERATE (CReportNode::TNodeMap, z, previously_seen[kPreviouslySeenFields][object_name].GetMap()) {
        if (!previously_seen[kPreviouslySeenFieldsThis].Exist(z->first)) {
            collector[field_prefix + z->first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z->first]
                .Add(*context.NewSeqdescObj(d, context.GetCurrentBioseqLabel()), false);
        }
    }

    // maintain object list for missing fields
    CRef<CDiscrepancyObject> this_disc_obj(context.NewSeqdescObj(d, context.GetCurrentBioseqLabel(), eKeepRef));
    previously_seen[kPreviouslySeenObjects][object_name].Add(*this_disc_obj, false);
    previously_seen[kPreviouslySeenFieldsThis].clear();
}


DISCREPANCY_CASE(INCONSISTENT_DBLINK, CSeq_inst, eDisc | eSubmitter | eSmart, "Inconsistent DBLink fields")
{
    if (obj.IsAa()) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d && d->GetUser().GetObjectType() != CUser_object::eObjectType_DBLink) {
        ++d;
    }
    if (!d) {
        m_Objs[kMissingDBLink].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary(), eKeepRef), false);
        // add missing for all previously seen fields
        AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), seq, &context.GetSeqSummary(), m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
    }
    while (d) {
        if (d->GetUser().GetObjectType() != CUser_object::eObjectType_DBLink) {
            ++d;
            continue;
        }
        CConstRef<CSeqdesc> dr(&(*d));
        AddUserObjectFieldItems(dr, seq, &context.GetSeqSummary(), m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
        ++d;
    }
}


void AnalyzeField(CReportNode& node, bool& all_present, bool& all_same)
{
    all_present = true;
    all_same = true;
    size_t num_values = 0;
    string value = kEmptyStr;
    bool first = true;
    ITERATE (CReportNode::TNodeMap, s, node.GetMap()) {
        if (NStr::Find(s->first, " missing field ") != string::npos) {
            all_present = false;
        } else {
            size_t pos = NStr::Find(s->first, " value '");
            if (pos != string::npos) {
                if (first) {
                    value = s->first.substr(pos);
                    num_values++;
                    first = false;
                } else if (!NStr::Equal(s->first.substr(pos), value)) {
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
    NON_CONST_ITERATE (CReportNode::TNodeMap, s, node.GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*(s->second), this_present, this_same);
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
    NON_CONST_ITERATE (CReportNode::TNodeMap, s, original.GetMap()){
        NON_CONST_ITERATE (TReportObjectList, q, s->second->GetObjects()) {
            new_home[s->first].Add(**q);
        }
    }
    NON_CONST_ITERATE (TReportObjectList, q, original.GetObjects()) {
        new_home.Add(**q);
    }
}


void AddSubFieldReport(CReportNode& node, const string& field_name, const string& top_label, CReportNode& m_Objs)
{
    bool this_present = true;
    bool this_same = true;
    AnalyzeField(node, this_present, this_same);
    string new_label = field_name + " " + GetSummaryLabel(this_present, this_same);
    NON_CONST_ITERATE (CReportNode::TNodeMap, s, node.GetMap()){
        NON_CONST_ITERATE (TReportObjectList, q, s->second->GetObjects()) {
            m_Objs[top_label][new_label][s->first].Add(**q);
        }
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
            CopyNode(m_Objs[top_label]["      " + it->first], *(it->second));
            it = m_Objs.GetMap().erase(it);
        } else {
            ++it;
        }
    }

    NON_CONST_ITERATE (CReportNode::TNodeMap, it, m_Objs[kDBLinkCollect].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*(it->second), this_present, this_same);
        string new_label = AdjustDBLinkFieldName(it->first) + " " + GetSummaryLabel(this_present, this_same);
        NON_CONST_ITERATE (CReportNode::TNodeMap, s, it->second->GetMap()){
            NON_CONST_ITERATE (TReportObjectList, q, s->second->GetObjects()) {
                m_Objs[top_label][new_label][s->first].Add(**q);
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

DISCREPANCY_CASE(INCONSISTENT_STRUCTURED_COMMENTS, CSeq_inst, eDisc | eSubmitter | eSmart, "Inconsistent structured comments")
{
    if (obj.IsAa()) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() != CUser_object::eObjectType_StructuredComment) {
            ++d;
            continue;
        }
        CConstRef<CSeqdesc> dr(&(*d));
        string prefix = CComment_rule::GetStructuredCommentPrefix(d->GetUser());
        if (NStr::IsBlank(prefix)) {
            prefix = "unnamed";
        }
        m_Objs[kStructuredCommentObservedPrefixesThis][prefix].Add(*context.NewSeqdescObj(dr, context.GetCurrentBioseqLabel(), eKeepRef), false);
        AddUserObjectFieldItems(dr, seq, &context.GetSeqSummary(), m_Objs[kStructuredCommentReport], m_Objs[kStructuredCommentPrevious], context, prefix + " structured comment", kStructuredCommentFieldPrefix);
        ++d;
    }

    //report prefixes seen previously, not found on this sequence
    ITERATE (CReportNode::TNodeMap, it, m_Objs[kStructuredCommentObservedPrefixes].GetMap()) {
        if (!m_Objs[kStructuredCommentObservedPrefixesThis].Exist(it->first)) {
            m_Objs["[n] Bioseq[s] [is] missing " + it->first + " structured comment"]
                .Add(*context.NewBioseqObj(seq, &context.GetSeqSummary(), eKeepRef), false);
            AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), seq, &context.GetSeqSummary(), m_Objs[kStructuredCommentReport],
                m_Objs[kStructuredCommentPrevious], context,
                it->first + " structured comment", kStructuredCommentFieldPrefix);
        }
    }

    // report prefixes found on this sequence but not on previous sequences
    ITERATE (CReportNode::TNodeMap, it, m_Objs[kStructuredCommentObservedPrefixesThis].GetMap()) {
        if (!m_Objs[kStructuredCommentObservedPrefixes].Exist(it->first)) {
            ITERATE (TReportObjectList, ro, m_Objs[kStructuredCommentsSeqs].GetObjects()) {
                const CBioseq* this_seq = dynamic_cast<const CBioseq*>((*ro)->GetObject().GetPointer());       
                CConstRef<CBioseq> this_seq_r(this_seq);
                m_Objs["[n] Bioseq[s] [is] missing " + it->first + " structured comment"]
                    .Add(*context.NewBioseqObj(this_seq_r, 0, eKeepRef), false);
                AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), this_seq_r, 0, m_Objs[kStructuredCommentReport],
                    m_Objs[kStructuredCommentPrevious], context,
                    it->first + " structured comment", kStructuredCommentFieldPrefix);
            }
        }
        // add to list of previously observed prefixes, for next time
        m_Objs[kStructuredCommentObservedPrefixes][it->first].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary(), eKeepRef), false);
    }

    m_Objs[kStructuredCommentObservedPrefixesThis].clear();
    m_Objs[kStructuredCommentsSeqs].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary(), eKeepRef), false);
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
    string top_label = "Structured Comment Report " + GetSummaryLabel(all_present, all_same);

    CReportNode::TNodeMap::iterator it = m_Objs.GetMap().begin();
    while (it != m_Objs.GetMap().end()) {
        if (!NStr::Equal(it->first, top_label)
            && !NStr::Equal(it->first, kStructuredCommentReport)) {
            CopyNode(m_Objs[top_label]["      " + it->first], *(it->second));
            it = m_Objs.GetMap().erase(it);
        } else {
            ++it;
        }
    }

    NON_CONST_ITERATE (CReportNode::TNodeMap, it, m_Objs[kStructuredCommentReport].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*(it->second), this_present, this_same);
        string new_label = it->first + " " + GetSummaryLabel(this_present, this_same);
        NON_CONST_ITERATE (CReportNode::TNodeMap, s, it->second->GetMap()) {
            string sub_label = s->first;
            if (this_present && this_same) {
                NStr::ReplaceInPlace(sub_label, "[n]", "All");
            }
            NON_CONST_ITERATE (TReportObjectList, q, s->second->GetObjects()) {
                m_Objs[top_label][new_label][sub_label].Add(**q);
            }
        }
    }
    m_Objs.GetMap().erase(kStructuredCommentReport);

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_STRUCTURED_COMMENT

DISCREPANCY_CASE(MISSING_STRUCTURED_COMMENT, CSeq_inst, eDisc, "Structured comment not included")
{
    if (obj.IsAa()) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) return;

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    size_t num_structured_comment = 0;
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_StructuredComment) {
            num_structured_comment++;
            break;
        }
        ++d;
    }
    if (num_structured_comment == 0) {
        m_Objs["[n] sequence[s] [does] not include structured comments."].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(MISSING_STRUCTURED_COMMENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_PROJECT

DISCREPANCY_CASE(MISSING_PROJECT, CSeq_inst, eDisc, "Project not included")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) return;

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    bool has_project = false;
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_DBLink) {
            if (d->GetUser().IsSetData()) {
                ITERATE (CUser_object::TData, it, d->GetUser().GetData()) {
                    if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
                        NStr::Equal((*it)->GetLabel().GetStr(), "BioProject")) {
                        has_project = true;
                        break;
                    }
                }
            }
        } else if (d->GetUser().IsSetType() && d->GetUser().GetType().IsStr() &&
            NStr::Equal(d->GetUser().GetType().GetStr(), "GenomeProjectsDB")) {
            has_project = true;
        }
        if (has_project) {
            break;
        }
        ++d;
    }
    if (!has_project) {
        m_Objs["[n] sequence[s] [does] not include project."].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(MISSING_PROJECT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_UNVERIFIED

DISCREPANCY_CASE(COUNT_UNVERIFIED, CSeq_inst, eOncaller, "Count number of unverified sequences")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) return;
    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    bool found = false;
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_Unverified) {
            found = true;
            break;
        }
        ++d;
    }
    if (found) {
        m_Objs["[n] sequence[s] [is] unverified"].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(COUNT_UNVERIFIED)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DEFLINE_PRESENT

const string kDeflineExists = "[n] Bioseq[s] [has] definition line";

DISCREPANCY_CASE(DEFLINE_PRESENT, CSeq_inst, eDisc, "Test defline existence")
{
    if (obj.IsAa()) {
        return;
    }

    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if (!bioseq) {
        return;
    }

    CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*bioseq);

    CSeqdesc_CI descrs(bioseq_h, CSeqdesc::e_Title);
    if (descrs) {
        m_Objs[kDeflineExists].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary()));
    }
}


DISCREPANCY_SUMMARIZE(DEFLINE_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNUSUAL_NT

const string kUnusualNT = "[n] sequence[s] contain[S] nucleotides that are not ATCG or N";

static bool ContainsUnusualNucleotide(const CSeq_data& seq_data)
{
    CSeq_data as_iupacna;
    TSeqPos nconv = CSeqportUtil::Convert(seq_data, &as_iupacna, CSeq_data::e_Iupacna);

    bool unusual_found = false;
    if (nconv) {
        const string& sequence = as_iupacna.GetIupacna().Get();
        for (auto nucleotide = sequence.begin(); nucleotide != sequence.end(); ++nucleotide) {
            if (!IsATGC(*nucleotide) && *nucleotide != 'N') {
                unusual_found = true;
                break;
            }
        }
    }
    return unusual_found;
}

DISCREPANCY_CASE(UNUSUAL_NT, CSeq_inst, eDisc | eSubmitter | eSmart, "Sequence contains unusual nucleotides")
{
    if (obj.IsNa()) {
        if (obj.IsSetSeq_data()) {
            if (ContainsUnusualNucleotide(obj.GetSeq_data())) {
                m_Objs[kUnusualNT].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
            }
        }
        else if (obj.IsSetExt() && obj.GetExt().IsDelta()) {
            const CSeq_ext::TDelta& deltas = obj.GetExt().GetDelta();
            if (deltas.IsSet()) {
                ITERATE (CDelta_ext::Tdata, delta, deltas.Get()) {
                    if ((*delta)->IsLiteral() && (*delta)->GetLiteral().IsSetSeq_data()) {
                        if ((*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_Gap || (*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_not_set) {
                            continue;
                        }
                        if (ContainsUnusualNucleotide((*delta)->GetLiteral().GetSeq_data())) {
                            m_Objs[kUnusualNT].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
                            break;
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNUSUAL_NT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TAXNAME_NOT_IN_DEFLINE

const string kNoTaxnameInDefline = "[n] defline[s] [does] not contain the complete taxname";

DISCREPANCY_CASE(TAXNAME_NOT_IN_DEFLINE, CSeq_inst, eDisc | eOncaller, "Complete taxname should be present in definition line")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq || seq->IsAa()) {
        return;
    }

    CBioseq_Handle seq_h = context.GetScope().GetBioseqHandle(*seq);
    CSeqdesc_CI source(seq_h, CSeqdesc::e_Source);

    if (source && source->GetSource().IsSetOrg() && source->GetSource().GetOrg().IsSetTaxname()) {

        string taxname = source->GetSource().GetOrg().GetTaxname();

        if (NStr::EqualNocase(taxname, "Human immunodeficiency virus 1")) {
            taxname = "HIV-1";
        }
        else if (NStr::EqualNocase(taxname, "Human immunodeficiency virus 2")) {
            taxname = "HIV-2";
        }

        bool no_taxname_in_defline = false;
        CSeqdesc_CI title(seq_h, CSeqdesc::e_Title);
        if (title) {
            const string& title_str = title->GetTitle();

            SIZE_TYPE taxname_pos = NStr::FindNoCase(title_str, taxname);
            if (taxname_pos == NPOS) {
                no_taxname_in_defline = true;
            }
            else {
                //capitalization must match for all but the first letter
                no_taxname_in_defline = NStr::CompareCase(title_str.c_str() + taxname_pos, 1, taxname.size() - 1, taxname.c_str() + 1) != 0;

                if (taxname_pos > 0 && !isspace(title_str[taxname_pos - 1]) && !ispunct(title_str[taxname_pos - 1])) {
                    no_taxname_in_defline = true;
                }
            }
        }

        if (no_taxname_in_defline) {
            m_Objs[kNoTaxnameInDefline].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&(*title)), context.GetCurrentBioseqLabel()), false);
        }
    }

}

DISCREPANCY_SUMMARIZE(TAXNAME_NOT_IN_DEFLINE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// HAS_PROJECT_ID

const string kProjId = "ProjectID";

static string GetProjectID(const CUser_object& user)
{
    string res;
    if (user.IsSetData()) {
        ITERATE (CUser_object::TData, field, user.GetData()) {
            if ((*field)->IsSetData() && (*field)->GetData().IsInt() && (*field)->IsSetLabel() && (*field)->GetLabel().IsStr() && (*field)->GetLabel().GetStr() == "ProjectID") {
                res = NStr::IntToString((*field)->GetData().GetInt());
                break;
            }
        }
    }

    return res;
}

DISCREPANCY_CASE(HAS_PROJECT_ID, CSeq_inst, eOncaller | eSmart, "Sequences with project IDs (looks for genome project IDs)")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) {
        return;
    }

    CBioseq_Handle seq_h = context.GetScope().GetBioseqHandle(*seq);
    for (CSeqdesc_CI user(seq_h, CSeqdesc::e_User); user; ++user) {
        if (user->GetUser().IsSetType() && user->GetUser().GetType().IsStr() && user->GetUser().GetType().GetStr() == "GenomeProjectsDB") {
            string proj_id = GetProjectID(user->GetUser());
            if (!proj_id.empty()) {
                m_Objs[kProjId][proj_id].Add(*context.NewBioseqObj(seq, &context.GetSeqSummary(), eKeepRef), false);
            }
        }
    }
}

typedef map<string, list<CConstRef<CBioseq> > > TBioseqMapByID;

static void CollectSequencesByProjectID(CReportNode& bioseqs, TBioseqMapByID& prots, TBioseqMapByID& nucs)
{
    NON_CONST_ITERATE (CReportNode::TNodeMap, node, bioseqs.GetMap()) {
        ITERATE (TReportObjectList, bioseq, node->second->GetObjects()) {
            const CDiscrepancyObject* dobj = dynamic_cast<const CDiscrepancyObject*>(bioseq->GetPointer());
            if (dobj) {
                const CBioseq* cur_bioseq = dobj->GetBioseq();
                if (cur_bioseq) {
                    if (cur_bioseq->IsNa()) {
                        nucs[node->first].push_back(ConstRef<CBioseq>(cur_bioseq));
                    }
                    else {
                        prots[node->first].push_back(ConstRef<CBioseq>(cur_bioseq));
                    }
                }
            }
        }
    }
}

static string kAllHasProjID = "[n] sequence[s] [has] project IDs ";
static string kProtHasProjID = "[n] protein sequence[s] [has] project IDs ";
static string kNucHasProjID = "[n] nucleotide sequence[s] [has] project IDs ";

static void FillReport(const TBioseqMapByID& projs, CReportNode& report, CDiscrepancyContext& context, const string& what)
{
    if (!projs.empty()) {
        ITERATE (TBioseqMapByID, proj, projs) {
            ITERATE (list<CConstRef<CBioseq> >, bioseq, proj->second) {
                report[what].Add(*context.NewBioseqObj(*bioseq, 0), false);
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(HAS_PROJECT_ID)
{
    TBioseqMapByID prots,
                   nucs;

    CollectSequencesByProjectID(m_Objs[kProjId], prots, nucs);

    CReportNode res;
    CReportNode* report = &res;
    if (!prots.empty() && !nucs.empty()) {
        string all = kAllHasProjID + (m_Objs[kProjId].GetMap().size() > 1 ? "(some different)" : "(all same)");
        report = &res[all];
    }

    string sub_group = kProtHasProjID + (prots.size() > 1 ? "(some different)" : "(all same)");
    FillReport(prots, *report, context, sub_group);

    sub_group = kNucHasProjID + (prots.size() > 1 ? "(some different)" : "(all same)");
    FillReport(nucs, *report, context, sub_group);

    m_ReportItems = res.Export(*this)->GetSubitems();
}


// MULTIPLE_CDS_ON_MRNA

const string kCDSOnMrna = "CDS on mRNA";
const string kPseudoCDSOnMRNA = "Pseudo";
const string kDisruptedCmt = "coding region disrupted by sequencing gap";
const string kLastBioseq = "Last Bioseq";
const string kMultipleCdsOnMrna = "[n] mRNA bioseq[s] [has] multiple CDS features";

void SummarizeMultCDSOnMrna(CReportNode& m_Objs)
{
    size_t num_cds = m_Objs[kCDSOnMrna].GetObjects().size();
    if (num_cds > 1 &&
        num_cds != m_Objs[kPseudoCDSOnMRNA].GetObjects().size() &&
        num_cds != m_Objs[kDisruptedCmt].GetObjects().size()) {
        CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(m_Objs[kLastBioseq].GetObjects().back().GetNCPointer()));
        m_Objs[kMultipleCdsOnMrna].Add(*seq_disc_obj, false);

    }
    m_Objs[kCDSOnMrna].clearObjs();
    m_Objs[kPseudoCDSOnMRNA].clearObjs();
    m_Objs[kDisruptedCmt].clearObjs();

}


DISCREPANCY_CASE(MULTIPLE_CDS_ON_MRNA, CSeq_feat_BY_BIOSEQ, eOncaller | eSubmitter | eSmart, "Multiple CDS on mRNA")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        SummarizeMultCDSOnMrna(m_Objs);
        CRef<CDiscrepancyObject> this_disc_obj(context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eKeepRef));
        m_Objs[kLastBioseq].Add(*this_disc_obj, false);
    }
    if (!context.IsCurrentSequenceMrna() || !obj.IsSetData() || !obj.GetData().IsCdregion()) {
        return;
    }
    m_Objs[kCDSOnMrna].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
    if (obj.IsSetComment() && NStr::Find(obj.GetComment(), kDisruptedCmt) != string::npos) {
        m_Objs[kDisruptedCmt].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
    }
    if (context.IsPseudo(obj)) {
        m_Objs[kPseudoCDSOnMRNA].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
    }
}


DISCREPANCY_SUMMARIZE(MULTIPLE_CDS_ON_MRNA)
{
    SummarizeMultCDSOnMrna(m_Objs);
    m_Objs.GetMap().erase(kCDSOnMrna);
    m_Objs.GetMap().erase(kPseudoCDSOnMRNA);
    m_Objs.GetMap().erase(kDisruptedCmt);
    m_Objs.GetMap().erase(kLastBioseq);
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// MRNA_SEQUENCE_MINUS_STRAND_FEATURES

static const string kMinusStrandOnMrna = "minusStrandFeatures";
static const string kOtherStrandOnMrna = "otherStrandFeatures";
static const string kMrnaSequenceMinusStrandFeatures = "[n] mRNA sequences have features on the complement strand.";

class CMinusStrandData : public CObject
{
public:
    list<CSeq_feat*> m_feats;
};

static void CopyMinusStrandData(const TReportObjectList& objs, CMinusStrandData& data)
{
    ITERATE(TReportObjectList, obj, objs) {

        CConstRef<CSerialObject> serial_obj = (*obj)->GetObject();
        CSeq_feat* feat = dynamic_cast<CSeq_feat*>(const_cast<CSerialObject*>(serial_obj.GetPointer()));
        data.m_feats.push_back(feat);
    }
}

static void SummarizeMinusOnMrna(CReportNode& m_Objs)
{
    if (m_Objs[kMinusStrandOnMrna].GetObjects().size() > 0) {

        CDiscrepancyObject* seq_disc_obj = dynamic_cast<CDiscrepancyObject*>(m_Objs[kLastBioseq].GetObjects().back().GetNCPointer());
        if (m_Objs[kOtherStrandOnMrna].empty()) { // fixable case, no features on strands other than minus

            CRef<CMinusStrandData> obj_data(new CMinusStrandData);
            CopyMinusStrandData(m_Objs[kMinusStrandOnMrna].GetObjects(), *obj_data);

            CRef<CReportObj> new_disc_obj(seq_disc_obj->Clone(true, CConstRef<CObject>(obj_data.GetPointer())));
            m_Objs[kMrnaSequenceMinusStrandFeatures].Add(*new_disc_obj, false);
        }
        else
            m_Objs[kMrnaSequenceMinusStrandFeatures].Add(*seq_disc_obj, false);

    }
    m_Objs[kOtherStrandOnMrna].clearObjs();
    m_Objs[kMinusStrandOnMrna].clearObjs();
}


DISCREPANCY_CASE(MRNA_SEQUENCE_MINUS_STRAND_FEATURES, CSeq_feat_BY_BIOSEQ, eOncaller, "mRNA sequences have CDS/gene on the complement strand")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        SummarizeMinusOnMrna(m_Objs);
        CRef<CDiscrepancyObject> this_disc_obj(context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eKeepRef));
        m_Objs[kLastBioseq].Add(*this_disc_obj, false);
    }

    if (context.IsCurrentSequenceMrna())
    {
        if (obj.GetLocation().GetStrand() != eNa_strand_minus || obj.GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind) {
            m_Objs[kOtherStrandOnMrna].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
        }
        else {
            m_Objs[kMinusStrandOnMrna].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(MRNA_SEQUENCE_MINUS_STRAND_FEATURES)
{
    SummarizeMinusOnMrna(m_Objs);
    m_Objs.GetMap().erase(kMinusStrandOnMrna);
    m_Objs.GetMap().erase(kOtherStrandOnMrna);
    m_Objs.GetMap().erase(kLastBioseq);
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


static bool ConvertToReverseComplement(const CBioseq& bioseq, const CMinusStrandData* data, CScope& scope)
{
    if (data == nullptr) {
        return false;
    }

    CBioseq_EditHandle seq_h = scope.GetBioseqEditHandle(bioseq);

    CRef<objects::CSeq_inst> new_inst(new objects::CSeq_inst());
    new_inst->Assign(seq_h.GetInst());
    ReverseComplement(*new_inst, &scope);
    seq_h.SetInst(*new_inst);

    CMinusStrandData* non_const_data = const_cast<CMinusStrandData*>(data);
    if (non_const_data) {
        NON_CONST_ITERATE(list<CSeq_feat*>, feat, non_const_data->m_feats) {
            edit::ReverseComplementFeature(**feat, scope);
        }
    }

    return true;
}

DISCREPANCY_AUTOFIX(MRNA_SEQUENCE_MINUS_STRAND_FEATURES)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {

        CDiscrepancyObject* disc_obj = dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
        const CBioseq* bioseq = dynamic_cast<const CBioseq*>(disc_obj->GetObject().GetPointer());
        CConstRef<CObject> data = disc_obj->GetMoreInfo();

        if (bioseq && ConvertToReverseComplement(*bioseq, dynamic_cast<const CMinusStrandData*>(data.GetPointer()), scope)) {
            n++;
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("MRNA_SEQUENCE_MINUS_STRAND_FEATURES: [n] sequence[s] [is] converted to reverse complement[s]", n) : 0);
}

// LOW_QUALITY_REGION

const string kLowQualityRegion = "[n] sequence[s] contain[S] low quality region";

const SIZE_TYPE MIN_SEQ_LEN = 30;
const SIZE_TYPE MAX_N_IN_SEQ = 7; // 25% of the sequence

static bool HasLowQualityRegion(const CSeq_data& seq_data)
{
    CSeq_data as_iupacna;
    TSeqPos nconv = CSeqportUtil::Convert(seq_data, &as_iupacna, CSeq_data::e_Iupacna);
    if (nconv == 0) {
        return false;
    }

    const string& iupacna_str = as_iupacna.GetIupacna().Get();
    size_t seq_len = iupacna_str.size();
    if (seq_len < MIN_SEQ_LEN)
        return false;

    size_t cur = 0;
    size_t num_of_n = 0;
    for (; cur < MIN_SEQ_LEN; ++cur) {
        if (!IsATGC(iupacna_str[cur])) {
            ++num_of_n;
        }
    }

    for (; cur < seq_len; ++cur) {
        if (num_of_n > MAX_N_IN_SEQ) {
            break;
        }

        if (!IsATGC(iupacna_str[cur - MIN_SEQ_LEN])) {
            --num_of_n;
        }

        if (!IsATGC(iupacna_str[cur])) {
            ++num_of_n;
        }
    }

    return (num_of_n > MAX_N_IN_SEQ);
}

DISCREPANCY_CASE(LOW_QUALITY_REGION, CSeq_inst, eDisc | eSubmitter | eSmart, "Sequence contains regions of low quality")
{
    if (obj.IsNa()) {
        if (obj.IsSetSeq_data()) {
            if (HasLowQualityRegion(obj.GetSeq_data())) {
                m_Objs[kLowQualityRegion].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
            }
        }
        else if (obj.IsSetExt() && obj.GetExt().IsDelta()) {
            const CSeq_ext::TDelta& deltas = obj.GetExt().GetDelta();
            if (deltas.IsSet()) {
                ITERATE (CDelta_ext::Tdata, delta, deltas.Get()) {
                    if ((*delta)->IsLiteral() && (*delta)->GetLiteral().IsSetSeq_data()) {
                        if ((*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_Gap || (*delta)->GetLiteral().GetSeq_data().Which() == CSeq_data::e_not_set) {
                            continue;
                        }
                        if (HasLowQualityRegion((*delta)->GetLiteral().GetSeq_data())) {
                            m_Objs[kLowQualityRegion].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
                            break;
                        }
                    }
                }
            }
        }
    }
}

DISCREPANCY_SUMMARIZE(LOW_QUALITY_REGION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DEFLINE_ON_SET

const string kDeflineOnSet = "[n] title[s] on sets were found";

DISCREPANCY_CASE(DEFLINE_ON_SET, CBioseq_set, eOncaller, "Titles on sets")
{
    if (obj.IsSetDescr()) {
        ITERATE (CSeq_descr::Tdata, descr, obj.GetDescr().Get()) {
            if ((*descr)->IsTitle()) {
                m_Objs[kDeflineOnSet].Add(*context.NewSeqdescObj(*descr, context.GetCurrentBioseqLabel()), false);
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DEFLINE_ON_SET)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}

// MITOCHONDRION_REQUIRED

const string kMitochondrionRequired = "[n] bioseq[s] [has] D-loop or control region misc_feature, but [is] do not have mitochondrial source";

DISCREPANCY_CASE(MITOCHONDRION_REQUIRED, COverlappingFeatures, eDisc | eOncaller, "If D-loop or control region misc_feat is present, source must be mitochondrial")
{
    if (context.GetCurrentGenome() == CBioSource::eGenome_mitochondrion) {
        return;
    }
    const vector<CConstRef<CSeq_feat> >& all = context.FeatAll();
    bool has_D_loop = false;
    bool has_misc_feat_with_control_region = false;
    ITERATE(vector<CConstRef<CSeq_feat>>, feat, all) {
        if ((*feat)->IsSetData()) {
            if ((*feat)->GetData().GetSubtype() == CSeqFeatData::eSubtype_D_loop) {
                has_D_loop = true;
                break;
            }
            else if ((*feat)->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
                if ((*feat)->IsSetComment() && NStr::FindNoCase((*feat)->GetComment(), "control region") != NPOS) {
                    has_misc_feat_with_control_region = true;
                    break;
                }
            }
        }
    }
    if (has_D_loop || has_misc_feat_with_control_region) {
        m_Objs[kMitochondrionRequired].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eNoRef, true));
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        const CBioseq* bioseq = dynamic_cast<const CBioseq*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (bioseq && FixGenome(*bioseq, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("MITOCHONDRION_REQUIRED: Genome was set to mitochondrion for [n] bioseq[s]", n) : 0);
}


// SHORT_CONTIG

const string kShortContig = "[n] contig[s] [is] shorter than 200 nt";

static bool IsMolProd(int biomol)
{
    return biomol == CMolInfo::eBiomol_mRNA ||
           biomol == CMolInfo::eBiomol_ncRNA ||
           biomol == CMolInfo::eBiomol_rRNA ||
           biomol == CMolInfo::eBiomol_pre_RNA ||
           biomol == CMolInfo::eBiomol_tRNA;

}

static bool IsmRNASequenceInGenProdSet(CDiscrepancyContext& context, const CBioseq& bioseq)
{
    bool ret = false;

    if (bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
        CConstRef<CBioseq_set> bio_set = bioseq.GetParentSet();
        if (bio_set && bio_set->IsSetClass()) {
            if (bio_set->GetClass() == CBioseq_set::eClass_nuc_prot) {
                bio_set = bio_set->GetParentSet();
            }

            if (bio_set && bio_set->IsSetClass() && bio_set->GetClass() == CBioseq_set::eClass_gen_prod_set) {
                const CMolInfo* mol_info = context.GetCurrentMolInfo();
                if (mol_info && mol_info->IsSetBiomol() && IsMolProd(mol_info->GetBiomol())) {
                    ret = true;
                }
            }
        }
    }

    return ret;
}


DISCREPANCY_CASE(SHORT_CONTIG, CSeq_inst, eDisc | eSubmitter | eSmart, "Short Contig")
{
    static TSeqPos MIN_CONTIG_LEN = 200;

    if (obj.IsNa() && obj.IsSetLength() && obj.GetLength() < MIN_CONTIG_LEN) {
        CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
        if (!IsmRNASequenceInGenProdSet(context, *bioseq)) {
            m_Objs[kShortContig].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary(), eKeepRef, true), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(SHORT_CONTIG)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


static bool RemoveBioseq(const CBioseq& bioseq, CScope& scope)
{
    if (bioseq.IsSetAnnot()) {
        ITERATE (CBioseq::TAnnot, annot_it, bioseq.GetAnnot()) {
            if ((*annot_it)->IsFtable()) {
                return false;
            }
        }
    }

    CBioseq_EditHandle bioseq_edit(scope.GetBioseqEditHandle(bioseq));
    bioseq_edit.Remove();

    return true;
}


DISCREPANCY_AUTOFIX(SHORT_CONTIG)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        const CBioseq* bioseq = dynamic_cast<const CBioseq*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (bioseq && RemoveBioseq(*bioseq, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("SHORT_CONTIG: [n] short bioseq[s] [is] removed", n) : 0);
}


// RNA_PROVIRAL

const string kRnaProviral = "[n] RNA bioseq[s] [is] proviral";

DISCREPANCY_CASE(RNA_PROVIRAL, CSeq_inst, eOncaller, "RNA bioseqs are proviral")
{
    if (obj.IsSetMol() && obj.GetMol() == CSeq_inst::eMol_rna) {
        const CBioSource* bio_src = context.GetCurrentBiosource();
        if (bio_src && bio_src->IsSetOrg() && bio_src->IsSetGenome() && bio_src->GetGenome() == CBioSource::eGenome_proviral) {
            m_Objs[kRnaProviral].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(RNA_PROVIRAL)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SMALL_GENOME_SET_PROBLEM

typedef list<const CSeqdesc*> TBioSourcesList;

static void GetBioSourceFromDescr(const CSeq_descr& descrs, TBioSourcesList& bio_srcs)
{
    ITERATE (CSeq_descr::Tdata, descr, descrs.Get()) {
        if ((*descr)->IsSource()) {
            bio_srcs.push_back(*descr);
            return;
        }
    }
}


static void CollectBioSources(const CBioseq_set& bio_set, TBioSourcesList& bio_srcs)
{
    if (bio_set.IsSetDescr()) {
        GetBioSourceFromDescr(bio_set.GetDescr(), bio_srcs);
    }

    ITERATE (CBioseq_set::TSeq_set, entry, bio_set.GetSeq_set()) {
        if ((*entry)->IsSet()) {
            CollectBioSources((*entry)->GetSet(), bio_srcs);
        }
        else {

            if ((*entry)->GetSeq().IsSetDescr()) {
                GetBioSourceFromDescr((*entry)->GetSeq().GetDescr(), bio_srcs);
            }
        }
    }
}

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

        ITERATE (COrgName::TMod, mod, bio_src.GetOrgname().GetMod()) {
            if ((*mod)->IsSetSubtype() && (*mod)->GetSubtype() == subtype && (*mod)->IsSetSubname()) {

                if (val.empty()) {
                    val = (*mod)->GetSubname();
                }
                else {
                    if ((*mod)->GetSubname() != val) {
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
        ITERATE (CBioSource::TSubtype, subtype, bio_src.GetSubtype()) {
            if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_segment) {
                ret = true;
                break;
            }
        }
    }
    return ret;
}

static const string kMissingSegment = "[n] biosource[s] should have segment qualifier but [does] not";


DISCREPANCY_CASE(SMALL_GENOME_SET_PROBLEM, CBioseq_set, eOncaller, "Problems with small genome sets")
{
    if (obj.IsSetClass() && obj.GetClass() == CBioseq_set::eClass_small_genome_set) {
        TBioSourcesList bio_srcs;
        CollectBioSources(obj, bio_srcs);
        string taxname,
               isolate,
               strain;
        bool all_taxname_same = true,
             all_isolate_same = true,
             all_strain_same = true;

        ITERATE (TBioSourcesList, descr_bio_src, bio_srcs) {
            const CBioSource& bio_src = (*descr_bio_src)->GetSource();
            if (context.HasLineage(bio_src, "", "Viruses")) {
                if (!IsSegmentSubtype(bio_src)) {
                    m_Objs[kMissingSegment].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(*descr_bio_src), context.GetCurrentBioseqLabel()), false);
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

static const string kUnwantedSetWrapper = "[n] unwanted set wrapper[s]";

static const CBioSource* GetBioSource(const CBioseq_Handle& bioseq)
{
    const CBioSource* ret = nullptr;
    if (bioseq.IsSetDescr()) {
        ITERATE (CSeq_descr::Tdata, descr, bioseq.GetDescr().Get()) {
            if ((*descr)->IsSource()) {
                ret = &(*descr)->GetSource();
                break;
            }
        }
    }
    return ret;
}


static bool IsMicroSatellite(const CSeq_feat_Handle& feat)
{
    bool ret = false;
    if (feat.GetFeatSubtype() == CSeqFeatData::eSubtype_repeat_region) {

        if (feat.IsSetQual()) {

            ITERATE (CSeq_feat::TQual, qual, feat.GetQual()) {
                if ((*qual)->IsSetQual() && (*qual)->IsSetVal() &&
                    NStr::EqualNocase("satellite", (*qual)->GetQual()) && NStr::StartsWith((*qual)->GetVal(), "microsatellite", NStr::eNocase)) {

                    ret = true;
                    break;
                }
            }
        }
    }

    return ret;
}


DISCREPANCY_CASE(UNWANTED_SET_WRAPPER, CBioseq_set, eOncaller, "Set wrapper on microsatellites or rearranged genes")
{
    if (obj.IsSetClass()) {
        CBioseq_set::EClass bio_set_class = obj.GetClass();
        if (bio_set_class == CBioseq_set::eClass_eco_set ||
            bio_set_class == CBioseq_set::eClass_mut_set ||
            bio_set_class == CBioseq_set::eClass_phy_set ||
            bio_set_class == CBioseq_set::eClass_pop_set) {

            bool has_rearranged = false;
            bool has_sat_feat = false;
            bool has_non_sat_feat = false;

            CBioseq_set_Handle bio_set_handle = context.GetScope().GetBioseq_setHandle(obj);
            for (CBioseq_CI bioseq(bio_set_handle); bioseq; ++bioseq) {
                if (!has_rearranged) {
                    const CBioSource* bio_src = GetBioSource(*bioseq);
                    if (bio_src != nullptr && bio_src->IsSetSubtype()) {
                        ITERATE (CBioSource::TSubtype, subtype, bio_src->GetSubtype()) {
                            if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_rearranged) {
                                has_rearranged = true;
                                break;
                            }
                        }
                    }
                }

                if (!has_sat_feat || !has_non_sat_feat) {
                    for (CFeat_CI feat(*bioseq); feat; ++feat) {
                        if (IsMicroSatellite(*feat)) {
                            has_sat_feat = true;
                        }
                        else {
                            has_non_sat_feat = true;
                        }
                    }
                }

                if (has_rearranged && has_sat_feat && has_non_sat_feat) {
                    break;
                }
            }

            if (has_rearranged || (has_sat_feat && !has_non_sat_feat)) {
                m_Objs[kUnwantedSetWrapper].Add(*context.NewDiscObj(CConstRef<CBioseq_set>(&obj)), false);
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNWANTED_SET_WRAPPER)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
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


class CIdxObject : public CObject
{
public:
    CIdxObject(size_t idx) :
        m_idx(idx) {}

    size_t GetIdx() const
    {
        return m_idx;
    }

private:
    size_t m_idx;
};


static void FindSuspectTextInObject(const CSerialObject& obj, list<size_t>& misspells)
{
    for (CStdTypeConstIterator<string> it(obj); it; ++it) {

        for (size_t i = 0; i < sizeof(kSpellFixes) / sizeof(kSpellFixes[0]); ++i) {

            size_t pos = kSpellFixes[i].m_whole_word ? NStr::FindWord(*it, kSpellFixes[i].m_misspell) : NStr::Find(*it, kSpellFixes[i].m_misspell);
            if (pos != NPOS) {
                misspells.push_back(i);
            }
        }
    }
}

static const string kFixable = "Fixable";
static const string kNonFixable = "Non-fixable";

void  AddMisspellsToReport(const list<size_t>& misspells, CReportNode& node, const CSeq_feat& obj, CDiscrepancyContext& context)
{
    ITERATE (list<size_t>, misspell_idx, misspells) {
        string subitem = string("[n] object[s] contain[S] ") + kSpellFixes[*misspell_idx].m_misspell;
        EKeepRef keep_ref = kSpellFixes[*misspell_idx].m_correct == nullptr ? eNoRef : eKeepRef;
        bool autofix = kSpellFixes[*misspell_idx].m_correct != nullptr;
        const string& fixable = (autofix ? kFixable : kNonFixable);
        node[fixable][subitem].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), keep_ref, autofix, CRef<CIdxObject>(new CIdxObject(*misspell_idx))));
    }
}


void  AddMisspellsToReport(const list<size_t>& misspells, CReportNode& node, const CSeqdesc& obj, CDiscrepancyContext& context)
{
    ITERATE (list<size_t>, misspell_idx, misspells) {
        string subitem = string("[n] object[s] contain[S] ") + kSpellFixes[*misspell_idx].m_misspell;
        EKeepRef keep_ref = kSpellFixes[*misspell_idx].m_correct == nullptr ? eNoRef : eKeepRef;
        bool autofix = kSpellFixes[*misspell_idx].m_correct != nullptr;
        const string& fixable = (autofix ? kFixable : kNonFixable);
        node[fixable][subitem].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), keep_ref, autofix, CRef<CIdxObject>(new CIdxObject(*misspell_idx))));
    }
}


DISCREPANCY_CASE(FLATFILE_FIND, CSeq_inst, eOncaller, "Flatfile representation of object contains suspect text")
{
// may need to rewrite
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*bioseq);
    for (CSeqdesc_CI descr(bioseq_h); descr; ++descr) {
        list<size_t> misspells;
        FindSuspectTextInObject(*descr, misspells);
        if (!misspells.empty()) {
            AddMisspellsToReport(misspells, m_Objs, *descr, context);
        }
    }
    for (CFeat_CI feat(bioseq_h); feat; ++feat) {
        list<size_t> misspells;
        FindSuspectTextInObject(feat->GetMappedFeature(), misspells);
        if (!misspells.empty()) {
            AddMisspellsToReport(misspells, m_Objs, feat->GetMappedFeature(), context);
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {

        CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>(it->GetPointer());

        const CObject* cur_obj_ptr = dobj->GetObject().GetPointer();
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(cur_obj_ptr);
        const CSeqdesc* descr = dynamic_cast<const CSeqdesc*>(cur_obj_ptr);

        size_t misspell_idx = dynamic_cast<const CIdxObject*>(dobj->GetMoreInfo().GetPointer())->GetIdx();

        if (feat) {
            if (FixTextInObject(const_cast<CSeq_feat*>(feat), misspell_idx)) {
                ++n;
            }
        }
        else if (descr) {
            if (FixTextInObject(const_cast<CSeqdesc*>(descr), misspell_idx)) {
                ++n;
            }
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("FLATFILE_FIND: [n] suspect text[s] [is] fixed", n) : 0);
}


DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER);
DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER_UNFIXABLE);
DISCREPANCY_ALIAS(FLATFILE_FIND, FLATFILE_FIND_ONCALLER_FIXABLE);


// SEQUENCES_ARE_SHORT

static const string kAllShort = "No sequences longer than 20,000 nt found";
static const string kLongerFound = "LongSeq";
static const size_t MIN_SEQUENCE_LEN = 20000;


DISCREPANCY_CASE(SEQUENCES_ARE_SHORT, CSeq_inst, eDisc | eSubmitter, "Short sequences test")
{
    if (obj.GetLength() > MIN_SEQUENCE_LEN) {
        m_Objs[kLongerFound].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(SEQUENCES_ARE_SHORT)
{
    if (m_Objs.GetMap().find(kLongerFound) == m_Objs.GetMap().end()) {
        // no sequences longer than 20000 nt
        m_Objs[kAllShort];
    }
    else {
        m_Objs.GetMap().erase(kLongerFound);
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
