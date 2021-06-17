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
* Author:  Brad Holmes, NCBI
*
* File Description:
*   Unit tests for the cached taxon
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/taxon3/taxon3__.hpp>
#include <objects/taxon3/cached_taxon3.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CMockTaxon : public ITaxon3
{
public:

    typedef list<CRef<CTaxon3_reply> > TReplies;

    CMockTaxon(TReplies replies,
        const string& estr = kEmptyStr) :
        m_replies(replies),
        m_error_string(estr)
    {
    }

private:
    TReplies m_replies;
    const string m_error_string;


public:

    virtual void Init(void) {}

    virtual void Init(const STimeout* timeout, unsigned reconnect_attempts /*= 5*/) {}

    virtual const string& GetLastError() const { return m_error_string; }

    virtual CRef< CTaxon3_reply > SendRequest(const CTaxon3_request& request)
    {
        CRef<CTaxon3_reply> v = m_replies.front();
        m_replies.pop_front();
        return v;
    }

    virtual CRef<CTaxon3_reply> SendOrgRefList(const vector<CRef< COrg_ref> >& list)
    {
        CRef<CTaxon3_reply> v = m_replies.front();
        m_replies.pop_front();
        return v;
    }
};

CRef<CTaxon3_request> s_GetDummyRequest()
{
    CRef<CTaxon3_request> request(new CTaxon3_request);

    CRef<CT3Request> rq(new CT3Request);
    rq->SetTaxid(9606);

    request->SetRequest().push_back(rq);

    return request;
}

CRef<CTaxon3_reply> s_CreateReplyWithMessage(const string& message)
{
    CRef<CTaxon3_reply> reply(new CTaxon3_reply);
    CRef<CT3Reply> t3reply(new CT3Reply);
    t3reply->SetError().SetLevel(CT3Reply::TError::eLevel_error);
    t3reply->SetError().SetMessage(message);
    reply->SetReply().push_back(t3reply);
    return reply;
}

CRef<COrg_ref> s_CreateOrgRef(const int& taxid, const string& taxname)
{
    CRef<COrg_ref> org_ref(new COrg_ref);
    org_ref->SetTaxId(taxid);
    org_ref->SetTaxname(taxname);
    return org_ref;
}


BOOST_AUTO_TEST_CASE(create_safe_cached_taxon)
{
    
    CMockTaxon::TReplies replies;
    const string message = "create_safe_cached_taxon";
    replies.push_back(s_CreateReplyWithMessage(message));


    AutoPtr<ITaxon3> mock_taxon(new CMockTaxon(replies, kEmptyStr));
    AutoPtr<CCachedTaxon3> cached_taxon = CCachedTaxon3::Create(mock_taxon, 100);

    BOOST_CHECK_NO_THROW(cached_taxon->SendRequest(*s_GetDummyRequest()));
}

BOOST_AUTO_TEST_CASE(send_request)
{

    CMockTaxon::TReplies replies;
    const string message = "create_safe_cached_taxon";
    replies.push_back(s_CreateReplyWithMessage(message));


    AutoPtr<ITaxon3> mock_taxon(new CMockTaxon(replies, kEmptyStr));
    AutoPtr<CCachedTaxon3> cached_taxon = CCachedTaxon3::Create(mock_taxon, 100);

    CRef<CTaxon3_reply> new_reply = cached_taxon->SendRequest(*s_GetDummyRequest());

    BOOST_CHECK_EQUAL(new_reply->GetReply().size(), 1);
    BOOST_CHECK_EQUAL(new_reply->GetReply().front()->GetError().GetMessage(), message);
}



BOOST_AUTO_TEST_CASE(create_unsafe_cached_taxon)
{
    CMockTaxon::TReplies replies;

    const string message = "create_unsafe_cached_taxon";
    replies.push_back(s_CreateReplyWithMessage(message));

    AutoPtr<ITaxon3> mock_taxon(new CMockTaxon(replies, kEmptyStr));
    CCachedTaxon3* cached_taxon = CCachedTaxon3::CreateUnSafe(mock_taxon, 100);

    CRef<CTaxon3_reply> new_reply = cached_taxon->SendRequest(*s_GetDummyRequest());

    BOOST_CHECK_EQUAL(new_reply->GetReply().size(), 1);
    BOOST_CHECK_EQUAL(new_reply->GetReply().front()->GetError().GetMessage(), message);
}


BOOST_AUTO_TEST_CASE(test_cache)
{
    CMockTaxon::TReplies replies;

    replies.push_back(s_CreateReplyWithMessage("reply1"));
    replies.push_back(s_CreateReplyWithMessage("reply2"));
    replies.push_back(s_CreateReplyWithMessage("reply3"));

    AutoPtr<ITaxon3> mock_taxon(new CMockTaxon(replies, kEmptyStr));
    AutoPtr<CCachedTaxon3> cached_taxon = CCachedTaxon3::Create(mock_taxon, 100);

    //Create org list to send.
    vector<CRef< COrg_ref> > org_list;
    org_list.push_back(s_CreateOrgRef(9606, "human"));
    org_list.push_back(s_CreateOrgRef(9606, "human"));
    org_list.push_back(s_CreateOrgRef(10900, "mouse"));


    CRef<CTaxon3_reply> new_reply = cached_taxon->SendOrgRefList(org_list);

    BOOST_CHECK_EQUAL(new_reply->GetReply().size(), 3);
    CTaxon3_reply::TReply::const_iterator it = new_reply->GetReply().begin();
    BOOST_CHECK_EQUAL((*it)->GetError().GetMessage(), "reply1");
    ++it;
    BOOST_CHECK_EQUAL((*it)->GetError().GetMessage(), "reply1");  //cache hit
    ++it;
    BOOST_CHECK_EQUAL((*it)->GetError().GetMessage(), "reply2");
}



