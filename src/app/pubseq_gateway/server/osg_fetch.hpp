#ifndef PSGS_OSGFETCH__HPP
#define PSGS_OSGFETCH__HPP

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

#include <corelib/ncbiobj.hpp>
#include <vector>

BEGIN_NCBI_NAMESPACE;

class CRequestContext;

BEGIN_NAMESPACE(objects);

class CID2_Request;
class CID2_Reply;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

USING_SCOPE(objects);


class COSGFetch : public CObject
{
public:
    enum EStatus {
        eIdle,
        eSent,
        eReceived,
        eFailed
    };

    explicit COSGFetch(CRef<CID2_Request> request);
    virtual ~COSGFetch();

    const CRef<CID2_Request>& GetRequest() const
        {
            return m_Request;
        }

    void SetContext(CRequestContext& rctx);

    typedef vector<CRef<CID2_Reply>> TReplies;
    const TReplies& GetReplies() const
        {
            return m_Replies;
        }
    bool EndOfReplies() const;
    void AddReply(CRef<CID2_Reply>&& reply);
    void ResetReplies();
    
    EStatus GetStatus() const
        {
            return m_Status;
        }
    bool IsStarted() const
        {
            return m_Status != eIdle;
        }
    bool IsFinished() const
        {
            return m_Status == eReceived || m_Status == eFailed;
        }

    void SetSent()
        {
            _ASSERT(m_Status == eIdle);
            m_Status = eSent;
        }
    void SetReplies(const TReplies& replies)
        {
            _ASSERT(m_Status == eSent);
            m_Replies = replies;
            m_Status = eReceived;
        }
    void SetFailed()
        {
            m_Status = eFailed;
        }

private:
    CRef<CID2_Request> m_Request;
    TReplies m_Replies;
    EStatus m_Status;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGPFETCH__HPP
