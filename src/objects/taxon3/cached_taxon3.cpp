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
 * Author:  Brad Holmes
 *
 * File Description:
 *     NCBI Taxonomy 3 service that caches a parameterized number of replies
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/taxon3/cached_taxon3.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/misc/error_codes.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objects_Taxonomy



BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

const string kInvalidReplyMsg = "Taxonomy service returned invalid reply";


CCachedTaxon3::CCachedTaxon3(AutoPtr<ITaxon3> taxon, TSizeType capacity)
    : CCache<string, CRef<CTaxon3_reply> >(capacity),
    m_taxon(taxon)
{
}


// The method to create the cache one-off
AutoPtr<CCachedTaxon3> CCachedTaxon3::Create(
    AutoPtr<ITaxon3> taxon, TSizeType capacity)
{                                                                  
    return AutoPtr<CCachedTaxon3>(CreateUnSafe(taxon, capacity));
}

// This should only be used if it will be
// immediately wrapped into a safe pointer
CCachedTaxon3* CCachedTaxon3::CreateUnSafe(
    AutoPtr<ITaxon3> taxon, TSizeType capacity)
{
    CCachedTaxon3* c = new CCachedTaxon3(taxon, capacity);
    return c;
}

void
CCachedTaxon3::Init(void)
{
    static const STimeout def_timeout = { 120, 0 };
    CCachedTaxon3::Init(&def_timeout);
}


void
CCachedTaxon3::Init(const STimeout* timeout, unsigned reconnect_attempts)
{
    if ( timeout ) {
        m_timeout_value = *timeout;
        m_timeout = &m_timeout_value;
    } else {
        m_timeout = 0;
    }

    m_nReconnectAttempts = reconnect_attempts;
    m_pchService = "TaxService3";
    const char* tmp;
    if( ( (tmp=getenv("NI_TAXON3_SERVICE_NAME")) != NULL ) ||
        ( (tmp=getenv("NI_SERVICE_NAME_TAXON3")) != NULL ) ) {
        m_pchService = tmp;
    }


#ifdef USE_TEXT_ASN
			m_eDataFormat = eSerial_AsnText;
#else
			m_eDataFormat = eSerial_AsnBinary;
#endif

}


CRef< CTaxon3_reply >
CCachedTaxon3::SendRequest(const CTaxon3_request& request)
{
    return m_taxon->SendRequest(request);
}


CRef<CTaxon3_reply> CCachedTaxon3::x_AddReplyToCache(
    const TCacheKey& key,
    const COrg_ref& org_ref)
{
    //Build a request for the Org-ref
    CRef<CTaxon3_request> request(new CTaxon3_request);
    CRef<CT3Request> rq(new CT3Request);
    rq->SetOrg(*SerialClone(org_ref));
    request->SetRequest().push_back(rq);

    //Send the request
    CRef<CTaxon3_reply> reply = m_taxon->SendRequest(*request);
    
    //Check for valid reply
    if (!reply->IsSetReply()) {

        //Do not cache, and return error.
        reply = Ref(new CTaxon3_reply);
        CRef<CT3Reply> t3reply = Ref(new CT3Reply);
        t3reply->SetError().SetLevel(CT3Reply::TError::eLevel_error);
        t3reply->SetError().SetMessage(kInvalidReplyMsg);
        reply->SetReply().push_back(t3reply);

        return reply;
    }

    // Cache the reply we do have, and return it.
    try {
        static const TAddFlags addflags = fAdd_NoReplace;
        const TOrder ord = Add(key, reply, 1, addflags);
        if (ord < 0) {
            LOG_POST(Warning << "Unable to add data for key: " << key);
        }
        static const int cache_getflags = fGet_NoTouch | fGet_NoCreate | fGet_NoInsert;
        return Get(key, cache_getflags);
    }
    catch (const CException& e) {
        LOG_POST(Error << "Unable to add reply to taxon cache: " << e.GetMsg());
    }
    return CRef<CTaxon3_reply>();
}

CRef<CTaxon3_reply> CCachedTaxon3::x_GetReplyForOrgRef(
    const COrg_ref& org_ref)
{

    string key = NStr::IntToString(org_ref.GetTaxId());
    if (org_ref.IsSetTaxname()) {
        key += org_ref.GetTaxname();
    }
    //First, try to get the item from the cache
    static const int cache_getflags = fGet_NoTouch | fGet_NoCreate | fGet_NoInsert;
    try {
        EGetResult retcode;
        CRef<CTaxon3_reply> reply = Get(key, cache_getflags, &retcode);
        if (retcode == eGet_Found) {
            return reply;
        }
    }
    catch (const CException& e) {
        LOG_POST(Trace << "Taxon cache miss for key " << key);
    }

    //Couldn't find it, so create a new one
    return x_AddReplyToCache(key, org_ref);
}

CRef<CTaxon3_reply> CCachedTaxon3::SendOrgRefList(const vector<CRef< COrg_ref> >& org_list)
{
    
    CRef<CTaxon3_reply> result(new CTaxon3_reply);

    ITERATE(vector<CRef< COrg_ref> >, it, org_list)
    {
        const COrg_ref&  org_ref = **it;
        result->SetReply().push_back(x_GetReplyForOrgRef(org_ref)->SetReply().front());
    }

    return result;
}



END_objects_SCOPE
END_NCBI_SCOPE
