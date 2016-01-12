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
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/seqfeat_macros.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objects/macro/String_constraint.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(transl_too_long);


// TRANSL_TOO_LONG

DISCREPANCY_CASE(TRANSL_TOO_LONG, CSeqFeatData, eDisc, "Transl_except longer than 3")
{

    if (!obj.IsCdregion() || !obj.GetCdregion().IsSetCode_break()) {
            return;
    }
    bool found = false;
    FOR_EACH_CODEBREAK_ON_CDREGION(code_break, obj.GetCdregion())
    {
        if ((*code_break)->IsSetLoc() && (*code_break)->IsSetAa())
        {
            int length = (*code_break)->GetLoc().GetTotalRange().GetLength();
            char aa = 0;
            if ((*code_break)->GetAa().IsNcbieaa())
            {
                aa = (*code_break)->GetAa().GetNcbieaa();
            }
            else if ((*code_break)->GetAa().IsNcbi8aa())
            {
                aa = (*code_break)->GetAa().GetNcbi8aa();
                vector<char> n(1, aa);
                vector<char> i;
                CSeqConvert::Convert(n, CSeqUtil::e_Ncbi8aa, 0, 1, i, CSeqUtil::e_Ncbieaa);
                aa = i.front();
            }
            else if ((*code_break)->GetAa().IsNcbistdaa())
            {
                aa = (*code_break)->GetAa().GetNcbistdaa();
                vector<char> n(1, aa);
                vector<char> i;
                CSeqConvert::Convert(n, CSeqUtil::e_Ncbistdaa, 0, 1, i, CSeqUtil::e_Ncbieaa);
                aa = i.front();
            }
            if (length > 3 && aa == '*')
            {
                found = true;
                break;
            }
        }
    }
    if (found)
        m_Objs["[n] feature[s] [has] translation exception[s] longer than 3 bp"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
}


DISCREPANCY_SUMMARIZE(TRANSL_TOO_LONG)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
