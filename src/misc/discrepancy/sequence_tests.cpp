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
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Org_ref.hpp>
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

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_GENOMEASSEMBLY_COMMENTS, CSeq_inst, eDisc, "Bioseqs should have GenomeAssembly structured comments")
//  ----------------------------------------------------------------------------
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
        m_Objs[kMissingGenomeAssemblyComments].Add(*context.NewDiscObj(seq),
                    false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_GENOMEASSEMBLY_COMMENTS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(DUP_DEFLINE, CSeq_inst, eOncaller, "Definition lines should be unique")
//  ----------------------------------------------------------------------------
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq || !seq->IsSetDescr() || obj.IsAa()) return;

    ITERATE(CBioseq::TDescr::Tdata, it, seq->GetDescr().Get()) {
        if ((*it)->IsTitle()) {
            CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj((*it), eKeepRef));
            m_Objs["titles"][(*it)->GetTitle()].Add(*this_disc_obj, false);
        }
    }
}


const string kIdenticalDeflines = "[n] definition line[s] [is] identical:";
const string kAllUniqueDeflines = "All deflines are unique";
const string kUniqueDeflines = "[n] definition line[s] [is] unique";

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(DUP_DEFLINE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    CReportNode::TNodeMap::iterator it = m_Objs["titles"].GetMap().begin();
    bool all_unique = true;
    while (it != m_Objs["titles"].GetMap().end() && all_unique) {
        if (it->second->GetObjects().size() > 1) {
            all_unique = false;
        }
        ++it;
    }
    it = m_Objs["titles"].GetMap().begin();
    while (it != m_Objs["titles"].GetMap().end()) {            
        NON_CONST_ITERATE(TReportObjectList, robj, m_Objs["titles"][it->first].GetObjects())
        {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeqdesc> title_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));
            if (it->second->GetObjects().size() > 1) {
                //non-unique definition line
                m_Objs[kIdenticalDeflines + it->first].Add(*context.NewDiscObj(title_desc), false);
            } else {
                //unique definition line
                if (all_unique) {
                    m_Objs[kAllUniqueDeflines].Add(*context.NewDiscObj(title_desc), false);
                } else {
                    m_Objs[kUniqueDeflines].Add(*context.NewDiscObj(title_desc), false);
                }
            }
        }  
        ++it;
    }
    m_Objs.GetMap().erase("titles");
    if (all_unique) {
        m_Objs[kAllUniqueDeflines].clearObjs();
    }

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TERMINAL_NS

const string kTerminalNs = "[n] sequence[s] [has] terminal Ns";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(TERMINAL_NS, CSeq_inst, eDisc, "Ns at end of sequences")
//  ----------------------------------------------------------------------------
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq || seq->IsAa()) {
        return;
    }

    bool has_terminal_n_or_gap = false;

    CBioseq_Handle b = context.GetScope().GetBioseqHandle(*seq);
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
        m_Objs[kTerminalNs].Add(*context.NewDiscObj(seq),
            false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(TERMINAL_NS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SHORT_PROT_SEQUENCES
const string kShortProtSeqs = "[n] protein sequences are shorter than 50 aa.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_PROT_SEQUENCES, CSeq_inst, eDisc | eOncaller, "Protein sequences should be at least 50 aa, unless they are partial")
//  ----------------------------------------------------------------------------
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

    m_Objs[kShortProtSeqs].Add(*context.NewDiscObj(seq), false);
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHORT_PROT_SEQUENCES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COMMENT_PRESENT

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(COMMENT_PRESENT, CSeqdesc, eOncaller, "Comment descriptor present")
//  ----------------------------------------------------------------------------
{
    if (obj.IsComment()) {
        CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSeqdesc>(&obj), eKeepRef));
        m_Objs["comment"][obj.GetComment()].Add(*this_disc_obj, false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(COMMENT_PRESENT)
//  ----------------------------------------------------------------------------
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
        NON_CONST_ITERATE(TReportObjectList, robj, m_Objs["comment"][it->first].GetObjects())
        {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeqdesc> comment_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));
            m_Objs[label].Add(*context.NewDiscObj(comment_desc), false);
        }
        ++it;
    }
    m_Objs.GetMap().erase("comment");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// mRNA_ON_WRONG_SEQUENCE_TYPE

const string kmRNAOnWrongSequenceType = "[n] mRNA[s] [is] located on eukaryotic sequence[s] that [does] not have genomic or plasmid source[s]";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(mRNA_ON_WRONG_SEQUENCE_TYPE, CSeq_feat_BY_BIOSEQ, eOncaller | eDisc, "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features")
//  ----------------------------------------------------------------------------
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


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(mRNA_ON_WRONG_SEQUENCE_TYPE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DISC_GAPS

const string kSequencesWithGaps = "[n] sequence[s] contain[S] gaps";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(GAPS, CSeq_inst, eDisc, "Sequences with gaps")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetRepr() && obj.GetRepr() == CSeq_inst::eRepr_delta) {

        bool has_gaps = false;
        if (obj.IsSetExt() && obj.GetExt().IsDelta()) {

            ITERATE(CDelta_ext::Tdata, it, obj.GetExt().GetDelta().Get()) {

                if ((*it)->IsLiteral() && (*it)->GetLiteral().IsSetSeq_data() && (*it)->GetLiteral().GetSeq_data().IsGap()) {

                    has_gaps = true;
                    break;
                }
            }
        }

        if (!has_gaps) {

            CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
            if (!bioseq || !bioseq->IsSetAnnot()) {
                return;
            }

            const CSeq_annot* annot = nullptr;
            ITERATE(CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
                if ((*annot_it)->IsFtable()) {
                    annot = *annot_it;
                    break;
                }
            }

            if (annot) {

                ITERATE(CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {

                    if ((*feat)->IsSetData() && (*feat)->GetData().GetSubtype() == CSeqFeatData::eSubtype_gap) {
                        has_gaps = true;
                        break;
                    }
                }
            }
        }

        if (has_gaps) {
            m_Objs[kSequencesWithGaps].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(GAPS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ONCALLER_BIOPROJECT_ID

const string kSequencesWithBioProjectIDs = "[n] sequence[s] contain[S] BioProject IDs";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BIOPROJECT_ID, CSeq_inst, eOncaller, "Sequences with BioProject IDs")
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle bioseq = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    CSeqdesc_CI user_desc_it(bioseq, CSeqdesc::E_Choice::e_User);
    for (; user_desc_it; ++user_desc_it) {

        const CUser_object& user = user_desc_it->GetUser();
        if (user.IsSetData() && user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "DBLink") {

            ITERATE(CUser_object::TData, user_field, user.GetData()) {

                if ((*user_field)->IsSetLabel() && (*user_field)->GetLabel().IsStr() && (*user_field)->GetLabel().GetStr() == "BioProject" &&
                    (*user_field)->IsSetData() && (*user_field)->GetData().IsStrs()) {

                    const CUser_field::C_Data::TStrs& strs = (*user_field)->GetData().GetStrs();
                    if (!strs.empty() && !strs[0].empty()) {

                        m_Objs[kSequencesWithBioProjectIDs].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
                        return;
                    }
                }
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BIOPROJECT_ID)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_DEFLINES

const string kMissingDeflines = "[n] bioseq[s] [has] no definition line";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_DEFLINES, CSeq_inst, eOncaller, "Missing definition lines")
//  ----------------------------------------------------------------------------
{
    if (obj.IsAa()) {
        return;
    }

    bool has_title = false;
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (seq && seq->IsSetDescr()) {
        ITERATE(CBioseq::TDescr::Tdata, d, seq->GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                has_title = true;
                break;
            }
        }
    }
    if (!has_title) {
        m_Objs[kMissingDeflines].Add(*context.NewDiscObj(seq), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_DEFLINES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// N_RUNS_14

const string kMoreThan14NRuns = "[n] sequence[s] [has] runs of 15 or more Ns";
const TSeqPos MIN_BAD_RUN_LEN = 15;

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(N_RUNS_14, CSeq_inst, eDisc, "Runs of more than 14 Ns")
//  ----------------------------------------------------------------------------
{
    if (obj.IsNa()) {

        if (obj.IsSetSeq_data()) {
            vector<CRange<TSeqPos> > runs;
            FindNRuns(runs, obj.GetSeq_data(), 0, MIN_BAD_RUN_LEN);

            if (!runs.empty()) {
                m_Objs[kMoreThan14NRuns].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
            }
        }
        else if (obj.IsSetExt() && obj.GetExt().IsDelta()) {

            const CSeq_ext::TDelta& deltas = obj.GetExt().GetDelta();
            if (deltas.IsSet()) {

                ITERATE(CDelta_ext::Tdata, delta, deltas.Get()) {

                    if ((*delta)->IsLiteral() && (*delta)->GetLiteral().IsSetSeq_data()) {

                        vector<CRange<TSeqPos> > runs;
                        FindNRuns(runs, (*delta)->GetLiteral().GetSeq_data(), 0, MIN_BAD_RUN_LEN);

                        if (!runs.empty()) {
                            m_Objs[kMoreThan14NRuns].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
                            break;
                        }
                    }
                }
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(N_RUNS_14)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// 10_PERCENTN

const string kMoreThan10PercentsN = "[n] sequence[s] [has] > 10%% Ns";
const double MIN_N_PERCENTAGE = 10.0;

DISCREPANCY_CASE(10_PERCENTN, CSeq_inst, eDisc, "Greater than 10 percent Ns")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }

    const CSeqSummary& sum = context.GetNucleotideCount();
    if (sum.N * 100. / sum.Len > MIN_N_PERCENTAGE) {
        m_Objs[kMoreThan10PercentsN].Add(*context.NewDiscObj(context.GetCurrentBioseq()));
    }
}


DISCREPANCY_SUMMARIZE(10_PERCENTN)
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_COUNT
const string kFeatureCountTop = "Feature Counts";
const string kFeatureCountTotal = "Feature Count Total";
const string kFeatureCountSub = "temporary";
const string kFeatureCountSequenceList = "sequence_list";

void AddFeatureCount(CReportNode& m_Objs, const string& key, size_t num)
{
    string label = "[n] bioseq[s] [has] " + NStr::NumericToString(num)
        + " " + key + " features";

    CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(m_Objs[kFeatureCountSequenceList].GetObjects().back().GetNCPointer()));
    m_Objs[kFeatureCountSub][key][label].Add(*seq_disc_obj, false);
}


void SummarizeFeatureCount(CReportNode& m_Objs)
{
    if (!m_Objs[kFeatureCountTop].empty()) {
        CReportNode::TNodeMap& map = m_Objs[kFeatureCountTop].GetMap(); 
        NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
            // add zeros to all previous sequences that do not have this feature type
            if (!m_Objs[kFeatureCountSub].Exist(it->first)) {
                ITERATE(TReportObjectList, ro, m_Objs[kFeatureCountSequenceList].GetObjects()) {
                    if (*ro != m_Objs[kFeatureCountSequenceList].GetObjects().back()) {
                        string label = "[n] bioseq[s] [has] 0 " + it->first + " features";
                        CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(ro->GetNCPointer()));
                        m_Objs[kFeatureCountSub][it->first][label].Add(*seq_disc_obj, false);
                    }
                }
            }

            // add non-zero count for this sequence
            AddFeatureCount(m_Objs, it->first, it->second->GetObjects().size());
        }
        // add zero for all feature types not found
        ITERATE(CReportNode::TNodeMap, z, m_Objs[kFeatureCountSub].GetMap()) {
            if (!m_Objs[kFeatureCountTop].Exist(z->first)) {
                AddFeatureCount(m_Objs, z->first, 0);
            }
        }
        m_Objs[kFeatureCountTop].clear();
    }
}


DISCREPANCY_CASE(FEATURE_COUNT, CSeq_feat_BY_BIOSEQ, eOncaller, "Count features present or missing from sequences")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        SummarizeFeatureCount(m_Objs);
        CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CBioseq>(context.GetCurrentBioseq()), eKeepRef));
        m_Objs[kFeatureCountSequenceList].Add(*this_disc_obj, false);
    }
    if (obj.GetData().GetSubtype() == CSeqFeatData::eSubtype_prot) {
        return;
    }
    string key = obj.GetData().IsGene() ? "gene" : obj.GetData().GetKey();
    m_Objs[kFeatureCountTop][key].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
    m_Objs[kFeatureCountTotal][key].Add(*(context.NewDiscObj(CConstRef<CSeq_feat>(&obj))), false);
}


DISCREPANCY_SUMMARIZE(FEATURE_COUNT)
{
    SummarizeFeatureCount(m_Objs);
    if (m_Objs.empty()) {
        return;
    }
    
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs[kFeatureCountSub].GetMap()) {
        string new_label = it->first + ": " +
            NStr::NumericToString(m_Objs[kFeatureCountTotal][it->first].GetObjects().size()) +
            " present";
        if (it->second->GetMap().size() > 1) {
            new_label += " (inconsistent)";
        }
        NON_CONST_ITERATE(CReportNode::TNodeMap, s, it->second->GetMap()){
            NON_CONST_ITERATE(TReportObjectList, q, s->second->GetObjects()) {
                m_Objs[new_label][s->first].Add(**q).Ext();
            }
        }
    }

    m_Objs.GetMap().erase(kFeatureCountTotal);
    m_Objs.GetMap().erase(kFeatureCountSub);
    m_Objs.GetMap().erase(kFeatureCountTop);
    m_Objs.GetMap().erase(kFeatureCountSequenceList);

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
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

DISCREPANCY_CASE(EXON_ON_MRNA, CSeq_feat_BY_BIOSEQ, eOncaller, "mRNA sequences should not have exons")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        SummarizeExonCount(m_Objs);
        CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CBioseq>(context.GetCurrentBioseq()), eKeepRef));
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

DISCREPANCY_CASE(INCONSISTENT_MOLINFO_TECH, CSeq_inst, eDisc, "Inconsistent Molinfo Techniques")
{
    if (obj.IsAa()) {
        return;
    }

    CBioseq_Handle bioseq = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    
    for (CSeqdesc_CI molinfo_desc_it(bioseq, CSeqdesc::E_Choice::e_Molinfo); molinfo_desc_it; ++molinfo_desc_it) {

        const CMolInfo& mol_info = molinfo_desc_it->GetMolinfo();
        if (!mol_info.IsSetTech()) {
            m_Objs[kMissingTech].Add(*context.NewDiscObj(context.GetCurrentBioseq()));
        }
        else {
            string tech = NStr::IntToString(mol_info.GetTech());
            m_Objs[tech].Add(*context.NewDiscObj(context.GetCurrentBioseq()));
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
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, the_map) {

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

DISCREPANCY_CASE(TITLE_ENDS_WITH_SEQUENCE, CSeqdesc, eDisc, "Sequence characters at end of defline")
{
    if (!obj.IsTitle()) {
        return;
    }

    if (EndsWithSequence(obj.GetTitle())) {
        m_Objs[kEndsWithSeq].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)));
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
    if (!obj.IsSetMol() || obj.GetMol() != CSeq_inst::eMol_dna) {
        return;
    }

    CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    CSeqdesc_CI mi(bioseq_h, CSeqdesc::e_Molinfo);

    if (mi && mi->GetMolinfo().IsSetBiomol() && mi->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_genomic) {

        const CSeq_annot* annot = nullptr;
        CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
        if (bioseq->IsSetAnnot()) {
            ITERATE(CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
                if ((*annot_it)->IsFtable()) {
                    annot = *annot_it;
                    break;
                }
            }
        }

        if (annot) {

            ITERATE(CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {

                if ((*feat)->IsSetData()) {
                    CSeqFeatData::ESubtype subtype = (*feat)->GetData().GetSubtype();
                    
                    if (subtype == CSeqFeatData::eSubtype_rRNA || subtype == CSeqFeatData::eSubtype_otherRNA) {
                        
                        m_Objs[kMoltypeMismatch].Add(*context.NewDiscObj(bioseq, eKeepRef, true));
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
        NON_CONST_ITERATE(CSeq_descr::Tdata, descr, descrs.Set()) {
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
    NON_CONST_ITERATE(TReportObjectList, it, list) {
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
        ITERATE(CUser_field::TData::TStrs, s, field.GetData().GetStrs()) {
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
 CReportNode& collector, 
 CReportNode& previously_seen,
 CDiscrepancyContext& context,
 const string& object_name,
 const string& field_prefix = kEmptyStr)
{
    if (!d) {
        // add missing for all previously seen fields
        ITERATE(CReportNode::TNodeMap, z, previously_seen[kPreviouslySeenFields].GetMap()) {
            collector[field_prefix + z->first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z->first]
                .Add(*context.NewDiscObj(seq, eKeepRef), false);
        }
        return;
    }
    
    ITERATE(CUser_object::TData, f, d->GetUser().GetData()) {
        if ((*f)->IsSetLabel() && (*f)->GetLabel().IsStr() && (*f)->IsSetData()) {
            string field_name = field_prefix + (*f)->GetLabel().GetStr();
            // add missing field to all previous objects that do not have this field
            if (!collector.Exist(field_name)) {
                ITERATE(TReportObjectList, ro, previously_seen[kPreviouslySeenObjects].GetObjects()) {
                    string missing_label = "[n] " + object_name + "[s] [is] missing field " + field_name;
                    CRef<CDiscrepancyObject> seq_disc_obj(dynamic_cast<CDiscrepancyObject*>(ro->GetNCPointer()));
                    collector[field_name][missing_label].Add(*seq_disc_obj, false);
                }
            }
            collector[field_name]
                ["[n] " + object_name + "[s] [has] field " + field_name + " value '" + GetFieldValueAsString(**f) + "'"]
                .Add(*context.NewDiscObj(d), false);
            previously_seen[kPreviouslySeenFieldsThis][(*f)->GetLabel().GetStr()].Add(*context.NewDiscObj(d), false);
            previously_seen[kPreviouslySeenFields][(*f)->GetLabel().GetStr()].Add(*context.NewDiscObj(d), false);
        }
    }
    // add missing for all previously seen fields not on this object
    ITERATE(CReportNode::TNodeMap, z, previously_seen[kPreviouslySeenFields].GetMap()) {
        if (!previously_seen[kPreviouslySeenFieldsThis].Exist(z->first)) {
            collector[field_prefix + z->first][" [n] " + object_name + "[s] [is] missing field " + field_prefix + z->first]
                .Add(*context.NewDiscObj(d), false);
        }
    }

    // maintain object list for missing fields
    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(d, eKeepRef));
    previously_seen[kPreviouslySeenObjects].Add(*this_disc_obj, false);

    previously_seen[kPreviouslySeenFieldsThis].clear();
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(INCONSISTENT_DBLINK, CSeq_inst, eDisc, "Inconsistent DBLink fields")
//  ----------------------------------------------------------------------------
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
        m_Objs[kMissingDBLink].Add(*context.NewDiscObj(seq, eKeepRef), false);
        // add missing for all previously seen fields
        AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), seq, m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
    }
    while (d) {
        if (d->GetUser().GetObjectType() != CUser_object::eObjectType_DBLink) {
            ++d;
            continue;
        }
        CConstRef<CSeqdesc> dr(&(*d));
        AddUserObjectFieldItems(dr, seq, m_Objs[kDBLinkCollect], m_Objs[kDBLinkObjectList], context, "DBLink object");
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
    ITERATE(CReportNode::TNodeMap, s, node.GetMap()) {
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
    NON_CONST_ITERATE(CReportNode::TNodeMap, s, node.GetMap()) {
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
    NON_CONST_ITERATE(CReportNode::TNodeMap, s, original.GetMap()){
        NON_CONST_ITERATE(TReportObjectList, q, s->second->GetObjects()) {
            new_home[s->first].Add(**q);
        }
    }
    NON_CONST_ITERATE(TReportObjectList, q, original.GetObjects()) {
        new_home.Add(**q);
    }
}

void AddSubFieldReport(CReportNode& node, const string& field_name, const string& top_label, CReportNode& m_Objs)
{
    bool this_present = true;
    bool this_same = true;
    AnalyzeField(node, this_present, this_same);
    string new_label = field_name + " " + GetSummaryLabel(this_present, this_same);
    NON_CONST_ITERATE(CReportNode::TNodeMap, s, node.GetMap()){
        NON_CONST_ITERATE(TReportObjectList, q, s->second->GetObjects()) {
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


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(INCONSISTENT_DBLINK)
//  ----------------------------------------------------------------------------
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

    NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs[kDBLinkCollect].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*(it->second), this_present, this_same);
        string new_label = AdjustDBLinkFieldName(it->first) + " " + GetSummaryLabel(this_present, this_same);
        NON_CONST_ITERATE(CReportNode::TNodeMap, s, it->second->GetMap()){
            NON_CONST_ITERATE(TReportObjectList, q, s->second->GetObjects()) {
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

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(INCONSISTENT_STRUCTURED_COMMENTS, CSeq_inst, eDisc, "Inconsistent structured comments")
//  ----------------------------------------------------------------------------
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
        
        m_Objs[kStructuredCommentObservedPrefixesThis][prefix].Add(*context.NewDiscObj(dr, eKeepRef), false);

        AddUserObjectFieldItems(dr, seq, m_Objs[kStructuredCommentReport],
                                m_Objs[kStructuredCommentPrevious], context, 
                                prefix + " structured comment", kStructuredCommentFieldPrefix);
        ++d;
    }

    //report prefixes seen previously, not found on this sequence
    ITERATE(CReportNode::TNodeMap, it, m_Objs[kStructuredCommentObservedPrefixes].GetMap()) {
        if (!m_Objs[kStructuredCommentObservedPrefixesThis].Exist(it->first)) {
            m_Objs["[n] Bioseq[s] [is] missing " + it->first + " structured comment"]
                .Add(*context.NewDiscObj(seq, eKeepRef), false);
            AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), seq, m_Objs[kStructuredCommentReport],
                m_Objs[kStructuredCommentPrevious], context,
                it->first + " structured comment", kStructuredCommentFieldPrefix);
        }
    }

    // report prefixes found on this sequence but not on previous sequences
    ITERATE(CReportNode::TNodeMap, it, m_Objs[kStructuredCommentObservedPrefixesThis].GetMap()) {
        if (!m_Objs[kStructuredCommentObservedPrefixes].Exist(it->first)) {
            ITERATE(TReportObjectList, ro, m_Objs[kStructuredCommentsSeqs].GetObjects()) {
                const CBioseq* this_seq = dynamic_cast<const CBioseq*>((*ro)->GetObject().GetPointer());       
                CConstRef<CBioseq> this_seq_r(this_seq);
                m_Objs["[n] Bioseq[s] [is] missing " + it->first + " structured comment"]
                    .Add(*context.NewDiscObj(this_seq_r, eKeepRef), false);
                AddUserObjectFieldItems(CConstRef<CSeqdesc>(NULL), this_seq_r, m_Objs[kStructuredCommentReport],
                    m_Objs[kStructuredCommentPrevious], context,
                    it->first + " structured comment", kStructuredCommentFieldPrefix);
            }
        }
        // add to list of previously observed prefixes, for next time
        m_Objs[kStructuredCommentObservedPrefixes][it->first].Add(*context.NewDiscObj(seq, eKeepRef), false);
    }

    m_Objs[kStructuredCommentObservedPrefixesThis].clear();
    m_Objs[kStructuredCommentsSeqs].Add(*context.NewDiscObj(seq, eKeepRef), false);
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(INCONSISTENT_STRUCTURED_COMMENTS)
//  ----------------------------------------------------------------------------
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

    NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs[kStructuredCommentReport].GetMap()) {
        bool this_present = true;
        bool this_same = true;
        AnalyzeField(*(it->second), this_present, this_same);
        string new_label = it->first + " " + GetSummaryLabel(this_present, this_same);
        NON_CONST_ITERATE(CReportNode::TNodeMap, s, it->second->GetMap()) {
            string sub_label = s->first;
            if (this_present && this_same) {
                NStr::ReplaceInPlace(sub_label, "[n]", "All");
            }
            NON_CONST_ITERATE(TReportObjectList, q, s->second->GetObjects()) {
                m_Objs[top_label][new_label][sub_label].Add(**q);
            }
        }
    }
    m_Objs.GetMap().erase(kStructuredCommentReport);


    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_STRUCTURED_COMMENT
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_STRUCTURED_COMMENT, CSeq_inst, eDisc, "Structured comment not included")
//  ----------------------------------------------------------------------------
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
        m_Objs["[n] sequence[s] [does] not include structured comments."].Add(*context.NewDiscObj(seq), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_STRUCTURED_COMMENT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_PROJECT
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_PROJECT, CSeq_inst, eDisc, "Project not included")
//  ----------------------------------------------------------------------------
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) return;

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*seq);
    bool has_project = false;
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_DBLink) {
            if (d->GetUser().IsSetData()) {
                ITERATE(CUser_object::TData, it, d->GetUser().GetData()) {
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
        m_Objs["[n] sequence[s] [does] not include project."].Add(*context.NewDiscObj(seq), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_PROJECT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_UNVERIFIED
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(COUNT_UNVERIFIED, CSeq_inst, eOncaller, "Count number of unverified sequences")
//  ----------------------------------------------------------------------------
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
        m_Objs["[n] sequence[s] [is] unverified"].Add(*context.NewDiscObj(seq), false);
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(COUNT_UNVERIFIED)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }

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
        m_Objs[kDeflineExists].Add(*context.NewDiscObj(bioseq));
    }
}


DISCREPANCY_SUMMARIZE(DEFLINE_PRESENT)
{
    if (m_Objs.empty()) {
        return;
    }
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

DISCREPANCY_CASE(UNUSUAL_NT, CSeq_inst, eDisc, "Sequence contains unusual nucleotides")
{
    if (obj.IsNa()) {

        if (obj.IsSetSeq_data()) {

            if (ContainsUnusualNucleotide(obj.GetSeq_data())) {
                m_Objs[kUnusualNT].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
            }
        }
        else if (obj.IsSetExt() && obj.GetExt().IsDelta()) {

            const CSeq_ext::TDelta& deltas = obj.GetExt().GetDelta();
            if (deltas.IsSet()) {

                ITERATE(CDelta_ext::Tdata, delta, deltas.Get()) {

                    if ((*delta)->IsLiteral() && (*delta)->GetLiteral().IsSetSeq_data()) {

                        if (ContainsUnusualNucleotide((*delta)->GetLiteral().GetSeq_data())) {
                            m_Objs[kMoreThan14NRuns].Add(*context.NewDiscObj(context.GetCurrentBioseq()), false);
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
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TAXNAME_NOT_IN_DEFLINE

const string kNoTaxnameInDefline = "[n] defline[s] [does] not contain the complete taxname";

DISCREPANCY_CASE(TAXNAME_NOT_IN_DEFLINE, CSeq_inst, eDisc | eOncaller, "Complete taxname should be present in definition line")
{
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) {
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

            SIZE_TYPE taxname_pos = NStr::FindNoCase(taxname, title_str);
            if (taxname_pos == NPOS) {
                no_taxname_in_defline = true;
            }
            else {
                //capitalization must match for all but the first letter
                no_taxname_in_defline = NStr::CompareNocase(taxname.c_str(), 1, taxname.size() - 1, title_str.c_str() + 1) != 0;

                if (taxname_pos > 0 && !isspace(title_str[taxname_pos - 1]) && !ispunct(title_str[taxname_pos - 1])) {
                    no_taxname_in_defline = true;
                }
            }
        }

        if (no_taxname_in_defline) {
            m_Objs[kNoTaxnameInDefline].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&(*title))), false);
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

        ITERATE(CUser_object::TData, field, user.GetData()) {
            if ((*field)->IsSetData() && (*field)->GetData().IsInt() && (*field)->IsSetLabel() && (*field)->GetLabel().IsStr() && (*field)->GetLabel().GetStr() == "ProjectID") {
                res = NStr::IntToString((*field)->GetData().GetInt());
                break;
            }
        }
    }

    return res;
}

DISCREPANCY_CASE(HAS_PROJECT_ID, CSeq_inst, eOncaller, "Sequences with project IDs (looks for genome project IDs)")
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
                m_Objs[kProjId][proj_id].Add(*context.NewDiscObj(seq, eKeepRef), false);
            }
        }
    }
}

typedef map<string, list<CConstRef<CBioseq> > > TBioseqMapByID;

static void CollectSequencesByProjectID(CReportNode& bioseqs, TBioseqMapByID& prots, TBioseqMapByID& nucs)
{
    NON_CONST_ITERATE(CReportNode::TNodeMap, node, bioseqs.GetMap()) {

        ITERATE(TReportObjectList, bioseq, node->second->GetObjects()) {
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
        ITERATE(TBioseqMapByID, proj, projs) {
            ITERATE(list<CConstRef<CBioseq> >, bioseq, proj->second) {
                report[what].Add(*context.NewDiscObj(*bioseq), false);
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
