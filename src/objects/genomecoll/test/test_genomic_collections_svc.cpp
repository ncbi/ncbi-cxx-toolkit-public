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

class CTestGenomicCollectionsSvcApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    void PrepareRequest(CGCClientRequest& request, const CArgs& args);
    int RunServerDirect(const CArgs& args, CNcbiOstream& ostr);
    int RunUsingClient(const CArgs& args, CNcbiOstream& ostr);
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
    argDesc->SetConstraint("request", &(*new CArgAllow_Strings,"get-chrtype-valid","get-assembly","get-assembly-blob","get-best-assembly","get-all-assemblies","get-equivalent-assemblies"));
    
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
                            "Find best assembly - filter",
                            CArgDescriptions::eInteger);

    argDesc->AddOptionalKey("-sort", "sort",
                            "Find best assembly - sort",
                            CArgDescriptions::eInteger);

    argDesc->AddOptionalKey("-limit", "limit",
                            "Find best assembly - return limit",
                            CArgDescriptions::eInteger);

    argDesc->AddOptionalKey("equiv", "equivalency",
                            "Get equivalent assemblies - equivalency type",
                            CArgDescriptions::eInteger);

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

    int retVal = 1;
    if(!args["cgi"]) {
        retVal = RunUsingClient(args, ostr);
    } else {
        retVal = RunServerDirect(args, ostr);
    }
    return retVal;
}

template <typename TReq> void SetRequestId(TReq& req, const CArgs& args)
{
    if(args["acc"]) {
        req.SetAccession(args["acc"].AsString());
    } else {
        req.SetRelease_id(args["rel_id"].AsInteger());
    }
}

void CTestGenomicCollectionsSvcApplication::PrepareRequest(CGCClientRequest& gc_request, const CArgs& args)
{
    string request = args["request"].AsString();

    if(request == "get-chrtype-valid")
    {
        CGCClient_ValidateChrTypeLocRequest& req = gc_request.SetGet_chrtype_valid();
        req.SetType(args["type"].AsString());
        req.SetLocation(args["loc"].AsString());
    }
    else if(request == "get-assembly")
    {
        CGCClient_GetAssemblyRequest& req = gc_request.SetGet_assembly();
        SetRequestId(req, args);

        if(args["-mode"])
        {
            CGCClient_GetAssemblyRequest::SRequestParam params;
            if(!params.SetMode(CGCClient_GetAssemblyRequest::EAssemblyMode(args["-mode"].AsInteger())))
                ERR_POST(Error << "Invalid get-assembly mode: " << args["-mode"].AsInteger());

            req.SetLevel(params.level);
            req.SetAssm_flags(params.assembly_flags);
            req.SetChrom_flags(params.chromosome_flags);
            req.SetScaf_flags(params.scaffold_flags);
            req.SetComponent_flags(params.component_flags);
        }
        else
        {
            req.SetLevel(args["-level"] ? args["-level"].AsInteger():CGCClient_GetAssemblyRequest::eLevel_scaffold);
            req.SetAssm_flags(args["-asm_flags"] ? args["-asm_flags"].AsInteger():eGCClient_AttributeFlags_none);
            req.SetChrom_flags(args["-chr_flags"] ? args["-chr_flags"].AsInteger():eGCClient_AttributeFlags_biosource);
            req.SetScaf_flags(args["-scf_flags"] ? args["-scf_flags"].AsInteger():eGCClient_AttributeFlags_none);
            req.SetComponent_flags(args["-comp_flags"] ? args["-comp_flags"].AsInteger():eGCClient_AttributeFlags_none);
        }
    }
    else if(request == "get-assembly-blob")
    {
        CGCClient_GetAssemblyBlobRequest& req = gc_request.SetGet_assembly_blob();
        SetRequestId(req, args);

        if(!args["-smode"] || args["-smode"].AsString().empty()) {
            ERR_POST(Error << "Invalid get-assembly-blob string mode");
        }
        req.SetMode(args["-smode"].AsString());
    }
    else if(request == "get-best-assembly" || request == "get-all-assemblies")
    {
        CGCClient_FindBestAssemblyRequest& req = gc_request.SetGet_best_assembly();
        NStr::Split(args["acc"].AsString(), ",", req.SetSeq_id_acc());
        if(args["-filter"])   req.SetFilter(args["-filter"].AsInteger());
        if(args["-sort"]  )   req.SetSort(args["-sort"].AsInteger());
        if(args["-limit"] )   req.SetAssembly_return_limit(args["-limit"].AsInteger());
        else if(request == "get-best-assembly")   req.SetAssembly_return_limit(1);
    }
    else if(request == "get-equivalent-assemblies")
    {
        CGCClient_GetEquivalentAssembliesRequest& req = gc_request.SetGet_equivalent_assemblies();
        req.SetAccession(args["acc"].AsString());
        req.SetEquivalency(args["equiv"].AsInteger());
    }
}

int CTestGenomicCollectionsSvcApplication::RunServerDirect(const CArgs& args, CNcbiOstream& ostr)
{
    string cmd = args["cgi"].AsString();
    
    CGCClientRequest request;
    PrepareRequest(request, args);
    
    cout << "\nDirectly invoking CGI with following post request :\n" << MSerial_AsnText << request << endl;
    
    std::ostringstream  objStream;
    objStream << MSerial_AsnBinary << request;
    string reqString = objStream.str();
    CNcbiIstrstream inStr(reqString.c_str(), reqString.size());
    
    ostringstream outStr(ios::out|ios::binary);
    ostringstream errStr;
    int exitCode = -1;
    const char* env[] = {"REQUEST_METHOD=POST", 0};

    vector<string> emptyArgs;
    CPipe::EFinish retVal = CPipe::ExecWait (cmd, emptyArgs, inStr, outStr, errStr, exitCode, kEmptyStr, env);
    
    string output;
    char buffer[1000];
    if(retVal != CPipe::eDone || exitCode != 0) {
        cout << "Process Killed or Aborted. CPipe::ExecWait return value " << retVal
            << ".  Process Exit code: " << exitCode << endl;
    } else {
        output = outStr.str();
        cout << "OutStream size = " << output.size() << endl;
        
        CNcbiIstrstream  objOutStream(output.c_str(), output.size());
        
        bool discarding = true; int linesDiscarded = 0;
        while(discarding) {
            objOutStream.getline(buffer, sizeof(buffer)-1);
            discarding = !( (strlen(buffer) == 0) || (strlen(buffer) == 1 && buffer[0] == 13));
            if(discarding) {
                cout << "Discarding header line " << ++linesDiscarded << " : " << buffer << endl;
            }
        }
        
        CGCClientResponse reply;
        objOutStream >> MSerial_AsnBinary >> reply;

        if(reply.IsGet_assembly())
            ostr << reply.GetGet_assembly();
        else if(reply.IsGet_best_assembly())
        {
            if(args["request"].AsString() == "get-best-assembly")
            {
                if(request.GetGet_best_assembly().GetSeq_id_acc().size() > 1)
                    ostr << *reply.GetGet_best_assembly().GetAssemblies().front();
                else
                    ostr << reply.GetGet_best_assembly().GetAssemblies().front()->GetAssembly();
            }
            else
                ostr << reply.GetGet_best_assembly();
        }
        else if(reply.IsGet_chrtype_valid())
            ostr << reply.GetGet_chrtype_valid();
        else
            ostr << reply;
    }
    cout << "ErrStream>>>\n" << errStr.str() << "\n<<< end ErrStream" << endl;
    
    return exitCode;
}

static
void RemoveVersions(CRef<CGC_Assembly>& reply)
{
    if (reply->IsAssembly_set() &&
        reply->SetAssembly_set().IsSetDesc() &&
        reply->SetAssembly_set().SetDesc().CanGetDescr())
    {
        list< CRef<CSeqdesc> >& l = reply->SetAssembly_set().SetDesc().SetDescr().Set();

        l.erase(remove_if(begin(l), end(l),
                          [](CRef<CSeqdesc> desc){
                              return desc->IsUser() &&
                                     desc->GetUser().IsSetType() &&
                                     desc->GetUser().GetType().IsStr() &&
                                     desc->GetUser().GetType().GetStr() == "versions";
                          }),
                l.end());
    }
}

int CTestGenomicCollectionsSvcApplication::RunUsingClient(const CArgs& args, CNcbiOstream& ostr)
{
    string sss = GetConfig().Get("genomic_collections_cli", "service");
    list<string> sect;    GetConfig().EnumerateSections(&sect);
    CConstRef< IRegistry > r1(GetConfig().FindByName("genomic_collections_cli"));
    CConstRef< IRegistry > r2(GetConfig().FindByContents("genomic_collections_cli"));

    CRef<CGenomicCollectionsService> cli(args["url"] ?
                    new CGenomicCollectionsService(args["url"].AsString()) : 
                    new CGenomicCollectionsService());

    LOG_POST("testing genomic collections cgi.");

    string request = args["request"].AsString();
    
    try {
        if(request == "get-chrtype-valid")
        {
            string type = args["type"].AsString();
            string loc  = args["loc"].AsString();

            ostr << cli->ValidateChrType(type, loc);
        }
        else if(request == "get-assembly")
        {
            CRef<CGC_Assembly> reply;

            if(args["-mode"])
            {
                if (args["acc"])
                    reply.Reset(cli->GetAssembly(args["acc"].AsString(), args["-mode"].AsInteger()));
                else if (args["rel_id"])
                    reply.Reset(cli->GetAssembly(args["rel_id"].AsInteger(), args["-mode"].AsInteger()));
            }
            else
            {
                int levelFlag = args["-level"] ? args["-level"].AsInteger():CGCClient_GetAssemblyRequest::eLevel_scaffold;
                int asmFlags = args["-asm_flags"] ? args["-asm_flags"].AsInteger():eGCClient_AttributeFlags_none;
                int chrAttrFlags = args["-chr_flags"] ? args["-chr_flags"].AsInteger():eGCClient_AttributeFlags_biosource; 
                int scafAttrFlags = args["-scf_flags"] ? args["-scf_flags"].AsInteger():eGCClient_AttributeFlags_none; 
                int compAttrFlags = args["-comp_flags"] ? args["-comp_flags"].AsInteger():eGCClient_AttributeFlags_none;

                if (args["acc"])
                    reply.Reset(cli->GetAssembly(args["acc"].AsString(), levelFlag, asmFlags, chrAttrFlags, scafAttrFlags, compAttrFlags));
                else if (args["rel_id"])
                    reply.Reset(cli->GetAssembly(args["rel_id"].AsInteger(), levelFlag, asmFlags, chrAttrFlags, scafAttrFlags, compAttrFlags));
            }

            RemoveVersions(reply);
            ostr << *reply;
        }
        else if(request == "get-assembly-blob")
        {
            if(!args["-smode"] || args["-smode"].AsString().empty()) {
                ERR_POST(Error << "Invalid get-assembly-blob string mode");
            }

            CRef<CGC_Assembly> reply;
            if (args["acc"])
                reply.Reset(cli->_GetAssemblyNew(args["acc"].AsString(), args["-smode"].AsString()));
            else if (args["rel_id"])
                reply.Reset(cli->_GetAssemblyNew(args["rel_id"].AsInteger(), args["-smode"].AsString()));
            else
                ERR_POST(Error << "Either accession or release id should be provided");

            RemoveVersions(reply);
            ostr << *reply;
        }
        else if(request == "get-best-assembly")
        {
            list<string> acc;
            NStr::Split(args["acc"].AsString(), ",", acc);

            int filter = args["filter"] ? args["filter"].AsInteger() : eGCClient_FindBestAssemblyFilter_any;
            int sort = args["sort"] ? args["sort"].AsInteger() : eGCClient_FindBestAssemblySort_default;

            if(acc.size() == 1)
            {
                CRef<CGCClient_AssemblyInfo> reply(cli->FindBestAssembly(acc.front(), filter, sort));
                ostr << *reply;
            }
            else
            {
                CRef<CGCClient_AssemblySequenceInfo> reply(cli->FindBestAssembly(acc, filter, sort));
                ostr << *reply;
            }
        }
        else if(request == "get-all-assemblies")
        {
            list<string> acc;
            NStr::Split(args["acc"].AsString(), ",", acc);

            int filter = args["filter"] ? args["filter"].AsInteger() : eGCClient_FindBestAssemblyFilter_any;
            int sort = args["sort"] ? args["sort"].AsInteger() : eGCClient_FindBestAssemblySort_default;

            CRef<CGCClient_AssembliesForSequences> reply(cli->FindAllAssemblies(acc, filter, sort));
            ostr << *reply;
        }
        else if(request == "get-equivalent-assemblies")
        {
            string acc = args["acc"].AsString();
            int equiv = args["equiv"].AsInteger();

            CRef<CGCClient_EquivalentAssemblies> reply(cli->GetEquivalentAssemblies(acc, equiv));
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
