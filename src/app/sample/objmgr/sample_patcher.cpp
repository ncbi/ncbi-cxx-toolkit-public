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
 * Author:  Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/impl/tse_info.hpp>

#include "sample_patcher.hpp"



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSampleAssigner::CSampleAssigner(const CAsnDB& db)
    : m_AsnDB(db)
{
}

void CSampleAssigner::LoadBioseq(CTSE_Info& tse, const TPlace& place, 
                               CRef<CSeq_entry> entry)
{
    if (entry->IsSeq()) {
        CBioseq& bioseq = entry->SetSeq();
        ITERATE(CBioseq::TId, it, bioseq.GetId()) {
            const CTSE_Info::TBlobId& blob = tse.GetBlobId();
            CRef<CBioseq> db_bioseq = m_AsnDB.Restore(blob.ToString(), **it);
            if ( db_bioseq ) {
                bioseq.Assign(*db_bioseq);
                // NcbiCout << (*it)->GetSeqIdString() << " ";
            }
        }
        //NcbiCout  << NcbiEndl;
    }
    CTSE_Default_Assigner::LoadBioseq(tse, place, entry);
}

//////////////////////////////////////////////////////////////////////////////////////
CSampleDataPatcher::CSampleDataPatcher(const string& db_path)
    : m_AsnDBGuard(new CAsnDB(db_path)), m_AsnDB(*m_AsnDBGuard)
{
}
CSampleDataPatcher::CSampleDataPatcher(const CAsnDB& db)
    : m_AsnDB(db)
{
}

CRef<ITSE_Assigner> CSampleDataPatcher::GetAssigner()
{
    if (!m_Assigner) {
        m_Assigner.Reset(new CSampleAssigner(m_AsnDB));
        m_Assigner->SetSeqIdTranslator(GetSeqIdTranslator());
    }
    return m_Assigner;
}


IDataPatcher::EPatchLevel CSampleDataPatcher::IsPatchNeeded(const CTSE_Info& tse)
{
    if ( m_AsnDB.HasWholeBlob(tse.GetBlobId().ToString()) )
        return eWholeTSE;
    if ( m_AsnDB.HasBlob(tse.GetBlobId().ToString()) )
        return ePartTSE;
    return eNone;
}

void CSampleDataPatcher::Patch(const CTSE_Info& tse, CSeq_entry& entry)
{
    const CTSE_Info::TBlobId& blob = tse.GetBlobId();
    CRef<CSeq_entry> en = m_AsnDB.RestoreTSE(blob.ToString());
    if (en)
        entry.Assign(*en);
    DoPatch(tse, entry);
    IDataPatcher::Patch(tse, entry);
}

void CSampleDataPatcher::DoPatch(const CTSE_Info& tse, CSeq_entry& entry)
{
    if (entry.IsSeq()) {
        DoPatch(tse, entry.SetSeq());
    }
    if (entry.IsSet()) {
        CBioseq_set& bs = entry.SetSet();
        if (bs.IsSetSeq_set()) {
            NON_CONST_ITERATE( CBioseq_set::TSeq_set, it, bs.SetSeq_set()) {
                DoPatch(tse, **it);
            }
        }
    }
}

void CSampleDataPatcher::DoPatch(const CTSE_Info& tse, CBioseq& bioseq)
{
    ITERATE(CBioseq::TId, it, bioseq.GetId()) {
        const CTSE_Info::TBlobId& blob = tse.GetBlobId();
        CRef<CBioseq> db_bioseq = m_AsnDB.Restore(blob.ToString(), **it);
        if ( db_bioseq ) {
            bioseq.Assign(*db_bioseq);
            return;
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/11/16 21:11:55  didenko
 * Fixed IDataPatcher and Patcher loader so they can corretly handle a whole TSE replacement
 *
 * Revision 1.1  2005/11/15 19:25:22  didenko
 * Added bioseq_edit_sample sample
 *
 * ===========================================================================
 */
