#ifndef PSGS_OSGCALLER__HPP
#define PSGS_OSGCALLER__HPP

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
 * File Description: class for communication with OSG
 *
 */

#include <corelib/ncbiobj.hpp>
#include <vector>

BEGIN_NCBI_NAMESPACE;

class CRequestContext;

BEGIN_NAMESPACE(objects);

class CID2_Request;
class CID2_Request_Packet;
class CID2_Reply;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

class COSGConnectionPool;
class COSGConnection;
class COSGFetch;
class CPSGS_OSGProcessorBase;

USING_SCOPE(objects);


class COSGCaller : public CObject
{
public:
    typedef vector<CRef<COSGFetch>> TFetches;
    
    explicit COSGCaller(const CRef<COSGConnectionPool>& connection_pool,
                        const CRef<CRequestContext>& context,
                        const TFetches& fetches);
    virtual ~COSGCaller();

    void WaitForReplies(CPSGS_OSGProcessorBase& processor);

protected:
    const CRef<COSGConnection>& GetConnection() const
        {
            return m_Connection;
        }
    
    void Start(const TFetches& fetches);
    CRef<CID2_Request> MakeInitRequest();
    void SetContext(CID2_Request& req, const CRef<COSGFetch>& fetch);
    void AddFetch(CID2_Request_Packet& packet, const CRef<COSGFetch>& fetch);
    CRef<CID2_Request_Packet> MakePacket(const TFetches& fetches);
    size_t GetRequestIndex(const CID2_Reply& reply) const;

    bool EndOfReplies(const CID2_Reply& reply) const;
    bool EndOfReplies(size_t index) const;
    
private:
    CRef<COSGConnectionPool> m_ConnectionPool;
    CRef<COSGConnection> m_Connection;
    CRef<CRequestContext> m_Context;
    CRef<CID2_Request_Packet> m_RequestPacket;
    vector<CRef<COSGFetch>> m_Fetches;
    size_t m_WaitingRequestCount;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGCALLER__HPP

