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

#include <objects/seqfeat/Org_ref_.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/ArticleIdSet.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/readers/message_listener.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include "remote_updater.hpp"
#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
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

void CRemoteUpdater::xUpdateOrgTaxname(COrg_ref& org)
{
    int taxid = org.GetTaxId();
    if (taxid == 0 && !org.IsSetTaxname())
        return;

    bool is_species, is_uncultured;
    string blast_name;

    if (m_taxClient.get() == 0)
    {
        m_taxClient.reset(new CTaxon1);
        m_taxClient->Init();
    }

    int new_taxid = m_taxClient->GetTaxIdByName(org.GetTaxname());

    if (taxid == 0)
        taxid = new_taxid;
    else
        if (taxid != new_taxid)
        {
            m_context.m_logger->PutError(
                *CLineError::Create(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                "Conflicting taxonomy info provided: taxid " + NStr::IntToString(taxid)));

            if (taxid <= 0)
                taxid = new_taxid;
            else
            {
                m_context.m_logger->PutError(
                    *CLineError::Create(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                    "taxonomy ID for the name '" + org.GetTaxname() + "' was determined as " + NStr::IntToString(taxid)));
                return;
            }
        }

    if (taxid <= 0)
    {
        m_context.m_logger->PutError(
            *CLineError::Create(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                "No unique taxonomy ID found for the name '" + org.GetTaxname() + "'"));
        return;
    }

    CConstRef<COrg_ref> new_org = m_taxClient->GetOrgRef(taxid, is_species, is_uncultured, blast_name);
    if (new_org.NotEmpty())
    {
        if (org.IsSetOrgname() && org.GetOrgname().IsSetMod()) // we need to preserve mods if they set
        {
           const COrgName_Base::TMod mods = org.GetOrgname().GetMod();
           org.Assign(*new_org);
           if (!mods.empty())
               org.SetOrgname().SetMod().insert(org.SetOrgname().SetMod().end(), mods.begin(), mods.end());
        }
        else
        {
           org.Assign(*new_org);
        }
    }
}

CRemoteUpdater::CRemoteUpdater(const CTable2AsnContext& ctx)
    : m_context(ctx)
{
}

CRemoteUpdater::~CRemoteUpdater()
{
    if (m_taxClient.get() != 0)
        m_taxClient->Fini();
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


    CSeq_descr::Tdata& descr = entry.SetDescr().Set();
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

void CRemoteUpdater::UpdateOrgReferences(objects::CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            UpdateOrgReferences(**it);
        }
    }

    if (!entry.IsSetDescr())
        return;

    NON_CONST_ITERATE(CSeq_descr::Tdata, it, entry.SetDescr().Set())
    {
        CSeqdesc& desc = **it;
        if (desc.IsOrg())
        {
            xUpdateOrgTaxname(desc.SetOrg());
        }
        else
        if (desc.IsSource() && desc.GetSource().IsSetOrg())
        {
            xUpdateOrgTaxname(desc.SetSource().SetOrg());
        }
    }
}

END_NCBI_SCOPE
