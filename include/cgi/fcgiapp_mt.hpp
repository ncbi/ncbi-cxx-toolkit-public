#ifndef CGI___FCGIAPP_MT__HPP
#define CGI___FCGIAPP_MT__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Base class for multithreaded Fast-CGI applications
*/

#include <cgi/cgiapp.hpp>
#include <fastcgi++/manager.hpp>
#include <fastcgi++/request.hpp>
#include <sstream>

/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class ICache;


/////////////////////////////////////////////////////////////////////////////
//  CFastCgiApplicationMT::
//

class CFastCgiThreadedRequest : public Fastcgipp::Request<char>
{
public:
    typedef Fastcgipp::Request<char> TParent;
    typedef CNcbiOstream TOutput;
    typedef CNcbiIstream TInput;

    CFastCgiThreadedRequest(void);
    ~CFastCgiThreadedRequest(void);

    bool response(void) override;
    void errorHandler(void) override;
    bool inProcessor(void) override;

    TInput& in(void) { return *m_InputStream; }
    TOutput& out(void) { return TParent::out; }
    TOutput& err(void) { return TParent::err; }
    const char* const* env(void) const {
        if ( m_Env.data.empty() ) x_ParseEnv();
        return m_Env.data.data();
    }

private:
    void x_ParseEnv(void) const;

    class CEnv {
    public:
        CEnv(void) {}
        ~CEnv(void) {
            for (auto p: data) {
                if ( p ) free(p);
            }
        }

        void Set(const string& name, const string& value) {
            string env = name + "=" + value;
            data.push_back(strdup(env.c_str()));
        }

        vector<char*> data;
    };

    shared_ptr<istream> m_InputStream;
    mutable CEnv        m_Env;
};

/////////////////////////////////////////////////////////////////////////////
//  CFastCgiApplicationMT::
//

class NCBI_XFCGI_MT_EXPORT CFastCgiApplicationMT : public CCgiApplication
{
public:
    CFastCgiApplicationMT(const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT());
    ~CFastCgiApplicationMT(void);

    /// Singleton
    static CFastCgiApplicationMT* Instance(void);

    /// This method is not used by multithreaded FastCGI and should never be called.
    /// Override CreateRequestProcessor and ICgiRequestProcessor::ProcessRequest instead.
    int ProcessRequest(CCgiContext& context) override { return -1; }

    bool IsFastCGI(void) const override { return true; }

protected:
    bool x_RunFastCGI(int* result, unsigned int def_iter = 10) override;
    void FASTCGI_ScheduleExit(void) override;

private:
    friend class CFastCgiThreadedRequest;
    friend void s_ScheduleFastCGIMTExit(void);

    void x_ProcessThreadedRequest(CFastCgiThreadedRequest& req);

    typedef Fastcgipp::Manager<CFastCgiThreadedRequest> TManager;

    unique_ptr<TManager>        m_Manager;
    CAtomicCounter              m_ErrorCounter; // failed requests
    bool                        m_IsStatLog;
    unique_ptr<CCgiStatistics>  m_Stat;
    unsigned int                m_MaxIterations;
    unique_ptr<CCgiWatchFile>   m_Watcher;
    unsigned int                m_WatchTimeout;
    int                         m_RestartDelay;
    bool                        m_StopIfFailed;
    CTime                       m_ModTime;
    bool                        m_ManagerStopped;
};


/////////////////////////////////////////////////////////////////////////////
//  CCgiRequestProcessorMT::
//

class NCBI_XFCGI_MT_EXPORT CCgiRequestProcessorMT : public CCgiRequestProcessor
{
    friend class CFastCgiApplicationMT;

public:
    CCgiRequestProcessorMT(CFastCgiApplicationMT& app);
    virtual ~CCgiRequestProcessorMT(void);

private:
    unique_ptr<CNcbiResource> m_Resource;
};


END_NCBI_SCOPE


/* @} */


#endif // CGI___FCGIAPP_MT__HPP
