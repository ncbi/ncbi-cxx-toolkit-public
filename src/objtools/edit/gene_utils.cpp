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
* Author: J. Chen, NCBI
*
* File Description:
*   functions for editing and working with genes
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objtools/edit/gene_utils.hpp> // need to add this in order to make export work

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CConstRef <CSeq_feat> GetGeneForFeature(const CSeq_feat& feat, CScope& scope)
{
  const CGene_ref* gene = feat.GetGeneXref();
  if (gene && gene->IsSuppressed()) {
    return (CConstRef <CSeq_feat> ());
  }

  if (gene) {
     CBioseq_Handle
       bioseq_hl = sequence::GetBioseqFromSeqLoc(feat.GetLocation(), scope);
     if (!bioseq_hl) {
        return (CConstRef <CSeq_feat> ());
     }
     CTSE_Handle tse_hl = bioseq_hl.GetTSE_Handle();
     if (gene->CanGetLocus_tag() && !(gene->GetLocus_tag().empty()) ) {
         CSeq_feat_Handle
             seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus_tag(), true);
         if (seq_feat_hl) {
              return (seq_feat_hl.GetOriginalSeq_feat());
         }
     }
     else if (gene->CanGetLocus() && !(gene->GetLocus().empty())) {
         CSeq_feat_Handle
              seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus(), false);
         if (seq_feat_hl) {
            return (seq_feat_hl.GetOriginalSeq_feat());
         }
     }
     else return (CConstRef <CSeq_feat>());
  }
  else {
   return(
     CConstRef <CSeq_feat>(sequence::GetBestOverlappingFeat(feat.GetLocation(),
                                                  CSeqFeatData::e_Gene,
                                                  sequence::eOverlap_Contained,
                                                  scope)));
  }

  return (CConstRef <CSeq_feat>());
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

