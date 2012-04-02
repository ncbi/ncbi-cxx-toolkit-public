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

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/remote_app.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#define PROGRAM_NAME "CNSSubmitRemoteJobApp"
#define PROGRAM_VERSION "1.1.0"

USING_NCBI_SCOPE;

class CNSSubmitRemoteJobApp : public CGridClientApp
{
public:
    CNSSubmitRemoteJobApp() {
        SetVersion(CVersionInfo(PROGRAM_VERSION, PROGRAM_NAME));
    }

    virtual void Init(void);
    virtual int Run(void);
    virtual string GetProgramVersion(void) const
    {
        return PROGRAM_NAME " version " PROGRAM_VERSION;
    }

protected:

    void PrintJobInfo(const string& job_key);
    void ShowBlob(const string& blob_key);

    virtual bool UseAutomaticCleanup() const { return false; }

};

void CNSSubmitRemoteJobApp::Init(void)
{
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


    arg_desc->AddOptionalKey("jf",
                             "jobs_file",
                             "File with job descriptions",
                             CArgDescriptions::eInputFile);

    arg_desc->AddFlag("batch", "Use batch submit mode");

    arg_desc->AddOptionalKey("bsize",
                             "batch_size",
                             "Size of submission batch",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("args", 
                             "cmd_args",
                             "Cmd args for the remote application",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("aff", 
                             "affinity",
                             "Affinity token",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("tfiles", 
                             "file_names",
                             "Files for transfer to the remote applicaion side separated by ;",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("runtime", 
                             "time",
                             "job run timeout",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("exclusive",
                      "Run job in the exclusive mode");

    arg_desc->AddOptionalKey("jout", 
                             "file_names",
                             "A file the remote applicaion stdout",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jerr", 
                             "file_names",
                             "A file the remote applicaion stderr",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("of", 
                             "file_names",
                             "Ouput files with job ids",
                             CArgDescriptions::eOutputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

static string s_FindParam(const string& str, const string& par_name)
{
    SIZE_TYPE pos = 0;
    pos = NStr::FindNoCase(str, par_name);
    if (pos != NPOS) {
        SIZE_TYPE pos1 = pos + par_name.size();
        while (pos1 != NPOS) {
            pos1 = NStr::Find(str, "\"", pos1);
            if(str[pos1-1] != '\\') 
                break;
            ++pos1;
        }
        if (pos1 == NPOS)
            return "";
        return NStr::ParseEscapes(str.substr(pos+par_name.size(), 
                                             pos1 - pos - par_name.size()));
    }
    return "";
}

int CNSSubmitRemoteJobApp::Run(void)
{

    const CArgs& args = GetArgs();
    IRWRegistry& reg = GetConfig();

    if (args["q"]) {
        reg.Set(kNetScheduleAPIDriverName, "queue_name", args["q"].AsString());
    }
    if (args["ns"]) {
        reg.Set(kNetScheduleAPIDriverName, "client_name", "ns_submit_remote_job");
        reg.Set(kNetScheduleAPIDriverName, "service", args["ns"].AsString());
        reg.Set(kNetScheduleAPIDriverName, "use_permanent_connection", "true");
        reg.Set(kNetScheduleAPIDriverName, "use_embedded_storage", "true");
    }

    if (args["nc"]) {
        reg.Set(kNetCacheAPIDriverName, "client_name", "ns_submit_remote_job");
        reg.Set(kNetCacheAPIDriverName, "service", args["nc"].AsString());
    }

    CGridClientApp::Init();

    CNcbiOstream* out = NULL;
    if (args["of"]) {
        out = &args["of"].AsOutputFile();
    }

    size_t input_size = GetGridClient().GetMaxServerInputSize();
    if (input_size == 0) {
        // this means that NS internal storage is not supported and 
        // we all input params will be save into NC anyway. So we are 
        // putting all input into one blob.
        input_size = kMax_UInt;
    } else {
        // here we need some empiric to calculate this size
        // for now just reduce it by 10%
        input_size = input_size - input_size / 10;
    }

    CRemoteAppRequest request(m_GridClient->GetNetCacheAPI());
    request.SetMaxInlineSize(input_size);
    CNetScheduleAPI::TJobMask jmask = CNetScheduleAPI::eEmptyMask;

    if (args["args"]) {
        string cmd = args["args"].AsString();
        string affinity;
        if (args["aff"]) {
            affinity = args["aff"].AsString();
        }
        list<string> tfiles;
        if (args["tfiles"]) {
            NStr::Split(args["tfiles"].AsString(), ";", tfiles);
        }
        request.SetCmdLine(cmd);
        if (tfiles.size() != 0) {
            ITERATE(list<string>, s, tfiles) {
                request.AddFileForTransfer(*s);
            }
        }
        string jout, jerr;
        if (args["jout"]) 
            jout = args["jout"].AsString();

        if (args["jerr"]) 
            jerr = args["jerr"].AsString();
        
        if (!jout.empty() && !jerr.empty())
            request.SetStdOutErrFileNames(jout, jerr, eLocalFile);

        if (args["runtime"]) {
            unsigned int rt = NStr::StringToUInt(args["runtime"].AsString());
            request.SetAppRunTimeout(rt);
        }
        if (args["exclusive"]) {
            jmask |= CNetScheduleAPI::eExclusiveJob;
        }

        CGridClient& grid_client = GetGridClient();
        grid_client.SetJobAffinity(affinity);
        grid_client.SetJobMask(jmask);
        request.Send(grid_client.GetOStream());
        string job_key = grid_client.Submit();
        if (out)
            *out << job_key << NcbiEndl;
        return 0;
    } else if (args["jf"]) {
        CGridJobBatchSubmitter* job_batch_submitter = NULL;
        if ( args["batch"]  ||  args["bsize"])
            job_batch_submitter = &GetGridClient().GetJobBatchSubmitter();

        int batch_size = 1000;
        if (args["bsize"])
            batch_size = args["bsize"].AsInteger();

        CNcbiIstream& is = args["jf"].AsInputFile();

        int njob_in_batch = 0;
        while ( !is.eof() && is.good() ) {
            string line;
            NcbiGetlineEOL(is, line);
            if( line.size() == 0 || line[0] == '#')
                continue;
            string cmd = s_FindParam(line, "args=\"");
            request.SetCmdLine(cmd);
            string affinity = s_FindParam(line, "aff=\"");
            string files = s_FindParam(line, "tfiles=\"");
            if (files.size() != 0) {
                list<string> tfiles;
                NStr::Split(files, ";", tfiles);
                if (tfiles.size() != 0) {
                    ITERATE(list<string>, s, tfiles) {
                        request.AddFileForTransfer(*s);
                    }
                }
            }
            string jout = s_FindParam(line, "jout=\"");
            string jerr = s_FindParam(line, "jerr=\"");

            if (!jout.empty() && !jerr.empty())
                request.SetStdOutErrFileNames(jout, jerr, eLocalFile);

            string srt = s_FindParam(line, "runtime=\"");
            if (!srt.empty()) {
                unsigned int rt = NStr::StringToUInt(srt);
                request.SetAppRunTimeout(rt);
            }
            CNetScheduleAPI::TJobMask jmask = CNetScheduleAPI::eEmptyMask;
            srt = s_FindParam(line, "exclusive=\"");
            if (!srt.empty()) {
                if(NStr::CompareNocase(srt, "yes") == 0 ||
                   NStr::CompareNocase(srt, "true") == 0 ||
                   srt == "1" )
                    jmask |= CNetScheduleAPI::eExclusiveJob;
            }
            if (!job_batch_submitter ) {
                CGridClient& grid_client = GetGridClient();
                request.Send(grid_client.GetOStream());
                grid_client.SetJobAffinity(affinity);
                grid_client.SetJobMask(jmask);
                string job_key = grid_client.Submit();
                if (out)
                    *out << job_key << NcbiEndl;
            } else {
                job_batch_submitter->PrepareNextJob();
                request.Send(job_batch_submitter->GetOStream());
                job_batch_submitter->SetJobAffinity(affinity);
                job_batch_submitter->SetJobMask(jmask);
                if (++njob_in_batch >= batch_size) {
                    job_batch_submitter->Submit();
                    if (out) {
                        const vector<CNetScheduleJob>& jobs = job_batch_submitter->GetBatch();
                        ITERATE(vector<CNetScheduleJob>, it, jobs)
                            *out << it->job_id << NcbiEndl;
                    }
                    job_batch_submitter->Reset();
                    njob_in_batch = 0;
                }
            }
        }
        if (job_batch_submitter) {
            job_batch_submitter->Submit();
            if (out) {
                const vector<CNetScheduleJob>& jobs = job_batch_submitter->GetBatch();
                ITERATE(vector<CNetScheduleJob>, it, jobs)
                    *out << it->job_id << NcbiEndl;
            }
            job_batch_submitter->Reset();
            job_batch_submitter = NULL;
        }

        return 0;
    }
    NCBI_THROW(CArgException, eInvalidArg,
               "Neither agrs nor jf arguments were specified");

}
int main(int argc, const char* argv[])
{
    return CNSSubmitRemoteJobApp().AppMain(argc, argv);
} 
