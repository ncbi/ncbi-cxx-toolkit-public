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
 * Authors: Oleksii Drozd based on Sema Kachalo's template
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

DISCREPANCY_MODULE(transl_note);

struct CFeatureInspectResults
{
    bool m_hasNote;
    bool m_hasException;
};

CFeatureInspectResults InspectSeqFeat(const CSeq_feat& obj)
{
    CFeatureInspectResults result = { false, false };
    static const string TRANSL_NOTE("TAA stop codon is completed by the addition of 3' A residues to the mRNA");
    if (obj.IsSetComment() && NStr::Find(obj.GetComment(), TRANSL_NOTE) != string::npos) {
        result.m_hasNote = true;
    }
    TSeqPos feature_stop;
    if (obj.IsSetData() && obj.CanGetData())
    {
        if (obj.IsSetLocation())
        {
            feature_stop = obj.GetLocation().GetStop(eExtreme_Biological);
        }
        const CSeqFeatData &data = obj.GetData();
        if (data.IsCdregion() && data.GetCdregion().IsSetCode_break())
        FOR_EACH_CODEBREAK_ON_CDREGION(code_break, data.GetCdregion())
        {
            if ((*code_break)->IsSetLoc() && (*code_break)->IsSetAa())
            {
                int length = (*code_break)->GetLoc().GetTotalRange().GetLength();
                TSeqPos stop_pos = (*code_break)->GetLoc().GetStop(eExtreme_Biological);
                if (length < 3 
                    && stop_pos == feature_stop) //and the location is at the end of the feature
                {
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
                    if (aa == '*') //Look for coding regions that have a translation exception (Cdregion.code-break) where aa is * (42)
                    {
                        result.m_hasException = true;
                        break;
                    }
                }
            }
        }
    }
    return result;
}

// TRANSL_NO_NOTE

DISCREPANCY_CASE(TRANSL_NO_NOTE, CSeq_feat, eAll, "Transl_except without Note")
{
    CFeatureInspectResults result = InspectSeqFeat(obj);
    if (result.m_hasException && !result.m_hasNote)
    {
        m_Objs["[n] feature[s] [has] a translation exception but no note"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
    }
}
DISCREPANCY_SUMMARIZE(TRANSL_NO_NOTE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

// NOTE_NO_TRANSL

DISCREPANCY_CASE(NOTE_NO_TRANSL, CSeq_feat, eAll, "Note without Transl_except")
{
    CFeatureInspectResults result = InspectSeqFeat(obj);
    if (result.m_hasNote && !result.m_hasException)
    {
        m_Objs["[n] feature[s] [has] a note but not translation exception"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
    }
}
DISCREPANCY_SUMMARIZE(NOTE_NO_TRANSL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
