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
#include <objects/valid/Comment_rule.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include "discrepancy_core.hpp"

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
    if (!seq) {
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

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
