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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * Print all the seqids in an ASN index.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_stats.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CAsnCacheApplication::


class CAsnCacheApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAsnCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Dump the contents of an ASN cache index.\n"
                              "The output consists of seq-id, version, "
                              "GI and sequnece length.");

    arg_desc->AddKey("cache", "Database",
                     "Path to ASN.1 cache",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey( "o", "GIOutputFile",
                            "If supplied, the list of seq IDs is written here.",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CAsnCacheApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CStopWatch sw;
    sw.Start();

    CDir    cache_dir( args["cache"].AsString() );
    if (! cache_dir.Exists() ) {
        LOG_POST( Error << cache_dir.GetPath() << " does not exist!" );
        return 1;
    } else if ( ! cache_dir.IsDir() ) {
        LOG_POST( Error << cache_dir.GetPath() << " does not point to a "
                    << "valid cache path!" );
        return 2;
    }

    CAsnCache   cache( cache_dir.GetPath() );
    CAsnCacheStats  cache_stats( cache );

    cache_stats.DumpSeqIds( args["o"].AsOutputFile() );
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAsnCacheApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAsnCacheApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
