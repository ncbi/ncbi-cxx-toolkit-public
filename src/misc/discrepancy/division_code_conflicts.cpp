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
 * Authors: Igor Filippov based on Sema Kachalo's template
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/macro/String_constraint.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(division_code_conflicts);


// DIVISION_CODE_CONFLICTS

static const string kDivCodeConfl = "Division code conflicts found";

DISCREPANCY_CASE(DIVISION_CODE_CONFLICTS, SEQUENCE, eOncaller, "Division Code Conflicts")
{
    const CSeqdesc* src = context.GetBiosource();
    if (src && src->GetSource().IsSetOrg() && src->GetSource().GetOrg().IsSetOrgname() && src->GetSource().GetOrg().GetOrgname().IsSetDiv() && !src->GetSource().GetOrg().GetOrgname().GetDiv().empty()) {
        m_Objs[kDivCodeConfl]["[n] bioseq[s] [has] division code " + src->GetSource().GetOrg().GetOrgname().GetDiv()].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(DIVISION_CODE_CONFLICTS)
{
    if (m_Objs[kDivCodeConfl].GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
