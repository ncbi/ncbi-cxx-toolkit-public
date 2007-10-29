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

#include <connect/services/remote_job.hpp>
#include <connect/services/blob_storage_netcache.hpp>

#include "info_collector.hpp"
#include "renderer.hpp"

USING_NCBI_SCOPE;

class CNSRemoveJobControlApp : public CNcbiApplication
{
public:
    CNSRemoveJobControlApp() {}

    virtual void Init(void);
    virtual int Run(void);

protected:
};

void CNSRemoveJobControlApp::Init(void)
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_NetCache(); 

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Remote application jobs submitter");

    arg_desc->AddOptionalKey("q", "queue_name", "NetSchedule queue name", 
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ns", "service", 
                             "NetSchedule service addrress (service_name or host:port)", 
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("nc", "service", 
                     "NetCache service addrress (service_name or host:port)", 
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ncprot", "protocol", 
                     "NetCache client protocol", 
                     CArgDescriptions::eString);
    arg_desc->SetConstraint("ncprot", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "simple", "persistent")
                            );

    arg_desc->AddOptionalKey("jlist",
                             "status",
                             "Show jobs by status",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("jlist", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "done", "failed", "running", "pending", 
                              "canceled", "returned", "all")
                            );

    arg_desc->AddFlag("qlist", "Show queue list");

    arg_desc->AddFlag("wnlist", "Show worker nodes");
    arg_desc->AddOptionalKey("jid",
                             "job_id",
                             "Show job by id",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("bid",
                             "blob_id",
                             "Show blob content",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("attr",
                             "attr",
                             "Show job's attr",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);

    arg_desc->SetConstraint("attr", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "minimal", "standard", "full",
                              "status", "retcode", "cmdline",
                              "stdin", "stdout", "stderr", 
                              "raw_input", "raw_output")
                            );

    arg_desc->AddOptionalKey("cmd",
                             "cmd_name",
                             "Perform a command",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("cmd", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "shutdown_nodes", "kill_nodes", "drop_jobs")
                            );


    arg_desc->AddOptionalKey("render",
                             "render_type",
                             "Renderer type",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("render", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "text", "xml")
                            );

    arg_desc->SetConstraint("jlist", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "done", "failed", "running", "pending", 
                              "canceled", "returned", "all")
                            );

    arg_desc->AddOptionalKey("of", 
                             "file_names",
                             "Output file",
                             CArgDescriptions::eOutputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

class CWNodeShutdownAction : public  CNSInfoCollector::IAction<CWNodeInfo>
{
public:
    CWNodeShutdownAction(CNetScheduleAdmin::EShutdownLevel level)
        : m_Level(level) {}

    virtual ~CWNodeShutdownAction() {};

    virtual void operator()(const CWNodeInfo& info)
    {
        info.Shutdown(m_Level);
    }
private:
    CNetScheduleAdmin::EShutdownLevel m_Level;
};


int CNSRemoveJobControlApp::Run(void)
{

    const CArgs& args = GetArgs();
    IRWRegistry& reg = GetConfig();

    if (args["q"]) {
        string queue = args["q"].AsString();   
        reg.Set(kNetScheduleAPIDriverName, "queue_name", queue);
    }

    if ( args["ns"]) {
        reg.Set(kNetScheduleAPIDriverName, "client_name", "ns_remote_job_control");
        reg.Set(kNetScheduleAPIDriverName, "service", args["ns"].AsString());
        reg.Set(kNetScheduleAPIDriverName, "use_permanent_connection", "true");
        reg.Set(kNetScheduleAPIDriverName, "use_embedded_storage", "true");
    }

    if ( args["nc"]) {
        reg.Set(kNetCacheAPIDriverName, "client_name", "ns_remote_job_control");
        reg.Set(kNetCacheAPIDriverName, "service", args["nc"].AsString());
	if ( args["ncprot"] )
	    reg.Set(kNetCacheAPIDriverName, "protocol", args["ncprot"].AsString());
    }


    auto_ptr<CNSInfoCollector> info_collector;
    CNcbiOstream* out = &NcbiCout;
    if (args["of"]) {
        out = &args["of"].AsOutputFile();
    }

    auto_ptr<ITagWriter> writer;
    if (args["render"]) {
        string type = args["render"].AsString();
        if(NStr::CompareNocase(type, "xml") == 0)
            writer.reset(new CXmlTagWriter(*out));
    }
    if (!writer.get())
        writer.reset(new CTextTagWriter(*out));

    try {

    CBlobStorageFactory factory(reg);
    if (args["bid"]) {
        info_collector.reset(new CNSInfoCollector(factory));
        auto_ptr<CNSInfoRenderer> renderer(new CNSInfoRenderer(*writer, *info_collector));
        string id = args["bid"].AsString();
        renderer->RenderBlob(id);
        return 0;
    }

    auto_ptr<CConfig::TParamTree> ptree(CConfig::ConvertRegToTree(reg));
    const CConfig::TParamTree* ns_tree = ptree->FindSubNode(kNetScheduleAPIDriverName);
    if (!ns_tree) 
        NCBI_THROW(CArgException, eInvalidArg,
                   "Could not find \"" + string(kNetScheduleAPIDriverName) + "\" section");
    
    CConfig ns_conf(ns_tree);
    string queue = ns_conf.GetString(kNetScheduleAPIDriverName, "queue_name", CConfig::eErr_NoThrow, "");
    NStr::TruncateSpacesInPlace(queue);
    if (queue.empty()) 
        //        NCBI_THROW(CArgException, eInvalidArg,
        //                   "\"queue_name\" parameter is not set neither in config file nor in cmd line");
        queue = "noname";

    string service = ns_conf.GetString(kNetScheduleAPIDriverName, "service", CConfig::eErr_NoThrow, "");
    NStr::TruncateSpacesInPlace(service);
    if (service.empty())
        NCBI_THROW(CArgException, eInvalidArg,
                   "\"service\" parameter is not set "
                   "neither in config file nor in cmd line");

    info_collector.reset(new CNSInfoCollector(queue, service, factory));

    CNSInfoRenderer::TFlags flags = 0;
    if (args["attr"]) {
        const CArgValue::TStringArray& strings = args["attr"].GetStringList();
        CArgValue::TStringArray::const_iterator it;
        for( it = strings.begin(); it != strings.end(); ++it) {
            if (NStr::CompareNocase(*it, "minmal") == 0)
                flags = CNSInfoRenderer::eMinimal;
            if (NStr::CompareNocase(*it, "standard") == 0)
                flags = CNSInfoRenderer::eStandard;
            if (NStr::CompareNocase(*it, "full") == 0)
                flags = CNSInfoRenderer::eFull;
            if (NStr::CompareNocase(*it, "status") == 0)
                flags |= CNSInfoRenderer::eStatus;
            if (NStr::CompareNocase(*it, "retcode") == 0)
                flags |= CNSInfoRenderer::eRetCode;
            if (NStr::CompareNocase(*it, "cmdline") == 0)
                flags |= CNSInfoRenderer::eCmdLine;
            if (NStr::CompareNocase(*it, "stdin") == 0)
                flags |= CNSInfoRenderer::eStdIn;
            if (NStr::CompareNocase(*it, "stdout") == 0)
                flags |= CNSInfoRenderer::eStdOut;
            if (NStr::CompareNocase(*it, "stderr") == 0)
                flags |= CNSInfoRenderer::eStdErr;
            if (NStr::CompareNocase(*it, "raw_input") == 0)
                flags |= CNSInfoRenderer::eRawInput;
            if (NStr::CompareNocase(*it, "raw_output") == 0)
                flags |= CNSInfoRenderer::eRawOutput;           
        }
    }

    auto_ptr<CNSInfoRenderer> renderer(new CNSInfoRenderer(*writer, *info_collector));

    if (args["jlist"]) {
        string sstatus = args["jlist"].AsString();
        if (NStr::CompareNocase(sstatus, "all") == 0) {
            for(int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {                
                CNetScheduleAPI::EJobStatus status = 
                    (CNetScheduleAPI::EJobStatus)i;
                renderer->RenderJobByStatus(status, flags);
            }
        } else {
            CNetScheduleAPI::EJobStatus status = 
                CNetScheduleAPI::StringToStatus(sstatus);
            renderer->RenderJobByStatus(status, flags);
        }
    } else if (args["jid"]) {
        string jid = args["jid"].AsString();
        renderer->RenderJob(jid, flags);
    } else if (args["wnlist"]) {
        renderer->RenderWNodes(flags);
    } else if (args["qlist"]) {
        renderer->RenderQueueList();
    } else if (args["cmd"]) {
        string cmd = args["cmd"].AsString();
        if (NStr::CompareNocase(cmd, "shutdown_nodes") == 0) {
            CNetScheduleAdmin::EShutdownLevel level = 
                CNetScheduleAdmin::eShutdownImmediate;
            CWNodeShutdownAction action(level);
            info_collector->TraverseNodes(action);
        }
        if (NStr::CompareNocase(cmd, "kill_nodes") == 0) {
            CNetScheduleAdmin::EShutdownLevel level = 
                level = CNetScheduleAdmin::eDie;
            CWNodeShutdownAction action(level);
            info_collector->TraverseNodes(action);
        }
        if (NStr::CompareNocase(cmd, "drop_jobs") == 0) {
            info_collector->DropQueue();
        }
    }

    } catch (exception& ex) {
        writer->WriteBeginTag("Error");
        writer->WriteText(ex.what());
        writer->WriteCloseTag("Error");
    } catch (...) {
        writer->WriteTag("Error", "Unknown error");
    }

    return 0;

}
int main(int argc, const char* argv[])
{
    return CNSRemoveJobControlApp().AppMain(argc, argv);
} 
