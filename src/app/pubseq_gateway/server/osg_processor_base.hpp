#ifndef PSGS_OSGPROCESSORBASE__HPP
#define PSGS_OSGPROCESSORBASE__HPP

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
 * File Description: base class for processors which may generate os_gateway
 *                   fetches
 *
 */

#include <corelib/request_status.hpp>
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "ipsgs_processor.hpp"

BEGIN_NCBI_NAMESPACE;

class CRequestContext;

BEGIN_NAMESPACE(objects);

class CID2_Request;
class CID2_Reply;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

class COSGFetch;
class COSGCaller;
class COSGConnectionPool;
class COSGConnection;


enum EDebugLevel {
    eDebug_none     = 0,
    eDebug_error    = 1,
    eDebug_open     = 2,
    eDebug_exchange = 4,
    eDebug_asn      = 5,
    eDebug_blob     = 8,
    eDebug_raw      = 9,
    eDebugLevel_default = eDebug_error
};

int GetDebugLevel();
void SetDebugLevel(int debug_level);

Severity GetDiagSeverity();
void SetDiagSeverity(EDiagSev severity);

USING_SCOPE(objects);


// actual OSG processor base class for communication with OSG
class CPSGS_OSGProcessorBase : public IPSGS_Processor
{
public:
    CPSGS_OSGProcessorBase(const CRef<COSGConnectionPool>& pool,
                           const shared_ptr<CPSGS_Request>& request,
                           const shared_ptr<CPSGS_Reply>& reply,
                           TProcessorPriority priority);
    virtual ~CPSGS_OSGProcessorBase();

    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority priority) const;
    virtual void Process(void);
    virtual void Cancel(void);
    virtual EPSGS_Status GetStatus(void);

protected:
    void Start();
    void CreateFetches();
    void WaitForFinish();
    void SetFinalStatus(EPSGS_Status status);
    void FinalizeResult(EPSGS_Status status);
    void FinalizeResult();

    typedef vector<CRef<COSGFetch>> TFetches;
    
    void AddRequest(const CRef<CID2_Request>& req);
    const TFetches& GetFetches() const
        {
            return m_Fetches;
        }
    
    virtual void CreateRequests() = 0;
    virtual void ProcessReplies() = 0;
    
private:
    CRef<CRequestContext> m_Context;
    CRef<COSGConnectionPool> m_ConnectionPool;
    CRef<COSGConnection> m_Connection;
    TFetches m_Fetches;
    CRef<COSGCaller> m_OSGCaller;
    EPSGS_Status m_Status;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGPROCESSORBASE__HPP
