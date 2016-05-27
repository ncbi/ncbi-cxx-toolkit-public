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
 *   Test for accessing the genomic_collection service.
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
#include <objects/genomecoll/GCClient_FindBestAssemblyR.hpp>
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

USING_NCBI_SCOPE;
USING_SCOPE(objects);


class CGenollService : public CGenomicCollectionsService
{
public:
    CGenollService(const string& cgi_url, bool nocache)
    : cgiUrl(cgi_url + (nocache ? "?nocache=true" :""))
    {
    }

    CGenollService(bool nocache)
    {
        if(nocache)
        {
            string service_name(GetService());
            CNcbiApplication::Instance()->SetEnvironment().Set((NStr::ToUpper(service_name) + "_CONN_ARGS").c_str(), "nocache=true");
        }
    }

private:
    virtual string x_GetURL() { return cgiUrl; }

    virtual void x_Connect()
    {
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

class CTestGenomicCollectionsSvcApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    int RunWithService(CGenomicCollectionsService& service, const CArgs& args, CNcbiOstream& ostr);
};

void CTestGenomicCollectionsSvcApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);

    // Specify USAGE context
    argDesc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Genomic Collections Service tester");

    argDesc->AddKey("request", "request",
                            "Type of request", CArgDescriptions::eString);
    argDesc->SetConstraint("request", &(*new CArgAllow_Strings,"get-chrtype-valid","get-assembly","get-assembly-blob","get-best-assembly","get-all-assemblies","get-equivalent-assemblies","get-assembly-by-sequence"));
    
    argDesc->AddOptionalKey("url", "Url",
                            "URL to genemic collections service.cgi", CArgDescriptions::eString);
    
    argDesc->AddOptionalKey("cgi", "path_to_cgi",
                    "Directly calls the CGI instead of using the gencoll client",
                    CArgDescriptions::eString);
    argDesc->SetDependency("cgi", argDesc->eExcludes, "url");
    
    argDesc->AddDefaultKey("o", "OutputFile",
                            "File for report",
                            CArgDescriptions::eOutputFile,
                            "-");

    argDesc->AddOptionalKey("acc", "assembly_accession",
                    "Accession for the assembly to retrieve",
                    CArgDescriptions::eString);
    
    argDesc->AddOptionalKey("rel_id", "release_id",
                    "Message to use for test",
                    CArgDescriptions::eInteger);
    argDesc->SetDependency("acc", argDesc->eExcludes, "rel_id");
    
    argDesc->AddOptionalKey("-mode", "mode",
                            "mode",
                            CArgDescriptions::eInteger);
    argDesc->AddOptionalKey("-smode", "mode",
                            "string mode",
                            CArgDescriptions::eString);

    argDesc->AddOptionalKey("-level", "level",
                            "level",
                            CArgDescriptions::eInteger);
    argDesc->SetConstraint("-level", &(*new CArgAllow_Strings,"0","1","2","3"));
    argDesc->AddAlias("l", "-level");
    
    argDesc->AddOptionalKey("-asm_flags", "assembly_flags",
                            "Assembly flags.  If not set use client default (scaffold).",
                            CArgDescriptions::eInteger);
    // argDesc->SetConstraint("-asm_flags", &(*new CArgAllow_Strings,"0","1","2","3","4","5","6","7"));
    argDesc->AddAlias("af","-asm_flags");

    argDesc->AddOptionalKey("-chr_flags", "chromosome_flags",
                            "Chromosome flags",
                            CArgDescriptions::eInteger);
    // argDesc->SetConstraint( "-chr_flags", &(*new CArgAllow_Strings,"0","1","2","3","4","5","6","7"));
    argDesc->AddAlias("chrf","-chr_flags");
    
    argDesc->AddOptionalKey( "-scf_flags", "scaffold_flags",
                             "Scaffold flags",
                             CArgDescriptions::eInteger);
    // argDesc->SetConstraint( "-scf_flags", &(*new CArgAllow_Strings,"0","1","2","3","4","5","6","7"));
    argDesc->AddAlias("sf", "-scf_flags");
    
    argDesc->AddOptionalKey("-comp_flags", "component_flags",
                            "Component flags",
                            CArgDescriptions::eInteger);
    // argDesc->SetConstraint("-comp_flags", &(*new CArgAllow_Strings,"0","1","2","3","4","5","6","7"));
    argDesc->AddAlias("cmpf","-comp_flags");

    argDesc->AddDefaultKey( "f", "format",
                            "Output Format. - ASN1 or XML", 
                            CArgDescriptions::eString, "ASN1");
    argDesc->SetConstraint("f", &(*new CArgAllow_Strings,"XML","XML1","xml","xml1","ASN.1","ASN1","asn1","asn.1"));
        
    argDesc->AddOptionalKey("type", "chr_type",
                    "Validate the Type Location",
                    CArgDescriptions::eString);

    argDesc->AddOptionalKey("loc", "chr_loc",
                    "Validate the Type Location",
                    CArgDescriptions::eString);

    argDesc->AddOptionalKey("-filter", "filter",
                            "Get assembly by sequence - filter",
                            CArgDescriptions::eString);

    argDesc->AddOptionalKey("-sort", "sort",
                            "Get assembly by sequence - sort",
                            CArgDescriptions::eString);

    argDesc->AddOptionalKey("-limit", "limit",
                            "Find best assembly - return limit",
                            CArgDescriptions::eInteger);

    argDesc->AddOptionalKey("top_asm", "top_assembly",
                            "Get assembly by sequence - return top assembly only",
                            CArgDescriptions::eBoolean);

    argDesc->AddOptionalKey("equiv", "equivalency",
                            "Get equivalent assemblies - equivalency type",
                            CArgDescriptions::eInteger);

    argDesc->AddFlag("nocache", "Do not use database cache; force fresh data");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(argDesc.release());
}

/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)
int CTestGenomicCollectionsSvcApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    
    if(args["f"] && NStr::FindNoCase(args["f"].AsString(),"XML") != NPOS)
        ostr << MSerial_Xml;
    else
        ostr << MSerial_AsnText;

    CRef<CGenomicCollectionsService> service;

    if(args["cgi"])      service.Reset(new CDirectCGIExec(args["cgi"].AsString(), args["nocache"]));
    else if(args["url"]) service.Reset(new CGenollService(args["url"].AsString(), args["nocache"]));
    else                 service.Reset(new CGenollService(args["nocache"]));

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

template<typename FUNC>
void ForEachID(const string& ids, FUNC func)
{
    list<string> id;
    NStr::Split(ids, ";,", id);
    for_each(id.begin(), id.end(), func);
}

int CTestGenomicCollectionsSvcApplication::RunWithService(CGenomicCollectionsService& service, const CArgs& args, CNcbiOstream& ostr)
{
    LOG_POST("testing genomic collections cgi.");

    const string request = args["request"].AsString();
    
    try {
        if(request == "get-chrtype-valid")
        {
            string type = args["type"].AsString();
            string loc  = args["loc"].AsString();

            ostr << service.ValidateChrType(type, loc);
        }
        else if(request == "get-assembly")
        {
            if(args["-mode"])
            {
                if (args["acc"])
                    ForEachID(args["acc"].AsString(), [&](const string& acc) { ostr << *RemoveVersions(service.GetAssembly(acc, args["-mode"].AsInteger())); });
                else if (args["rel_id"])
                    ForEachID(args["rel_id"].AsString(), [&](const string& rel_id) { ostr << *RemoveVersions(service.GetAssembly(NStr::StringToInt(rel_id), args["-mode"].AsInteger())); });
                else
                    ERR_POST(Error << "Either accession or release id should be provided");
            }
            else
            {
                int levelFlag = args["-level"] ? args["-level"].AsInteger():CGCClient_GetAssemblyRequest::eLevel_scaffold;
                int asmFlags = args["-asm_flags"] ? args["-asm_flags"].AsInteger():eGCClient_AttributeFlags_none;
                int chrAttrFlags = args["-chr_flags"] ? args["-chr_flags"].AsInteger():eGCClient_AttributeFlags_biosource; 
                int scafAttrFlags = args["-scf_flags"] ? args["-scf_flags"].AsInteger():eGCClient_AttributeFlags_none; 
                int compAttrFlags = args["-comp_flags"] ? args["-comp_flags"].AsInteger():eGCClient_AttributeFlags_none;

                if (args["acc"])
                    ForEachID(args["acc"].AsString(), [&](const string& acc) { ostr << *RemoveVersions(service.GetAssembly(acc, levelFlag, asmFlags, chrAttrFlags, scafAttrFlags, compAttrFlags)); });
                else if (args["rel_id"])
                    ForEachID(args["rel_id"].AsString(), [&](const string& rel_id) { ostr << *RemoveVersions(service.GetAssembly(NStr::StringToInt(rel_id), levelFlag, asmFlags, chrAttrFlags, scafAttrFlags, compAttrFlags)); });
                else
                    ERR_POST(Error << "Either accession or release id should be provided");
            }
        }
        else if(request == "get-assembly-blob")
        {
            if(!args["-smode"] || args["-smode"].AsString().empty())
                ERR_POST(Error << "Invalid get-assembly-blob string mode");

            if (args["acc"])
                ForEachID(args["acc"].AsString(), [&](const string& acc) { ostr << *RemoveVersions(service.GetAssembly(acc, args["-smode"].AsString())); });
            else if (args["rel_id"])
                ForEachID(args["rel_id"].AsString(), [&](const string& rel_id) { ostr << *RemoveVersions(service.GetAssembly(NStr::StringToInt(rel_id), args["-smode"].AsString())); });
            else
                ERR_POST(Error << "Either accession or release id should be provided");
        }
        else if(request == "get-best-assembly")
        {
            list<string> acc;
            NStr::Split(args["acc"].AsString(), ",", acc);

            int filter = args["filter"] ? args["filter"].AsInteger() : eGCClient_FindBestAssemblyFilter_all;
            int sort = args["sort"] ? args["sort"].AsInteger() : eGCClient_FindBestAssemblySort_default;

            if(acc.size() == 1)
            {
                CRef<CGCClient_AssemblyInfo> reply(service.FindBestAssembly(acc.front(), filter, sort));
                ostr << *reply;
            }
            else
            {
                CRef<CGCClient_AssemblySequenceInfo> reply(service.FindBestAssembly(acc, filter, sort));
                ostr << *reply;
            }
        }
        else if(request == "get-all-assemblies")
        {
            list<string> acc;
            NStr::Split(args["acc"].AsString(), ",", acc);

            int filter = args["filter"] ? args["filter"].AsInteger() : eGCClient_FindBestAssemblyFilter_all;
            int sort = args["sort"] ? args["sort"].AsInteger() : eGCClient_FindBestAssemblySort_default;

            CRef<CGCClient_AssembliesForSequences> reply(service.FindAllAssemblies(acc, filter, sort));
            ostr << *reply;
        }
        else if(request == "get-assembly-by-sequence")
        {
            list<string> acc, filter_s;
            NStr::Split(args["acc"].AsString(), ",", acc);
            NStr::Split(args["filter"] ? args["filter"].AsString() : "all", ",", filter_s);

            int filter = 0;
            ITERATE(list<string>, it, filter_s)
                filter |= ENUM_METHOD_NAME(EGCClient_GetAssemblyBySequenceFilter)()->FindValue(*it);

            int sort = args["sort"] ? CGCClient_GetAssemblyBySequenceRequest::ENUM_METHOD_NAME(ESort)()->FindValue(args["sort"].AsString()) : CGCClient_GetAssemblyBySequenceRequest::eSort_default;
            bool top_asm = args["top_asm"] ? args["top_asm"].AsBoolean() : false;

            if(top_asm)
            {
                CRef<CGCClient_AssemblySequenceInfo> reply(service.FindOneAssemblyBySequences(acc, filter, CGCClient_GetAssemblyBySequenceRequest::ESort(sort)));
                ostr << *reply;
            }
            else
            {
                CRef<CGCClient_AssembliesForSequences> reply(service.FindAssembliesBySequences(acc, filter, CGCClient_GetAssemblyBySequenceRequest::ESort(sort)));
                ostr << *reply;
            }
        }
        else if(request == "get-equivalent-assemblies")
        {
            string acc = args["acc"].AsString();
            int equiv = args["equiv"].AsInteger();

            CRef<CGCClient_EquivalentAssemblies> reply(service.GetEquivalentAssemblies(acc, equiv));
            ostr << *reply;
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
    return CTestGenomicCollectionsSvcApplication().AppMain(argc, argv);
}
