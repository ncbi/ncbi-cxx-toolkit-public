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
 * Authors:  Vinay Kumar
 *
 * File Description:
 *   Client for accessing the genomic_collection service.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>
#include <objects/genomecoll/GC_AssemblySet.hpp>
#include <objects/genomecoll/GC_AssemblyUnit.hpp>
#include <objects/genomecoll/GC_AssemblyDesc.hpp>
#include <objects/genomecoll/GCClient_ValidateChrTypeLo.hpp>
#include <objects/genomecoll/GCClient_AssemblyInfo.hpp>
#include <objects/genomecoll/GCClient_AssemblySequenceI.hpp>
#include <objects/genomecoll/GCClient_AssembliesForSequ.hpp>
#include <objects/genomecoll/GCClient_EquivalentAssembl.hpp>
#include <objects/genomecoll/GCClient_GetEquivalentAsse.hpp>
#include <objects/genomecoll/GCClient_GetAssemblyBlobRe.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <sstream>
#include <numeric>

using namespace ncbi;
using namespace ncbi::objects;
using namespace std;

class CGencollService : public CGenomicCollectionsService
{
public:
    CGencollService(const string& cgi_url, bool nocache)
    : cgiUrl(cgi_url + (nocache ? "?nocache=true" :""))
    {
    }

    CGencollService(bool nocache)
    {
        if(nocache)
        {
            string service_name(GetService());
            CNcbiApplication::Instance()->SetEnvironment().Set((NStr::ToUpper(service_name) + "_CONN_ARGS").c_str(), "nocache=true");
        }
    }
    
    CGencollService(const CArgs& args) : CGenomicCollectionsService(args)
    {
    }

private:
    virtual string x_GetURL() { return cgiUrl; }

    virtual void x_Connect()
    {
        CUrlArgs args(m_RetryCtx.GetArgs());
        if(args.IsSetValue("cache_request_id"))
            cout << "cache_request_id=" << args.GetValue("cache_request_id") << endl;

#ifdef _DEBUG
        LOG_POST(Info << "Connecting to url:" << x_GetURL().c_str());
#endif
        if(x_GetURL().empty())
            CGenomicCollectionsService::x_Connect();
        else
            x_ConnectURL(x_GetURL());
    }

    const string cgiUrl;
};

class CDirectCGIExec : public CGenomicCollectionsService
{
public:
    CDirectCGIExec(const string& cgi_path, bool nocache)
    : cgiPath(cgi_path)
    {
        if(nocache)
        {
            cgiArgs.push_back("-nocache");
            cgiArgs.push_back("true");
        }
    }

    virtual void Ask(const CGCClientRequest& request, CGCClientResponse& reply)
    {
        cout << "\nDirectly invoking CGI with following post request :\n" << MSerial_AsnText << request << endl;

        ostringstream errStr;
        stringstream  outStr(ios::in|ios::out|ios::binary);
        stringstream  inStr (ios::in|ios::out|ios::binary);
        inStr << MSerial_AsnBinary << request;

        int exitCode = -1;
        const char* env[] = {"REQUEST_METHOD=POST", 0};

        CPipe::EFinish retVal = CPipe::ExecWait(cgiPath, cgiArgs, inStr, outStr, errStr, exitCode, kEmptyStr, env);

        if(retVal != CPipe::eDone || exitCode != 0)
        {
            cout << "Process Killed or Aborted. CPipe::ExecWait return value " << retVal
                << ".  Process Exit code: " << exitCode << endl;
            exit(exitCode);
        }

        cout << "OutStream size = " << outStr.str().size() << endl;

        cout << "ErrStream >>>>>>>>>" << endl
            << errStr.str() << endl
            << "<<<<<<<<< ErrStream" << endl;

        SkipHeader(outStr);

        outStr >> MSerial_AsnBinary >> reply;
    }

private:
    const string cgiPath;
    vector<string> cgiArgs;

    void SkipHeader(istream& is)
    {
        char buffer[1000];
        bool discarding = true; int linesDiscarded = 0;
        while(discarding)
        {
            is.getline(buffer, sizeof(buffer)-1);
            discarding = !( (strlen(buffer) == 0) || (strlen(buffer) == 1 && buffer[0] == 13));
            if(discarding)
                cout << "Discarding header line " << ++linesDiscarded << " : " << buffer << endl;
        }
    }
};


class CClientGenomicCollectionsSvcApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    int RunWithService(CGenomicCollectionsService& service, const CArgs& args, CNcbiOstream& ostr);
};

void CClientGenomicCollectionsSvcApplication::Init(void)
{
    auto AddCommonArgs = [](CArgDescriptions* arg_desc) 
    { 
        arg_desc->AddOptionalKey("url", "url_to_service", "URL to genemic collections service.cgi", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("cgi", "path_to_cgi", "Directly calls the CGI instead of using the gencoll client", CArgDescriptions::eString);
        arg_desc->SetDependency("cgi", arg_desc->eExcludes, "url");
        arg_desc->AddOptionalKey("retries", "retry_count", "Max times to retry asking the CGI for a response.", CArgDescriptions::eInteger);
        arg_desc->SetDependency("retries", arg_desc->eExcludes, "cgi");
        arg_desc->AddDefaultKey("o", "OutputFile", "File for report", CArgDescriptions::eOutputFile, "-");
        arg_desc->AddDefaultKey( "f", "format", "Output Format. - ASN1 or XML", CArgDescriptions::eString, "ASN1");
        arg_desc->SetConstraint("f", &(*new CArgAllow_Strings,"XML","XML1","xml","xml1","ASN.1","ASN1","asn1","asn.1"));
        arg_desc->AddFlag("nocache", "Do not use database cache; force fresh data");
    };

    auto AddSeqAcc = [](CArgDescriptions* arg_desc)
    {
        arg_desc->AddKey("acc", "ACC_VER", "Comma-separated list of sequences", CArgDescriptions::eString);
        arg_desc->AddKey("acc_file", "acc_file", "File with list of sequences - one per line", CArgDescriptions::eInputFile);
        arg_desc->SetDependency("acc_file", arg_desc->eExcludes, "acc");
        arg_desc->AddFlag("roles", "Add sequence role data to output");
    };

    auto AddAccRelId = [&](CArgDescriptions* arg_desc)
    {
        arg_desc->AddKey("acc", "ACC_VER", "Comma-separated list of assembly accessions", CArgDescriptions::eString);
        arg_desc->AddKey("acc_file", "acc_file", "File with list of assembly accessions - one per line", CArgDescriptions::eInputFile);
        arg_desc->SetDependency("acc_file", arg_desc->eExcludes, "acc");
        arg_desc->AddKey("rel_id", "release_id", "Comma-separated list of assembly release id's'", CArgDescriptions::eString);
        arg_desc->SetDependency("rel_id", arg_desc->eExcludes, "acc");
        arg_desc->SetDependency("rel_id", arg_desc->eExcludes, "acc_file");
        arg_desc->AddKey("rel_file", "rel_file", "File with list of assembly release_ids - one per line", CArgDescriptions::eInputFile);
        arg_desc->SetDependency("rel_file", arg_desc->eExcludes, "acc");
        arg_desc->SetDependency("rel_file", arg_desc->eExcludes, "acc_file");
        arg_desc->SetDependency("rel_file", arg_desc->eExcludes, "rel_id");
        CGenomicCollectionsService::AddArguments(*arg_desc);
    };

    auto AddFilterSort = [](CArgDescriptions* arg_desc)
    {
        arg_desc->AddOptionalKey("-filter", "filter", "Get assembly by sequence - filter", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("-sort", "sort", "Get assembly by sequence - sort", CArgDescriptions::eString);
    };

    unique_ptr<CCommandArgDescriptions> cmds_desc(new CCommandArgDescriptions());
    cmds_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Genomic Collections Service client application");

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Validate chromosome type and location");
    arg_desc->AddKey("type", "chr_type", "chromosome type", CArgDescriptions::eString);
    arg_desc->AddKey("loc", "chr_loc", "chromosome location", CArgDescriptions::eString);
    AddCommonArgs(arg_desc.get());
    cmds_desc->AddCommand("get-chrtype-valid", arg_desc.release(), "vc");

    arg_desc.reset(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Get assembly");
    arg_desc->AddDefaultKey("-mode", "AllSequences", "Assembly retrieval mode", CArgDescriptions::eString, "AssemblyOnly");
    AddAccRelId(arg_desc.get());
    AddCommonArgs(arg_desc.get());
    cmds_desc->AddCommand("get-assembly", arg_desc.release(), "ga");

    arg_desc.reset(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Get assemblies containing sequence");
    arg_desc->AddFlag("top_asm", "Return top assembly only");
    AddSeqAcc(arg_desc.get());
    AddFilterSort(arg_desc.get());
    AddCommonArgs(arg_desc.get());
    cmds_desc->AddCommand("get-assembly-by-sequence", arg_desc.release(), "gas");

    arg_desc.reset(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Get assemblies equivalent to a given one");
    arg_desc->AddKey("acc", "ACC_VER", "Assembly accession", CArgDescriptions::eString);
    arg_desc->AddKey("equiv", "equivalency", "Get equivalent assemblies - equivalency type", CArgDescriptions::eInteger);
    AddCommonArgs(arg_desc.get());
    cmds_desc->AddCommand("get-equivalent-assemblies", arg_desc.release(), "gea");

    SetupArgDescriptions(cmds_desc.release());
}

/////////////////////////////////////////////////////////////////////////////
int CClientGenomicCollectionsSvcApplication::Run(void)
{
    const CArgs& args = GetArgs();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    
    if(args["f"] && NStr::FindNoCase(args["f"].AsString(),"XML") != NPOS)
        ostr << MSerial_Xml;
    else
        ostr << MSerial_AsnText;

    CRef<CGenomicCollectionsService> service;

    if(args["cgi"])         service.Reset(new CDirectCGIExec(args["cgi"].AsString(), args["nocache"]));
    else if(args["url"])    service.Reset(new CGencollService(args["url"].AsString(), args["nocache"]));
    else if(args.Exist("gc-cache") && args["gc-cache"])  
                            service.Reset(new CGencollService(args));
    else                    service.Reset(new CGencollService(args["nocache"]));

    if (args["retries"]) service->SetRetryLimit(args["retries"].AsInteger());

    return RunWithService(*service, args, ostr);
}

static
CGC_AssemblyDesc* GetAssebmlyDesc(CRef<CGC_Assembly>& assembly)
{
    return assembly->IsAssembly_set() && assembly->GetAssembly_set().IsSetDesc() ? &assembly->SetAssembly_set().SetDesc() :
           assembly->IsUnit() && assembly->GetUnit().IsSetDesc() ? &assembly->SetUnit().SetDesc() :
           NULL;

}

static
bool isVersionsObject(CRef<CSeqdesc> desc)
{
    return desc->IsUser() &&
        desc->GetUser().IsSetType() &&
        desc->GetUser().GetType().IsStr() &&
        desc->GetUser().GetType().GetStr() == "versions";
}

static
CRef<CGC_Assembly> RemoveVersions(CRef<CGC_Assembly> assembly)
{
    CGC_AssemblyDesc* desc = GetAssebmlyDesc(assembly);
    if (desc && desc->CanGetDescr())
    {
        list< CRef<CSeqdesc> >& l = desc->SetDescr().Set();
        l.erase(remove_if(l.begin(), l.end(),
                          isVersionsObject),
                l.end());
    }

    return assembly;
}

static list<string> GetIDs(const string& ids)
{
    list<string> id_list;
    NStr::Split(ids, ";,", id_list, NStr::fSplit_Tokenize);
    return id_list;
}

static list<string> GetIDsFromFile(CNcbiIstream& istr)
{
    list<string> accessions;
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()  ||  line[0] == '#') {
            continue;
        }
        accessions.push_back(line);
    }
    return accessions;
}

static list<string> GetAccessions(const CArgs& args)
{
    return args["acc"].HasValue()      ? GetIDs(args["acc"].AsString()) :
           args["acc_file"].HasValue() ? GetIDsFromFile(args["acc_file"].AsInputFile()) :
                                         list<string>();
}
static list<string> GetReleaseIds(const CArgs& args)
{
    return args["rel_id"].HasValue()   ? GetIDs(args["rel_id"].AsString()) :
           args["rel_file"].HasValue() ? GetIDsFromFile(args["rel_file"].AsInputFile()) :
                                         list<string>();
}

int CClientGenomicCollectionsSvcApplication::RunWithService(CGenomicCollectionsService& service, const CArgs& args, CNcbiOstream& ostr)
{
    try {
        if(args.GetCommand() == "get-chrtype-valid")
        {
            ostr << service.ValidateChrType(args["type"].AsString(), args["loc"].AsString());
        }
        else if(args.GetCommand() == "get-assembly")
        {
            if (args["acc"] || args["acc_file"])
                for (auto acc: GetAccessions(args)) ostr << *RemoveVersions(service.GetAssembly(acc, args["-mode"].AsString()));
            else if (args["rel_id"] || args["rel_file"])
                for (auto rel_id: GetReleaseIds(args)) ostr << *RemoveVersions(service.GetAssembly(NStr::StringToInt(rel_id), args["-mode"].AsString()));
            else
                ERR_POST(Error << "Either accession or release id should be provided");
        }
        else if(args.GetCommand() == "get-assembly-by-sequence")
        {
            list<string> filter_s;
            NStr::Split(args["filter"] ? args["filter"].AsString() : "all", ",", filter_s, NStr::fSplit_Tokenize);

            const int filter = accumulate(filter_s.begin(), filter_s.end(), 0, [](int acc, const string& f)
                               {
                                   return acc | ENUM_METHOD_NAME(EGCClient_GetAssemblyBySequenceFilter)()->FindValue(f);
                               });
            const int sort = args["sort"] ? CGCClient_GetAssemblyBySequenceRequest::ENUM_METHOD_NAME(ESort)()->FindValue(args["sort"].AsString()) : CGCClient_GetAssemblyBySequenceRequest::eSort_default;
            const bool with_roles = args["roles"];

            if (args["top_asm"].HasValue())
                ostr << *service.FindOneAssemblyBySequences(GetAccessions(args), filter, CGCClient_GetAssemblyBySequenceRequest::ESort(sort), with_roles);
            else
                ostr << *service.FindAssembliesBySequences(GetAccessions(args), filter, CGCClient_GetAssemblyBySequenceRequest::ESort(sort), with_roles);
        }
        else if(args.GetCommand() == "get-equivalent-assemblies")
        {
            ostr << *service.GetEquivalentAssemblies(args["acc"].AsString(), args["equiv"].AsInteger());
        }
    } catch (CException& ex) {
        ERR_POST(Error << "Caught an exception from client library ..." << ex.what());
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    GetDiagContext().SetOldPostFormat(false);
    return CClientGenomicCollectionsSvcApplication().AppMain(argc, argv);
}
