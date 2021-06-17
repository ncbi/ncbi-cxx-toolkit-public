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
 * Authors:  Justin Foley
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <objtools/edit/external_annots.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/iterator.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>

#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annot_id.hpp>

#include <objmgr/annot_selector.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_annot_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

void CAnnotGetter::AddAnnotations(const SAnnotSelector& sel,
                               CScope& scope,
                               CRef<CSeq_entry> seq_entry)
{
    if (seq_entry.IsNull()) {
        return;
    }

    if (seq_entry->IsSeq()) {
        x_AddAnnotations(sel, scope, seq_entry->SetSeq());
        return;
    }


    CBioseq_set& set = seq_entry->SetSet();
    for (CRef<CSeq_entry> sub_entry : set.SetSeq_set()) {
        AddAnnotations(sel, scope, sub_entry);
    }
}


void CAnnotGetter::x_AddAnnotations(const SAnnotSelector& sel,
    CScope& scope,
    CBioseq& bioseq)
{
    const CSeq_id* id = bioseq.GetFirstId();
    if (!id) {
        return;
    }

    CBioseq_Handle bsh = scope.GetBioseqHandle(*id);
    if (!bsh) {
        return;
    }

    CAnnot_CI annot_it(CFeat_CI(bsh, sel));
    for (; annot_it; ++annot_it) {
        CRef<CSeq_annot> new_annot = x_GetCompleteSeqAnnot(*annot_it);
        bioseq.SetAnnot().push_back(new_annot);
    }
}

CRef<CSeq_annot> CAnnotGetter::x_GetCompleteSeqAnnot(const CSeq_annot_Handle& annot_handle)
{
    CConstRef<CSeq_annot> annot = annot_handle.GetCompleteSeq_annot();

    CRef<CSeq_annot> new_annot = Ref(new CSeq_annot());
    if (annot->IsSetId()) {
        for (CRef<CAnnot_id> id : annot->GetId()) {
            new_annot->SetId().push_back(id);
        }
    }

    if (annot->IsSetDb()) {
        new_annot->SetDb() = annot->GetDb();
    }

    if (annot->IsSetName()) {
        new_annot->SetName() = annot->GetName();
    }

    if (annot->IsSetDesc()) {
        new_annot->SetDesc().Assign(annot->GetDesc());
    }

    CFeat_CI feat_it(annot_handle);
    for (; feat_it; ++feat_it) {
        CRef<CSeq_feat> feat = Ref(new CSeq_feat());
        feat->Assign(feat_it->GetMappedFeature());
        new_annot->SetData().SetFtable().push_back(feat);
    }
    return new_annot;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

