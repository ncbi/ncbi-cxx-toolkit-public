/*
 * $Id$
 *
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *            National Center for Biotechnology Information (NCBI)
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government do not place any restriction on its use or reproduction.
 *  We would, however, appreciate having the NCBI and the author cited in
 *  any work or product based on this material
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 * ===========================================================================
 *
 * Author: Jie Chen
 *
 * File Description:
 *   autofix
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include <misc/discrepancy_report/hDiscRep_autofix.hpp>
#include <misc/discrepancy_report/hDiscRep_tests.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(DiscRepNmSpc)

USING_NCBI_SCOPE;
USING_SCOPE(objects);

void MarkOverlappingCDSs(vector <CRef <CObject> >& objs, vector <string>& msgs, CScope* scope)
{
  string kOverlappingCDSNoteText("overlaps another CDS with the same product name");
  string strtmp; 
  bool has_title = false;
  if (!objs.empty()) {
      msgs.push_back("Added \"overlaps another CDS with the same product name\" to CDS note for overlapping CDSs with similar product names\n");
      has_title = true;
  }
  NON_CONST_ITERATE (vector <CRef <CObject> >, it, objs) {
    CSeq_feat* feat = dynamic_cast<CSeq_feat*>((*it).GetPointer());
    if (feat) {
       if (feat->GetData().IsCdregion() && feat->CanGetComment()) {
          strtmp = feat->GetComment();
          if (NStr::FindNoCase(strtmp, kOverlappingCDSNoteText) == string::npos) {
             strtmp += ("; " + kOverlappingCDSNoteText);
             feat->SetComment(strtmp);
          }
       }
    }

    strtmp 
       = GetDiscrepancyItemText (*(dynamic_cast<const CSeq_feat*>((*it).GetPointer())), scope);
    msgs.push_back ("Added overlapping CDS note to " + strtmp + "\n");
  }
  if (has_title) msgs.push_back("\n");
};

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE
