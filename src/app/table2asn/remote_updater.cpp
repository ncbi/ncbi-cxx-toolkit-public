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
*   Front-end class for making remote request to MLA and taxon
* 
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <objects/taxon3/taxon3.hpp>
#include <objects/mla/mla_client.hpp>

#include <objtools/readers/message_listener.hpp>
#include <objtools/format/items/reference_item.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>

#include "remote_updater.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class CCachedTaxon3_impl
{
public:
    typedef map<string, CRef<CT3Reply> > CCachedReplyMap;

    void Init()
    {
        if (m_taxon.get() == 0)
        {
            m_taxon.reset(new CTaxon3);
            m_taxon->Init();
            m_cache.reset(new CCachedReplyMap);
        }
    }

    CRef<COrg_ref> GetOrg(const COrg_ref& org, objects::IMessageListener* logger = 0)
    {
        CRef<COrg_ref> result;
        CRef<CT3Reply> reply = GetOrgReply(org);
        if (reply->IsError() && logger)
        {
            logger->PutError(
                *CLineError::Create(ILineError::eProblem_Unset, eDiag_Warning, "", 0,
                string("Taxon update: ") + 
                (org.IsSetTaxname() ? org.GetTaxname() : NStr::IntToString(org.GetTaxId())) + ": " +
                reply->GetError().GetMessage()));
        }
        else
        if (reply->IsData() && reply->SetData().IsSetOrg())
        {
            result.Reset(&reply->SetData().SetOrg());
        }
        return result;
    }

    CRef<CT3Reply> GetOrgReply(const COrg_ref& in_org)
    {
        string id;

        NStr::IntToString(id, in_org.GetTaxId());
        if (in_org.IsSetTaxname())
            id += in_org.GetTaxname();

        CRef<CT3Reply>& reply = (*m_cache)[id];
        if (reply.Empty())
        {
            CTaxon3_request request;

            CRef<CT3Request> rq(new CT3Request);                    
            CRef<COrg_ref> org(new COrg_ref);
            org->Assign(in_org);
            rq->SetOrg(*org);

            request.SetRequest().push_back(rq);
            CRef<CTaxon3_reply> result = m_taxon->SendRequest (request);
            reply = *result->SetReply().begin();
        }
        else
        {
#ifdef _DEBUG
            //cerr << "Using cache for:" << id << endl;
#endif
        }
        return reply;
    }

    CRef<CTaxon3_reply> SendOrgRefList(const vector< CRef<COrg_ref> >& query, bool use_cache)
    {
        if (!use_cache)
            return m_taxon->SendOrgRefList(query);

        CRef<CTaxon3_reply> result(new CTaxon3_reply);

        ITERATE (vector<CRef< COrg_ref> >, it, query)
        {            
            result->SetReply().push_back(GetOrgReply(**it));
        }

        return result;
    }  
protected:
    auto_ptr<CTaxon3> m_taxon;
    auto_ptr<CCachedReplyMap> m_cache;
};

USING_SCOPE(objects);

namespace
{

int FindPMID(CMLAClient& mlaClient, const CPub_equiv::Tdata& arr)
{
    ITERATE(CPub_equiv::Tdata, item_it, arr)
    {
        if ((**item_it).IsPmid())
        {
            return (**item_it).GetPmid().Get();
        }
        else
        if ((**item_it).IsMuid())
        {
            int id = (**item_it).GetMuid();
            return mlaClient.AskUidtopmid(id);
        }
    }
    return 0;
}

// the method is not used at the momment
void CreatePubPMID(CMLAClient& mlaClient, CPub_equiv::Tdata& arr, int id)
{
    try {
        CPubMedId req(id);
        CRef<CPub> new_pub = mlaClient.AskGetpubpmid(req);
        if (new_pub.NotEmpty())
        {
            // authors come back in a weird format that we need
            // to convert to ISO
            CReferenceItem::ChangeMedlineAuthorsToISO(new_pub);

            arr.clear();
            CRef<CPub> new_pmid(new CPub);
            new_pmid->SetPmid().Set(id);
            arr.push_back(new_pmid);
            arr.push_back(new_pub);
        }
    } catch(...) {
        // don't worry if we can't look it up
    }

}

}// end anonymous namespace


void CRemoteUpdater::UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeqdesc& obj)
{
    if (obj.IsOrg())
    {
        xUpdateOrgTaxname(logger, obj.SetOrg());
    }
    else
    if (obj.IsSource() && obj.GetSource().IsSetOrg())
    {
        xUpdateOrgTaxname(logger, obj.SetSource().SetOrg());
    }
}

void CRemoteUpdater::xUpdateOrgTaxname(objects::IMessageListener* logger, COrg_ref& org)
{
    int taxid = org.GetTaxId();
    if (taxid == 0 && !org.IsSetTaxname())
        return;

    if (m_taxClient.get() == 0)
    {
        m_taxClient.reset(new CCachedTaxon3_impl);
        m_taxClient->Init();
    }
        
    CRef<COrg_ref> new_org = m_taxClient->GetOrg(org, logger);
    if (new_org.NotEmpty())
    {
        if (new_org->IsSetSyn())
            new_org->ResetSyn();

        org.Assign(*new_org);
    }
}

CRemoteUpdater& CRemoteUpdater::GetInstance()
{
    static CRemoteUpdater instance;
    return instance;
}

CRemoteUpdater::CRemoteUpdater(bool enable_caching)
    :m_enable_caching(enable_caching)
{
}

CRemoteUpdater::~CRemoteUpdater()
{
}

void CRemoteUpdater::UpdatePubReferences(objects::CSeq_entry_EditHandle& obj)
{
    for (CBioseq_CI it(obj); it; ++it)
    {
        xUpdatePubReferences(it->GetEditHandle().SetDescr());
    }
}

void CRemoteUpdater::UpdatePubReferences(CSerialObject& obj)
{
    if (obj.GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
    {
        CSeq_entry* entry = (CSeq_entry*)(&obj);
        xUpdatePubReferences(*entry);
    }
    else
    if (obj.GetThisTypeInfo()->IsType(CSeq_submit::GetTypeInfo()))
    {
        CSeq_submit* submit = (CSeq_submit*)(&obj);
        NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, submit->SetData().SetEntrys())
        {
            xUpdatePubReferences(**it);
        }
    }
}

void CRemoteUpdater::xUpdatePubReferences(CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        NON_CONST_ITERATE(CBioseq_set::TSeq_set, it2, entry.SetSet().SetSeq_set())
        {
            xUpdatePubReferences(**it2);
        }
    }

    if (!entry.IsSetDescr())
        return;

    xUpdatePubReferences(entry.SetDescr());
}

void CRemoteUpdater::xUpdatePubReferences(objects::CSeq_descr& seq_descr)
{
    CSeq_descr::Tdata& descr = seq_descr.Set();
    size_t count = descr.size();
    CSeq_descr::Tdata::iterator it = descr.begin();

    for (size_t i=0; i<count; ++it,  ++i)
    {
        if (! ( (**it).IsPub() && (**it).GetPub().IsSetPub() ) )
            continue;

        CPub_equiv::Tdata& arr = (**it).SetPub().SetPub().Set();
        if (m_mlaClient.Empty())
            m_mlaClient.Reset(new CMLAClient);

        int id = FindPMID(*m_mlaClient, arr);
        if (id>0)
        {
            CreatePubPMID(*m_mlaClient, arr, id);
        }
        else
        // nothing was found
        NON_CONST_ITERATE(CPub_equiv::Tdata, item_it, arr)
        {
            if ((**item_it).IsArticle())
            try
            {
                int id = m_mlaClient->AskCitmatchpmid(**item_it);
                if (id>0)
                {
                    CreatePubPMID(*m_mlaClient, arr, id);
                    break;
                }
            }
            catch(CException& ex)
            {
            }
        }
    }
}

void CRemoteUpdater::UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            UpdateOrgFromTaxon(logger, **it);
        }
    }

    if (!entry.IsSetDescr())
        return;

    NON_CONST_ITERATE(CSeq_descr::Tdata, it, entry.SetDescr().Set())
    {
        CSeqdesc& desc = **it;
        if (desc.IsOrg())
        {
            xUpdateOrgTaxname(logger, desc.SetOrg());
        }
        else
        if (desc.IsSource() && desc.GetSource().IsSetOrg())
        {
            xUpdateOrgTaxname(logger, desc.SetSource().SetOrg());
        }
    }
}

void CRemoteUpdater::UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeq_entry_EditHandle& obj)
{
    for (CBioseq_CI bioseq_it(obj); bioseq_it; ++bioseq_it)
    {
        for (CSeqdesc_CI desc_it(bioseq_it->GetEditHandle()); desc_it; ++desc_it)
        {
            UpdateOrgFromTaxon(logger, (CSeqdesc&)*desc_it);
        }
    }
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
