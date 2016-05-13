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
#include <objects/valid/comment_rule.hpp>
#include <objmgr/seqdesc_ci.hpp>

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
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();
    if (!seq) return;
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
