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
 * Authors:  Kevin Bealer
 *
 */

/// @file demo/remote_blast_demo.cpp
/// Example program for remote_blast C++ interface

#include <ncbi_pch.hpp>
#include "queue_poll.hpp"
#include "align_parms.hpp"
#include "search_opts.hpp"
#include <connect/ncbi_core_cxx.hpp>
#include <corelib/ncbiapp.hpp>

//  Remote Blast Demo App

USING_NCBI_SCOPE;

/// Application demonstrating use of CRemoteBlast class.
///
/// This application demonstrates use of the Blast4 API to process
/// searches on the SplitD system.  An options handle and CRemoteBlast
/// object are created from user specified command line options and
/// sent to the server.  Data is retrieved when ready, and formatted.
/// There is considerable framework here to select the type of API and
/// control available options.  Ordinary usage may in fact be simpler,
/// particularly if the user is always searching using the same type
/// of request, for example dicontiguous megablast or blastp.

class CRemote_blastApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
    
private:
    auto_ptr<CArgDescriptions> m_ArgDesc;
    
    void x_AddRemoteBlastKeys(void);
};

// Builds interface elements specific to this program

void CRemote_blastApplication::x_AddRemoteBlastKeys(void)
{
    // Program, Service, and Algorithm Selection options
    
    m_ArgDesc.reset(new CArgDescriptions);
    
    m_ArgDesc->AddDefaultKey("program", "ProgramName",
                            "Program type (blastn, blastp, blastx, tblastn, "
                            "tblastx) (Default is blastp.)",
                            CArgDescriptions::eString,
                            "blastp");
    m_ArgDesc->AddDefaultKey("service", "ServiceType",
                            "Service Type (default 'plain').",
                            CArgDescriptions::eString,
                            "plain");
    m_ArgDesc->AddFlag("megablast", "Use MegaBlast algorithm.");
    
    
    // Input and Output
    
    m_ArgDesc->AddDefaultKey("believedef", "BelieveDef",
                            "Believe the query defline default is false).",
                            CArgDescriptions::eBoolean,
                            "F");
    m_ArgDesc->AddKey       ("infile", "InputFilename",
                             "Filename of input query.",
                             CArgDescriptions::eInputFile);
    m_ArgDesc->AddDefaultKey("outputasn", "AsnOutput",
                            "Output raw ASN.1 objects.",
                            CArgDescriptions::eBoolean,
                            "F");
    m_ArgDesc->AddFlag("async_mode", "Return request-id or status immediately.");
    m_ArgDesc->AddOptionalKey("get_results",
                              "GetResults",
                              "Get results for the specified request-id",
                              CArgDescriptions::eString);
    
    
    // Domain Options
    
    m_ArgDesc->AddDefaultKey("db", "DatabaseName",
                            "Database name. (Default is nr.)",
                            CArgDescriptions::eString,
                            "nr");
    m_ArgDesc->AddFlag("apitrace", "Show method calls to the Blast API.");
    
    
    // Verbose debugging option - dumps information about network
    // traffic and text ASN.1 of transmitted objects.
    
    m_ArgDesc->AddFlag("verbose", "Verbose (debug) output.");
}


void CRemote_blastApplication::Init(void)
{
    // Keys specific to remote blast
    x_AddRemoteBlastKeys();
    
    // Algorithm & Domain Options
    CNetblastSearchOpts::CreateInterface(* m_ArgDesc.get());
    
    // Program description
    string prog_description = "remote_blast\n";
    
    m_ArgDesc->SetUsageContext(GetArguments().GetProgramBasename(),
                               prog_description,
                               false);
    
    // Pass argument descriptions to the application
    SetupArgDescriptions(m_ArgDesc.release());
}


// If the service type is "plain", this adjusts the service.  It
// should be expanded in the future to deal with PSI blast, RPS
// (possibly), and other useful bits and pieces.
// 
// 1. If phi_query is specified, service = "phi".
// 2. If megablast is specified, service = "megablast".

void s_SetService(string & service, string & /*program*/, const CArgs & args)
{
    int phi_supported = 0;
    
    if (service == "plain") {
        if (phi_supported && args["phi_query"].HasValue()) {
            service = "phi";
        } else if (args["megablast"]) {
            service = "megablast";
        }
    }
}


int CRemote_blastApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    
    CONNECT_Init(&GetConfig());
    
    // Process cmd line args
    
    const CArgs & args = GetArgs();
    
    // These options are remote_blast program options - and are not
    // passed on to the server, as such.
    
    CNcbiIstream & query_in(args["infile"]. AsInputFile());
    
    bool verbose(false);
    
    verbose = args["verbose"];
    
    // These parameters are required to queue the search - any default
    // values are supplied locally.
    
    string program       = args["program"]    .AsString();
    string database      = args["db"]         .AsString();
    string service       = args["service"]    .AsString();
    bool   trust_defline = args["believedef"] .AsBoolean();
    
    bool   raw_asn       = args["outputasn"]  .AsBoolean();
    trace_blast_api      = args["apitrace"];
    
    bool   async_mode    = args["async_mode"];
    
    
    // Workaround for formatting problem.
    if (program == "tblastx") {
        raw_asn = true;
    }
    
    string get_RID;
    
    if (args["get_results"].HasValue()) {
        get_RID = args["get_results"].AsString();
    }
    
    s_SetService(service, program, args);
    
    // These parameters are optional when queueing the search, and can
    // take server-specified defaults.
    
    // NOTE that anything added here should be defined above using 
    // AddOptionalKey() not AddDefaultKey().
    
    CNetblastSearchOpts opts(args);
    
    CAlignParms alparms;
    
    alparms.SetNumAlgn(opts.NumAligns());
    
    return QueueAndPoll(program,
                        service,
                        database,
                        opts,
                        query_in,
                        verbose,
                        trust_defline,
                        raw_asn,
                        alparms,
                        async_mode,
                        get_RID);
}


int main(int argc, const char* argv[])
{
    return CRemote_blastApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/06/09 16:07:04  bealer
 * - Document application class.
 *
 * Revision 1.2  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/04/20 15:42:23  bealer
 * - Change name of remote_blast.cpp to avoid doxygen and gdb issues.
 *
 * Revision 1.8  2004/04/19 14:37:52  bealer
 * - Fix compiler warnings.
 *
 * Revision 1.7  2004/04/12 14:59:29  ucko
 * Don't redeclare trace_blast_api.  (search_opts.hpp already declares it.)
 *
 * Revision 1.6  2004/03/23 14:11:04  camacho
 * Minor doxygen fix
 *
 * Revision 1.5  2004/03/22 20:46:22  bealer
 * - Fix non-literate comments to look less like doxygen comments.
 *
 * Revision 1.4  2004/03/18 13:49:04  camacho
 * Correct use of namespaces
 *
 * Revision 1.3  2004/03/16 19:41:03  vasilche
 * Namespace qualifier is invalid in extern declaration
 *
 * Revision 1.2  2004/02/18 16:53:19  bealer
 * - Adapt source from blast_client to Remote Blast API.
 *
 * ===========================================================================
 */
