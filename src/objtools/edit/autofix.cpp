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

#include <objtools/edit/autofix.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(AutoFix);

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//void MarkOverlappingCDSs(vector <CConstRef <CObject> > ori_objs, vector <CRef <CObject> > fixed_objs, CLogInfo* log )
void MarkOverlappingCDSs(vector <CConstRef <CObject> >& ori_objs, vector <CRef <CObject> >& fixed_objs)
{
  string kOverlappingCDSNoteText("overlaps another CDS with the same product name");
  string strtmp; 
  CRef <CSeq_feat> fixed_feat;
  ITERATE (vector <CConstRef <CObject> >, it, ori_objs) {
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>((*it).GetPointer());
    fixed_feat.Reset(new CSeq_feat);
    if (feat) {
       fixed_feat->Assign(*feat); 
       if (feat->GetData().IsCdregion() && feat->CanGetComment()) {
          strtmp = feat->GetComment();
          if (NStr::FindNoCase(strtmp, kOverlappingCDSNoteText) == string::npos) {
             strtmp += ("; " + kOverlappingCDSNoteText);
             fixed_feat->SetComment(strtmp);
          }
       }
    }
    fixed_objs.push_back(CRef <CObject>(fixed_feat.GetPointer()));

#if 0
    if (lip != NULL && lip->fp != NULL) {
        if (!has_title) {
          fprintf (lip->fp, "Added \"overlaps another CDS with the same product name\" to CDS note for overlapping CDSs with similar product names\n");
          has_title = TRUE;
        }

        feat_txt = GetDiscrepancyItemText (vnp);
        fprintf (lip->fp, "Added overlapping CDS note to %s", feat_txt);
        feat_txt = MemFree (feat_txt);
        lip->data_in_log = TRUE;
    }
#endif 
  }
#if 0
  if (has_title) {
    fprintf (lip->fp, "\n");
  }
#endif
};

END_SCOPE(AutoFix);
END_NCBI_SCOPE
