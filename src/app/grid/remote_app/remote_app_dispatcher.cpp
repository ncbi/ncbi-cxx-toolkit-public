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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/services/netcache_client.hpp>
#include <connect/services/remote_app.hpp>

#include <connect/services/blob_storage_netcache.hpp>
#include <connect/services/ra_dispatcher_client.hpp>
#include <connect/services/remote_app.hpp>

#define PROGRAM_NAME "RemoteAppDispatcher"
#define PROGRAM_VERSION "1.0.0"

USING_NCBI_SCOPE;

class CRemoteAppDispatcher : public CCgiApplication
{
public:
    CRemoteAppDispatcher() {
        SetVersion(CVersionInfo(PROGRAM_VERSION, PROGRAM_NAME));
    }

    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

private:
    static CNetCacheAPI* x_CreateNCClient(const string& service);
    static CNetScheduleAPI* x_CreateNSClient(const string& service, 
                                                const string& qname);

    static void x_RunNewJob(CGridClient& grid, const string& app_name,
                            CNcbiIstream& is, CNcbiOstream& os);
    static void x_CheckJobStatus(CGridClient& grid,  const string& app_name,
                                 const string& jid, CNcbiOstream& os);
    static void x_CancelJob(CGridClient& grid, const string& jid, CNcbiOstream& os);
};


void CRemoteAppDispatcher::Init()
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_NetCache(); 
    // Standard CGI framework initialization
    CCgiApplication::Init();
    
    CCgiRequest::TFlags flags = 0;
    flags |= CCgiRequest::fDoNotParseContent;
    SetRequestFlags(flags);
}


int CRemoteAppDispatcher::ProcessRequest(CCgiContext& ctx)
{

    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();
    response.SetContentType("text/plain");
    response.WriteHeader();

    IRegistry& reg = GetConfig();

    string app_name, jid;
    int icmd;

    try {
    
        CNcbiIstream* is = request.GetInputStream();
        if( !is ) {
            NCBI_THROW(CArgException, eInvalidArg, 
                       "Cannot read input data.");
        }
        
        *is >> icmd;
        CRADispatcherClient::ECmdId cmd = (CRADispatcherClient::ECmdId) icmd;
        if (cmd == CRADispatcherClient::eNewJobCmd)
            ReadStrWithLen(*is, app_name);
        else {
            string token;
            ReadStrWithLen(*is, token);
            NStr::SplitInTwo(token, "|", app_name, jid);            
            if (app_name.empty() || jid.empty())
                NCBI_THROW(CArgException, eInvalidArg, 
                           "Invalid token: \"" + token + "\".");
        }
   
        if (!reg.HasEntry(app_name) ) {
            NCBI_THROW(CArgException, eInvalidArg, 
                       "Application \"" + app_name + "\"is not supported.");
        }

        if (!reg.HasEntry(app_name,"ns_service") ) {
            NCBI_THROW(CArgException, eInvalidArg, 
                       "Application \"" + app_name + "\"is not properly configuredd." 
                       "Missing \"ns_service\" parameter.");
        }

        if (!reg.HasEntry(app_name,"ns_queue") ) {
            NCBI_THROW(CArgException, eInvalidArg, 
                       "Application \"" + app_name + "\"is not properly configuredd." 
                       "Missing \"ns_queue\" parameter.");
        }

        if (!reg.HasEntry(app_name,"nc_service") ) {
            NCBI_THROW(CArgException, eInvalidArg, 
                       "Application \"" + app_name + "\"is not properly configuredd." 
                       "Missing \"nc_service\" parameter.");
        }

        string service = reg.Get(app_name,"ns_service");
        string qname = reg.Get(app_name,"ns_queue");
        auto_ptr<CNetScheduleAPI> ns_client( x_CreateNSClient(service, qname) );
        service = reg.Get(app_name,"nc_service");
        auto_ptr<IBlobStorage> storage( new CBlobStorage_NetCache(x_CreateNCClient(service)) );

        CGridClient grid(ns_client->GetSubmitter(), *storage, 
                         CGridClient::eManualCleanup,
                         CGridClient::eProgressMsgOff);
        
        switch( cmd ) {            
        case CRADispatcherClient::eNewJobCmd:
            x_RunNewJob(grid, app_name, *is, response.out() );
            break;          
        case CRADispatcherClient::eGetStatusCmd:
            x_CheckJobStatus(grid, app_name, jid, response.out());
            break;
        case CRADispatcherClient::eCancelJobCmd:
            x_CancelJob(grid, jid, response.out() );
            break;
        };

    } catch (CArgException& ex) {
        response.out() << CRADispatcherClient::eErrorRes << " ";
        WriteStrWithLen(response.out(), ex.what());
    } catch (CException& ex) {
        response.out() << CRADispatcherClient::eErrorRes << " ";
        WriteStrWithLen(response.out(), ex.what());
    } catch (...) {
        response.out() << CRADispatcherClient::eErrorRes << " ";
        WriteStrWithLen(response.out(), "Unknown error");
    }

    response.Flush();
    return 0;
  
}

/* static */
void CRemoteAppDispatcher::x_RunNewJob(CGridClient& grid, const string& app_name,
                                       CNcbiIstream& is, CNcbiOstream& os)
{
    CGridJobSubmitter& submitter = grid.GetJobSubmitter();
    CNcbiOstream& jos = submitter.GetOStream();
    WriteStrWithLen(jos, app_name);
    if (is.good()) {
        jos << is.rdbuf();
    }
    submitter.SetJobMask(CRemoteAppRequestSB::kSingleBlobMask);
    string job_key = submitter.Submit();

    x_CheckJobStatus(grid, app_name, job_key, os);
}

/* static */
void CRemoteAppDispatcher::x_CheckJobStatus(CGridClient& grid, const string& app_name,
                                            const string& jid, CNcbiOstream& os)
{
    CGridJobStatus& stat = grid.GetJobStatus(jid);
    CNetScheduleAPI::EJobStatus st = stat.GetStatus();
    os << (int)CRADispatcherClient::eJobStatusRes << " " << (int)st << " ";
    string token = app_name + '|' + jid;
    WriteStrWithLen(os, token);
    switch( st ) {
    case CNetScheduleAPI::eDone:        
        os << stat.GetIStream().rdbuf();
        break;
    case CNetScheduleAPI::eFailed:
        WriteStrWithLen(os, stat.GetErrorMessage());
        break;
    default:
        break;
    }
}


/* static */
void CRemoteAppDispatcher::x_CancelJob(CGridClient& grid, const string& jid, CNcbiOstream& os)
{
    grid.CancelJob(jid);
    os << (int)CRADispatcherClient::eOkRes;
}

/* static */
CNetCacheAPI* CRemoteAppDispatcher::x_CreateNCClient(const string& service)
{
    return new CNetCacheAPI("remote_app_dispatcher", service);
}
/* static */
CNetScheduleAPI* CRemoteAppDispatcher::x_CreateNSClient(const string& service, 
                                                        const string& qname)
{
    auto_ptr<CNetScheduleAPI> cln;
    cln.reset(new CNetScheduleAPI(service, "remote_app_dispatcher", qname));
    cln->SetProgramVersion(PROGRAM_NAME " version " PROGRAM_VERSION);
    return cln.release();
}

int main(int argc, const char* argv[])
{
    return CRemoteAppDispatcher().AppMain(argc, argv);
} 
