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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Demo of using the C++ Object Manager (OM)
 *
 */

#include <ncbi_pch.hpp>
#include "queue_poll.hpp"
#include <connect/ncbi_core_cxx.hpp>
#include <corelib/ncbiapp.hpp>

using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//

class CBlast_clientApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CBlast_clientApplication::Init(void)
{
    // Prepare command line descriptions
    
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("program", "ProgramName",
                            "Program type (blastn, blastp, blastx, tblastn, "
                            "tblastx) (Default is blastp.)",
                            CArgDescriptions::eString,
                            "blastp");
    
    arg_desc->AddDefaultKey("service", "ServiceType",
                            "Service Type (default 'plain').",
                            CArgDescriptions::eString,
                            "plain");
    
    arg_desc->AddDefaultKey("db", "DatabaseName",
                            "Database name. (Default is nr.)",
                            CArgDescriptions::eString,
                            "nr");
    
    arg_desc->AddDefaultKey("believedef", "BelieveDef",
                            "Believe the query defline default is false).",
                            CArgDescriptions::eBoolean,
                            "F");
    
    arg_desc->AddKey("infile", "InputFilename",
                     "Filename of input query.",
                     CArgDescriptions::eInputFile);
    
    arg_desc->AddFlag("verbose", "Verbose (debug) output.");
    
    arg_desc->AddFlag("megablast", "Use MegaBlast algorithm.");
    
    arg_desc->AddDefaultKey("outputasn", "AsnOutput",
                            "Output raw ASN.1 objects.",
                            CArgDescriptions::eBoolean,
                            "F");
    
    
    //arg_desc->AddOptionalKey("E", "ExpectValue",
    //                         "Expect value (cutoff).",
    //                         CArgDescriptions::eDouble);
    
    CNetblastSearchOpts::CreateInterface(*arg_desc);
    
    // Program description
    string prog_description = "blast_client\n";
    
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);
    
    // Pass argument descriptions to the application
    //
    
    SetupArgDescriptions(arg_desc.release());
}

// If the service type is "plain", this adjusts the service using the
// following rules:

// 1. If phi_query is specified, service = "phi"
//

void s_SetService(string & service, string & /*program*/, const CArgs & args)
{
    if (service == "plain") {
        if (args["phi_query"].HasValue()) {
            service = "phi";
        } else if (args["megablast"]) {
            service = "megablast";
        }
    }
}

int CBlast_clientApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    
    CONNECT_Init(&GetConfig());
    
    // Process cmd line args
    
    const CArgs & args = GetArgs();
    
    // These options are blast_client program options - and are not passed
    // on to the server, as such.
    
    CNcbiIstream & query_in(args["infile"]. AsInputFile());
    bool           verbose (args["verbose"]);
    
    // These parameters are required to queue the search - any default
    // values are supplied locally.
    
    string program       = args["program"]   .AsString();
    string database      = args["db"]        .AsString();
    string service       = args["service"]   .AsString();
    bool   trust_defline = args["believedef"].AsBoolean();
    
    bool   raw_asn       = args["outputasn"] .AsBoolean();
    
    s_SetService(service, program, args);
    
    // These parameters are optional when queueing the search, and can
    // take server-specified defaults.
    
    // NOTE that anything added here should be defined above using 
    // AddOptionalKey() not AddDefaultKey().
    
    CNetblastSearchOpts opts(args);
    
    CAlignParms alparms;
    
    alparms.SetNumAlgn(opts.NumAligns());
    //alparms.SetNumDesc(opts.NumDesc());
    
    return QueueAndPoll(program,
                        database,
                        service,
                        opts,
                        query_in,
                        verbose,
                        trust_defline,
                        raw_asn,
                        alparms);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CBlast_clientApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2004/05/21 21:41:38  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/09/26 20:01:13  bealer
 * - Fix compile warning.
 *
 * Revision 1.1  2003/09/26 16:53:49  bealer
 * - Add blast_client project for netblast protocol, initial code commit.
 *
 * ===========================================================================
 */
