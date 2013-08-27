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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Front-end class for making remote request to MLA
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objtools/format/context.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/mla/mla_client.hpp>

#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>

#include <objmgr/object_manager.hpp>

#include "remote_updater.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRemoteUpdater::CRemoteUpdater()
{
}

CRemoteUpdater::~CRemoteUpdater()
{
}


void UpdatePub(CMLAClient& mlaClient, CPub& pub)
{
    CRef<CPub> new_pub;

    try {
        switch(pub.Which()) {
        case CPub::e_Pmid:
            {
                const int pmid = pub.GetPmid().Get();

                CPubMedId req(pmid);
                CMLAClient::TReply reply;
                new_pub = mlaClient.AskGetpubpmid(req, &reply);
                pub.Assign(*new_pub);
            }
            break;
        case CPub::e_Muid:
            {
                const int muid = pub.GetMuid();

                const int pmid = mlaClient.AskUidtopmid(muid);
                if( pmid > 0 ) {
                    CPubMedId req(pmid);
                    CMLAClient::TReply reply;
                    new_pub = mlaClient.AskGetpubpmid(req, &reply);
                    pub.Assign(*new_pub);
                }
            }
            break;
        default:
            // ignore if type unknown
            break;
        }
    } catch(...) {
        // don't worry if we can't look it up
    }

    if( new_pub ) {
        // authors come back in a weird format that we need
        // to convert to ISO
        //x_ChangeMedlineAuthorsToISO(new_pub);

        //new_pubs.push_back(new_pub);
    }
}

void CRemoteUpdater::UpdatePubReferences(CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            UpdatePubReferences(**it);
        }
    }

    if (!entry.IsSetDescr())
        return;

    //CRef<CObjectManager> mgr(CObjectManager::GetInstance());

    //CScope scope(*mgr);

    //CSeq_entry_Handle entry_h = scope.AddTopLevelSeqEntry(entry);

    NON_CONST_ITERATE(CSeq_descr::Tdata, it, entry.SetDescr().Set())
    {
        if ((**it).IsPub())
        {
            NON_CONST_ITERATE( CPub_equiv::Tdata, pubit, (**it).SetPub().SetPub().Set())
            {
                if (mlaclient.Empty())
                    mlaclient.Reset(new CMLAClient);

                UpdatePub(*mlaclient, **pubit);
            }
        }
    }
}

END_NCBI_SCOPE
