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
 * Authors: Eugene Vasilchenko
 *
 * File Description: class to keep single ID2 request/replies
 *
 */

#include <ncbi_pch.hpp>

#include "osg_fetch.hpp"

#include <corelib/request_ctx.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Reply.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


COSGFetch::COSGFetch(CRef<CID2_Request> request,
                     const CRef<CRequestContext>& context)
    : m_Request(request),
      m_Context(context),
      m_Status(eIdle)
{
    _ASSERT(request);
}


COSGFetch::~COSGFetch()
{
}


void COSGFetch::ResetReplies()
{
    m_Replies.clear();
}


void COSGFetch::AddReply(CRef<CID2_Reply>&& reply)
{
    _ASSERT(!EndOfReplies());
    m_Replies.push_back(reply);
}


bool COSGFetch::EndOfReplies() const
{
    return !m_Replies.empty() && m_Replies.back()->IsSetEnd_of_reply();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
