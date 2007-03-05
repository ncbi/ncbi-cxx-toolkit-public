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
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/data_loader.hpp>

#include <serial/objostr.hpp>
#include <serial/serial.hpp>


#include "sample_saver.hpp"

//#include <fstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSampleEditSaver::CSampleEditSaver(const string& db_path)
    : m_AsnDBGuard(new CAsnDB(db_path)), m_AsnDB(*m_AsnDBGuard)
{
}
CSampleEditSaver::CSampleEditSaver(CAsnDB& db)
    : m_AsnDB(db)

{
}

CSampleEditSaver::~CSampleEditSaver()
{
}


void CSampleEditSaver::BeginTransaction()
{
    x_CleanUp();
}
void CSampleEditSaver::CommitTransaction()
{
    TBlobCont::const_iterator it;
    for (it = m_Blobs.begin(); it != m_Blobs.end(); ++it) {
        const CBlobIdKey& id = it->first;
        const TBioseqCont& cont = it->second;
        TBioseqCont::const_iterator bit;
        for (bit = cont.begin(); bit != cont.end(); ++bit) {
            const CBioseq_Handle& bh = *bit;
            CConstRef<CBioseq> bioseq = bh.GetCompleteBioseq();
            m_AsnDB.Save(id.ToString(), *bioseq);
        }
    }
    TTSECont::const_iterator itt;
    for( itt = m_TSEs.begin(); itt != m_TSEs.end(); ++itt) {
        const CSeq_entry_Handle& sh = *itt;
        CBlobIdKey id = sh.GetTSE_Handle().GetBlobId();
        CConstRef<CSeq_entry> entry = sh.GetCompleteSeq_entry();
        m_AsnDB.SaveTSE(id.ToString(), *entry);
    }

    x_CleanUp();
}
void CSampleEditSaver::RollbackTransaction()
{
    x_CleanUp();
}

void CSampleEditSaver::UpdateSeq(const CBioseq_Handle& handle,
                                 IEditSaver::ECallMode)
{
    CSeq_entry_Handle entry = handle.GetParentEntry();
    CBlobIdKey id = entry.GetTSE_Handle().GetBlobId();
    TBioseqCont& bioseqs = m_Blobs[id];
    bioseqs.insert(handle);
}

void CSampleEditSaver::UpdateTSE(const CSeq_entry_Handle& handle, 
                                 IEditSaver::ECallMode mode)
{
    m_TSEs.insert(handle);
}

void CSampleEditSaver::x_CleanUp()
{
    m_Blobs.clear();
    m_TSEs.clear();
}

END_SCOPE(objects)
END_NCBI_SCOPE
