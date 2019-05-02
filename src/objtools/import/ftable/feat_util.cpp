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
 * File Description:  
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include "feat_util.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//  ============================================================================
bool
FeatUtil::ContainsData(
    const CSeq_annot& annot)
//  ============================================================================
{
    return (annot.IsFtable()  &&  !annot.GetData().GetFtable().empty());
}


//  ============================================================================
CRef<CSeq_loc>
FeatUtil::AddLocations(
    const CSeq_loc& lhs,
    const CSeq_loc& rhs)
//  ============================================================================
{
    CRef<CSeq_loc> pMerged(new CSeq_loc);
    pMerged->Assign(lhs);
    if (pMerged->IsNull()) {
        if (!rhs.IsNull()) {
            pMerged->Assign(rhs);
        }
    }
    else {
        if (!rhs.IsNull()) {
            pMerged = lhs.Add(rhs, CSeq_loc::fSortAndMerge_All, nullptr);
        }
    }
    return pMerged;
} 
    
//  ============================================================================
CRef<CCode_break>
FeatUtil::MakeCodeBreak(
    const CSeq_id& id,
    const string& codeBreakStr)
//  ============================================================================
{
    CRef<CCode_break> pCodeBreak;

    const string cdstr_start = "(pos:";
    const string cdstr_div = ",aa:";
    const string cdstr_end = ")";
    
    if (!NStr::StartsWith(codeBreakStr, cdstr_start)  
            ||  !NStr::EndsWith(codeBreakStr, cdstr_end)) {
        return pCodeBreak;
    }
    size_t pos_start = cdstr_start.length();
    size_t pos_stop = codeBreakStr.find(cdstr_div);
    string posstr = codeBreakStr.substr(pos_start, pos_stop-pos_start);
    string aaa = codeBreakStr.substr(pos_stop+cdstr_div.length());
    aaa = aaa.substr(0, aaa.length()-cdstr_end.length());

    const string posstr_compl = "complement(";
    ENa_strand strand = eNa_strand_plus;
    if (NStr::StartsWith(posstr, posstr_compl)) {
        posstr = posstr.substr(posstr_compl.length());
        posstr = posstr.substr(0, posstr.length()-1);
        strand = eNa_strand_minus;
    }
    const string posstr_div = "..";
    size_t pos_div = posstr.find(posstr_div);
    if (pos_div == string::npos) {
        return pCodeBreak;
    }

    int from, to;
    try {
        from = NStr::StringToInt(posstr.substr(0, pos_div))-1;
        to = NStr::StringToInt(posstr.substr(pos_div + posstr_div.length()))-1;
    }
    catch(...) {
        return pCodeBreak;
    }
    int aaCode = 'U'; // for now

    pCodeBreak.Reset(new CCode_break);
    pCodeBreak->SetAa().SetNcbieaa(aaCode);
    auto& cbLoc = pCodeBreak->SetLoc().SetInt();
    cbLoc.SetId().Assign(id);
    cbLoc.SetFrom(from);
    cbLoc.SetTo(to);
    cbLoc.SetStrand(strand);
    
    return pCodeBreak;
}

END_SCOPE(objects)
END_NCBI_SCOPE
