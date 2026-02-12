/* $Id$
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
 * Author:  Eyal Mozes
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuGiLookup.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Dense_seg.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

CScope &CDGiLookup::x_GetScope()
{
    if (!m_Scope) {
        CRef<CObjectManager> om(CObjectManager::GetInstance());
        CGBDataLoader::RegisterInObjectManager(*om);

        m_Scope.Reset(new CScope(*om));
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}

TGi CDGiLookup::x_LookupGi(const CSeq_id &id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if (m_GiIndex.count(idh)) {
        return m_GiIndex[idh];
    }

    CSeq_id_Handle gi = sequence::GetId(idh, x_GetScope(),
                                        sequence::eGetId_ForceGi);
    return gi.GetGi();
}

void CDGiLookup::x_IndexGi(const CBioseq& bioseq)
{
    CSeq_id_Handle existing_gi =
        sequence::GetId(bioseq, sequence::eGetId_ForceGi);
    if (!existing_gi) {
        return;
    }

    for (const CRef<CSeq_id> &id : bioseq.GetId()) {
        if (!id->IsGi()) {
            m_GiIndex[CSeq_id_Handle::GetHandle(*id)] = existing_gi.GetGi();
        }
    }
}

void CDGiLookup::x_ReportGiNotFound(const CSeq_id &id, bool on_bioseq)
{
   switch (m_Policy) {
   case eLeaveUnchanged:
       ERR_POST(Warning << "Can't find gi for " << id.AsFastaString()
                        << "; leaving unchanged");
       break;

   case eRemove:
       ERR_POST(Warning << "Can't find gi for " << id.AsFastaString()
                        << "; removing from set of "
                        << (on_bioseq ? "bioseqs" : "alignments"));
       break;

   case eThrow:
       NCBI_THROW(CException, eUnknown, "Can't find gi for "
                         + id.AsFastaString());
   }
}

void CDGiLookup::InsertGis(vector< CRef< CBioseq > >& bioseqs)
{
    for (vector< CRef< CBioseq > >::iterator it = bioseqs.begin();
         it != bioseqs.end(); ++it)
    {
        CBioseq &bioseq = **it;
        CSeq_id_Handle existing_gi =
            sequence::GetId(bioseq, sequence::eGetId_ForceGi);
        if (!existing_gi) {
            TGi new_gi = x_LookupGi(*bioseq.GetNonLocalId());
            if (new_gi == ZERO_GI) {
                x_ReportGiNotFound(*bioseq.GetNonLocalId(), true);
                if (m_Policy == eRemove) {
                    bioseqs.erase(it--);
                }
                continue;
            }
            CRef<CSeq_id> gi_id(new CSeq_id(CSeq_id::e_Gi, new_gi));
            bioseq.SetId().push_back(gi_id);
        }
        x_IndexGi(bioseq);
    }
}

void CDGiLookup::InsertGis(CSeq_align_set& alignments)
{
    for (CSeq_align_set::Tdata::iterator it = alignments.Set().begin();
         it != alignments.Set().end(); )
    {
        CSeq_align &align = **it;
        if (!align.GetSegs().IsDenseg()) {
            NCBI_THROW(CException, eUnknown,
                       "Only Dense-seg alignments supported");
        }
        bool not_found = false;
        for (CRef<CSeq_id> &id : align.SetSegs().SetDenseg().SetIds()) {
            if (!id->IsGi() && !id->IsLocal() && 
                (!id->IsPdb() || m_ReplacePDBs) &&
                (!id->IsGeneral() || id->GetGeneral().GetDb() != "Cdd"))
            {
                TGi new_gi = x_LookupGi(*id);
                if (new_gi == ZERO_GI) {
                    x_ReportGiNotFound(*id, false);
                    not_found = true;
                } else {
                    id->SetGi(new_gi);
                }
            }
        }
        if (not_found && m_Policy == eRemove) {
            it = alignments.Set().erase(it);
        } else {
            ++it;
        }
    }
}

void CDGiLookup::IndexGis(const vector< CRef< CBioseq > >& bioseqs)
{
    for (const CRef<CBioseq> &bioseq : bioseqs) {
        x_IndexGi(*bioseq);
    }
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
