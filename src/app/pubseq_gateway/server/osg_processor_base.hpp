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
#include <thread>

BEGIN_NCBI_NAMESPACE;

class CRequestContext;

BEGIN_NAMESPACE(objects);

class CID2_Request;
class CID2_Reply;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

const string kOSGProcessorGroupName = "OSG";

class COSGFetch;
class COSGConnectionPool;
class COSGConnection;
class CPSGS_OSGProcessorBase;
class COSGCallDoProcessTask;

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


class COSGStateReporter
{
public:
    explicit COSGStateReporter(const CPSGS_OSGProcessorBase* processor)
        : m_ProcessorPtr(processor)
        {
        }
    CNcbiOstream& Print(CNcbiOstream& out) const;
private:
    const CPSGS_OSGProcessorBase* m_ProcessorPtr;
};
inline CNcbiOstream& operator<<(CNcbiOstream& out, const COSGStateReporter& state)
{
    return state.Print(out);
}

// actual OSG processor base class for communication with OSG
class CPSGS_OSGProcessorBase : public IPSGS_Processor
{
public:
    enum EEnabledFlags {
        fEnabledWGS = 1<<0,
        fEnabledSNP = 1<<1,
        fEnabledCDD = 1<<2,
        fEnabledAllAnnot = fEnabledSNP | fEnabledCDD,
        fEnabledAll = fEnabledWGS | fEnabledSNP | fEnabledCDD
    };
    typedef int TEnabledFlags;
    
    CPSGS_OSGProcessorBase(TEnabledFlags enabled_flags,
                           const CRef<COSGConnectionPool>& pool,
                           const shared_ptr<CPSGS_Request>& request,
                           const shared_ptr<CPSGS_Reply>& reply,
                           TProcessorPriority priority);
    virtual ~CPSGS_OSGProcessorBase();

    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority priority) const override;
    virtual void Process(void) override;
    virtual void Cancel(void) override;
    virtual EPSGS_Status GetStatus(void) override;

    virtual void WaitForOtherProcessors();

    void WaitForCassandra();

    bool NeedTrace() const
        {
            return m_NeedTrace;
        }
    void SendTrace(const string& str);
    
    bool IsCanceled() const
        {
            return m_Status == ePSGS_Canceled;
        }

    TEnabledFlags GetEnabledFlags() const
        {
            return m_EnabledFlags;
        }
    
    // notify processor about communication events
    virtual void NotifyOSGCallStart();
    virtual void NotifyOSGCallReply(const CID2_Reply& reply);
    virtual void NotifyOSGCallEnd();
    
    typedef vector<CRef<COSGFetch>> TFetches;
    const TFetches& GetFetches() const
        {
            return m_Fetches;
        }
    
protected:
    COSGConnectionPool& GetConnectionPool() const {
        return m_ConnectionPool.GetNCObject();
    }

    void StopAsyncThread();
    void SetFinalStatus(EPSGS_Status status);
    void FinalizeResult(EPSGS_Status status);
    void FinalizeResult();
    // if canceled return false
    // otherwise increment background processing counter and return true
    bool SignalStartOfBackgroundProcessing();
    // decrement background processing counter and notify dispatcher if zero
    void SignalEndOfBackgroundProcessing();

    void SignalStartOfUVLoop();
    void SignalEndOfUVLoop();
    
    class CUVLoopGuard
    {
    public:
        explicit CUVLoopGuard(CPSGS_OSGProcessorBase& processor)
            : m_Processor(processor)
            {
                processor.SignalStartOfUVLoop();
            }
        ~CUVLoopGuard()
            {
                m_Processor.SignalEndOfUVLoop();
            }
        CUVLoopGuard(const CUVLoopGuard& guard) = delete;
        CUVLoopGuard& operator=(const CUVLoopGuard& guard) = delete;
    private:
        CPSGS_OSGProcessorBase& m_Processor;
    };
    friend class CUVLoopGuard;

    class CBackgroundProcessingGuard : public CObject
    {
    public:
        explicit CBackgroundProcessingGuard(CPSGS_OSGProcessorBase& processor)
            : m_ProcessorPtr(processor.SignalStartOfBackgroundProcessing()? &processor: 0)
            {
            }
        ~CBackgroundProcessingGuard()
            {
                if ( m_ProcessorPtr ) {
                    m_ProcessorPtr->SignalEndOfBackgroundProcessing();
                }
            }
        
        CBackgroundProcessingGuard(const CBackgroundProcessingGuard& guard) = delete;
        CBackgroundProcessingGuard& operator=(const CBackgroundProcessingGuard& guard) = delete;

        CPSGS_OSGProcessorBase* GetGuardedProcessor() const
            {
                return m_ProcessorPtr;
            }
    private:
        CPSGS_OSGProcessorBase* m_ProcessorPtr;
    };
    friend class CBackgroundProcessingGuard;
    typedef AutoPtr<CBackgroundProcessingGuard> TBGProcToken;
    TBGProcToken x_CreateBGProcToken();
    static bool x_Valid(const TBGProcToken& token)
        {
            return token && token->GetGuardedProcessor();
        }
    static CPSGS_OSGProcessorBase* x_GetProcessor(const TBGProcToken& token)
        {
            return token->GetGuardedProcessor();
        }
    static void* x_BGProcTokenToVoidP(TBGProcToken& token)
        {
            return token.release();
        }
    static TBGProcToken x_BGProcTokenFromVoidP(void* ptr)
        {
            return TBGProcToken(static_cast<CBackgroundProcessingGuard*>(ptr));
        }
    void x_SignalFinishProcessing(const char* from);
    
    void CallDoProcess();
    void CallDoProcessSync();
    void CallDoProcessAsync();
    void CallDoProcessCallback(TBGProcToken token);
    void DoProcess(TBGProcToken token);

    void CallDoProcessReplies(TBGProcToken token);
    void CallDoProcessRepliesSync();
    void CallDoProcessRepliesAsync(TBGProcToken token);
    void CallDoProcessRepliesCallback(TBGProcToken token);
    static void s_CallDoProcessRepliesUvCallback(void* proc);
    void DoProcessReplies();

    friend class COSGCallDoProcessTask;

    static void s_CallFinalizeUvCallback(void *data);
    
    friend class COSGStateReporter;
    COSGStateReporter State() const
        {
            return COSGStateReporter(this);
        }

    void AddRequest(const CRef<CID2_Request>& req);

    // create ID2 requests for the PSG request
    virtual void CreateRequests() = 0;
    // process ID2 replies and issue PSG reply
    virtual void ProcessReplies() = 0;
    // reset processing state in case of ID2 communication failure before next attempt
    virtual void ResetReplies();

private:
    CRef<CRequestContext> m_Context;
    CRef<COSGConnectionPool> m_ConnectionPool;
    TEnabledFlags m_EnabledFlags;
    CRef<COSGConnection> m_Connection;
    TFetches m_Fetches;
    CMutex m_StatusMutex;
    EPSGS_Status m_Status;
    int m_BackgroundProcesing;
    bool m_NeedTrace;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGPROCESSORBASE__HPP
