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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BDB cache verification utility
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>

#include <db/bdb/bdb_expt.hpp>
#include <db/bdb/bdb_blobcache.hpp>



USING_NCBI_SCOPE;



///////////////////////////////////////////////////////////////////////


/// BDB cache verify application
///
/// @internal
///
class CBDB_CacheVerifyApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

void CBDB_CacheVerifyApp::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "BDB cache verify");

    arg_desc->AddPositional("cache_path",
                            "BDB cache path",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("cache_name",
                            "BDB cache name.",
                            CArgDescriptions::eString);

    arg_desc->AddOptionalKey("errfile",
                             "error_file",
                             "File to dump error messages",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("fr", "Force environment remove");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CBDB_CacheVerifyApp::Run(void)
{
    try
    {
        CArgs args = GetArgs();
        const string& cache_path = args["cache_path"].AsString();
        const string& cache_name = args["cache_name"].AsString();

        const char* err_file = 0;
        if (args["errfile"]) {
            err_file = args["errfile"].AsString().c_str();
        }

        bool fr = args["fr"];

        CBDB_Cache cache;
        cache.Verify(cache_path.c_str(), cache_name.c_str(), err_file, fr);

    }
    catch (CBDB_ErrnoException& ex)
    {
        NcbiCerr << "Error: DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        NcbiCerr << "Error: DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CBDB_CacheVerifyApp().AppMain(argc, argv);
}
