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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *     Save a file or stdin to NetCache BLOB.
 *     Takes two CGI arguments:
 *       key=NC_KEY
 *       fmt=FORMAT  (default: "image/png")
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistre.hpp>
#include <connect/services/netcache_api.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>

USING_NCBI_SCOPE;


/// Save a file or stdin to NetCache BLOB
///
class CSaveToNetCacheApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

void CSaveToNetCacheApp::Init()
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Save To Net cache");
    
    arg_desc->AddPositional("service", 
        "NetCache service name or host:port", 
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("if", 
                             "file",
                             "intput file",
                             CArgDescriptions::eInputFile);
    
    arg_desc->AddOptionalKey("of", 
                             "file",
                             "output file",
                             CArgDescriptions::eOutputFile);
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CSaveToNetCacheApp::Run()
{
    const CArgs& args = GetArgs();
    string service = args["service"].AsString();
    CNetCacheAPI cln(service, "save_to_netcache");
    CNcbiIstream* istream = NULL;
    CNcbiOstream* ostream = NULL;
    if( args["if"] ) {
        istream = &args["if"].AsInputFile();
    } else {
        istream = &NcbiCin;
    }
    
    if( args["of"] ) {
        ostream = &args["of"].AsOutputFile();
    } else {
        ostream = &NcbiCout;
    }

    string key;
    {
    auto_ptr<IWriter> writer(cln.PutData(&key));
    if (!writer.get()) {
        ERR_POST( "Could not create a writer for \"" << service << "\" NetCache service.");
        return 1;
    }
    CWStream strm(writer.get());
    strm << istream->rdbuf();
    }
    (*ostream) << key << NcbiEndl;

    return 0;
}

int main(int argc, const char* argv[])
{
    GetDiagContext().SetOldPostFormat(false);
    return CSaveToNetCacheApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


